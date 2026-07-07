#pragma once
#include <Arduino.h>

constexpr uint8_t BREW_MAX_STAGES = 5;

struct BrewStage {
    char name[18];
    float target_g;
    uint32_t hold_seconds;
    bool enabled;
};

struct BrewRecipe {
    char name[24];
    float coffee_g;
    float water_g;
    float bloom_g;
    uint32_t bloom_seconds;
    uint8_t stage_count;
    BrewStage stages[BREW_MAX_STAGES];
};

inline void set_stage(BrewRecipe& r, uint8_t i, const char* name, float target_g, uint32_t hold_seconds) {
    if (i >= BREW_MAX_STAGES) return;
    strncpy(r.stages[i].name, name, sizeof(r.stages[i].name) - 1);
    r.stages[i].name[sizeof(r.stages[i].name) - 1] = '\0';
    r.stages[i].target_g = target_g;
    r.stages[i].hold_seconds = hold_seconds;
    r.stages[i].enabled = true;
}

inline BrewRecipe default_recipe() {
    BrewRecipe r = {};
    strncpy(r.name, "Generic Pour Over", sizeof(r.name) - 1);
    r.coffee_g = 20.0f;
    r.water_g = 320.0f;
    r.bloom_g = 64.0f;
    r.bloom_seconds = 45;
    r.stage_count = 3;
    set_stage(r, 0, "Bloom", 64.0f, 45);
    set_stage(r, 1, "Main Pour", 192.0f, 0);
    set_stage(r, 2, "Finish", 320.0f, 0);
    for (uint8_t i = r.stage_count; i < BREW_MAX_STAGES; i++) r.stages[i].enabled = false;
    return r;
}

inline uint8_t recipe_enabled_stage_count(const BrewRecipe& recipe) {
    uint8_t count = 0;
    const uint8_t max_count = recipe.stage_count > BREW_MAX_STAGES ? BREW_MAX_STAGES : recipe.stage_count;
    for (uint8_t i = 0; i < max_count; i++) {
        if (recipe.stages[i].enabled && recipe.stages[i].target_g > 0.0f) count++;
    }
    return count;
}
