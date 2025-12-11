#include <WiFi.h>
#include <WebServer.h>

// simple wifi relay midi player

// wifi AP details
const char *AP_SSID = "RelayMidi";
const char *AP_PASS = "relay1234";

// relay board wiring (LC ESP32_Relay_X8)
const uint8_t RELAY_PINS[8] = {
  32, // relay 1
  33, // relay 2
  25, // relay 3
  26, // relay 4
  27, // relay 5
  14, // relay 6
  12, // relay 7
  13  // relay 8
};

const uint8_t STATUS_LED_PIN = 23;

// one "note" in the song
struct Note {
  uint8_t  mask;    // which relays are buzzing (bit 0..7)
  uint32_t durMs;   // nominal duration in ms (can be large)
};

// ------------------------------------------------------------------
// paste the generated song data here
//
// from the python script, copy ONLY:
//
//   const Note SONG_RELAY_MIDI[] = { ... };
//   const size_t SONG_RELAY_MIDI_LEN = ...;
//
// don’t copy the struct Note again.
// ------------------------------------------------------------------

// example placeholder, comment this out once you paste the real one
/*
const Note SONG_RELAY_MIDI[] = {
  {0x02, 200},
  {0x04, 200},
  {0x08, 200},
  {0x10, 200},
  {0x00, 400}
};
const size_t SONG_RELAY_MIDI_LEN = sizeof(SONG_RELAY_MIDI) / sizeof(SONG_RELAY_MIDI[0]);
*/
// ------------------------------------------------------------------

// playback state
const Note *currentSong    = nullptr;
size_t      currentSongLen = 0;

size_t       currentNoteIndex = 0;
unsigned long noteStartMs      = 0;
unsigned long lastToggleUs     = 0;
bool          relayPhase       = false;
bool          isPlaying        = false;

// 50–300% speed from the slider
int speedPercent = 150;

// per-relay "pitch" in Hz-ish. tweak by ear.
const uint16_t NOTE_FREQS[8] = {
  45,  55,  70,  85,
  100, 120, 140, 160
};

WebServer server(80);

// ------------------------------------------------------------------
// helpers

static void applyMask(uint8_t mask) {
  for (int i = 0; i < 8; ++i) {
    bool on = (mask >> i) & 0x01;
    digitalWrite(RELAY_PINS[i], on ? HIGH : LOW);  // active high
  }
}

static void allOff() {
  applyMask(0);
}

// first relay bit set decides the "pitch"
static uint16_t maskToFreq(uint8_t mask) {
  for (int i = 0; i < 8; ++i) {
    if (mask & (1 << i)) {
      return NOTE_FREQS[i];
    }
  }
  return 0;
}

// simple duration scaling from slider
static unsigned long scaledDurationMs(uint32_t baseMs) {
  // use 64-bit math so long durations + scaling don’t overflow
  uint64_t d = (uint64_t)baseMs * 100ULL / (uint64_t)speedPercent;
  if (d < 30) d = 30;
  // optional: clamp silly huge values (e.g. > 10 minutes)
  if (d > 600000UL) d = 600000UL;
  return (unsigned long)d;
}

// ------------------------------------------------------------------
// very small web ui

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Relay MIDI Player</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: sans-serif; background:#111; color:#eee; text-align:center; }
h1   { margin-top:20px; }
button {
  font-size:16px; padding:8px 16px; margin:6px;
  border:none; border-radius:4px; cursor:pointer;
}
#play { background:#28a745; color:#fff; }
#stop { background:#dc3545; color:#fff; }
.card {
  display:inline-block; margin-top:20px; padding:16px 20px;
  background:#222; border-radius:8px; box-shadow:0 0 8px #000;
}
input[type=range] { width:80%; }
small { color:#aaa; }
</style>
</head>
<body>
<h1>ESP32 Relay MIDI Player</h1>
<div class="card">
  <p>
    <button id="play" onclick="playSong()">Play</button>
    <button id="stop" onclick="stopPlay()">Stop</button>
  </p>
  <p>
    Speed:
    <input id="speed" type="range" min="50" max="300" value="150"
           oninput="updateSpeed(this.value)">
    <span id="speedLabel">150%</span>
  </p>
  <p><small>Song is generated from a MIDI file. All 8 relays are used as “pitches”.</small></p>
</div>
<script>
function playSong() {
  const spd = document.getElementById('speed').value;
  fetch('/play?speed=' + spd).catch(()=>{});
}
function stopPlay() {
  fetch('/stop').catch(()=>{});
}
function updateSpeed(v) {
  document.getElementById('speedLabel').innerText = v + '%';
  fetch('/speed?value=' + v).catch(()=>{});
}
</script>
</body>
</html>
)rawliteral";

static void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handlePlay() {
  // make sure song data is actually there
  if (!SONG_RELAY_MIDI_LEN) {
    server.send(500, "text/plain", "No song data compiled in.");
    return;
  }

  if (server.hasArg("speed")) {
    speedPercent = constrain(server.arg("speed").toInt(), 50, 300);
  }

  currentSong      = SONG_RELAY_MIDI;
  currentSongLen   = SONG_RELAY_MIDI_LEN;
  currentNoteIndex = 0;
  noteStartMs      = millis();
  lastToggleUs     = micros();
  relayPhase       = false;
  isPlaying        = true;

  digitalWrite(STATUS_LED_PIN, HIGH);
  server.send(200, "text/plain", "Playing song");
}

static void handleStop() {
  isPlaying = false;
  allOff();
  digitalWrite(STATUS_LED_PIN, LOW);
  server.send(200, "text/plain", "Stopped");
}

static void handleSpeed() {
  if (server.hasArg("value")) {
    speedPercent = constrain(server.arg("value").toInt(), 50, 300);
  }
  server.send(200, "text/plain", "Speed updated");
}

static void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ------------------------------------------------------------------

static void setupRelays() {
  for (int i = 0; i < 8; ++i) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
}

static void setupWiFiAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

static void setupServer() {
  server.on("/",      HTTP_GET, handleRoot);
  server.on("/play",  HTTP_GET, handlePlay);
  server.on("/stop",  HTTP_GET, handleStop);
  server.on("/speed", HTTP_GET, handleSpeed);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void setup() {
  Serial.begin(115200);
  delay(300);

  setupRelays();
  setupWiFiAP();
  setupServer();

  speedPercent = 150;
  Serial.println("Relay MIDI player ready.");
}

void loop() {
  server.handleClient();

  if (!isPlaying || currentSong == nullptr || currentSongLen == 0) {
    return;
  }

  unsigned long nowMs = millis();
  unsigned long nowUs = micros();

  const Note &n = currentSong[currentNoteIndex];
  unsigned long targetDur = scaledDurationMs(n.durMs);

  // go to next note?
  if (nowMs - noteStartMs >= targetDur) {
    currentNoteIndex++;
    if (currentNoteIndex >= currentSongLen) {
      // loop whole track
      currentNoteIndex = 0;
    }
    noteStartMs = nowMs;
    relayPhase  = false;
    allOff();
    lastToggleUs = nowUs;
    return;
  }

  // rest
  if (n.mask == 0) {
    allOff();
    return;
  }

  // active note: toggle relays at their "pitch" freq
  uint16_t freq = maskToFreq(n.mask);
  if (freq == 0) {
    allOff();
    return;
  }

  uint32_t periodUs = 1000000UL / freq;
  if (periodUs < 2000) periodUs = 2000;   // ~500 Hz max
  uint32_t halfPeriod = periodUs / 2;

  if (nowUs - lastToggleUs >= halfPeriod) {
    lastToggleUs = nowUs;
    relayPhase   = !relayPhase;
    applyMask(relayPhase ? n.mask : 0);
  }
}
