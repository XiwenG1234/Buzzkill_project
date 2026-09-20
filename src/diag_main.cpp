// Minimal serial heartbeat - no libraries, no peripherals. Used to isolate
// Serial/USB-CDC issues from anything in the real application/test code.
// Run with: pio run -e diag -t upload --upload-port <port>

#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
}

int counter = 0;

void loop() {
    Serial.printf("heartbeat %d\n", counter++);
    digitalWrite(LED_BUILTIN, counter % 2);
    delay(500);
}
