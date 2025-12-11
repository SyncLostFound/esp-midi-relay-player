# esp-midi-relay-player

Little side project to make an ESP32 relay board play MIDI files.  
Totally unnecessary, kinda cursed, and **very** hard on the relays – but it sounds great in a horrible way.

This repo has two files:

- `main.cpp` – ESP32 sketch (Arduino-style) that runs a Wi-Fi AP and plays a compiled-in “song” on 8 relays.
- `midi_to_relay_notes.py` – Python script that converts a `.mid` file into that song data.

---

## ⚠ Relay abuse warning

This is **not** a good way to treat relays.

The code is happily clicking them at ~50–150+ Hz, sometimes several at once, for long periods.  
Expect:

- arcing  
- heat  
- contacts wearing / welding  
- relays dying earlier than they should

Don’t put this on anything important or expensive. Think of this as a party trick / noise box, not a serious control system.

Use at your own risk. If you cook your board, that’s on you 🙂

---

## What it does

- Uses an ESP32 relay board (I’m using the LC **ESP32_Relay_X8**).
- ESP32 runs as a tiny Wi-Fi access point:
  - **SSID:** `RelayMidi`  
  - **Password:** `relay1234`
- You connect to it and open `http://192.168.4.1/`
- Web page lets you:
  - **Play**
  - **Stop**
  - change **Speed** (50–300%)

The “music” is basically an array of notes:

```cpp
struct Note {
  uint8_t  mask;   // which relays are on (bit 0..7)
  uint32_t durMs;  // how long this time slice lasts
};

The ESP just loops through that array forever. For each entry it:

    Keeps that note active for durMs (scaled by the speed slider).

    While it’s active, it toggles the relays at different frequencies depending which bits are set in mask (so each relay is a different “pitch”).

So multiple bits set in mask = chords (multiple relays buzzing together).

The ESP32 doesn’t know anything about MIDI – it just eats these Note structs.
How the MIDI → relay conversion works

That lives in midi_to_relay_notes.py (Python, uses mido).

Rough flow:

    Load the .mid file.

    Look at ticks-per-beat and the first tempo event to work out ms per tick (defaults to 500000 µs/qn if no tempo is found).

    Walk all tracks, grab every note_on / note_off into one list with absolute tick times.

    Sort events by time.

    Step through the events, keeping a set called active = pitches currently on.

    For each region where active doesn’t change:

        calculate the duration in ticks → ms

        if active is empty → mask = 0x00 (rest)

        if not empty:

            map each pitch into one of 8 “bins” between min and max pitch

            OR them together into an 8-bit mask

At the end it prints C++:

const Note SONG_RELAY_MIDI[] = { ... };
const size_t SONG_RELAY_MIDI_LEN = sizeof(SONG_RELAY_MIDI) / sizeof(SONG_RELAY_MIDI[0]);

You paste that into main.cpp where it says:

    // paste the generated song data here

Yes, it looks horrible having a massive wall of {0x01, 120}, in the main file, but it keeps it dead simple to use with Arduino IDE or PlatformIO – no extra headers, no linking issues, just paste and flash.
Hardware

I’m using:

    Board: LC ESP32_Relay_X8 (8 relays + ESP32 on one PCB)

Relay pins in main.cpp are currently:

const uint8_t RELAY_PINS[8] = {
  32, 33, 25, 26, 27, 14, 12, 13
};

If your board is wired differently, just change those GPIO numbers at the top of the file.

Make sure you power the board the way it wants (relay supply + ESP32 supply, common ground, ect). I’m not doing anything weird in hardware, it’s just the stock board being abused.
Quick start
1. Flash the ESP32

You can use Arduino IDE or PlatformIO.

    Open main.cpp as your sketch / main file.

    Make sure the board is set to some ESP32 dev board that matches yours.

    Compile and flash.

On boot you should see a Wi-Fi AP called RelayMidi.
Connect to it, open http://192.168.4.1/ and with the little placeholder data you should already get some relay clicking.
2. Install Python tools

On your PC:

pip install mido python-rtmidi

(You only need this on the machine where you convert MIDI, not on the ESP.)
3. Convert a MIDI file

Put midi_to_relay_notes.py and your .mid file in the same folder, then run:

py midi_to_relay_notes.py your_song.mid > relay_song_snippet.h

(or python instead of py depending on your setup)

Open relay_song_snippet.h in a text editor. You’ll see the big SONG_RELAY_MIDI array and SONG_RELAY_MIDI_LEN.

    Copy those two definitions.

    Paste them into main.cpp in the “paste the generated song data here” block, replacing the dummy example.

    Don’t paste the struct Note again – the main file already has it.

Rebuild, flash again, reconnect to RelayMidi, hit Play.
Now the relays should be playing your track. Speed slider just scales all durations (50–300%) without changing the mapping.
Copyright note

I tested this with a few game MIDIs (Doom etc, obviously) and it sounds stupidly good in a very cursed way, but I’m not going to ship any converted game tracks in this repo.

The script is here, so if you have a MIDI you’re legally allowed to use, you can convert it yourself locally. That keeps the repo clean of any copyright headaches.
Random notes

    This is a toy / demo, not production-grade anything.

    If you want to be nice to your relays, don’t run it at 300% for an hour :)

    The same Note format would work for steppers / solenoids / whatever else you want to make scream.

If you build your own version, or run some cursed MIDI through it and record the noises, feel free to share – I’d actually love to hear what other people do with it.

::contentReference[oaicite:0]{index=0}

