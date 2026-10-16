#include "AudioVisual.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

namespace AudioVisual {

namespace {

// --- Pin wiring --
constexpr int LIGHT_STRIP_PIN = 3; // WS2812/NeoPixel strip DIN
constexpr int PIN_AMP_IN = 4;      // PAM8904 IN
constexpr int PIN_OLED_SCL = 6;
constexpr int PIN_OLED_SDA = 7;

// --- Light strip ---
Adafruit_NeoPixel strip(NUM_STRIP_LEDS, LIGHT_STRIP_PIN, NEO_GRB + NEO_KHZ800);

bool audioLightShowActive = false;
unsigned long audioLightShowStartMs = 0;
unsigned long soundEffectDurationMs = 0; // 0 = indefinite (background music)

// One distinct color per LED, used while audio is playing.
uint32_t audioLightShowColors[NUM_STRIP_LEDS];

// --- Direction arrows ---
// LED indices (0 to NUM_STRIP_LEDS - 1) lit for each direction.
// TODO: replace these placeholders with the LEDs that form each arrow.
const int LEFT_ARROW_LEDS[]     = {2, 8, 10, 11, 12, 13, 14, 18, 22};
const int RIGHT_ARROW_LEDS[]    = {2, 6, 10, 11, 12, 13, 14, 16, 22};
const int FORWARD_ARROW_LEDS[]  = {2, 7, 10, 12, 14, 16, 17, 18, 22};
const int BACKWARD_ARROW_LEDS[] = {2, 6, 7, 8, 10, 12, 14, 17, 22};
const int STOP_LEDS[]           = {10, 11, 12, 13, 14};

struct DirectionPattern {
    const char *name;
    const int *leds;
    int numLeds;
    uint8_t r, g, b;
};

template <int N>
constexpr DirectionPattern makePattern(const char *name, const int (&leds)[N],
                                       uint8_t r, uint8_t g, uint8_t b) {
    return {name, leds, N, r, g, b};
}

// Must be in the same order as the Direction enum. Only red/orange/yellow/green:
// blue is too dim with the strip on 3.3V.
const DirectionPattern directionPatterns[] = {
    makePattern("left",     LEFT_ARROW_LEDS,     255, 80, 0),    // orange
    makePattern("right",    RIGHT_ARROW_LEDS,    255, 80, 0),    // orange
    makePattern("forward",  FORWARD_ARROW_LEDS,  0, 255, 0),     // green
    makePattern("backward", BACKWARD_ARROW_LEDS, 255, 200, 0),   // yellow
    makePattern("stop",     STOP_LEDS,           255, 0, 0),     // red
};

// --- OLED ---
constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;
const uint8_t OLED_I2C_ADDRS[] = {0x3C, 0x3D}; // SSD1306 addresses, tried in order

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool oledReady = false;
unsigned long lastOledProbeMs = 0;
constexpr unsigned long OLED_RETRY_MS = 2000; // re-probe a missing OLED this often

// --- Speaker amp ---
// The PAM8904 is a piezo sounder driver, not a linear audio amp: it's
// driven with a digital square wave at the desired tone frequency
// (via LEDC/PWM), and it boosts + drives the piezo speaker differentially.
// If SHDN/gain-select pins are wired to GPIOs rather than hardwired,
// they'll need to be set separately (SHDN driven active to enable output).
constexpr int AMP_PWM_RESOLUTION = 8; // bits; 50% duty via ledcWriteTone gives a clean square wave
constexpr int AMP_INIT_FREQ_HZ = 1000;

// Tones swept by runToneSweepTest() (Hz).
const int testTones[] = {262, 330, 392, 440, 523, 880, 1047};

// Note frequencies (Hz) for playNote(): index 1-7 = do re mi fa sol la ti
// (C major, C5 octave); index 0 = rest.
const int NOTE_HZ[] = {0, 523, 587, 659, 698, 784, 880, 988};

// Frequency for a numbered note: 1-7 = C5 octave, -1 to -7 = C4 octave
// (half the frequency), anything else = rest (0).
int noteToHz(int note) {
    if (note >= 1 && note <= 7) return NOTE_HZ[note];
    if (note <= -1 && note >= -7) return NOTE_HZ[-note] / 2;
    return 0;
}

// Background music: Ode to Joy (Beethoven) in C major.
// {note, length}: note 1-7 = do-ti, -5 = low sol, 0 = rest; length in
// eighth notes (1 = eighth, 2 = quarter, 3 = dotted quarter, 4 = half).
const int ODE_TO_JOY[][2] = {
    // 3 3 4 5 | 5 4 3 2 | 1 1 2 3 | 3. 2 2 -
    {3, 2}, {3, 2}, {4, 2}, {5, 2},
    {5, 2}, {4, 2}, {3, 2}, {2, 2},
    {1, 2}, {1, 2}, {2, 2}, {3, 2},
    {3, 3}, {2, 1}, {2, 4},

    // 3 3 4 5 | 5 4 3 2 | 1 1 2 3 | 2. 1 1 -
    {3, 2}, {3, 2}, {4, 2}, {5, 2},
    {5, 2}, {4, 2}, {3, 2}, {2, 2},
    {1, 2}, {1, 2}, {2, 2}, {3, 2},
    {2, 3}, {1, 1}, {1, 4},

    // Short rest before the song loops.
    {0, 4},
};

// Background music tempo: ms per eighth note (150 = 300 ms per beat);
// smaller = faster, bigger = slower.
constexpr int BACKGROUND_EIGHTH_MS = 150;

// --- Background melody state (serviced by update()) ---
const int (*melody)[2] = nullptr;
int melodyCount = 0;
int melodyIndex = 0;
int melodyEighthMs = 200;
bool melodyLoop = false;
unsigned long melodyNoteStartMs = 0;
unsigned long melodyNoteMs = 0;
bool melodyInGap = false;

// Gap of silence at the end of each note, so repeated notes sound separate.
unsigned long noteGapMs(unsigned long noteMs) {
    return noteMs / 10;
}

void startMelodyNote(unsigned long now) {
    int note = melody[melodyIndex][0];
    melodyNoteMs = (unsigned long)melody[melodyIndex][1] * melodyEighthMs;
    melodyNoteStartMs = now;
    melodyInGap = false;
    int hz = noteToHz(note);
    if (hz > 0) {
        ledcWriteTone(PIN_AMP_IN, hz);
    } else {
        ledcWriteTone(PIN_AMP_IN, 0);
    }
}

void serviceMelody() {
    if (melody == nullptr) return;
    unsigned long now = millis();
    unsigned long elapsed = now - melodyNoteStartMs;

    if (elapsed < melodyNoteMs) {
        if (!melodyInGap && elapsed >= melodyNoteMs - noteGapMs(melodyNoteMs)) {
            ledcWriteTone(PIN_AMP_IN, 0);
            melodyInGap = true;
        }
        return;
    }

    // Next note starts when this one was *scheduled* to end, not "now", so a
    // late update() (e.g. during an OLED refresh) doesn't stretch the tempo.
    // If we fell more than a whole note behind, resync to now instead.
    unsigned long nextStartMs = melodyNoteStartMs + melodyNoteMs;
    if (now - nextStartMs > melodyNoteMs) {
        nextStartMs = now;
    }

    melodyIndex++;
    if (melodyIndex >= melodyCount) {
        if (!melodyLoop) {
            melody = nullptr;
            ledcWriteTone(PIN_AMP_IN, 0);
            return;
        }
        melodyIndex = 0;
        Serial.println("Melody: looping back to start");
    }
    startMelodyNote(nextStartMs);
}
constexpr int numTestTones = sizeof(testTones) / sizeof(testTones[0]);

void initAudioLightShowColors() {
    // Spread one full rainbow (red -> purple -> pink) across the strip,
    // so this works for any NUM_STRIP_LEDS.
    for (int i = 0; i < NUM_STRIP_LEDS; i++) {
        uint16_t hue = (uint32_t)i * 65536 / NUM_STRIP_LEDS;
        audioLightShowColors[i] = strip.gamma32(strip.ColorHSV(hue));
    }
}

void showAudioLightShow() {
    for (int i = 0; i < NUM_STRIP_LEDS; i++) {
        strip.setPixelColor(i, audioLightShowColors[i]);
    }
    strip.show();
    audioLightShowActive = true;
    audioLightShowStartMs = millis();
}

void initStrip() {
    strip.begin();
    // 25 LEDs at full brightness can pull ~1.5 A - keep this modest until
    // the power budget for the light strip is confirmed.
    strip.setBrightness(80); // 0-255
    strip.clear();
    strip.show();
    initAudioLightShowColors();
}

// Looks for the OLED and starts it if found. Quiet on failure so it can be
// retried from showOledStatus() - lets the OLED be plugged in after boot.
bool connectOled() {
    lastOledProbeMs = millis();

    // display.begin() reports success even with no OLED attached, so check
    // that something actually ACKs first - otherwise every showOledStatus()
    // call floods Serial with i2cWrite() errors.
    uint8_t addr = 0;
    for (uint8_t candidate : OLED_I2C_ADDRS) {
        Wire.beginTransmission(candidate);
        if (Wire.endTransmission() == 0) {
            addr = candidate;
            break;
        }
    }
    if (addr == 0) {
        return false;
    }

    if (!display.begin(SSD1306_SWITCHCAPVCC, addr)) {
        Serial.println("OLED init FAILED - out of memory for display buffer");
        return false;
    }
    oledReady = true;
    Serial.printf("OLED init OK at 0x%02X\n", addr);
    return true;
}

void initOled() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    // 400 kHz: a full screen update takes ~25 ms instead of ~100 ms, so it
    // barely delays a background melody's note timing.
    Wire.setClock(400000);
    if (connectOled()) {
        showOledStatus("OLED: OK");
    } else {
        Serial.println("OLED not found at 0x3C/0x3D - check SDA->GPIO7, SCL->GPIO6, VCC, GND. Will keep retrying.");
    }
}

void initAmp() {
    if (!ledcAttach(PIN_AMP_IN, AMP_INIT_FREQ_HZ, AMP_PWM_RESOLUTION)) {
        Serial.println("ledcAttach FAILED on amp pin");
    }
    ledcWriteTone(PIN_AMP_IN, 0); // stay silent until something plays
}

} // namespace

void init() {
    initStrip();
    initOled();
    initAmp();

    // TODO: configure turn-signal LED pins
}

void update() {
    // Auto-clear a timed sound effect's light show once it's done playing.
    if (audioLightShowActive && soundEffectDurationMs > 0) {
        if (millis() - audioLightShowStartMs >= soundEffectDurationMs) {
            clearLightStrip();
            soundEffectDurationMs = 0;
        }
    }

    serviceMelody();

    // TODO: service turn-signal blink timing
}

void setIndicatorColor(uint8_t r, uint8_t g, uint8_t b) {
    // TODO: implement single-color status LED set (separate from the light strip)
}

void setTurnSignal(TurnSignal signal) {
    // TODO: implement left/right/hazard blinking pattern
}

void setStripPixel(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 0 || index >= NUM_STRIP_LEDS) {
        return;
    }
    strip.setPixelColor(index, strip.Color(r, g, b));
    strip.show();
}

void setStripColor(uint8_t r, uint8_t g, uint8_t b) {
    strip.fill(strip.Color(r, g, b));
    strip.show();
}

void clearLightStrip() {
    strip.clear();
    strip.show();
    audioLightShowActive = false;
}

void showColorWheel(int count) {
    if (count > NUM_STRIP_LEDS) count = NUM_STRIP_LEDS;
    strip.clear();
    for (int i = 0; i < count; i++) {
        uint16_t hue = (uint32_t)i * 65536 / count;
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue)));
    }
    strip.show();
    audioLightShowActive = false;
}

void showDirection(Direction dir) {
    const DirectionPattern &p = directionPatterns[static_cast<int>(dir)];
    strip.clear();
    for (int i = 0; i < p.numLeds; i++) {
        if (p.leds[i] >= 0 && p.leds[i] < NUM_STRIP_LEDS) {
            strip.setPixelColor(p.leds[i], strip.Color(p.r, p.g, p.b));
        }
    }
    strip.show();
    audioLightShowActive = false; // arrow replaces any running light show
}

void showLeftArrow()     { showDirection(Direction::LEFT); }
void showRightArrow()    { showDirection(Direction::RIGHT); }
void showForwardArrow()  { showDirection(Direction::FORWARD); }
void showBackwardArrow() { showDirection(Direction::BACKWARD); }
void showStopSign()      { showDirection(Direction::STOP); }

void showOledStatus(const String &line1, const String &line2) {
    if (!oledReady) {
        // Retry now and then, in case the OLED was plugged in after boot.
        if (millis() - lastOledProbeMs < OLED_RETRY_MS || !connectOled()) {
            return;
        }
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Buzzkill");
    display.println("----------------");
    display.println(line1);
    if (line2.length()) {
        display.println(line2);
    }
    display.display();
}

void scanI2C() {
    Serial.println("Scanning I2C bus...");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  Found device at 0x%02X\n", addr);
            found++;
        }
    }
    if (found == 0) {
        Serial.println("  No I2C devices found - check wiring/pull-ups");
    }
}

void playTone(int freqHz) {
    ledcWriteTone(PIN_AMP_IN, freqHz);
}

void stopTone() {
    ledcWriteTone(PIN_AMP_IN, 0);
}

void playNote(int note, int durationMs) {
    int hz = noteToHz(note);
    if (hz > 0) {
        playTone(hz);
    } else {
        stopTone(); // 0 (or anything out of range) = rest
    }
    int gapMs = noteGapMs(durationMs);
    delay(durationMs - gapMs);
    stopTone();
    delay(gapMs);
}

void playSong(const int *notes, int count, int noteMs) {
    for (int i = 0; i < count; i++) {
        playNote(notes[i], noteMs);
    }
}

void startMelody(const int (*notes)[2], int count, int eighthMs, bool loop) {
    if (notes == nullptr || count <= 0) return;
    melody = notes;
    melodyCount = count;
    melodyIndex = 0;
    melodyEighthMs = eighthMs;
    melodyLoop = loop;
    startMelodyNote(millis());
}

void stopMelody() {
    melody = nullptr;
    stopTone();
}

bool isMelodyPlaying() {
    return melody != nullptr;
}

void playBackgroundMusic() {
    startMelody(ODE_TO_JOY, BACKGROUND_EIGHTH_MS);
}

void stopBackgroundMusic() {
    stopMelody();
}

void playSoundEffect(SoundEffect effect) {
    soundEffectDurationMs = 400; // TODO: tune per effect once real audio is wired up
    showAudioLightShow();
    // TODO: implement sound effect playback, dispatch on `effect`
}

void stopAudio() {
    stopMelody();
    clearLightStrip();
    soundEffectDurationMs = 0;
}

void runStripChaseTest() {
    const uint8_t colors[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    const char *names[] = {"red", "green", "blue"};
    for (int c = 0; c < 3; c++) {
        Serial.printf("Strip chase: %s\n", names[c]);
        showOledStatus("Strip: chase", names[c]);
        for (int i = 0; i < NUM_STRIP_LEDS; i++) {
            clearLightStrip();
            setStripPixel(i, colors[c][0], colors[c][1], colors[c][2]);
            delay(100);
        }
    }
    clearLightStrip();
}

void runToneSweepTest() {
    // Rainbow light show stays on for the whole tone sweep.
    showAudioLightShow();
    for (int i = 0; i < numTestTones; i++) {
        int freq = testTones[i];
        Serial.printf("Playing %d Hz through PAM8904\n", freq);
        showOledStatus("Amp: playing", String(freq) + " Hz");

        playTone(freq);
        delay(400);
        stopTone();
        delay(150);
    }
    clearLightStrip();
}

void runDirectionTest() {
    const Direction dirs[] = {Direction::LEFT, Direction::RIGHT, Direction::FORWARD,
                              Direction::BACKWARD, Direction::STOP};
    for (Direction dir : dirs) {
        const char *name = directionPatterns[static_cast<int>(dir)].name;
        Serial.printf("Direction: %s\n", name);
        //showOledStatus("Strip: direction", name);
        showDirection(dir);
        delay(1500);
    }
    clearLightStrip();
}

void runStripColorTest() {
    const uint8_t colors[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {0, 0, 0}};
    const char *names[] = {"red", "green", "blue", "off"};
    for (int c = 0; c < 4; c++) {
        Serial.printf("Strip color: %s\n", names[c]);
        setStripColor(colors[c][0], colors[c][1], colors[c][2]);
        delay(1000);
    }
}

} // namespace AudioVisual
