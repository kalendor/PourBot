#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <IPAddress.h>
#include "brew/recipe.h"
#include "brew/brew_engine.h"

struct WebDashboardState {
    float weight_g = 0.0f;
    float flow_gps = 0.0f;
    float target_g = 320.0f;
    float calibration_factor = 0.0f;
    uint32_t elapsed_ms = 0;
    bool running = false;
    int battery_percent = 0;
    float battery_voltage_v = 0.0f;
    int battery_raw_adc = 0;
    float battery_pin_voltage_v = 0.0f;
    bool battery_valid = false;
    bool battery_percent_valid = false;
    bool charging = false;
    bool prebrew_pending = false;
    bool pour_ready = false;
    const char* screen_name = "Home";
    BrewRecipe recipe = default_recipe();
    BrewStageStatus stage;
};

class WebDashboard {
public:
    using ActionCallback = void (*)();
    using TargetCallback = void (*)(float grams);
    using CalibrateCallback = bool (*)(float grams);
    using RecipeCallback = bool (*)(const BrewRecipe& recipe);

    WebDashboard();
    void begin(ActionCallback tare_cb, ActionCallback start_cb, ActionCallback reset_cb, TargetCallback target_cb, ActionCallback cal_tare_cb, CalibrateCallback calibrate_cb, RecipeCallback recipe_cb);
    void update();
    void set_state(const WebDashboardState& new_state);
    IPAddress ip() const { return dashboard_ip; }

private:
    WebServer server;
    WebDashboardState state;
    ActionCallback on_tare = nullptr;
    ActionCallback on_start = nullptr;
    ActionCallback on_reset = nullptr;
    TargetCallback on_target = nullptr;
    ActionCallback on_cal_tare = nullptr;
    CalibrateCallback on_calibrate = nullptr;
    RecipeCallback on_recipe = nullptr;
    IPAddress dashboard_ip;
    bool ap_mode = false;

    void handle_root();
    void handle_settings();
    void handle_recipes();
    void handle_wifi();
    void handle_status();
    void handle_tare();
    void handle_start();
    void handle_reset();
    void handle_target();
    void handle_cal_tare();
    void handle_calibrate();
    void handle_recipe();
    void handle_wifi_save();
    void handle_wifi_scan();
    void handle_wifi_clear();
    void send_action_ok(const char* action);
    String build_html() const;
    String build_settings_html() const;
    String build_recipes_html() const;
    String build_wifi_html() const;
    String build_json() const;
    bool wifi_restart_pending = false;
    uint32_t wifi_restart_at_ms = 0;
};
