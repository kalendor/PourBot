#include "brew_engine.h"

void BrewEngine::reset() {
    active_stage = 0;
    stage_start_elapsed_ms = 0;
    was_running = false;
    final_complete = false;
    cached_hold_remaining_seconds = 0;
    holding_stage = false;
    hold_start_elapsed_ms = 0;
}

uint8_t BrewEngine::enabled_count(const BrewRecipe& recipe) const {
    return recipe_enabled_stage_count(recipe);
}

int BrewEngine::actual_index_for_enabled_position(const BrewRecipe& recipe, uint8_t enabled_position) const {
    uint8_t pos = 0;
    const uint8_t max_count = recipe.stage_count > BREW_MAX_STAGES ? BREW_MAX_STAGES : recipe.stage_count;
    for (uint8_t i = 0; i < max_count; i++) {
        if (recipe.stages[i].enabled && recipe.stages[i].target_g > 0.0f) {
            if (pos == enabled_position) return i;
            pos++;
        }
    }
    return -1;
}

uint8_t BrewEngine::normalize_active(const BrewRecipe& recipe, uint8_t desired) const {
    const uint8_t count = enabled_count(recipe);
    if (count == 0) return 0;
    return desired >= count ? count - 1 : desired;
}

float BrewEngine::previous_stage_target(const BrewRecipe& recipe, uint8_t enabled_position) const {
    if (enabled_position == 0) return 0.0f;
    int prev_actual = actual_index_for_enabled_position(recipe, enabled_position - 1);
    if (prev_actual < 0) return 0.0f;
    return recipe.stages[prev_actual].target_g;
}

void BrewEngine::update(const BrewRecipe& recipe, float weight_g, uint32_t elapsed_ms, bool brew_running) {
    const uint8_t count = enabled_count(recipe);
    if (count == 0) {
        reset();
        return;
    }

    active_stage = normalize_active(recipe, active_stage);

    if (!brew_running && elapsed_ms == 0) {
        reset();
        return;
    }

    if (brew_running && !was_running) {
        stage_start_elapsed_ms = elapsed_ms;
    }
    was_running = brew_running;

    if (!brew_running || final_complete) {
        cached_hold_remaining_seconds = 0;
        holding_stage = false;
        return;
    }

    int actual = actual_index_for_enabled_position(recipe, active_stage);
    if (actual < 0) return;

    const BrewStage& stage = recipe.stages[actual];
    const bool target_met = weight_g >= (stage.target_g - 0.5f);

    // Hold timers are started only after the stage weight target is reached.
    // This gives the brewer a clear pour -> hold -> pour rhythm instead of
    // counting the hold from the beginning of the stage.
    if (target_met && stage.hold_seconds > 0 && !holding_stage) {
        holding_stage = true;
        hold_start_elapsed_ms = elapsed_ms;
    }

    bool hold_met = (stage.hold_seconds == 0);
    if (holding_stage) {
        const uint32_t hold_elapsed_s = (elapsed_ms >= hold_start_elapsed_ms) ? ((elapsed_ms - hold_start_elapsed_ms) / 1000UL) : 0;
        hold_met = hold_elapsed_s >= stage.hold_seconds;
        cached_hold_remaining_seconds = hold_met ? 0 : (stage.hold_seconds - hold_elapsed_s);
    } else {
        cached_hold_remaining_seconds = 0;
    }

    if (target_met && hold_met) {
        if (active_stage + 1 < count) {
            active_stage++;
            stage_start_elapsed_ms = elapsed_ms;
            holding_stage = false;
            hold_start_elapsed_ms = 0;
            cached_hold_remaining_seconds = 0;
        } else {
            final_complete = true;
            holding_stage = false;
            cached_hold_remaining_seconds = 0;
        }
    }
}

BrewStageStatus BrewEngine::status(const BrewRecipe& recipe) const {
    BrewStageStatus s;
    s.stage_count = enabled_count(recipe);
    s.recipe_mode = s.stage_count > 1;
    s.active_index = normalize_active(recipe, active_stage);
    s.final_complete = final_complete;

    if (s.stage_count == 0) {
        s.stage_target_g = recipe.water_g;
        return s;
    }

    int actual = actual_index_for_enabled_position(recipe, s.active_index);
    if (actual >= 0) {
        const BrewStage& stage = recipe.stages[actual];
        s.stage_name = stage.name;
        s.stage_start_g = previous_stage_target(recipe, s.active_index);
        s.stage_target_g = stage.target_g;
        s.hold_seconds = stage.hold_seconds;
        s.hold_remaining_seconds = cached_hold_remaining_seconds;
        s.holding = holding_stage && cached_hold_remaining_seconds > 0;
    }
    return s;
}
