#include "touch_driver.h"
#include <esp_log.h>

namespace {
constexpr uint32_t kTouchI2CFrequencyHz = 300000;
constexpr int kTouchI2CTimeoutMs = 5;
}

void TouchDriver::init() {
    if (initialized) return;
#if DEBUG_SUPPRESS_TOUCH_I2C_ERRORS
    esp_log_level_set("i2c.master", ESP_LOG_NONE);
    esp_log_level_set("esp32-hal-i2c-ng", ESP_LOG_NONE);
    esp_log_level_set("Wire", ESP_LOG_NONE);
#endif

    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = static_cast<gpio_num_t>(HW_TOUCH_I2C_SDA_PIN);
    bus_config.scl_io_num = static_cast<gpio_num_t>(HW_TOUCH_I2C_SCL_PIN);
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = 1;

    if (i2c_new_master_bus(&bus_config, &bus_handle) != ESP_OK) {
        disabled = true;
        return;
    }

    i2c_device_config_t device_config = {};
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.device_address = HW_TOUCH_I2C_ADDRESS;
    device_config.scl_speed_hz = kTouchI2CFrequencyHz;
#if DEBUG_SUPPRESS_TOUCH_I2C_ERRORS
    device_config.flags.disable_ack_check = 1;
#endif

    if (i2c_master_bus_add_device(bus_handle, &device_config, &device_handle) != ESP_OK) {
        disabled = true;
        return;
    }

    initialized = true;
    disabled = false;

    // Same as the official Waveshare FT3168 demo: put the controller into normal mode.
    uint8_t normal_mode[2] = {0x00, 0x00};
    esp_err_t mode_err = i2c_master_transmit(device_handle, normal_mode, sizeof(normal_mode), kTouchI2CTimeoutMs);
    if (mode_err != ESP_OK) {
        Serial.printf("FT3168 normal-mode write failed: %d\n", mode_err);
    } else {
        Serial.println("FT3168 normal mode set");
    }
}

void TouchDriver::update() {
    if (!initialized || disabled || device_handle == nullptr) return;

    uint8_t touch_count = 0;
    uint8_t reg = 0x02;
    esp_err_t err = i2c_master_transmit_receive(device_handle, &reg, sizeof(reg), &touch_count, 1, kTouchI2CTimeoutMs);
    if (err != ESP_OK) {
        last_touch.pressed = false;
        last_touch.i2c_ok = false;
        last_touch.touches = 0;
        return;
    }

    last_touch.i2c_ok = true;
    last_touch.touches = touch_count & 0x0F;

    if (last_touch.touches > 0) {
        uint8_t buf[4] = {0};
        reg = 0x03;
        err = i2c_master_transmit_receive(device_handle, &reg, sizeof(reg), buf, sizeof(buf), kTouchI2CTimeoutMs);
        if (err != ESP_OK) {
            last_touch.pressed = false;
            last_touch.i2c_ok = false;
            return;
        }
        uint16_t x = ((uint16_t)(buf[0] & 0x0F) << 8) | buf[1];
        uint16_t y = ((uint16_t)(buf[2] & 0x0F) << 8) | buf[3];
        if (x > HW_DISPLAY_WIDTH_PX) x = HW_DISPLAY_WIDTH_PX;
        if (y > HW_DISPLAY_HEIGHT_PX) y = HW_DISPLAY_HEIGHT_PX;
        last_touch.x = x;
        last_touch.y = y;
        last_touch.pressed = true;
    } else {
        last_touch.pressed = false;
    }
}
