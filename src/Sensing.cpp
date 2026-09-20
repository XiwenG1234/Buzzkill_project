#include "Sensing.h"

namespace Sensing {

void init() {
    // TODO: configure line sensor pins (e.g. pinMode for each IR sensor)
}

void update() {
    // TODO: poll line sensor array and cache results for this loop iteration
}

void readLineSensors(int *outValues, int count) {
    // TODO: read each sensor (digitalRead/analogRead) into outValues
}

float getLineError() {
    // TODO: compute weighted position error from the sensor array
    return 0.0f;
}

bool isOnLine() {
    // TODO: implement line detection logic
    return false;
}

float readBatteryVoltage() {
    // TODO: implement ADC read + voltage divider scaling
    return 0.0f;
}

} // namespace Sensing
