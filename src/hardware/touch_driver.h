#pragma once
#include <Arduino.h>
#include <driver/i2c_master.h>
#include "../config/hardware.h"

struct TouchData {
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t touches = 0;
    bool pressed = false;
    bool i2c_ok = false;
};

class TouchDriver {
public:
    void init();
    void update();
    TouchData get_touch_data() const { return last_touch; }

private:
    TouchData last_touch;
    uint8_t consecutive_read_errors = 0;
    uint32_t last_good_read_ms = 0;
    bool initialized = false;
    bool disabled = false;
    i2c_master_bus_handle_t bus_handle = nullptr;
    i2c_master_dev_handle_t device_handle = nullptr;
};
