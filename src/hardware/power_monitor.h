#pragma once
#include <Arduino.h>
#include "config/hardware.h"


struct PowerStatus {
    float voltage_v = 0.0f;
    int percent = 0;              // -1 means percent unknown while USB/system rail is present
    bool percent_valid = false;
    bool valid = false;
    bool charge_estimate = false;
    int raw_adc = 0;
    float pin_voltage_v = 0.0f;
};

class PowerMonitor {
public:
    void begin();
    void update();
    PowerStatus status() const { return current; }
private:
    PowerStatus current;
    uint32_t last_read_ms = 0;
    float filtered_voltage_v = 0.0f;
    int last_battery_percent = -1;
    // Smoothed on top of the already-smoothed voltage so quantization steps in
    // voltage_to_percent() don't produce a visibly jittery displayed percent.
    float filtered_percent = -1.0f;
#if HW_BATTERY_ADC_ENABLED
    bool adc_ready = false;
#endif
    int read_adc_raw();
    float read_system_voltage(int* raw_out, float* pin_v_out);
    int voltage_to_percent(float voltage_v) const;
};
