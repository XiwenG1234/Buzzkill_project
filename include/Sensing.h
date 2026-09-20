#pragma once
// Sensing subsystem for Buzzkill (ESP32-C6)
// Detects the black line and reports direction info for the
// autonomous line-following algorithm (owned by Control in main.cpp).
// TODO: finalize sensor type (IR reflectance array, e.g. QTR-5) and pins.

#include <Arduino.h>

namespace Sensing {

// Number of line sensors in the array. TODO: confirm final count/layout.
constexpr int NUM_LINE_SENSORS = 5;

// Call once from setup() to configure sensor pins.
void init();

// Call each loop() iteration to take fresh sensor readings.
void update();

// Raw digital/analog reading of each line sensor, left to right.
// outValues must have space for NUM_LINE_SENSORS entries.
void readLineSensors(int *outValues, int count);

// Signed line position error for steering correction.
// 0 = centered on line, negative = line is to the left, positive = right.
float getLineError();

// True if at least one sensor currently sees the black line.
bool isOnLine();

// Battery/supply voltage as seen by the ESP32 (Power subsystem is
// hardware-only; this is the one software touchpoint for monitoring it).
// TODO: wire to an ADC pin if voltage monitoring is added.
float readBatteryVoltage();

} // namespace Sensing
