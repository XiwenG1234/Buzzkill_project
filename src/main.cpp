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

    Sensing::init();
    AudioVisual::init();
    Motion::init();
    Wireless::init();
    Control::init();
}

void loop() {
    Sensing::update();
    Wireless::update();
    Control::update();
    Motion::update();
    AudioVisual::update();
}

// --- Control implementation (stubs) ---

namespace Control {

static Mode currentMode = Mode::AUTO_LINE_FOLLOW;

void init() {
    // TODO: initialize control/state-machine state
}

void update() {
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
    // TODO: implement stop
}

void setMotorSpeeds(int leftSpeed, int rightSpeed) {
    // TODO: implement differential drive PWM output
}

void moveForward(int speed) {
    // TODO: implement forward movement
}

void moveBackward(int speed) {
    // TODO: implement backward movement
}

void turnLeft(int speed) {
    // TODO: implement left turn
}

void turnRight(int speed) {
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
