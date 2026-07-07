#pragma once
#include <Arduino.h>
#include "recipe.h"

struct BrewStageStatus {
    bool recipe_mode = false;
    bool running = false;
    bool final_complete = false;
    uint8_t active_index = 0;
    uint8_t stage_count = 0;
    const char* stage_name = "Target";
    float stage_start_g = 0.0f;
    float stage_target_g = 0.0f;
    uint32_t hold_seconds = 0;
    uint32_t hold_remaining_seconds = 0;
    bool holding = false;
};

class BrewEngine {
public:
    void reset();
    void update(const BrewRecipe& recipe, float weight_g, uint32_t elapsed_ms, bool brew_running);
    BrewStageStatus status(const BrewRecipe& recipe) const;

private:
    uint8_t active_stage = 0;
    uint32_t stage_start_elapsed_ms = 0;
    bool was_running = false;
    bool final_complete = false;
    uint32_t cached_hold_remaining_seconds = 0;
    bool holding_stage = false;
    uint32_t hold_start_elapsed_ms = 0;

    uint8_t normalize_active(const BrewRecipe& recipe, uint8_t desired) const;
    uint8_t enabled_count(const BrewRecipe& recipe) const;
    int actual_index_for_enabled_position(const BrewRecipe& recipe, uint8_t enabled_position) const;
    float previous_stage_target(const BrewRecipe& recipe, uint8_t enabled_position) const;
};
