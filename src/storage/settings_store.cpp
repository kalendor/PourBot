#include "settings_store.h"
#include <Preferences.h>

namespace {
Preferences prefs;
constexpr const char* kNamespace = "pourover";
constexpr const char* kCalKey = "cal_factor";
constexpr const char* kTargetKey = "target_g";
constexpr const char* kRecipeNameKey = "recipe_name";
constexpr const char* kStageCountKey = "stage_count";

void make_key(char* out, size_t out_len, const char* prefix, uint8_t index) {
    snprintf(out, out_len, "%s%u", prefix, (unsigned)index);
}
}

void SettingsStore::begin() {
    if (initialized) return;
    prefs.begin(kNamespace, false);
    initialized = true;
}

float SettingsStore::load_calibration_factor(float fallback) {
    begin();
    return prefs.getFloat(kCalKey, fallback);
}

void SettingsStore::save_calibration_factor(float factor) {
    begin();
    prefs.putFloat(kCalKey, factor);
}

float SettingsStore::load_target_water_g(float fallback) {
    begin();
    return prefs.getFloat(kTargetKey, fallback);
}

void SettingsStore::save_target_water_g(float grams) {
    begin();
    prefs.putFloat(kTargetKey, grams);
}

bool SettingsStore::load_recipe(BrewRecipe& recipe) {
    begin();
    if (!prefs.isKey(kStageCountKey)) return false;

    String recipe_name = prefs.getString(kRecipeNameKey, recipe.name);
    strncpy(recipe.name, recipe_name.c_str(), sizeof(recipe.name) - 1);
    recipe.name[sizeof(recipe.name) - 1] = '\0';

    uint8_t count = prefs.getUChar(kStageCountKey, recipe.stage_count);
    if (count < 1) count = 1;
    if (count > BREW_MAX_STAGES) count = BREW_MAX_STAGES;
    recipe.stage_count = count;

    for (uint8_t i = 0; i < BREW_MAX_STAGES; i++) {
        char key[12];
        make_key(key, sizeof(key), "sname", i);
        String stage_name = prefs.getString(key, recipe.stages[i].name);
        strncpy(recipe.stages[i].name, stage_name.c_str(), sizeof(recipe.stages[i].name) - 1);
        recipe.stages[i].name[sizeof(recipe.stages[i].name) - 1] = '\0';

        make_key(key, sizeof(key), "stgt", i);
        recipe.stages[i].target_g = prefs.getFloat(key, recipe.stages[i].target_g);

        make_key(key, sizeof(key), "shold", i);
        recipe.stages[i].hold_seconds = prefs.getUInt(key, recipe.stages[i].hold_seconds);

        make_key(key, sizeof(key), "sen", i);
        recipe.stages[i].enabled = prefs.getBool(key, i < count);
    }

    if (recipe.stage_count > 0) {
        recipe.water_g = recipe.stages[recipe.stage_count - 1].target_g;
        recipe.bloom_g = recipe.stages[0].target_g;
        recipe.bloom_seconds = recipe.stages[0].hold_seconds;
    }
    return true;
}

void SettingsStore::save_recipe(const BrewRecipe& recipe) {
    begin();
    prefs.putString(kRecipeNameKey, recipe.name);
    prefs.putUChar(kStageCountKey, recipe.stage_count);
    for (uint8_t i = 0; i < BREW_MAX_STAGES; i++) {
        char key[12];
        make_key(key, sizeof(key), "sname", i);
        prefs.putString(key, recipe.stages[i].name);
        make_key(key, sizeof(key), "stgt", i);
        prefs.putFloat(key, recipe.stages[i].target_g);
        make_key(key, sizeof(key), "shold", i);
        prefs.putUInt(key, recipe.stages[i].hold_seconds);
        make_key(key, sizeof(key), "sen", i);
        prefs.putBool(key, recipe.stages[i].enabled);
    }
}
