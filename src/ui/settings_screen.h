#pragma once
#include <lvgl.h>

class SettingsScreen {
public:
    void create(lv_event_cb_t calibration_cb, lv_event_cb_t back_cb);
private:
    lv_obj_t* make_button(lv_obj_t* parent, const char* text, int y, lv_color_t color, lv_event_cb_t cb);
};
