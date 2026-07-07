#include "hx711_scale.h"
#include <math.h>
#include <algorithm>

void HX711Scale::pump_idle_if_due() {
    if (!idle_pump) return;
    uint32_t now = millis();
    // Throttle so we're not hammering display/web work every 1ms of the wait loop.
    if (now - last_idle_pump_ms < 20) return;
    last_idle_pump_ms = now;
    idle_pump();
}

bool HX711Scale::wait_ready(uint32_t timeout_ms) {
    uint32_t start = millis();
    while (!hx.is_ready()) {
        if (millis() - start >= timeout_ms) return false;
        pump_idle_if_due();
        delay(1);
    }
    return true;
}

bool HX711Scale::read_raw_average_wait(uint8_t samples_to_read, uint32_t timeout_ms, long& average_out) {
    if (samples_to_read == 0) return false;

    int64_t sum = 0;
    for (uint8_t i = 0; i < samples_to_read; ++i) {
        if (!wait_ready(timeout_ms)) return false;
        sum += hx.read();
        pump_idle_if_due();
        delay(2);
    }

    average_out = (long)(sum / samples_to_read);
    return true;
}

float HX711Scale::read_raw_grams_once() {
    // Live loop: one fresh HX711 sample only. Averaging multiple reads here makes a
    // 10 SPS HX711 feel slow. Smoothing happens after sampling, like the grinder project.
    long raw_counts = hx.read();
    return ((float)(raw_counts - zero_raw_offset)) / cal_factor;
}

void HX711Scale::clear_samples() {
    sample_write = 0;
    sample_count = 0;
    for (uint8_t i = 0; i < SAMPLE_BUF_SIZE; ++i) {
        samples[i].grams = 0.0f;
        samples[i].ms = 0;
    }
}

void HX711Scale::add_sample(float grams, uint32_t now) {
    samples[sample_write].grams = grams;
    samples[sample_write].ms = now;
    sample_write = (sample_write + 1) % SAMPLE_BUF_SIZE;
    if (sample_count < SAMPLE_BUF_SIZE) sample_count++;
}

float HX711Scale::smoothed_grams(uint32_t window_ms) const {
    if (sample_count == 0) return raw_g;

    uint32_t now = millis();
    float vals[SAMPLE_BUF_SIZE];
    uint8_t count = 0;

    for (uint8_t i = 0; i < sample_count; ++i) {
        uint8_t idx = (sample_write + SAMPLE_BUF_SIZE - 1 - i) % SAMPLE_BUF_SIZE;
        uint32_t age = now - samples[idx].ms;
        if (age <= window_ms) {
            vals[count++] = samples[idx].grams;
        } else {
            break;
        }
    }

    if (count == 0) return raw_g;
    if (count == 1) return vals[0];

    std::sort(vals, vals + count);

    // Grinder-style trimmed average: reject a high/low outlier when there are enough samples.
    uint8_t start = 0;
    uint8_t end = count;
    if (count >= 5) {
        start = 1;
        end = count - 1;
    }

    float sum = 0.0f;
    uint8_t used = 0;
    for (uint8_t i = start; i < end; ++i) {
        sum += vals[i];
        used++;
    }
    return used ? sum / used : vals[count / 2];
}

float HX711Scale::display_filtered_grams(float current_g) {
    if (!display_filter_initialized) {
        filtered_g = current_g;
        display_filter_initialized = true;
        return filtered_g;
    }

    float delta = current_g - filtered_g;
    float abs_delta = fabsf(delta);

    // Quiet mode: the HX711/load-cell combo can naturally wander a few counts,
    // which shows up as +/- 0.1g to 0.2g on the display. Hold tiny movement,
    // smooth small movement, and still jump quickly for real weight changes.
    if (abs_delta < 0.09f) {
        return filtered_g;
    }

    // Large changes should feel immediate: placing/removing a vessel or active pouring.
    if (abs_delta > 2.0f) {
        filtered_g = current_g;
    }
    // Normal pouring: mostly follow the fresh smoothed value.
    else if (abs_delta > 0.65f) {
        filtered_g += delta * 0.68f;
    }
    // Small changes: smooth more aggressively to suppress idle shimmer.
    else if (abs_delta > 0.22f) {
        filtered_g += delta * 0.30f;
    }
    // Tiny changes: very stable idle behavior.
    else {
        filtered_g += delta * 0.08f;
    }

    if (fabsf(filtered_g) < 0.10f) filtered_g = 0.0f;
    return filtered_g;
}

void HX711Scale::begin(uint8_t dout_pin, uint8_t sck_pin, float calibration_factor) {
    cal_factor = calibration_factor;
    hx.begin(dout_pin, sck_pin);
    delay(300);

    is_ready = hx.is_ready();
    if (is_ready) {
        hx.set_scale(cal_factor);
        long zero_avg = 0;
        if (read_raw_average_wait(20, 1500, zero_avg)) {
            zero_raw_offset = zero_avg;
        } else {
            zero_raw_offset = 0;
            Serial.println("HX711 begin warning: zero average timeout");
        }
        hx.set_offset(zero_raw_offset);

        clear_samples();
        raw_g = 0.0f;
        filtered_g = 0.0f;
        display_g = 0.0f;
        previous_display_g = 0.0f;
        flow_gps_filtered = 0.0f;
        display_filter_initialized = false;
        Serial.printf("HX711 zero offset: %ld\n", zero_raw_offset);
    }
    last_sample_ms = millis();
    last_flow_ms = last_sample_ms;
}

void HX711Scale::set_calibration_factor(float factor) {
    if (factor == 0.0f || isnan(factor) || isinf(factor)) return;
    cal_factor = factor;
    hx.set_scale(cal_factor);
    hx.set_offset(zero_raw_offset);

    clear_samples();
    raw_g = 0.0f;
    filtered_g = 0.0f;
    display_g = 0.0f;
    previous_display_g = 0.0f;
    flow_gps_filtered = 0.0f;
    display_filter_initialized = false;
}

bool HX711Scale::calibrate_with_known_weight(float known_grams, float& new_factor) {
    if (known_grams <= 0.0f) return false;

    // Do not depend on hx.is_ready() at the exact instant the button is tapped.
    // The live update loop may have just consumed the latest conversion, which made
    // calibration feel flaky. Wait for fresh conversions and average raw counts.
    long loaded_raw = 0;
    if (!read_raw_average_wait(20, 1500, loaded_raw)) {
        Serial.println("CAL failed: HX711 timeout while reading loaded weight");
        return false;
    }

    const long raw_delta = loaded_raw - zero_raw_offset;

    Serial.printf("CAL known=%.2f g, zero=%ld, loaded=%ld, delta=%ld\n",
                  known_grams, zero_raw_offset, loaded_raw, raw_delta);

    if (labs(raw_delta) < 100) {
        Serial.println("CAL failed: raw delta too small. Was the known weight placed after tare?");
        return false;
    }

    new_factor = (float)raw_delta / known_grams;
    if (fabs(new_factor) < 1.0f || isnan(new_factor) || isinf(new_factor)) {
        Serial.println("CAL failed: bad factor");
        return false;
    }

    set_calibration_factor(new_factor);

    raw_g = known_grams;
    filtered_g = known_grams;
    display_g = known_grams;
    previous_display_g = known_grams;
    display_filter_initialized = true;
    flow_gps_filtered = 0.0f;
    clear_samples();
    add_sample(known_grams, millis());

    Serial.printf("CAL success factor=%.4f\n", new_factor);
    return true;
}


bool HX711Scale::tare_blocking(uint8_t samples_to_read, uint32_t timeout_ms) {
    long zero_avg = 0;
    if (!read_raw_average_wait(samples_to_read, timeout_ms, zero_avg)) {
        Serial.println("TARE failed: HX711 timeout");
        return false;
    }

    zero_raw_offset = zero_avg;
    hx.set_offset(zero_raw_offset);
    hx.set_scale(cal_factor);

    clear_samples();
    raw_g = 0.0f;
    filtered_g = 0.0f;
    display_g = 0.0f;
    previous_display_g = 0.0f;
    flow_gps_filtered = 0.0f;
    display_filter_initialized = true;
    add_sample(0.0f, millis());

    Serial.printf("TARE zero offset: %ld\n", zero_raw_offset);
    return true;
}

void HX711Scale::tare() {
    // Blocking tare is intentional: the user expects the TARE action to finish
    // reliably, even if it takes a fraction of a second.
    tare_blocking(20, 1500);
}


void HX711Scale::update() {
    uint32_t now = millis();

    is_ready = hx.is_ready();
    if (!is_ready) return;

    // Only read when the HX711 has a fresh conversion ready. No extra averaging here.
    raw_g = read_raw_grams_once();
    add_sample(raw_g, now);

    // Grinder-inspired display path: trimmed time-window average, then a display filter.
    // 260ms quiets +/- 0.1g to 0.2g idle noise without making pouring feel delayed.
    float smoothed = smoothed_grams(260);

    previous_display_g = display_g;
    float candidate_display_g = display_filtered_grams(smoothed);

    // Decimal-display hysteresis: do not let the visible value bounce by one
    // tenth unless the filtered value has moved enough to be meaningful.
    if (fabsf(candidate_display_g - previous_display_g) < 0.14f) {
        display_g = previous_display_g;
    } else {
        display_g = candidate_display_g;
    }

    // Flow is based on a 600ms window for stability, not the display deadband.
    float newest = 0.0f;
    float oldest = 0.0f;
    uint32_t newest_ms = 0;
    uint32_t oldest_ms = 0;
    uint8_t found = 0;
    const uint32_t flow_window_ms = 600;

    for (uint8_t i = 0; i < sample_count; ++i) {
        uint8_t idx = (sample_write + SAMPLE_BUF_SIZE - 1 - i) % SAMPLE_BUF_SIZE;
        uint32_t age = now - samples[idx].ms;
        if (age <= flow_window_ms) {
            if (found == 0) {
                newest = samples[idx].grams;
                newest_ms = samples[idx].ms;
            }
            oldest = samples[idx].grams;
            oldest_ms = samples[idx].ms;
            found++;
        } else {
            break;
        }
    }

    float raw_flow = 0.0f;
    if (found >= 2 && newest_ms != oldest_ms) {
        float dt = (newest_ms - oldest_ms) / 1000.0f;
        raw_flow = (newest - oldest) / dt;
        if (raw_flow < 0.10f || raw_flow > 25.0f) raw_flow = 0.0f;
    }

    const float flow_alpha = raw_flow > 0.0f ? 0.28f : 0.18f;
    flow_gps_filtered = flow_gps_filtered * (1.0f - flow_alpha) + raw_flow * flow_alpha;
    if (flow_gps_filtered < 0.10f) flow_gps_filtered = 0.0f;

    last_sample_ms = now;
}
