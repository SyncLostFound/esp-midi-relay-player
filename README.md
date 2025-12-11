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
