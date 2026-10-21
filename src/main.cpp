// Buzzkill - ESP32-C6-DevKitM-1 main entry point
//
// Acts as the Control subsystem (the "brain"): reads Sensing, runs the
// line-following algorithm in AUTO mode, decodes BLE commands from the
// Flutter app in MANUAL mode, and drives Motion + AudioVisual.
//
// Also hosts the Motion subsystem (motor driver) and Wireless subsystem
// (BLE) as stubs below, since only Sensing/AudioVisual/main were split
// into separate files. Power is hardware-only (feeds Motion + ESP32
// directly) and has no dedicated software module.
// TODO: split Motion/Wireless into their own files once their scope is final.
// "pio device monitor" to view serial output from the ESP32-C6-DevKitM-1
#include "Arduino.h"
#include "Sensing.h"
#include "AudioVisual.h"

// --- Control subsystem: operating mode ---

enum class Mode {
    AUTO_LINE_FOLLOW,
    MANUAL_CONTROL
};

namespace Control {

void init();
void update();

Mode getMode();
void setMode(Mode mode);

// Autonomous line-following: reads Sensing::getLineError() and drives
// Motion to keep the vehicle centered on the black line.
void runLineFollowing();

// Manual mode: applies the latest command received over BLE (Wireless)
// to Motion / AudioVisual.
void runManualControl();

// Demo: cycles left -> right -> forward -> backward -> stop every 1.5 s
// through Motion (which shows the matching arrow). Used while DEMO_MODE is
// on, until line following / BLE control are implemented.
void runDirectionDemo();

} // namespace Control

// --- Motion subsystem (stubs) ---
// Owns movement: DC motors + motor driver(s) for the tracked chassis.
namespace Motion {

void init();
void update();

void stop();
// Differential drive: independent left/right track speed, e.g. -255..255
// (negative = reverse).
void setMotorSpeeds(int leftSpeed, int rightSpeed);
void moveForward(int speed);
void moveBackward(int speed);
void turnLeft(int speed);
void turnRight(int speed);

} // namespace Motion

// --- Wireless Communication subsystem (stubs) ---
// BLE link between the ESP32 and the Flutter smartphone app: receives
// manual drive/LED commands and mode-switch requests.
namespace Wireless {

void init();
void update();

bool isConnected();

// True if a new command arrived since the last check; also copies the
// command string out via outCommand.
bool pollCommand(String &outCommand);

// Push a status update (e.g. mode, battery) to the connected app.
void sendStatus(const String &status);

} // namespace Wireless

void setup() {
    Serial.begin(115200);
    // Don't block when no serial monitor is reading: otherwise every print
    // stalls loop() ~100 ms once the USB buffer fills, slowing the music.
    Serial.setTxTimeoutMs(0);
    delay(300);
    Serial.println("\nBuzzkill starting");

    Sensing::init();
    AudioVisual::init();
    Motion::init();
    Wireless::init();
    Control::init();

    // Loops forever in the background (serviced by AudioVisual::update()).
    Serial.println("Playing background music (looping)");
    AudioVisual::playBackgroundMusic();
}

void loop() {
    Sensing::update();
    Wireless::update();
    Control::update();
    Motion::update();
    AudioVisual::update();

    // Test: send "0" from the serial monitor to fire the gunshot effect once.
    while (Serial.available()) {
        if (Serial.read() == '0') {
            Serial.println("Gunshot!");
            AudioVisual::playSoundEffect(AudioVisual::SoundEffect::GUNSHOT);
        }
    }
    // No delay() in loop() - it would stall the background music.
}

// --- Control implementation (stubs) ---

namespace Control {

static Mode currentMode = Mode::AUTO_LINE_FOLLOW;

// true: ignore the mode and run runDirectionDemo(). Set to false once line
// following / manual control are implemented.
constexpr bool DEMO_MODE = true;

void init() {
    // TODO: initialize control/state-machine state
}

void update() {
    if (DEMO_MODE) {
        runDirectionDemo();
        return;
    }

    // TODO: check Wireless for mode-switch commands, then dispatch:
    if (currentMode == Mode::AUTO_LINE_FOLLOW) {
        runLineFollowing();
    } else {
        runManualControl();
    }
}

Mode getMode() {
    return currentMode;
}

void setMode(Mode mode) {
    // TODO: handle mode transition (e.g. stop motors, play SoundEffect)
    currentMode = mode;
}

void runLineFollowing() {
    // TODO: use Sensing::getLineError() to compute Motion::setMotorSpeeds()
}

void runManualControl() {
    // TODO: apply latest Wireless command to Motion / AudioVisual
}

void runDirectionDemo() {
    // Stage: 0 = left, 1 = right, 2 = forward, 3 = backward, 4 = stop.
    static int stage = 0;
    static unsigned long lastStageMs = 0;
    constexpr unsigned long STAGE_MS = 1500; // time per stage
    constexpr int DEMO_SPEED = 150;          // placeholder motor speed

    if (lastStageMs != 0 && millis() - lastStageMs < STAGE_MS) {
        return;
    }
    lastStageMs = millis();

    const char *name = "";
    switch (stage) {
        case 0:
            name = "Left";
            Motion::turnLeft(DEMO_SPEED);
            break;
        case 1:
            name = "Right";
            Motion::turnRight(DEMO_SPEED);
            break;
        case 2:
            name = "Forward";
            Motion::moveForward(DEMO_SPEED);
            break;
        case 3:
            name = "Backward";
            Motion::moveBackward(DEMO_SPEED);
            break;
        case 4:
            name = "Stop";
            Motion::stop();
            break;
    }
    Serial.printf("Stage %d: %s\n", stage, name);
    AudioVisual::showOledStatus("Stage " + String(stage), name);

    stage = (stage + 1) % 5; // 0 -> 1 -> 2 -> 3 -> 4 -> 0 ...
}

} // namespace Control

// --- Motion implementation (stubs) ---

namespace Motion {

void init() {
    // TODO: configure motor driver pins/peripherals (e.g. PWM channels)
}

void update() {
    // TODO: service any non-blocking motion state
}

void stop() {
    AudioVisual::showStopSign();
    // TODO: implement stop
}

void setMotorSpeeds(int leftSpeed, int rightSpeed) {
    // TODO: implement differential drive PWM output
}

void moveForward(int speed) {
    AudioVisual::showForwardArrow();
    // TODO: implement forward movement
}

void moveBackward(int speed) {
    AudioVisual::showBackwardArrow();
    // TODO: implement backward movement
}

void turnLeft(int speed) {
    AudioVisual::showLeftArrow();
    // TODO: implement left turn
}

void turnRight(int speed) {
    AudioVisual::showRightArrow();
    // TODO: implement right turn
}

} // namespace Motion

// --- Wireless implementation (stubs) ---

namespace Wireless {

void init() {
    // TODO: initialize BLE stack, advertise service/characteristics
    // for the Flutter app (e.g. using NimBLE-Arduino)
}

void update() {
    // TODO: service BLE stack / connection state
}

bool isConnected() {
    // TODO: implement connection status check
    return false;
}

bool pollCommand(String &outCommand) {
    // TODO: implement incoming command check
    return false;
}

void sendStatus(const String &status) {
    // TODO: implement BLE notify/write to app
}

} // namespace Wireless
