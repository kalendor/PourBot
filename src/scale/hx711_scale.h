#pragma once
#include <Arduino.h>
#include <HX711.h>

class HX711Scale {
public:
    void begin(uint8_t dout_pin, uint8_t sck_pin, float calibration_factor);
    void update();
    void tare();
    bool tare_blocking(uint8_t samples = 20, uint32_t timeout_ms = 1500);
    bool ready() const { return is_ready; }
    float grams() const { return display_g; }
    float raw_grams() const { return raw_g; }
    float flow_gps() const { return flow_gps_filtered; }
    float calibration_factor() const { return cal_factor; }
    long zero_offset() const { return zero_raw_offset; }
    void set_calibration_factor(float factor);
    bool calibrate_with_known_weight(float known_grams, float& new_factor);

    // Blocking reads (tare_blocking / calibrate_with_known_weight) can take up to
    // ~1.5-1.8s. Without this, the display and the web server both freeze for that
    // whole window because nothing else runs during the wait. Register a callback
    // here (e.g. display.update()/web.update()) and it will be pumped periodically
    // (throttled to ~every 20ms) while a blocking read is in progress.
    using IdlePumpCallback = void (*)();
    void set_idle_pump_callback(IdlePumpCallback cb) { idle_pump = cb; }
private:
    void pump_idle_if_due();
    IdlePumpCallback idle_pump = nullptr;
    uint32_t last_idle_pump_ms = 0;

    HX711 hx;
    bool is_ready = false;
    float cal_factor = -7050.0f;

    struct GramSample {
        float grams;
        uint32_t ms;
    };

    static const uint8_t SAMPLE_BUF_SIZE = 24;
    GramSample samples[SAMPLE_BUF_SIZE];
    uint8_t sample_write = 0;
    uint8_t sample_count = 0;

    float raw_g = 0.0f;
    float filtered_g = 0.0f;
    float display_g = 0.0f;
    float previous_display_g = 0.0f;
    float flow_gps_filtered = 0.0f;

    bool display_filter_initialized = false;
    uint32_t last_sample_ms = 0;
    uint32_t last_flow_ms = 0;
    long zero_raw_offset = 0;

    bool wait_ready(uint32_t timeout_ms);
    bool read_raw_average_wait(uint8_t samples, uint32_t timeout_ms, long& average_out);
    float read_raw_grams_once();
    void clear_samples();
    void add_sample(float grams, uint32_t now);
    float smoothed_grams(uint32_t window_ms) const;
    float display_filtered_grams(float current_g);
};
