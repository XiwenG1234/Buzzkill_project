// Buzzkill hardware bring-up test: OLED display + PAM8904 amp/speaker
// + 25-LED NeoPixel light strip (via AudioVisual)
//
// Wiring:
//   OLED (SSD1306, I2C)  SCL -> GPIO6, SDA -> GPIO7
//   PAM8904 amplifier    IN  -> GPIO4 (square-wave tone input)
//   NeoPixel light strip DIN -> GPIO3 (LIGHT_STRIP_PIN in AudioVisual.cpp)
//
// The PAM8904 is a piezo sounder driver, not a linear audio amp: it's
// driven with a digital square wave at the desired tone frequency
// (via LEDC/PWM), and it boosts + drives the piezo speaker differentially.
// If SHDN/gain-select pins are wired to GPIOs rather than hardwired,
// they'll need to be set separately (SHDN driven active to enable output).
//
// Build/run just this test with:
//   pio run -e hwtest -t upload -t monitor

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AudioVisual.h"

// --- Pin wiring ---
constexpr int PIN_OLED_SDA = 7;
constexpr int PIN_OLED_SCL = 6;
constexpr int PIN_AMP_IN = 4;

// --- OLED config ---
constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;
constexpr uint8_t OLED_I2C_ADDR = 0x3C; // common default; try 0x3D if init fails

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool oledReady = false;

// --- Amplifier/speaker config ---
constexpr int AMP_PWM_RESOLUTION = 8; // bits; 50% duty via ledcWriteTone gives a clean square wave

// Tones to sweep through the PAM8904 + speaker (Hz).
const int testTones[] = {262, 330, 392, 440, 523, 880, 1047};
constexpr int numTestTones = sizeof(testTones) / sizeof(testTones[0]);

void showOledStatus(const String &line1, const String &line2 = "") {
    if (!oledReady) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Buzzkill HW Test");
    display.println("----------------");
    display.println(line1);
    if (line2.length()) {
        display.println(line2);
    }
    display.display();
}

// Prints any devices found on the I2C bus. Handy if the OLED doesn't
// show up at OLED_I2C_ADDR - run this to find its real address.
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

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\nBuzzkill hardware bring-up test");

    // --- OLED init ---
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    scanI2C();

    if (display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        oledReady = true;
        Serial.println("OLED init OK");
        showOledStatus("OLED: OK", "Amp: starting...");
    } else {
        Serial.println("OLED init FAILED - check wiring/address (try 0x3D)");
    }

    // --- PAM8904 amplifier init ---
    if (!ledcAttach(PIN_AMP_IN, testTones[0], AMP_PWM_RESOLUTION)) {
        Serial.println("ledcAttach FAILED on amp pin");
    }
    ledcWriteTone(PIN_AMP_IN, 0); // stay silent until the loop starts

    // --- Light strip init ---
    AudioVisual::init();
}

// Lights each strip LED in turn (red, green, blue) to check every pixel
// and the GRB color order.
void stripChaseTest() {
    const uint8_t colors[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    const char *names[] = {"red", "green", "blue"};
    for (int c = 0; c < 3; c++) {
        Serial.printf("Strip chase: %s\n", names[c]);
        showOledStatus("Strip: chase", names[c]);
        for (int i = 0; i < AudioVisual::NUM_STRIP_LEDS; i++) {
            AudioVisual::clearLightStrip();
            AudioVisual::setStripPixel(i, colors[c][0], colors[c][1], colors[c][2]);
            delay(100);
        }
    }
    AudioVisual::clearLightStrip();
}

void loop() {
    stripChaseTest();

    // Rainbow light show stays on for the whole tone sweep.
    AudioVisual::playBackgroundMusic();
    for (int i = 0; i < numTestTones; i++) {
        int freq = testTones[i];
        Serial.printf("Playing %d Hz through PAM8904\n", freq);
        showOledStatus("Amp: playing", String(freq) + " Hz");

        ledcWriteTone(PIN_AMP_IN, freq);
        delay(400);
        ledcWriteTone(PIN_AMP_IN, 0);
        delay(150);
    }
    AudioVisual::stopBackgroundMusic();

    showOledStatus("Sweep complete", "Repeating...");
    delay(1000);
}
