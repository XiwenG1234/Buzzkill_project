#include "AudioVisual.h"
#include <Adafruit_NeoPixel.h>

namespace AudioVisual {

namespace {

// WS2812/NeoPixel light strip data line.
// TODO: confirm GPIO against the final pin map (GPIO4 = amp IN,
// GPIO6/7 = OLED SCL/SDA are already taken).
constexpr int LIGHT_STRIP_PIN = 3;

Adafruit_NeoPixel strip(NUM_STRIP_LEDS, LIGHT_STRIP_PIN, NEO_GRB + NEO_KHZ800);

bool audioLightShowActive = false;
unsigned long audioLightShowStartMs = 0;
unsigned long soundEffectDurationMs = 0; // 0 = indefinite (background music)

// One distinct color per LED, used while audio is playing.
uint32_t audioLightShowColors[NUM_STRIP_LEDS];

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

} // namespace

void init() {
    strip.begin();
    // 25 LEDs at full brightness can pull ~1.5 A - keep this modest until
    // the power budget for the light strip is confirmed.
    strip.setBrightness(80); // 0-255
    strip.clear();
    strip.show();
    initAudioLightShowColors();

    // TODO: configure turn-signal LED pins and speaker/amp pins/peripherals
}

void update() {
    // Auto-clear a timed sound effect's light show once it's done playing.
    if (audioLightShowActive && soundEffectDurationMs > 0) {
        if (millis() - audioLightShowStartMs >= soundEffectDurationMs) {
            clearLightStrip();
            soundEffectDurationMs = 0;
        }
    }

    // TODO: service turn-signal blink timing and any non-blocking audio playback
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

void clearLightStrip() {
    strip.clear();
    strip.show();
    audioLightShowActive = false;
}

void playBackgroundMusic() {
    soundEffectDurationMs = 0; // runs until stopBackgroundMusic()/stopAudio()
    showAudioLightShow();
    // TODO: implement background music playback (loop)
}

void stopBackgroundMusic() {
    clearLightStrip();
    // TODO: implement background music stop
}

void playSoundEffect(SoundEffect effect) {
    soundEffectDurationMs = 400; // TODO: tune per effect once real audio is wired up
    showAudioLightShow();
    // TODO: implement sound effect playback, dispatch on `effect`
}

void stopAudio() {
    clearLightStrip();
    soundEffectDurationMs = 0;
    // TODO: implement immediate audio stop
}

} // namespace AudioVisual
