#pragma once
// Audio/Visual subsystem for Buzzkill (ESP32-C6)
// Provides user feedback and decoration: turn-indicator/decorative LEDs,
// a 25-LED addressable light strip, an SSD1306 OLED status display, and a
// PAM8904-driven speaker for background music, turn-signal sounds, and effects.
//
// Wiring:
//   OLED (SSD1306, I2C)  SCL -> GPIO6, SDA -> GPIO7
//   PAM8904 amplifier    IN  -> GPIO4 (square-wave tone input)
//   NeoPixel light strip DIN -> GPIO3
// TODO: finalize discrete LED type for turn signals.

#include "Arduino.h"

namespace AudioVisual {

enum class TurnSignal {
    NONE,
    LEFT,
    RIGHT,
    HAZARD
};

enum class SoundEffect {
    STARTUP,
    MODE_CHANGE,
    TURN_SIGNAL_TICK,
    ERROR
};

// Drive direction shown as an arrow on the light strip.
enum class Direction {
    LEFT,
    RIGHT,
    FORWARD,
    BACKWARD,
    STOP
};

// Number of LEDs on the addressable light strip.
constexpr int NUM_STRIP_LEDS = 25;

// Call once from setup() (after Serial.begin()) to set up the light strip,
// OLED and speaker amp. Prints init results to Serial.
void init();

// Call each loop() iteration to service blinking LEDs / the light strip
// animation / any non-blocking audio.
void update();

// --- Visual (single-color status LEDs) ---

// Set a solid color on the decorative/status LEDs.
void setIndicatorColor(uint8_t r, uint8_t g, uint8_t b);

// Turn indicator lights on/off/blinking, mirroring the manual-mode
// direction commands from the smartphone app.
void setTurnSignal(TurnSignal signal);

// --- Visual (25-LED addressable light strip) ---

// Set a single pixel on the light strip (shown immediately).
void setStripPixel(int index, uint8_t r, uint8_t g, uint8_t b);

// Set every pixel on the light strip to one color (shown immediately).
void setStripColor(uint8_t r, uint8_t g, uint8_t b);

// Turn all light strip LEDs off.
void clearLightStrip();

// Light the first `count` LEDs, each a different color spread evenly
// around the color wheel (red -> yellow -> green -> blue -> purple).
// LEDs past `count` are turned off.
void showColorWheel(int count);

// Show the arrow for a drive direction on the light strip (replaces
// whatever was showing). Arrow shapes/colors are set in AudioVisual.cpp.
void showDirection(Direction dir);

// One function per arrow (same as showDirection(Direction::...)).
void showLeftArrow();
void showRightArrow();
void showForwardArrow();
void showBackwardArrow();
void showStopSign();

// --- Visual (OLED display) ---

// Show a title plus up to two status lines on the OLED. Does nothing if
// the OLED failed to init.
void showOledStatus(const String &line1, const String &line2 = "");

// Prints any devices found on the I2C bus. Handy if the OLED doesn't
// show up at its expected address - run this to find its real address.
void scanI2C();

// --- Audio (speaker) ---

// Play a continuous square-wave tone through the PAM8904 until
// stopTone()/stopAudio() is called.
void playTone(int freqHz);
void stopTone();

// Notes in numbered notation: 1 = do, 2 = re, 3 = mi, 4 = fa, 5 = sol,
// 6 = la, 7 = ti (C major, C5 octave), 0 = rest. Negative = one octave
// lower (e.g. -5 = low sol).
// Plays one note for durationMs (blocking), with a short gap at the end so
// repeated notes sound separate.
void playNote(int note, int durationMs);

// Plays a list of notes, each noteMs long (blocking), e.g.
//   const int song[] = {3, 3, 4, 5, 5, 4, 3, 2};
//   AudioVisual::playSong(song);
// (Pointer + count versions take no defaults on purpose: otherwise a call
// like playSong(song, 250) would match them with 250 as the note count.)
void playSong(const int *notes, int count, int noteMs);

template <size_t N>
void playSong(const int (&notes)[N], int noteMs = 400) {
    playSong(notes, N, noteMs);
}

// --- Background melody (non-blocking) ---
// A melody is a list of {note, length} pairs; length is in eighth notes
// (1 = eighth, 2 = quarter, 3 = dotted quarter, 4 = half). e.g.
//   const int song[][2] = {{3, 2}, {3, 2}, {4, 2}, {5, 2}};
//   AudioVisual::startMelody(song);
// Keeps playing in the background as long as update() is called every
// loop() - don't use delay() in loop() while a melody plays.
void startMelody(const int (*notes)[2], int count, int eighthMs, bool loop);

template <size_t N>
void startMelody(const int (&notes)[N][2], int eighthMs = 200, bool loop = true) {
    startMelody(notes, N, eighthMs, loop);
}

void stopMelody();
bool isMelodyPlaying();

// Start/stop looping background music (Ode to Joy, set in AudioVisual.cpp).
// Non-blocking: keeps playing as long as update() is called every loop().
void playBackgroundMusic();
void stopBackgroundMusic();

// Play a short predefined sound effect (non-blocking). Also lights the
// strip for the duration of the effect.
void playSoundEffect(SoundEffect effect);

// Silence all audio output immediately and clear the light strip.
void stopAudio();

// --- Hardware bring-up tests (blocking; for hwtest_main.cpp) ---

// Lights each strip LED in turn (red, green, blue) to check every pixel
// and the GRB color order.
void runStripChaseTest();

// Sweeps a set of tones through the speaker with the rainbow light show on.
void runToneSweepTest();

// Shows each direction arrow in turn, to check the arrow shapes.
void runDirectionTest();

// Fills the whole strip red -> green -> blue -> off (1 s each), to check
// that the strip accepts new data at all.
void runStripColorTest();

} // namespace AudioVisual
