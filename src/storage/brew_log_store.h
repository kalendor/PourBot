#pragma once

#include <Arduino.h>
#include <FS.h>
#include "brew/recipe.h"
#include "brew/brew_engine.h"

class BrewLogStore {
public:
    bool begin();
    void update(uint32_t elapsed_ms, bool running, float weight_g, float flow_gps,
                const BrewRecipe& recipe, const BrewStageStatus& stage);
    bool available() const { return mounted; }
    bool logging() const { return active; }
    String list_json();
    File open_log(const String& name);
    int delete_all_logs();

private:
    bool mounted = false;
    bool active = false;
    File log_file;
    uint32_t last_sample_elapsed_ms = 0;
    uint8_t unflushed_rows = 0;

    void start_log(const BrewRecipe& recipe);
    void finish_log();
    static bool valid_name(const String& name);
    static String csv_text(const char* text);
};

extern BrewLogStore brew_logs;
