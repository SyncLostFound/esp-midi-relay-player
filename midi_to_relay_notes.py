#!/usr/bin/env python3
import sys
import mido

# name used for the C array it spits out
ARRAY_NAME = "SONG_RELAY_MIDI"


def main():
    if len(sys.argv) < 2:
        print("usage: midi_to_relay_notes.py input.mid > relay_song.h")
        sys.exit(1)

    midi_path = sys.argv[1]
    mid = mido.MidiFile(midi_path)
    tpq = mid.ticks_per_beat

    # find a tempo, fall back to 500000us/qn if there isn't one
    tempo_us = 500000
    found_tempo = False

    for t in mid.tracks:
        for msg in t:
            if msg.type == "set_tempo":
                tempo_us = msg.tempo
                found_tempo = True
                break
        if found_tempo:
            break

    ms_per_tick = tempo_us / 1000.0 / tpq

    # collect note on/off events from ALL tracks into one list
    # (except we don't care which track, just time and pitch)
    events = []  # (tick, kind, pitch) where kind = 1 on, 0 off

    for track in mid.tracks:
        abs_tick = 0
        for msg in track:
            abs_tick += msg.time
            if msg.type == "note_on":
                if msg.velocity > 0:
                    events.append((abs_tick, 1, msg.note))
                else:
                    # note_on with vel 0 = note_off
                    events.append((abs_tick, 0, msg.note))
            elif msg.type == "note_off":
                events.append((abs_tick, 0, msg.note))

    if not events:
        print("no note events found at all", file=sys.stderr)
        sys.exit(1)

    # sort by time
    events.sort(key=lambda e: e[0])

    # get pitch range used (for mapping to 8 relays)
    pitches_seen = {p for (_, _, p) in events}
    min_p = min(pitches_seen)
    max_p = max(pitches_seen)

    def pitch_to_relay_index(pitch: int) -> int:
        if max_p == min_p:
            return 0
        norm = (pitch - min_p) / float(max_p - min_p)
        idx = int(round(norm * 7))
        if idx < 0:
            idx = 0
        if idx > 7:
            idx = 7
        return idx

    notes = []
    active = set()   # set of pitches currently "on"
    prev_tick = events[0][0]

    # walk through all events and build segments where active set is constant
    i = 0
    n_events = len(events)

    while i < n_events:
        tick = events[i][0]

        # segment from prev_tick -> tick with current "active" set
        if tick > prev_tick:
            dt_ticks = tick - prev_tick
            dur_ms = int(round(dt_ticks * ms_per_tick))
            if dur_ms > 0:
                if active:
                    # merge all active pitches into one bitmask
                    mask = 0
                    for p in active:
                        idx = pitch_to_relay_index(p)
                        mask |= (1 << idx)
                else:
                    mask = 0x00
                notes.append((mask, dur_ms))
            prev_tick = tick

        # apply all events at this tick
        while i < n_events and events[i][0] == tick:
            _, kind, pitch = events[i]
            if kind == 1:
                active.add(pitch)
            else:
                active.discard(pitch)
            i += 1

    # we ignore any trailing time after the last event on purpose

    if not notes:
        print("no usable segments built", file=sys.stderr)
        sys.exit(1)

    # spit out the C++ chunk you paste into the sketch
    print("// generated from", midi_path)
    print()
    print(f"const Note {ARRAY_NAME}[] = {{")
    for mask, dur in notes:
        print(f"  {{0x{mask:02X}, {dur}}},")
    print("};")
    print(f"const size_t {ARRAY_NAME}_LEN = sizeof({ARRAY_NAME}) / sizeof({ARRAY_NAME}[0]);")


if __name__ == "__main__":
    main()
