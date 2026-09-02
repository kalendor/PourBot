#pragma once
#include "hardware/display_manager.h"
#include "scale/hx711_scale.h"
#include "brew/brew_state.h"
#include "brew/recipe.h"
#include "brew/brew_engine.h"
#include "ui/home_screen.h"
#include "ui/calibration_screen.h"
#include "ui/settings_screen.h"
#include "storage/settings_store.h"
#include "network/web_dashboard.h"
#include "hardware/power_monitor.h"

class App {
public:
    void begin();
    void update();
    void tare();
    void toggle_brew();
    void reset_brew();
    void reset_brew_and_tare();
    void show_home();
    void show_calibration();
    void show_settings();
    void set_calibration_known_weight(float grams);
    void run_calibration();
    void run_web_calibration_tare();
    bool run_web_calibration(float grams);
    void set_target_water(float grams);
    bool set_recipe_from_web(const BrewRecipe& new_recipe);
private:
    struct HardwareButton {
        uint8_t pin = 0;
        bool raw_pressed = false;
        bool pressed = false;
        uint32_t raw_changed_ms = 0;
        uint32_t pressed_ms = 0;
    };

    enum class Screen { Home, Settings, Calibration } screen = Screen::Home;
    DisplayManager display;
    HX711Scale scale;
    BrewState brew;
    BrewEngine brew_engine;
    BrewRecipe recipe = default_recipe();
    HomeScreen home;
    CalibrationScreen calibration;
    SettingsScreen settings_screen;
    SettingsStore settings;
    WebDashboard web;
    PowerMonitor power;
    uint32_t last_ui_ms = 0;
    uint32_t last_web_state_ms = 0;
    HardwareButton start_pause_button;
    HardwareButton tare_sleep_button;

    void update_hardware_buttons();
    bool update_button(HardwareButton& button, uint32_t now);
    void enter_sleep();

    float known_calibration_weight_g = 100.0f;
    const char* calibration_message = "Ready";
};

extern App* g_app;
