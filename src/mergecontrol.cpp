
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <math.h>
#include <string>
#include <cstring>

#include "Sensing.h"
#include "AudioVisual.h"

// ==========================================
// BUZZKILL - ESP32-C6-DevKitM-1
// Control + BLE + Motion integration
// ==========================================

// Set to 1 only after configuring the
// actual DRV8833 pins from your PCB.
#define MOTOR_OUTPUT_ENABLED 0

#if MOTOR_OUTPUT_ENABLED
// REPLACE these with your verified GPIOs.
// Do not use pins reserved by your other modules.
constexpr int LEFT_IN1  = -1;
constexpr int LEFT_IN2  = -1;
constexpr int RIGHT_IN1 = -1;
constexpr int RIGHT_IN2 = -1;
#endif

constexpr int PWM_FREQ = 5000;
constexpr int PWM_RES = 8;
constexpr int MAX_PWM = 255;

constexpr unsigned long COMMAND_TIMEOUT_MS = 500;
constexpr unsigned long TELEMETRY_INTERVAL_MS = 500;

// Nordic UART BLE
#define SERVICE_UUID \
 "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_UUID \
 "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define TX_UUID \
 "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ==========================================
// CONTROL MODE
// ==========================================

enum class Mode {
    AUTO_LINE_FOLLOW,
    MANUAL_CONTROL
};

namespace Control {
    void init();
    void update();

    Mode getMode();
    void setMode(Mode mode);

    void runLineFollowing();
    void runManualControl();
    void runDirectionDemo();
}

// ==========================================
// MOTION SUBSYSTEM
// ==========================================

namespace Motion {
    void init();
    void update();

    void stop();
    void setMotorSpeeds(int left, int right);

    void moveForward(int speed);
    void moveBackward(int speed);
    void turnLeft(int speed);
    void turnRight(int speed);
}

// ==========================================
// WIRELESS SUBSYSTEM
// ==========================================

namespace Wireless {
    void init();
    void update();

    bool isConnected();
    bool pollCommand(String &outCommand);
    void sendStatus(const String &status);
}

// ==========================================
// MAIN SETUP
// ==========================================

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    delay(300);

    Serial.println("\nBuzzkill starting");

    Sensing::init();
    AudioVisual::init();
    Motion::init();
    Wireless::init();
    Control::init();

    Serial.println("Playing background music");
    AudioVisual::playBackgroundMusic();
}

// ==========================================
// MAIN LOOP - NON BLOCKING
// ==========================================

void loop() {
    Sensing::update();
    Wireless::update();
    Control::update();
    Motion::update();
    AudioVisual::update();

    while (Serial.available()) {
        if (Serial.read() == '0') {
            Serial.println("Gunshot!");
            AudioVisual::playSoundEffect(
                AudioVisual::SoundEffect::GUNSHOT
            );
        }
    }

    // NO DELAY - preserve music playback.
}

// ==========================================
// CONTROL IMPLEMENTATION
// ==========================================

namespace Control {

static Mode currentMode = Mode::MANUAL_CONTROL;

// Direction demo disabled for BLE operation.
constexpr bool DEMO_MODE = false;

// Joystick state
static float joystickX = 0.0f;
static float joystickY = 0.0f;

static bool emergencyStopped = false;
static unsigned long lastCommandMs = 0;
static bool wasConnected = false;

void init() {
    currentMode = Mode::MANUAL_CONTROL;
    joystickX = 0;
    joystickY = 0;
    emergencyStopped = false;
    lastCommandMs = 0;

    Motion::stop();

    Serial.println("Control initialized");
    Serial.println("Mode: MANUAL");
}

Mode getMode() {
    return currentMode;
}

void setMode(Mode mode) {
    Motion::stop();

    joystickX = 0;
    joystickY = 0;
    lastCommandMs = 0;

    currentMode = mode;

    if (mode == Mode::MANUAL_CONTROL) {
        Serial.println("Switched to MANUAL");
        AudioVisual::showOledStatus(
            "BuzzKill", "MANUAL"
        );
        Wireless::sendStatus("MODE:MANUAL");
    } else {
        Serial.println("Switched to AUTO");
        AudioVisual::showOledStatus(
            "BuzzKill", "AUTO"
        );
        Wireless::sendStatus("MODE:AUTO");
    }
}

void update() {

    if (DEMO_MODE) {
        runDirectionDemo();
        return;
    }

    bool connected = Wireless::isConnected();

    // Fail-safe on BLE disconnection.
    if (!connected) {
        if (wasConnected) {
            Serial.println("BLE lost - STOP");
        }

        joystickX = 0;
        joystickY = 0;
        lastCommandMs = 0;
        Motion::stop();

        wasConnected = false;
        return;
    }

    wasConnected = true;

    // Process all pending commands.
    String command;

    while (Wireless::pollCommand(command)) {
        command.trim();

        if (command == "STOP") {
            emergencyStopped = true;
            joystickX = 0;
            joystickY = 0;
            Motion::stop();

            Wireless::sendStatus("STOPPED");
            continue;
        }

        if (command == "MODE:MANUAL") {
            emergencyStopped = false;
            setMode(Mode::MANUAL_CONTROL);
            continue;
        }

        if (command == "MODE:AUTO") {
            emergencyStopped = false;
            setMode(Mode::AUTO_LINE_FOLLOW);
            continue;
        }

        // Format: J:x,y
        // Example: J:0.50,0.80
        if (command.startsWith("J:")) {
            float x = 0;
            float y = 0;
            char extra;

            // Reject malformed or trailing data.
            int parsed = sscanf(
                command.c_str(),
                "J:%f,%f %c",
                &x, &y, &extra
            );

            if (parsed != 2) {
                continue;
            }

            if (!isfinite(x) || !isfinite(y)) {
                continue;
            }

            if (x < -1.0f || x > 1.0f ||
                y < -1.0f || y > 1.0f) {
                continue;
            }

            if (currentMode == Mode::MANUAL_CONTROL &&
                !emergencyStopped) {

                joystickX = x;
                joystickY = y;
                lastCommandMs = millis();
            }
        }
    }

    if (emergencyStopped) {
        Motion::stop();
        return;
    }

    if (currentMode == Mode::AUTO_LINE_FOLLOW) {
        runLineFollowing();
    } else {
        runManualControl();
    }
}

void runManualControl() {

    // Stop when the app stops updating joystick.
    if (lastCommandMs == 0 ||
        millis() - lastCommandMs > COMMAND_TIMEOUT_MS) {

        Motion::stop();
        return;
    }

    float x = joystickX;
    float y = joystickY;

    // Dead zone.
    if (fabsf(x) < 0.08f) x = 0;
    if (fabsf(y) < 0.08f) y = 0;

    // Differential-drive mixing.
    float left = y + x;
    float right = y - x;

    float magnitude = fmaxf(
        fabsf(left),
        fabsf(right)
    );

    if (magnitude > 1.0f) {
        left /= magnitude;
        right /= magnitude;
    }

    int leftSpeed = roundf(left * MAX_PWM);
    int rightSpeed = roundf(right * MAX_PWM);

    Motion::setMotorSpeeds(
        leftSpeed,
        rightSpeed
    );
}

void runLineFollowing() {
    // TODO: integrate actual four-sensor algorithm.
    // Sensing::getLineError() can be used here.
    //
    // For now, stay stopped in AUTO mode.
    Motion::stop();
}

void runDirectionDemo() {

    static int stage = 0;
    static unsigned long lastStageMs = 0;

    constexpr unsigned long STAGE_MS = 1500;
    constexpr int DEMO_SPEED = 150;

    if (lastStageMs != 0 &&
        millis() - lastStageMs < STAGE_MS) {
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

        default:
            name = "Stop";
            Motion::stop();
            break;
    }

    AudioVisual::showOledStatus(
        "Stage " + String(stage),
        name
    );

    stage = (stage + 1) % 5;
}

} // namespace Control

// ==========================================
// MOTION IMPLEMENTATION
// ==========================================

namespace Motion {

static int currentLeft = 0;
static int currentRight = 0;

void init() {

#if MOTOR_OUTPUT_ENABLED
    ledcAttach(LEFT_IN1, PWM_FREQ, PWM_RES);
    ledcAttach(LEFT_IN2, PWM_FREQ, PWM_RES);
    ledcAttach(RIGHT_IN1, PWM_FREQ, PWM_RES);
    ledcAttach(RIGHT_IN2, PWM_FREQ, PWM_RES);
#endif

    stop();
    Serial.println("Motion initialized");
}

void update() {
    // Non-blocking motion updates can go here.
}

// Use this for DRV8833 differential drive.
void setMotorSpeeds(int leftSpeed, int rightSpeed) {

    leftSpeed = constrain(leftSpeed, -255, 255);
    rightSpeed = constrain(rightSpeed, -255, 255);

    if (leftSpeed == currentLeft &&
        rightSpeed == currentRight) {
        return;
    }

    currentLeft = leftSpeed;
    currentRight = rightSpeed;

#if MOTOR_OUTPUT_ENABLED

    // Left motor
    if (leftSpeed > 0) {
        ledcWrite(LEFT_IN1, leftSpeed);
        ledcWrite(LEFT_IN2, 0);
    } else if (leftSpeed < 0) {
        ledcWrite(LEFT_IN1, 0);
        ledcWrite(LEFT_IN2, -leftSpeed);
    } else {
        ledcWrite(LEFT_IN1, 0);
        ledcWrite(LEFT_IN2, 0);
    }

    // Right motor
    if (rightSpeed > 0) {
        ledcWrite(RIGHT_IN1, rightSpeed);
        ledcWrite(RIGHT_IN2, 0);
    } else if (rightSpeed < 0) {
        ledcWrite(RIGHT_IN1, 0);
        ledcWrite(RIGHT_IN2, -rightSpeed);
    } else {
        ledcWrite(RIGHT_IN1, 0);
        ledcWrite(RIGHT_IN2, 0);
    }

#endif

    // Show matching arrow only when direction changes.
    static int previousDirection = -1;
    int direction = 0;

    if (leftSpeed == 0 && rightSpeed == 0) {
        direction = 0; // Stop
    } else if (leftSpeed > rightSpeed + 20) {
        direction = 1; // Right turn
    } else if (rightSpeed > leftSpeed + 20) {
        direction = 2; // Left turn
    } else if (leftSpeed + rightSpeed >= 0) {
        direction = 3; // Forward
    } else {
        direction = 4; // Backward
    }

    if (direction == previousDirection) {
        return;
    }

    previousDirection = direction;

    switch (direction) {
        case 0:
            AudioVisual::showStopSign();
            break;
        case 1:
            AudioVisual::showRightArrow();
            break;
        case 2:
            AudioVisual::showLeftArrow();
            break;
        case 3:
            AudioVisual::showForwardArrow();
            break;
        case 4:
            AudioVisual::showBackwardArrow();
            break;
    }
}

void stop() {
    setMotorSpeeds(0, 0);
}

void moveForward(int speed) {
    setMotorSpeeds(speed, speed);
}

void moveBackward(int speed) {
    setMotorSpeeds(-speed, -speed);
}

void turnLeft(int speed) {
    setMotorSpeeds(-speed, speed);
}

void turnRight(int speed) {
    setMotorSpeeds(speed, -speed);
}

} // namespace Motion

// ==========================================
// WIRELESS IMPLEMENTATION
// NimBLE-Arduino 2.x
// ==========================================

namespace Wireless {

static NimBLEServer* bleServer = nullptr;
static NimBLECharacteristic* txCharacteristic = nullptr;

// Small fixed-size queue to transfer BLE commands
// safely from the BLE callback to loop().
constexpr size_t QUEUE_SIZE = 8;
constexpr size_t MAX_COMMAND_LEN = 64;

static char commandQueue[QUEUE_SIZE][MAX_COMMAND_LEN + 1];
static size_t queueHead = 0;
static size_t queueTail = 0;
static size_t queueCount = 0;

static portMUX_TYPE queueMutex = portMUX_INITIALIZER_UNLOCKED;

static unsigned long lastTelemetryMs = 0;

// Called in BLE task context.
void enqueueCommand(const std::string &value) {

    if (value.empty() || value.size() > MAX_COMMAND_LEN) {
        return;
    }

    portENTER_CRITICAL(&queueMutex);

    // Drop oldest if queue is full.
    if (queueCount == QUEUE_SIZE) {
        queueTail = (queueTail + 1) % QUEUE_SIZE;
        queueCount--;
    }

    memcpy(
        commandQueue[queueHead],
        value.data(),
        value.size()
    );

    commandQueue[queueHead][value.size()] = '\0';

    queueHead = (queueHead + 1) % QUEUE_SIZE;
    queueCount++;

    portEXIT_CRITICAL(&queueMutex);
}

// ==========================================
// BLE CALLBACKS
// ==========================================

class ServerCallbacks : public NimBLEServerCallbacks {

    void onConnect(
        NimBLEServer* server,
        NimBLEConnInfo& connInfo
    ) override {
        Serial.println("Flutter connected");
    }

    void onDisconnect(
        NimBLEServer* server,
        NimBLEConnInfo& connInfo,
        int reason
    ) override {
        Serial.println("Flutter disconnected");

        // Resume advertising.
        NimBLEDevice::getAdvertising()->start();
    }
};

class RXCallbacks : public NimBLECharacteristicCallbacks {

    void onWrite(
        NimBLECharacteristic* characteristic,
        NimBLEConnInfo& connInfo
    ) override {

        std::string value = characteristic->getValue();

        enqueueCommand(value);
    }
};

// ==========================================
// BLE INITIALIZATION
// ==========================================

void init() {

    Serial.println("Initializing BLE");

    NimBLEDevice::init("BuzzKill_Robot");

    bleServer = NimBLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks());

    NimBLEService* service =
        bleServer->createService(SERVICE_UUID);

    // RX: Flutter -> ESP32
    NimBLECharacteristic* rxCharacteristic =
        service->createCharacteristic(
            RX_UUID,
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::WRITE_NR
        );

    rxCharacteristic->setCallbacks(
        new RXCallbacks()
    );

    // TX: ESP32 -> Flutter
    txCharacteristic =
        service->createCharacteristic(
            TX_UUID,
            NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::NOTIFY
        );

    txCharacteristic->setValue("READY");

    service->start();

    NimBLEAdvertising* advertising =
        NimBLEDevice::getAdvertising();

    advertising->addServiceUUID(SERVICE_UUID);
    advertising->start();

    Serial.println("--------------------------");
    Serial.println("BLE Ready");
    Serial.println("Name: BuzzKill_Robot");
    Serial.println("Waiting for Flutter...");
    Serial.println("--------------------------");
}

bool isConnected() {
    return bleServer != nullptr &&
           bleServer->getConnectedCount() > 0;
}

// Retrieve one pending command.
bool pollCommand(String &outCommand) {

    char localBuffer[MAX_COMMAND_LEN + 1];

    portENTER_CRITICAL(&queueMutex);

    if (queueCount == 0) {
        portEXIT_CRITICAL(&queueMutex);
        return false;
    }

    strcpy(localBuffer, commandQueue[queueTail]);

    queueTail = (queueTail + 1) % QUEUE_SIZE;
    queueCount--;

    portEXIT_CRITICAL(&queueMutex);

    outCommand = String(localBuffer);

    Serial.print("BLE RX: ");
    Serial.println(outCommand);

    return true;
}

void sendStatus(const String &status) {

    if (!isConnected() || txCharacteristic == nullptr) {
        return;
    }

    txCharacteristic->setValue(status.c_str());
    txCharacteristic->notify();
}

void update() {

    if (!isConnected()) {
        return;
    }

    if (millis() - lastTelemetryMs <
        TELEMETRY_INTERVAL_MS) {
        return;
    }

    lastTelemetryMs = millis();

    String status = "MODE:";

    status += (
        Control::getMode() == Mode::MANUAL_CONTROL
        ? "MANUAL"
        : "AUTO"
    );

    sendStatus(status);
}

} // namespace Wireless
