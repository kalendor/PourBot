#include "home_screen.h"
#include <Arduino.h>

static const lv_color_t COLOR_BG       = lv_color_hex(0x020617);
static const lv_color_t COLOR_CARD     = lv_color_hex(0x05070B);
static const lv_color_t COLOR_BORDER   = lv_color_hex(0x1F2937);
static const lv_color_t COLOR_TEXT     = lv_color_hex(0xF8FAFC);
static const lv_color_t COLOR_MUTED    = lv_color_hex(0x94A3B8);
static const lv_color_t COLOR_DIM      = lv_color_hex(0x334155);
static const lv_color_t COLOR_AMBER    = lv_color_hex(0xFDBA4B);
static const lv_color_t COLOR_BLUE     = lv_color_hex(0x38BDF8);
static const lv_color_t COLOR_GREEN    = lv_color_hex(0x4ADE80);

void HomeScreen::start_button_event_router(lv_event_t* e) {
    HomeScreen* self = static_cast<HomeScreen*>(lv_event_get_user_data(e));
    if (self) self->handle_start_button_event(e);
}

void HomeScreen::handle_start_button_event(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t now = millis();

    if (code == LV_EVENT_PRESSED) {
        start_button_pressed_ms = now;
        start_button_long_reset_sent = false;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        if (!start_button_long_reset_sent && start_button_pressed_ms > 0 && (now - start_button_pressed_ms) >= 2000UL) {
            start_button_long_reset_sent = true;
            if (reset_button_cb) reset_button_cb(nullptr);
        }
        return;
    }

    if (code == LV_EVENT_CLICKED) {
        if (!start_button_long_reset_sent && start_button_cb) {
            start_button_cb(nullptr);
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        start_button_pressed_ms = 0;
    }
}

void HomeScreen::set_timer_text(const char* text) {
    // Fixed-position timer characters prevent visual shifting when proportional
    // digits such as 1 and 8 have different widths.
    for (int i = 0; i < 7; i++) {
        char c[2] = { text[i] ? text[i] : ' ', '\0' };
        if (timer_chars[i]) {
            lv_label_set_text(timer_chars[i], c);
        }
    }
}

void HomeScreen::set_state_colors(bool running, float grams, const BrewRecipe& recipe, const BrewStageStatus* stage) {
    const float complete_target = stage ? stage->stage_target_g : recipe.water_g;
    const bool complete = (complete_target > 0.0f && grams >= complete_target - 0.5f);
    const bool holding = stage && stage->holding;
    // During a recipe hold, use a slower amber pulse as a visual cue:
    // amber = hold/wait, green = ready for the next pour.
    const bool pulse_on = holding && ((millis() / 850UL) % 2UL == 0);

    lv_color_t accent = running ? COLOR_AMBER : COLOR_BLUE;
    if (complete) accent = COLOR_GREEN;
    if (pulse_on) accent = COLOR_AMBER;

    lv_obj_set_style_arc_color(progress_arc, accent, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(timer_box, accent, 0);
    lv_obj_set_style_bg_color(flow_dot, accent, 0);
    for (int i = 0; i < 7; i++) {
        if (timer_chars[i]) lv_obj_set_style_text_color(timer_chars[i], accent, 0);
    }
}

void HomeScreen::create(const BrewRecipe& recipe, lv_event_cb_t tare_cb, lv_event_cb_t start_cb, lv_event_cb_t, lv_event_cb_t reset_cb) {
    start_button_cb = start_cb;
    reset_button_cb = reset_cb;
    start_button_pressed_ms = 0;
    start_button_long_reset_sent = false;
    lv_obj_t* scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Home display plus a direct on-screen TARE control.
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_CLICKABLE);

    card = lv_obj_create(scr);
    lv_obj_set_size(card, 270, 418);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 28, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COLOR_BORDER, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

    // Battery / USB status. Drawn with LVGL primitives instead of emoji so it
    // works with the enabled Montserrat fonts.
    status_battery = lv_label_create(card);
    lv_label_set_text(status_battery, "PWR --");
    lv_obj_set_style_text_color(status_battery, COLOR_MUTED, 0);
    lv_obj_set_style_text_font(status_battery, &lv_font_montserrat_14, 0);
    lv_obj_align(status_battery, LV_ALIGN_TOP_RIGHT, -16, 10);

    battery_body = lv_obj_create(card);
    lv_obj_set_size(battery_body, 24, 12);
    lv_obj_align_to(battery_body, status_battery, LV_ALIGN_OUT_LEFT_MID, -8, 0);
    lv_obj_set_style_bg_opa(battery_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(battery_body, 1, 0);
    lv_obj_set_style_border_color(battery_body, COLOR_MUTED, 0);
    lv_obj_set_style_radius(battery_body, 2, 0);
    lv_obj_set_style_pad_all(battery_body, 1, 0);
    lv_obj_clear_flag(battery_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(battery_body, LV_OBJ_FLAG_CLICKABLE);

    battery_fill = lv_obj_create(battery_body);
    lv_obj_set_size(battery_fill, 0, 8);
    lv_obj_align(battery_fill, LV_ALIGN_LEFT_MID, 1, 0);
    lv_obj_set_style_bg_color(battery_fill, COLOR_MUTED, 0);
    lv_obj_set_style_bg_opa(battery_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(battery_fill, 0, 0);
    lv_obj_set_style_radius(battery_fill, 1, 0);
    lv_obj_clear_flag(battery_fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(battery_fill, LV_OBJ_FLAG_CLICKABLE);

    battery_tip = lv_obj_create(card);
    lv_obj_set_size(battery_tip, 3, 6);
    lv_obj_align_to(battery_tip, battery_body, LV_ALIGN_OUT_RIGHT_MID, 1, 0);
    lv_obj_set_style_bg_color(battery_tip, COLOR_MUTED, 0);
    lv_obj_set_style_bg_opa(battery_tip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(battery_tip, 0, 0);
    lv_obj_set_style_radius(battery_tip, 1, 0);
    lv_obj_clear_flag(battery_tip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(battery_tip, LV_OBJ_FLAG_CLICKABLE);

    // Concept-B inspired circular progress display.
    progress_arc = lv_arc_create(card);
    lv_obj_set_size(progress_arc, 238, 238);
    lv_obj_align(progress_arc, LV_ALIGN_TOP_MID, 0, 10);
    lv_arc_set_range(progress_arc, 0, 1000);
    lv_arc_set_value(progress_arc, 0);
    lv_arc_set_bg_angles(progress_arc, 135, 45);
    lv_arc_set_rotation(progress_arc, 0);
    lv_obj_remove_style(progress_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(progress_arc, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_width(progress_arc, 9, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x1F2937), LV_PART_MAIN);
    lv_obj_set_style_arc_color(progress_arc, COLOR_AMBER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(progress_arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(progress_arc, false, LV_PART_INDICATOR);

    weight_target_label = lv_label_create(card);
    lv_label_set_text(weight_target_label, "0 / 320");
    lv_obj_set_style_text_color(weight_target_label, COLOR_MUTED, 0);
    lv_obj_set_style_text_font(weight_target_label, &lv_font_montserrat_24, 0);
    lv_obj_align(weight_target_label, LV_ALIGN_TOP_MID, 0, 74);

    weight_label = lv_label_create(card);
    lv_label_set_text(weight_label, "0.0");
    lv_obj_set_size(weight_label, 260, 72);
    lv_obj_set_style_text_align(weight_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(weight_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(weight_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(weight_label, &lv_font_montserrat_48, 0);
    lv_obj_align(weight_label, LV_ALIGN_TOP_MID, 0, 111);

    grams_label = lv_label_create(card);
    lv_label_set_text(grams_label, "grams");
    lv_obj_set_style_text_color(grams_label, COLOR_MUTED, 0);
    lv_obj_set_style_text_font(grams_label, &lv_font_montserrat_16, 0);
    lv_obj_align(grams_label, LV_ALIGN_TOP_MID, 0, 177);

    ready_label = lv_label_create(card);
    lv_label_set_text(ready_label, "READY");
    lv_obj_set_style_text_color(ready_label, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(ready_label, &lv_font_montserrat_16, 0);
    lv_obj_align(ready_label, LV_ALIGN_TOP_MID, 0, 196);
    lv_obj_add_flag(ready_label, LV_OBJ_FLAG_HIDDEN);

    // Timer capsule.
    timer_box = lv_obj_create(card);
    lv_obj_set_size(timer_box, 178, 45);
    lv_obj_align(timer_box, LV_ALIGN_TOP_MID, 0, 218);
    lv_obj_set_style_bg_color(timer_box, lv_color_hex(0x080B12), 0);
    lv_obj_set_style_bg_opa(timer_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(timer_box, 2, 0);
    lv_obj_set_style_border_color(timer_box, COLOR_AMBER, 0);
    lv_obj_set_style_radius(timer_box, 23, 0);
    lv_obj_set_style_pad_all(timer_box, 0, 0);
    lv_obj_clear_flag(timer_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(timer_box, LV_OBJ_FLAG_CLICKABLE);

    const int timer_x[7] = {19, 42, 64, 90, 113, 135, 155};
    const char* init_timer = "00:00.0";
    for (int i = 0; i < 7; i++) {
        timer_chars[i] = lv_label_create(timer_box);
        char c[2] = { init_timer[i], '\0' };
        lv_label_set_text(timer_chars[i], c);
        lv_obj_set_style_text_color(timer_chars[i], COLOR_AMBER, 0);
        lv_obj_set_style_text_font(timer_chars[i], &lv_font_montserrat_28, 0);
        lv_obj_set_width(timer_chars[i], 22);
        lv_obj_set_style_text_align(timer_chars[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(timer_chars[i], LV_ALIGN_LEFT_MID, timer_x[i] - 11, 0);
    }

    // Flow row.
    flow_dot = lv_obj_create(card);
    lv_obj_set_size(flow_dot, 11, 11);
    lv_obj_align(flow_dot, LV_ALIGN_TOP_MID, -58, 288);
    lv_obj_set_style_bg_color(flow_dot, COLOR_AMBER, 0);
    lv_obj_set_style_bg_opa(flow_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(flow_dot, 0, 0);
    lv_obj_set_style_radius(flow_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(flow_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(flow_dot, LV_OBJ_FLAG_CLICKABLE);

    flow_label = lv_label_create(card);
    lv_label_set_text(flow_label, "0.0 g/s");
    lv_obj_set_style_text_color(flow_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(flow_label, &lv_font_montserrat_28, 0);
    lv_obj_align_to(flow_label, flow_dot, LV_ALIGN_OUT_RIGHT_MID, 12, 0);

    tare_button = lv_button_create(card);
    lv_obj_set_size(tare_button, 125, 86);
    lv_obj_align(tare_button, LV_ALIGN_BOTTOM_LEFT, 8, -10);
    lv_obj_set_style_bg_color(tare_button, COLOR_AMBER, 0);
    lv_obj_set_style_bg_opa(tare_button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tare_button, 0, 0);
    lv_obj_set_style_radius(tare_button, 29, 0);
    lv_obj_set_style_bg_color(tare_button, lv_color_hex(0xD97706), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(tare_button, 0, 0);
    lv_obj_clear_flag(tare_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(tare_button, tare_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* tare_text = lv_label_create(tare_button);
    lv_label_set_text(tare_text, "TARE");
    lv_obj_set_style_text_color(tare_text, COLOR_BG, 0);
    lv_obj_set_style_text_font(tare_text, &lv_font_montserrat_24, 0);
    lv_obj_center(tare_text);

    start_button = lv_button_create(card);
    lv_obj_set_size(start_button, 125, 86);
    lv_obj_align(start_button, LV_ALIGN_BOTTOM_RIGHT, -8, -10);
    lv_obj_set_style_bg_color(start_button, COLOR_GREEN, 0);
    lv_obj_set_style_bg_opa(start_button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(start_button, 0, 0);
    lv_obj_set_style_radius(start_button, 29, 0);
    lv_obj_set_style_bg_color(start_button, lv_color_hex(0x16A34A), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(start_button, 0, 0);
    lv_obj_clear_flag(start_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(start_button, start_button_event_router, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(start_button, start_button_event_router, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(start_button, start_button_event_router, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(start_button, start_button_event_router, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(start_button, start_button_event_router, LV_EVENT_PRESS_LOST, this);

    start_button_label = lv_label_create(start_button);
    lv_label_set_text(start_button_label, "START");
    lv_obj_set_style_text_color(start_button_label, COLOR_BG, 0);
    lv_obj_set_style_text_font(start_button_label, &lv_font_montserrat_24, 0);
    lv_obj_center(start_button_label);

    progress_label = lv_label_create(card);
    lv_label_set_text(progress_label, "0% to target");
    lv_obj_set_width(progress_label, 238);
    lv_obj_set_style_text_align(progress_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(progress_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(progress_label, &lv_font_montserrat_14, 0);
    lv_obj_align(progress_label, LV_ALIGN_TOP_MID, 0, 268);

    set_state_colors(false, 0.0f, recipe);
}

void HomeScreen::update(float grams, float flow_gps, uint32_t elapsed_ms, bool running, const BrewRecipe& recipe, const BrewStageStatus& stage, int battery_percent, bool battery_valid, bool charging, bool prebrew_pending, bool ready) {
    char buf[64];


    if (status_battery) {
        lv_color_t bat_color = COLOR_DIM;
        int fill_w = 0;
        if (battery_valid) {
            if (charging && battery_percent < 0) {
                lv_label_set_text(status_battery, "USB");
            } else if (charging) {
                snprintf(buf, sizeof(buf), "USB %d%%", battery_percent);
                lv_label_set_text(status_battery, buf);
            } else {
                snprintf(buf, sizeof(buf), "BAT %d%%", battery_percent);
                lv_label_set_text(status_battery, buf);
            }

            bat_color = COLOR_MUTED;
            if (charging) bat_color = COLOR_GREEN;
            else if (battery_percent <= 10) bat_color = lv_color_hex(0xEF4444);
            else if (battery_percent <= 20) bat_color = COLOR_AMBER;

            if (charging && battery_percent < 0) {
                fill_w = 10; // USB present, battery percentage unknown from this ADC path
            } else {
                fill_w = (battery_percent * 20) / 100;
                if (fill_w < 1) fill_w = 1;
                if (fill_w > 20) fill_w = 20;
            }
        } else {
            lv_label_set_text(status_battery, "PWR --");
            fill_w = 0;
        }
        lv_obj_set_style_text_color(status_battery, bat_color, 0);
        if (battery_body) lv_obj_set_style_border_color(battery_body, bat_color, 0);
        if (battery_tip) lv_obj_set_style_bg_color(battery_tip, bat_color, 0);
        if (battery_fill) {
            lv_obj_set_width(battery_fill, fill_w);
            lv_obj_set_style_bg_color(battery_fill, bat_color, 0);
        }
    }

    snprintf(buf, sizeof(buf), "%.1f", grams);
    lv_label_set_text(weight_label, buf);
    lv_obj_align(weight_label, LV_ALIGN_TOP_MID, 0, 111);

    uint32_t minutes = elapsed_ms / 60000UL;
    uint32_t seconds = (elapsed_ms / 1000UL) % 60UL;
    uint32_t tenths = (elapsed_ms / 100UL) % 10UL;
    snprintf(buf, sizeof(buf), "%02lu:%02lu.%lu", minutes, seconds, tenths);
    set_timer_text(buf);

    snprintf(buf, sizeof(buf), "%.1f g/s", flow_gps);
    lv_label_set_text(flow_label, buf);

    float safe_grams = grams < 0 ? 0 : grams;
    if (stage.recipe_mode) {
        snprintf(buf, sizeof(buf), "%s  %.0f / %.0f", stage.stage_name, safe_grams, stage.stage_target_g);
    } else {
        snprintf(buf, sizeof(buf), "%.0f / %.0f", safe_grams, recipe.water_g);
    }
    lv_label_set_text(weight_target_label, buf);

    float pct = 0.0f;
    if (stage.recipe_mode && stage.stage_target_g > stage.stage_start_g) {
        pct = constrain((safe_grams - stage.stage_start_g) / (stage.stage_target_g - stage.stage_start_g), 0.0f, 1.0f);
    } else if (recipe.water_g > 0.0f) pct = constrain(safe_grams / recipe.water_g, 0.0f, 1.0f);
    int arc_value = (int)(pct * 1000.0f);
    lv_arc_set_value(progress_arc, arc_value);

    if (ready_label) {
        if (ready && !running) lv_obj_clear_flag(ready_label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(ready_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (start_button_label) {
        lv_label_set_text(start_button_label, running ? "PAUSE" : "START");
    }

    if (prebrew_pending) {
        lv_label_set_text(progress_label, "settling / auto tare");
    } else if (ready && !running) {
        lv_label_set_text(progress_label, "ready to pour");
    } else {
        if (stage.recipe_mode) {
            if (stage.holding) {
                snprintf(buf, sizeof(buf), "HOLD · %lus", (unsigned long)stage.hold_remaining_seconds);
            } else {
                snprintf(buf, sizeof(buf), "stage %u/%u · %d%%", (unsigned)(stage.active_index + 1), (unsigned)stage.stage_count, (int)(pct * 100.0f + 0.5f));
            }
        } else {
            snprintf(buf, sizeof(buf), "%d%% to target", (int)(pct * 100.0f + 0.5f));
        }
        lv_label_set_text(progress_label, buf);
    }

    // Always refresh state colors. The pre-brew/post-brew automation can change
    // the display from settling blue -> ready amber/green conditions without a
    // running-state change, so a one-time cached color update can leave the arc
    // blue on the next brew cycle.
    set_state_colors(running, prebrew_pending ? 0.0f : safe_grams, recipe, &stage);
    last_running = running;
}
