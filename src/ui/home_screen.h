#pragma once
#include <lvgl.h>
#include "../brew/recipe.h"
#include "../brew/brew_engine.h"

class HomeScreen {
public:
    void create(const BrewRecipe& recipe, lv_event_cb_t tare_cb, lv_event_cb_t start_cb, lv_event_cb_t swipe_up_cb, lv_event_cb_t reset_cb);
    void update(float grams, float flow_gps, uint32_t elapsed_ms, bool running, const BrewRecipe& recipe, const BrewStageStatus& stage, int battery_percent, bool battery_valid, bool charging, bool prebrew_pending = false, bool ready = false);
private:
    lv_obj_t* card = nullptr;
    lv_obj_t* status_battery = nullptr;
    lv_obj_t* battery_body = nullptr;
    lv_obj_t* battery_tip = nullptr;
    lv_obj_t* battery_fill = nullptr;
    lv_obj_t* progress_arc = nullptr;
    lv_obj_t* weight_target_label = nullptr;
    lv_obj_t* weight_label = nullptr;
    lv_obj_t* grams_label = nullptr;
    lv_obj_t* timer_box = nullptr;
    lv_obj_t* timer_chars[7] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* flow_label = nullptr;
    lv_obj_t* flow_dot = nullptr;
    lv_obj_t* progress_label = nullptr;
    lv_obj_t* tare_button = nullptr;
    lv_obj_t* start_button = nullptr;
    lv_obj_t* start_button_label = nullptr;
    lv_obj_t* ready_label = nullptr;
    lv_event_cb_t start_button_cb = nullptr;
    lv_event_cb_t reset_button_cb = nullptr;
    uint32_t start_button_pressed_ms = 0;
    bool start_button_long_reset_sent = false;
    static void start_button_event_router(lv_event_t* e);
    void handle_start_button_event(lv_event_t* e);
    bool last_running = false;
    void set_timer_text(const char* text);
    void set_state_colors(bool running, float grams, const BrewRecipe& recipe, const BrewStageStatus* stage = nullptr);
};
