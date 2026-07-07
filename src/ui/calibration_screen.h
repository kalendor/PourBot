#pragma once
#include <lvgl.h>

typedef void (*known_weight_set_cb_t)(float grams);

class CalibrationScreen {
public:
    void create(lv_event_cb_t tare_cb, known_weight_set_cb_t known_weight_cb, lv_event_cb_t calibrate_cb, lv_event_cb_t back_cb);
    void update(float live_grams, float known_grams, float calibration_factor, const char* message);
private:
    lv_obj_t* weight_label = nullptr;
    lv_obj_t* known_label = nullptr;
    lv_obj_t* factor_label = nullptr;
    lv_obj_t* message_label = nullptr;
    lv_obj_t* keypad_panel = nullptr;
    lv_obj_t* keypad_value_label = nullptr;
    known_weight_set_cb_t on_known_weight_set = nullptr;
    float current_known_grams = 100.0f;
    char keypad_buffer[12] = {0};

    lv_obj_t* make_button(lv_obj_t* parent, const char* text, int x, int y, int w, int h, lv_color_t color, lv_event_cb_t cb);
    lv_obj_t* make_key(lv_obj_t* parent, const char* text, int x, int y, int w, int h, lv_color_t color);
    void create_keypad(lv_obj_t* parent);
    void show_keypad();
    void hide_keypad();
    void append_key(const char* key);
    void refresh_keypad_label();

    static CalibrationScreen* active;
    static void known_button_cb(lv_event_t* e);
    static void keypad_key_cb(lv_event_t* e);
};
