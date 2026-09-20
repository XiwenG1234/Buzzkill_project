#pragma once
// Audio/Visual subsystem for Buzzkill (ESP32-C6)
// Provides user feedback and decoration: turn-indicator/decorative LEDs
// and a speaker for background music, turn-signal sounds, and effects.
// TODO: finalize LED type (addressable e.g. NeoPixel vs discrete) and
// speaker driver (passive buzzer via PWM vs I2S/DAC audio module).

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

// Call once from setup() to configure LED/speaker pins or peripherals.
void init();

// Call each loop() iteration to service blinking LEDs / non-blocking audio.
void update();

// --- Visual (LEDs) ---

// Set a solid color on the decorative/status LEDs.
void setIndicatorColor(uint8_t r, uint8_t g, uint8_t b);

// Turn indicator lights on/off/blinking, mirroring the manual-mode
// direction commands from the smartphone app.
void setTurnSignal(TurnSignal signal);

// --- Audio (speaker) ---

// Start/stop looping background music.
void playBackgroundMusic();
void stopBackgroundMusic();

// Play a short predefined sound effect (non-blocking).
void playSoundEffect(SoundEffect effect);

// Silence all audio output immediately.
void stopAudio();

} // namespace AudioVisual
