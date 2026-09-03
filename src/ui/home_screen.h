#pragma once
#include <lvgl.h>
#include "../brew/recipe.h"
#include "../brew/brew_engine.h"

class HomeScreen {
public:
    void create(const BrewRecipe& recipe);
    void update(float grams, float flow_gps, uint32_t elapsed_ms, bool running, const BrewRecipe& recipe, const BrewStageStatus& stage, int battery_percent, bool battery_valid, bool charging, bool prebrew_pending = false, bool ready = false);
private:
    lv_obj_t* screen_obj = nullptr;
    lv_obj_t* card = nullptr;
    lv_obj_t* progress_panel = nullptr;
    lv_obj_t* stage_label = nullptr;
    lv_obj_t* weight_label = nullptr;
    lv_obj_t* grams_label = nullptr;
    lv_obj_t* timer_box = nullptr;
    lv_obj_t* timer_chars[7] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* hold_label = nullptr;
    lv_obj_t* progress_label = nullptr;
    lv_obj_t* flow_title_label = nullptr;
    lv_obj_t* flow_value_label = nullptr;
    lv_obj_t* flow_bar = nullptr;
    lv_obj_t* flow_scale_label = nullptr;
    lv_obj_t* ready_label = nullptr;
    void set_timer_text(const char* text);
    void set_state_colors(bool running, float grams, const BrewRecipe& recipe, const BrewStageStatus* stage = nullptr);
};
