#include "app.h"
#include <Arduino.h>
#include "config/hardware.h"
#include "config/version.h"

App* g_app = nullptr;

static void tare_event_cb(lv_event_t*) { if (g_app) g_app->tare(); }
static void start_event_cb(lv_event_t*) { if (g_app) g_app->toggle_brew(); }
static void reset_event_cb(lv_event_t*) { if (g_app) g_app->reset_brew(); }
static void show_settings_event_cb(lv_event_t*) { if (g_app) g_app->show_settings(); }
static void show_cal_event_cb(lv_event_t*) { if (g_app) g_app->show_calibration(); }
static void show_home_event_cb(lv_event_t*) { if (g_app) g_app->show_home(); }
static void calibrate_event_cb(lv_event_t*) { if (g_app) g_app->run_calibration(); }
static void known_weight_set_cb(float grams) { if (g_app) g_app->set_calibration_known_weight(grams); }
static void web_tare_cb() { if (g_app) g_app->tare(); }
static void web_start_cb() { if (g_app) g_app->toggle_brew(); }
static void web_reset_cb() { if (g_app) g_app->reset_brew(); }
static void web_target_cb(float grams) { if (g_app) g_app->set_target_water(grams); }
static void web_cal_tare_cb() { if (g_app) g_app->run_web_calibration_tare(); }
static bool web_calibrate_cb(float grams) { return g_app ? g_app->run_web_calibration(grams) : false; }
static bool web_recipe_cb(const BrewRecipe& recipe) { return g_app ? g_app->set_recipe_from_web(recipe) : false; }

void App::begin() {
    g_app = this;
    display.init();
    Serial.println(FIRMWARE_NAME " " FIRMWARE_VERSION " - hold cue amber pulse");

    settings.begin();
    power.begin();
    const float default_calibration_factor = -7050.0f;
    const float saved_calibration_factor = settings.load_calibration_factor(default_calibration_factor);
    settings.load_recipe(recipe);
    recipe.water_g = settings.load_target_water_g(recipe.water_g);
    // Keep the final recipe stage synced with the normal target weight.
    if (recipe.stage_count > 0) recipe.stages[recipe.stage_count - 1].target_g = recipe.water_g;
    scale.begin(HW_LOADCELL_DOUT_PIN, HW_LOADCELL_SCK_PIN, saved_calibration_factor);
    Serial.println(scale.ready() ? "HX711 ready" : "HX711 not ready");
    Serial.printf("Calibration factor: %.2f\n", saved_calibration_factor);

    // Tare/calibration reads block for up to ~1.5-1.8s. Keep the display and web
    // dashboard alive during that window instead of freezing until it's done.
    scale.set_idle_pump_callback([]() {
        if (g_app) {
            g_app->display.update();
            g_app->web.update();
        }
    });

    show_home();
    web.begin(web_tare_cb, web_start_cb, web_reset_cb, web_target_cb, web_cal_tare_cb, web_calibrate_cb, web_recipe_cb);
}

void App::show_home() {
    screen = Screen::Home;
    home.create(recipe, tare_event_cb, start_event_cb, show_settings_event_cb, reset_event_cb);
}

void App::show_settings() {
    screen = Screen::Settings;
    settings_screen.create(show_cal_event_cb, show_home_event_cb);
}

void App::show_calibration() {
    screen = Screen::Calibration;
    calibration_message = "Empty scale, then TARE";
    calibration.create(tare_event_cb, known_weight_set_cb, calibrate_event_cb, show_home_event_cb);
}

void App::update() {
    display.update();
    scale.update();
    power.update();
    PowerStatus power_status = power.status();

    const float current_weight_g = scale.grams();
    // v57: back to basics. All auto-start, pre-brew auto-tare, and
    // post-brew auto-reset automation is disabled. Tare, Start/Pause,
    // and Reset are now manual only.
    const bool prep_pending = false;
    const bool ready_to_pour = false;
    const float ui_weight_g = current_weight_g;

    brew_engine.update(recipe, ui_weight_g, brew.elapsed_ms(), brew.is_running());
    BrewStageStatus stage_status = brew_engine.status(recipe);

    WebDashboardState web_state;
    web_state.weight_g = ui_weight_g;
    web_state.flow_gps = scale.flow_gps();
    web_state.target_g = recipe.water_g;
    web_state.calibration_factor = scale.calibration_factor();
    web_state.elapsed_ms = brew.elapsed_ms();
    web_state.running = brew.is_running();
    web_state.battery_percent = power_status.percent;
    web_state.battery_voltage_v = power_status.voltage_v;
    web_state.battery_raw_adc = power_status.raw_adc;
    web_state.battery_pin_voltage_v = power_status.pin_voltage_v;
    web_state.battery_valid = power_status.valid;
    web_state.battery_percent_valid = power_status.percent_valid;
    web_state.charging = power_status.charge_estimate;
    web_state.prebrew_pending = prep_pending;
    web_state.pour_ready = ready_to_pour;
    web_state.screen_name = (screen == Screen::Home) ? "Home" : (screen == Screen::Settings) ? "Settings" : "Calibration";
    web_state.recipe = recipe;
    web_state.stage = stage_status;
    web.set_state(web_state);
    web.update();

    uint32_t now = millis();
    if (now - last_ui_ms >= 100) {
        last_ui_ms = now;
        if (screen == Screen::Home) {
            home.update(ui_weight_g, scale.flow_gps(), brew.elapsed_ms(), brew.is_running(), recipe, stage_status, power_status.percent, power_status.percent_valid, power_status.charge_estimate, prep_pending, ready_to_pour);
        } else if (screen == Screen::Calibration) {
            calibration.update(current_weight_g, known_calibration_weight_g, scale.calibration_factor(), calibration_message);
        }
    }
}

void App::tare() {
    bool ok = scale.tare_blocking(screen == Screen::Calibration ? 24 : 16, 1500);
    if (screen == Screen::Calibration) {
        calibration_message = ok ? "Tared. Place known weight." : "Tare failed. Try again.";
    }
}

void App::toggle_brew() { brew.toggle(); }
void App::reset_brew() {
    brew.reset();
    brew_engine.reset();
}

void App::set_calibration_known_weight(float grams) {
    if (grams < 1.0f) grams = 1.0f;
    if (grams > 5000.0f) grams = 5000.0f;
    known_calibration_weight_g = grams;
    calibration_message = "Known weight set";
}

void App::run_calibration() {
    float new_factor = 0.0f;
    if (scale.calibrate_with_known_weight(known_calibration_weight_g, new_factor)) {
        settings.save_calibration_factor(new_factor);
        calibration_message = "Saved. Reading should match.";
        Serial.printf("Saved calibration factor: %.2f\n", new_factor);
    } else {
        calibration_message = "Failed. Empty > TARE, then add weight.";
    }
}

void App::run_web_calibration_tare() {
    bool ok = scale.tare_blocking(24, 1500);
    calibration_message = ok ? "Web calibration tare complete" : "Web calibration tare failed";
    Serial.println(ok ? "WEB CAL TARE complete" : "WEB CAL TARE failed");
}

bool App::run_web_calibration(float grams) {
    if (grams < 1.0f) grams = 1.0f;
    if (grams > 5000.0f) grams = 5000.0f;
    known_calibration_weight_g = grams;

    float new_factor = 0.0f;
    const bool ok = scale.calibrate_with_known_weight(known_calibration_weight_g, new_factor);
    if (ok) {
        settings.save_calibration_factor(new_factor);
        calibration_message = "Web calibration saved";
        Serial.printf("WEB CAL saved factor: %.2f for %.1f g\n", new_factor, known_calibration_weight_g);
    } else {
        calibration_message = "Web calibration failed";
        Serial.printf("WEB CAL failed for %.1f g\n", known_calibration_weight_g);
    }
    return ok;
}

void App::set_target_water(float grams) {
    if (grams < 10.0f) grams = 10.0f;
    if (grams > 5000.0f) grams = 5000.0f;
    recipe.water_g = grams;
    if (recipe.stage_count > 0) recipe.stages[recipe.stage_count - 1].target_g = grams;
    settings.save_target_water_g(grams);
    settings.save_recipe(recipe);
    Serial.printf("Target water set: %.1f g\n", grams);
}


bool App::set_recipe_from_web(const BrewRecipe& new_recipe) {
    BrewRecipe cleaned = new_recipe;
    if (cleaned.stage_count < 1) cleaned.stage_count = 1;
    if (cleaned.stage_count > BREW_MAX_STAGES) cleaned.stage_count = BREW_MAX_STAGES;

    float last_target = 0.0f;
    for (uint8_t i = 0; i < cleaned.stage_count; i++) {
        cleaned.stages[i].enabled = true;
        if (cleaned.stages[i].name[0] == '\0') {
            snprintf(cleaned.stages[i].name, sizeof(cleaned.stages[i].name), "Stage %u", (unsigned)(i + 1));
        }
        if (cleaned.stages[i].target_g < 1.0f) cleaned.stages[i].target_g = 1.0f;
        if (cleaned.stages[i].target_g > 5000.0f) cleaned.stages[i].target_g = 5000.0f;
        if (cleaned.stages[i].target_g < last_target + 1.0f) cleaned.stages[i].target_g = last_target + 1.0f;
        if (cleaned.stages[i].hold_seconds > 900UL) cleaned.stages[i].hold_seconds = 900UL;
        last_target = cleaned.stages[i].target_g;
    }
    for (uint8_t i = cleaned.stage_count; i < BREW_MAX_STAGES; i++) cleaned.stages[i].enabled = false;

    cleaned.water_g = cleaned.stages[cleaned.stage_count - 1].target_g;
    cleaned.bloom_g = cleaned.stages[0].target_g;
    cleaned.bloom_seconds = cleaned.stages[0].hold_seconds;

    recipe = cleaned;
    settings.save_target_water_g(recipe.water_g);
    settings.save_recipe(recipe);
    brew_engine.reset();
    Serial.printf("Recipe saved: %s, %u stages, final %.1f g\n", recipe.name, recipe.stage_count, recipe.water_g);
    return true;
}
