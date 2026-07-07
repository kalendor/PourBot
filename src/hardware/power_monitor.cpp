#include "power_monitor.h"
#include "config/hardware.h"

void PowerMonitor::begin() {
    last_read_ms = 0;
    filtered_voltage_v = 0.0f;
    filtered_percent = -1.0f;
    current = PowerStatus{};
#if HW_BATTERY_ADC_ENABLED
    // Use Arduino analogRead for this board. The Waveshare schematic shows BAT_ADC on IO4.
    // The ESP-IDF oneshot path returned 0 on some Arduino-core builds, while analogRead
    // uses the core's ADC setup path and is more reliable under PlatformIO/Arduino.
    analogReadResolution(12);
    analogSetPinAttenuation(HW_BATTERY_ADC_PIN, ADC_11db);
    pinMode(HW_BATTERY_ADC_PIN, INPUT);
    adc_ready = true;
    Serial.printf("Power ADC init: analogRead GPIO%d\n", HW_BATTERY_ADC_PIN);
#endif
}

void PowerMonitor::update() {
#if HW_BATTERY_ADC_ENABLED
    const uint32_t now = millis();
    if (now - last_read_ms < HW_BATTERY_READ_INTERVAL_MS) return;
    last_read_ms = now;

    int raw = 0;
    float pin_v = 0.0f;
    float voltage = read_system_voltage(&raw, &pin_v);

    current.raw_adc = raw;
    current.pin_voltage_v = pin_v;
    current.voltage_v = voltage;

    if (raw <= 10 || voltage < 0.5f || voltage > 6.5f) {
        current.valid = false;
        current.percent = -1;
        current.percent_valid = false;
        current.charge_estimate = false;
        Serial.printf("Power ADC invalid: gpio=%d raw=%d pin=%.3fV sys=%.3fV\n", HW_BATTERY_ADC_PIN, raw, pin_v, voltage);
        return;
    }

    if (filtered_voltage_v <= 0.1f) filtered_voltage_v = voltage;
    else filtered_voltage_v = filtered_voltage_v * 0.85f + voltage * 0.15f;

    current.voltage_v = filtered_voltage_v;
    current.valid = true;

    // This Waveshare ADC reading behaves like system/USB rail voltage when USB-C is plugged in.
    // In that condition it is NOT a reliable battery voltage, so don't convert it to 100%.
    current.charge_estimate = filtered_voltage_v >= HW_USB_POWER_ESTIMATE_V;
    if (current.charge_estimate) {
        current.percent = last_battery_percent;      // preserve last known battery estimate if available
        current.percent_valid = (last_battery_percent >= 0);
    } else {
        const int raw_percent = voltage_to_percent(filtered_voltage_v);
        if (filtered_percent < 0.0f) {
            filtered_percent = (float)raw_percent;
        } else {
            // Slow exponential smoothing on the percent itself (on top of the
            // voltage smoothing above) so small voltage wiggles under Wi-Fi TX
            // bursts etc. don't make the displayed percent hop around.
            filtered_percent = filtered_percent * 0.90f + (float)raw_percent * 0.10f;
        }
        current.percent = (int)(filtered_percent + 0.5f);
        current.percent_valid = true;
        last_battery_percent = current.percent;
    }

    // Routine power telemetry is intentionally not printed every read; frequent Serial output can stall the loop
    // and make the web UI/timer feel less responsive when USB serial is connected.
#else
    current.valid = false;
#endif
}

int PowerMonitor::read_adc_raw() {
#if HW_BATTERY_ADC_ENABLED
    if (!adc_ready) return 0;
    int raw_sum = 0;
    int valid = 0;
    const int samples = HW_BATTERY_ADC_SAMPLES;
    for (int i = 0; i < samples; i++) {
        int raw = analogRead(HW_BATTERY_ADC_PIN);
        if (raw >= 0) {
            raw_sum += raw;
            valid++;
        }
        delay(2);
    }
    if (valid == 0) return 0;
    return raw_sum / valid;
#else
    return 0;
#endif
}

float PowerMonitor::read_system_voltage(int* raw_out, float* pin_v_out) {
#if HW_BATTERY_ADC_ENABLED
    const int raw_avg = read_adc_raw();
    if (raw_out) *raw_out = raw_avg;

    // Raw ADC pin voltage estimate. This is only diagnostic; the useful value
    // is system voltage below, calibrated from Waveshare's demo.
    const float pin_v = ((float)raw_avg / 4095.0f) * 3.3f;
    if (pin_v_out) *pin_v_out = pin_v;

    // Waveshare wiki ADC demo: raw ~1900 corresponds to system voltage ~4.9V.
    // 4.9 / 1900 = 0.002579 V/count.
    return (float)raw_avg * HW_BATTERY_ADC_RAW_TO_SYSTEM_V;
#else
    if (raw_out) *raw_out = 0;
    if (pin_v_out) *pin_v_out = 0.0f;
    return 0.0f;
#endif
}

int PowerMonitor::voltage_to_percent(float v) const {
    struct Point { float v; int p; };
    static const Point curve[] = {
        {4.20f,100},{4.10f,90},{4.00f,80},{3.92f,70},{3.85f,60},
        {3.79f,50},{3.74f,40},{3.68f,30},{3.60f,20},{3.50f,10},{3.35f,0}
    };
    if (v >= curve[0].v) return 100;
    const int count = sizeof(curve) / sizeof(curve[0]);
    if (v <= curve[count - 1].v) return 0;
    for (int i = 0; i < count - 1; i++) {
        if (v <= curve[i].v && v >= curve[i + 1].v) {
            const float span = curve[i].v - curve[i + 1].v;
            const float pos = (v - curve[i + 1].v) / span;
            return curve[i + 1].p + (int)((curve[i].p - curve[i + 1].p) * pos + 0.5f);
        }
    }
    return 0;
}
