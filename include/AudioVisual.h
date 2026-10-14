#pragma once
// Audio/Visual subsystem for Buzzkill (ESP32-C6)
// Provides user feedback and decoration: turn-indicator/decorative LEDs,
// a 25-LED addressable light strip, and a speaker for background music,
// turn-signal sounds, and effects.
// TODO: finalize discrete LED type for turn signals (the light strip is
// now a WS2812/NeoPixel strip) and speaker driver (PAM8904, see hwtest_main.cpp).

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

// Number of LEDs on the addressable light strip.
constexpr int NUM_STRIP_LEDS = 25;

// Call once from setup() to configure LED/speaker pins or peripherals.
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

// Set a single pixel on the light strip. Takes effect on the next
// strip.show() (done automatically by update()/the audio light show).
void setStripPixel(int index, uint8_t r, uint8_t g, uint8_t b);

// Turn all light strip LEDs off.
void clearLightStrip();

// --- Audio (speaker) ---

// Start/stop looping background music. Lights the strip with a
// different color per LED for as long as audio is playing (see update()).
void playBackgroundMusic();
void stopBackgroundMusic();

// Play a short predefined sound effect (non-blocking). Also lights the
// strip for the duration of the effect.
void playSoundEffect(SoundEffect effect);

// Silence all audio output immediately and clear the light strip.
void stopAudio();

} // namespace AudioVisual
