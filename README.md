Here’s a cleaned up README you can drop straight into `README.md`.
I’ve fixed the code fences so it won’t turn half the file into a code block, and I added the “paste the generated data directly into main.cpp, no include” bit.

---

# esp-midi-relay-player

Little side project to make an ESP32 relay board play MIDI files.
Totally unnecessary, kinda cursed, and very hard on the relays – but it sounds great in a horrible way.

This repo has two main files:

* `main.cpp` – ESP32 sketch (Arduino-style) that runs a Wi-Fi AP and plays a compiled-in “song” on 8 relays.
* `midi_to_relay_notes.py` – Python script that converts a `.mid` file into that song data.

---

## Relay abuse warning

This is **not** a nice way to treat relays.

The code happily clicks them at something like 50–150+ Hz, sometimes several at once, and for long periods. Expect:

* arcing
* heat
* contacts wearing / welding
* relays dying earlier than they should

Don’t put this on anything important or expensive. Think of it as a party trick / noise box, not a serious control system.

Use at your own risk.

---

## What it does

* Uses an ESP32 relay board (I’m using an LC ESP32_Relay_X8, 8 relays + ESP32).
* ESP32 runs as a tiny Wi-Fi access point:

  * SSID: `RelayMidi`
  * Password: `relay1234`
* You connect to it and open `http://192.168.4.1/`
* Web page lets you:

  * Play
  * Stop
  * Change speed (50–300%)

The “music” is basically an array of notes:

```cpp
struct Note {
  uint8_t  mask;   // which relays are on (bit 0..7)
  uint32_t durMs;  // how long this time slice lasts
};
```

The ESP just loops through that array forever. For each entry it:

* keeps that note active for `durMs` (scaled by the speed slider), and
* while it’s active, toggles the relays at different frequencies depending which bits are set in `mask` (so each relay is a different “pitch”).

Multiple bits set in `mask` = chords (multiple relays buzzing together).

The ESP32 doesn’t know anything about MIDI – it just eats these `Note` structs and abuses hardware accordingly.

---

## How the MIDI → relay conversion works

That part lives in `midi_to_relay_notes.py` (Python, uses `mido`).

Rough flow:

1. Load the `.mid` file.
2. Look at ticks-per-beat and the first tempo event to work out **ms per tick** (defaults to 500000 µs/qn if no tempo event is found).
3. Walk *all* tracks, grab every `note_on` / `note_off` into one list with absolute tick times.
4. Sort events by time.
5. Step through the events, keeping a set called `active` = pitches currently on.
6. For each span where `active` doesn’t change:

   * calculate the duration in ticks → ms
   * if `active` is empty → `mask = 0x00` (rest)
   * if not empty:

     * map each pitch into one of 8 “bins” between min and max pitch
     * OR them together into an 8-bit mask

At the end it prints C++ like:

```cpp
const Note SONG_RELAY_MIDI[] = { ... };
const size_t SONG_RELAY_MIDI_LEN = sizeof(SONG_RELAY_MIDI) / sizeof(SONG_RELAY_MIDI[0]);
```

You copy those two definitions into `main.cpp` where it says:

```cpp
// paste the generated song data here
```

No includes, no extra files – just dump that array straight into the main file.
Yes, it looks ugly having a giant wall of `{0x01, 120},` in there, but it keeps it dead simple for Arduino IDE / PlatformIO.

---

## Hardware

I’m using:

* Board: LC ESP32_Relay_X8 (8 relays + ESP32 on one PCB)

Relay pins in `main.cpp` are currently:

```cpp
const uint8_t RELAY_PINS[8] = {
  32, 33, 25, 26, 27, 14, 12, 13
};
```

If your board is wired differently, just change those GPIO numbers at the top of the file.

Make sure you power the board the way it wants (relay supply + ESP32 supply, common ground, etc). The hardware here is just stock board wiring being abused.

---

## Quick start

### 1. Flash the ESP32

You can use Arduino IDE or PlatformIO.

* Open `main.cpp` as your sketch / main file.
* Before you build, generate a song and paste the `SONG_RELAY_MIDI` array + `SONG_RELAY_MIDI_LEN` directly into the “paste the generated song data here” section in `main.cpp`.

  * Don’t try to `#include` it as a separate header – the sketch expects the data to live right in the main file.
* Make sure the board is set to some ESP32 dev board that matches yours.
* Compile and flash.

On boot you should see a Wi-Fi AP called `RelayMidi`.
Connect to it, open `http://192.168.4.1/` and with the placeholder / test data you should already get some relay clicking.

---

### 2. Install Python tools

On your PC:

```bash
pip install mido python-rtmidi
```

(You only need this on the machine where you convert MIDI, not on the ESP.)

---

### 3. Convert a MIDI file

Put `midi_to_relay_notes.py` and your `.mid` file in the same folder, then run:

```bash
py midi_to_relay_notes.py your_song.mid > relay_song_snippet.h
# or:
python midi_to_relay_notes.py your_song.mid > relay_song_snippet.h
```

Open `relay_song_snippet.h` in a text editor. You’ll see:

* the big `SONG_RELAY_MIDI[]` array
* the `SONG_RELAY_MIDI_LEN` definition

Do this:

1. Copy those two definitions.
2. Paste them into `main.cpp` in the “paste the generated song data here” block, replacing the dummy example.
3. Don’t paste `struct Note` again – the main file already has it.

Rebuild, flash again, reconnect to `RelayMidi`, hit Play.
Now the relays should be playing your track. The speed slider just scales all durations (50–300%) without changing the mapping.

---

## Copyright note

I tested this with a few game MIDIs (Doom etc, obviously) and it sounds stupidly good in a very cursed way, but I’m not going to ship any converted game tracks in this repo.

The script is here so if you have a MIDI you’re legally allowed to use, you can convert it yourself locally. That keeps the repo clean of any copyright headaches.

---

## Random notes

* This is a toy / demo, not production-grade anything.
* If you want to be nice to your relays, don’t run it at 300% for an hour.
* The same `Note` format would work for steppers, solenoids, etc, if you want to make other hardware scream.

If you build your own version, or run some cursed MIDI through it and record the noises, I’d actually love to hear what you do with it.
