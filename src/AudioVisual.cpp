#include "AudioVisual.h"

namespace AudioVisual {

void init() {
    // TODO: configure LED/speaker pins or begin peripherals (e.g. NeoPixel.begin())
}

void update() {
    // TODO: service LED blink timing and any non-blocking audio playback
}

void setIndicatorColor(uint8_t r, uint8_t g, uint8_t b) {
    // TODO: implement LED color set
}

void setTurnSignal(TurnSignal signal) {
    // TODO: implement left/right/hazard blinking pattern
}

void playBackgroundMusic() {
    // TODO: implement background music playback (loop)
}

void stopBackgroundMusic() {
    // TODO: implement background music stop
}

void playSoundEffect(SoundEffect effect) {
    // TODO: implement sound effect playback, dispatch on `effect`
}

void stopAudio() {
    // TODO: implement immediate audio stop
}

} // namespace AudioVisual
