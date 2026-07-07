#pragma once
#include "brew/recipe.h"

class SettingsStore {
public:
    void begin();
    float load_calibration_factor(float fallback);
    void save_calibration_factor(float factor);
    float load_target_water_g(float fallback);
    void save_target_water_g(float grams);
    bool load_recipe(BrewRecipe& recipe);
    void save_recipe(const BrewRecipe& recipe);
private:
    bool initialized = false;
};
