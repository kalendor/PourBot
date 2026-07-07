#include "calibration_screen.h"
#include <Arduino.h>
#include <cstring>
#include <cstdlib>

CalibrationScreen* CalibrationScreen::active = nullptr;

lv_obj_t* CalibrationScreen::make_button(lv_obj_t* parent, const char* text, int x, int y, int w, int h, lv_color_t color, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, x, y);
    lv_obj_set_style_radius(btn, 18, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);
    return label;
}

lv_obj_t* CalibrationScreen::make_key(lv_obj_t* parent, const char* text, int x, int y, int w, int h, lv_color_t color) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_radius(btn, 16, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, keypad_key_cb, LV_EVENT_CLICKED, (void*)text);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_center(label);
    return btn;
}

void CalibrationScreen::create_keypad(lv_obj_t* parent) {
    keypad_panel = lv_obj_create(parent);
    lv_obj_set_size(keypad_panel, 264, 372);
    lv_obj_align(keypad_panel, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(keypad_panel, lv_color_hex(0x020617), 0);
    lv_obj_set_style_bg_opa(keypad_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(keypad_panel, 24, 0);
    lv_obj_set_style_border_width(keypad_panel, 1, 0);
    lv_obj_set_style_border_color(keypad_panel, lv_color_hex(0x334155), 0);
    lv_obj_set_style_pad_all(keypad_panel, 0, 0);
    lv_obj_clear_flag(keypad_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(keypad_panel);
    lv_label_set_text(title, "KNOWN WEIGHT");
    lv_obj_set_style_text_color(title, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    keypad_value_label = lv_label_create(keypad_panel);
    lv_label_set_text(keypad_value_label, "100 g");
    lv_obj_set_style_text_color(keypad_value_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(keypad_value_label, &lv_font_montserrat_32, 0);
    lv_obj_align(keypad_value_label, LV_ALIGN_TOP_MID, 0, 40);

    const int key_w = 70;
    const int key_h = 48;
    const int gap = 10;
    const int x0 = 17;
    const int y0 = 92;
    lv_color_t num = lv_color_hex(0x334155);
    lv_color_t action = lv_color_hex(0x1E293B);
    lv_color_t ok = lv_color_hex(0x16A34A);

    make_key(keypad_panel, "1", x0, y0, key_w, key_h, num);
    make_key(keypad_panel, "2", x0 + key_w + gap, y0, key_w, key_h, num);
    make_key(keypad_panel, "3", x0 + 2 * (key_w + gap), y0, key_w, key_h, num);

    make_key(keypad_panel, "4", x0, y0 + 58, key_w, key_h, num);
    make_key(keypad_panel, "5", x0 + key_w + gap, y0 + 58, key_w, key_h, num);
    make_key(keypad_panel, "6", x0 + 2 * (key_w + gap), y0 + 58, key_w, key_h, num);

    make_key(keypad_panel, "7", x0, y0 + 116, key_w, key_h, num);
    make_key(keypad_panel, "8", x0 + key_w + gap, y0 + 116, key_w, key_h, num);
    make_key(keypad_panel, "9", x0 + 2 * (key_w + gap), y0 + 116, key_w, key_h, num);

    make_key(keypad_panel, "CLR", x0, y0 + 174, key_w, key_h, action);
    make_key(keypad_panel, "0", x0 + key_w + gap, y0 + 174, key_w, key_h, num);
    make_key(keypad_panel, ".", x0 + 2 * (key_w + gap), y0 + 174, key_w, key_h, action);

    make_key(keypad_panel, "CANCEL", 17, 324, 110, 40, action);
    make_key(keypad_panel, "OK", 137, 324, 110, 40, ok);

    lv_obj_add_flag(keypad_panel, LV_OBJ_FLAG_HIDDEN);
}

void CalibrationScreen::create(lv_event_cb_t tare_cb, known_weight_set_cb_t known_weight_cb, lv_event_cb_t calibrate_cb, lv_event_cb_t back_cb) {
    active = this;
    on_known_weight_set = known_weight_cb;

    lv_obj_t* scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x020617), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "CALIBRATION");
    lv_obj_set_style_text_color(title, lv_color_hex(0xCBD5E1), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t* hint = lv_label_create(scr);
    lv_label_set_text(hint, "1. Empty scale > TARE\n2. Tap Known to enter weight\n3. Place weight > CALIBRATE");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 46);

    lv_obj_t* card = lv_obj_create(scr);
    lv_obj_set_size(card, 264, 194);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 122);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_radius(card, 26, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    weight_label = lv_label_create(card);
    lv_label_set_text(weight_label, "0.0 g");
    lv_obj_set_style_text_color(weight_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(weight_label, &lv_font_montserrat_32, 0);
    lv_obj_align(weight_label, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t* known_btn = lv_button_create(card);
    lv_obj_set_size(known_btn, 218, 44);
    lv_obj_align(known_btn, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_set_style_radius(known_btn, 18, 0);
    lv_obj_set_style_bg_color(known_btn, lv_color_hex(0x0EA5E9), 0);
    lv_obj_set_style_bg_opa(known_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(known_btn, 0, 0);
    lv_obj_add_event_cb(known_btn, known_button_cb, LV_EVENT_CLICKED, nullptr);

    known_label = lv_label_create(known_btn);
    lv_label_set_text(known_label, "Known: 100 g");
    lv_obj_set_style_text_color(known_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(known_label, &lv_font_montserrat_24, 0);
    lv_obj_center(known_label);

    factor_label = lv_label_create(card);
    lv_label_set_text(factor_label, "Factor: ----");
    lv_obj_set_style_text_color(factor_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(factor_label, &lv_font_montserrat_16, 0);
    lv_obj_align(factor_label, LV_ALIGN_TOP_MID, 0, 116);

    message_label = lv_label_create(card);
    lv_label_set_text(message_label, "Ready");
    lv_obj_set_style_text_color(message_label, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_text_font(message_label, &lv_font_montserrat_16, 0);
    lv_obj_set_width(message_label, 240);
    lv_obj_set_style_text_align(message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(message_label, LV_ALIGN_BOTTOM_MID, 0, -14);

    make_button(scr, "TARE", -70, -72, 124, 52, lv_color_hex(0x475569), tare_cb);
    make_button(scr, "BACK", 70, -72, 124, 52, lv_color_hex(0x1E293B), back_cb);
    make_button(scr, "CALIBRATE", 0, -12, 260, 52, lv_color_hex(0x16A34A), calibrate_cb);

    create_keypad(scr);
}

void CalibrationScreen::update(float live_grams, float known_grams, float calibration_factor, const char* message) {
    current_known_grams = known_grams;

    char buf[80];
    snprintf(buf, sizeof(buf), "%.1f g", live_grams);
    lv_label_set_text(weight_label, buf);

    snprintf(buf, sizeof(buf), "Known: %.1f g", known_grams);
    lv_label_set_text(known_label, buf);

    snprintf(buf, sizeof(buf), "Factor: %.2f", calibration_factor);
    lv_label_set_text(factor_label, buf);

    lv_label_set_text(message_label, message ? message : "Ready");
}

void CalibrationScreen::show_keypad() {
    snprintf(keypad_buffer, sizeof(keypad_buffer), "%.0f", current_known_grams);
    refresh_keypad_label();
    lv_obj_remove_flag(keypad_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keypad_panel);
}

void CalibrationScreen::hide_keypad() {
    lv_obj_add_flag(keypad_panel, LV_OBJ_FLAG_HIDDEN);
}

void CalibrationScreen::refresh_keypad_label() {
    char buf[24];
    if (strlen(keypad_buffer) == 0) {
        snprintf(buf, sizeof(buf), "0 g");
    } else {
        snprintf(buf, sizeof(buf), "%s g", keypad_buffer);
    }
    lv_label_set_text(keypad_value_label, buf);
}

void CalibrationScreen::append_key(const char* key) {
    if (!key) return;

    if (strcmp(key, "CANCEL") == 0) {
        hide_keypad();
        return;
    }
    if (strcmp(key, "CLR") == 0) {
        keypad_buffer[0] = '\0';
        refresh_keypad_label();
        return;
    }
    if (strcmp(key, "OK") == 0) {
        float value = atof(keypad_buffer);
        if (value > 0.0f && on_known_weight_set) {
            on_known_weight_set(value);
        }
        hide_keypad();
        return;
    }
    if (strcmp(key, ".") == 0 && strchr(keypad_buffer, '.')) {
        return;
    }

    size_t len = strlen(keypad_buffer);
    if (len >= sizeof(keypad_buffer) - 1) return;

    // Avoid leading zeros like 000500, unless entering a decimal value.
    if (len == 1 && keypad_buffer[0] == '0' && strcmp(key, ".") != 0) {
        keypad_buffer[0] = '\0';
        len = 0;
    }

    keypad_buffer[len] = key[0];
    keypad_buffer[len + 1] = '\0';
    refresh_keypad_label();
}

void CalibrationScreen::known_button_cb(lv_event_t*) {
    if (active) active->show_keypad();
}

void CalibrationScreen::keypad_key_cb(lv_event_t* e) {
    if (!active) return;
    const char* key = static_cast<const char*>(lv_event_get_user_data(e));
    active->append_key(key);
}
