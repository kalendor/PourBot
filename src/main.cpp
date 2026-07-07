#include <Arduino.h>
#include "app.h"
#include "config/hardware.h"

App app;

void setup() {
    Serial.begin(HW_SERIAL_BAUD_RATE);
    delay(300);
    app.begin();
}

void loop() {
    app.update();
    delay(5);
}
