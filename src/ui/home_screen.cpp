#include "home_screen.h"
#include "amoled_theme.h"
#include <Arduino.h>

static const lv_color_t COLOR_BG       = lv_color_hex(0x020617);
static const lv_color_t COLOR_CARD     = lv_color_hex(0x05070B);
static const lv_color_t COLOR_BORDER   = lv_color_hex(0x1F2937);
static const lv_color_t COLOR_TEXT     = lv_color_hex(0xF8FAFC);
static const lv_color_t COLOR_ORANGE_PRESSED = lv_color_hex(0x78350F);
static const lv_color_t COLOR_GREEN    = lv_color_hex(0x4ADE80);
static constexpr uint32_t START_BUTTON_HOLD_MS = 1000UL;

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
        if (!start_button_long_reset_sent && start_button_pressed_ms > 0 && (now - start_button_pressed_ms) >= START_BUTTON_HOLD_MS) {
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
    (void)running;
    (void)grams;
    (void)recipe;

    const AmoledThemeColors& theme = amoled_theme.colors();
    const lv_color_t fill_accent = lv_color_hex((stage && stage->holding) ? theme.hold_fill : theme.pour_fill);
    if (progress_panel) lv_obj_set_style_bg_color(progress_panel, fill_accent, LV_PART_INDICATOR);
    if (screen_obj) lv_obj_set_style_bg_color(screen_obj, lv_color_hex(theme.background), 0);
    if (tare_button) lv_obj_set_style_bg_color(tare_button, lv_color_hex(theme.button), 0);
    if (start_button) lv_obj_set_style_bg_color(start_button, lv_color_hex(theme.button), 0);
    if (weight_label) {
        lv_obj_set_style_text_color(weight_label, lv_color_hex(theme.weight_text), 0);
        lv_obj_set_style_text_outline_stroke_color(weight_label, lv_color_hex(theme.weight_text), 0);
    }
    if (grams_label) lv_obj_set_style_text_color(grams_label, lv_color_hex(theme.weight_text), 0);
    if (hold_label) lv_obj_set_style_text_color(hold_label, lv_color_hex(theme.timer_text), 0);
    lv_obj_set_style_border_color(timer_box, lv_color_hex(theme.timer_text), 0);
    for (int i = 0; i < 7; i++) {
        if (timer_chars[i]) lv_obj_set_style_text_color(timer_chars[i], lv_color_hex(theme.timer_text), 0);
    }
}

void HomeScreen::create(const BrewRecipe& recipe, lv_event_cb_t tare_cb, lv_event_cb_t start_cb, lv_event_cb_t, lv_event_cb_t reset_cb) {
    start_button_cb = start_cb;
    reset_button_cb = reset_cb;
    start_button_pressed_ms = 0;
    start_button_long_reset_sent = false;
    lv_obj_t* scr = lv_screen_active();
    screen_obj = scr;
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

    // Full-width weight panel. It preserves the same bottom-up fill behavior
    // while using the top of the AMOLED much more efficiently than a circle.
    progress_panel = lv_bar_create(card);
    lv_obj_set_size(progress_panel, 254, 200);
    lv_obj_align(progress_panel, LV_ALIGN_TOP_MID, 0, 8);
    lv_bar_set_range(progress_panel, 0, 1000);
    lv_bar_set_orientation(progress_panel, LV_BAR_ORIENTATION_VERTICAL);
    lv_bar_set_value(progress_panel, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_panel, lv_color_hex(0x080B12), 0);
    lv_obj_set_style_bg_opa(progress_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(progress_panel, 0, 0);
    lv_obj_set_style_radius(progress_panel, 18, 0);
    lv_obj_set_style_pad_all(progress_panel, 0, 0);
    lv_obj_set_style_bg_color(progress_panel, lv_color_hex(amoled_theme.colors().pour_fill), LV_PART_INDICATOR);
    // Use the exact solid burnt orange shown on the TARE and START/PAUSE buttons.
    lv_obj_set_style_bg_opa(progress_panel, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(progress_panel, 0, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(progress_panel, 0, LV_PART_INDICATOR);
    lv_obj_clear_flag(progress_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(progress_panel, LV_OBJ_FLAG_CLICKABLE);

    stage_label = lv_label_create(card);
    lv_label_set_text(stage_label, "TARGET");
    lv_obj_set_width(stage_label, 230);
    lv_label_set_long_mode(stage_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(stage_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(stage_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(stage_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_outline_stroke_color(stage_label, COLOR_BG, 0);
    lv_obj_set_style_text_outline_stroke_width(stage_label, 1, 0);
    lv_obj_align(stage_label, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_add_flag(stage_label, LV_OBJ_FLAG_HIDDEN);

    weight_label = lv_label_create(card);
    lv_label_set_text(weight_label, "0.0");
    lv_obj_set_size(weight_label, 260, 64);
    lv_obj_set_style_text_align(weight_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(weight_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(weight_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(weight_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_outline_stroke_color(weight_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_outline_stroke_width(weight_label, 3, 0);
    lv_obj_set_style_text_outline_stroke_opa(weight_label, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_pivot_x(weight_label, 130, 0);
    lv_obj_set_style_transform_pivot_y(weight_label, 32, 0);
    lv_obj_set_style_transform_scale(weight_label, 320, 0);
    lv_obj_align_to(weight_label, progress_panel, LV_ALIGN_CENTER, 0, -12);

    grams_label = lv_label_create(card);
    lv_label_set_text(grams_label, "GRAMS");
    lv_obj_set_width(grams_label, 180);
    lv_obj_set_style_text_align(grams_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(grams_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(grams_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_letter_space(grams_label, 3, 0);
    lv_obj_set_style_text_outline_stroke_color(grams_label, COLOR_BG, 0);
    lv_obj_set_style_text_outline_stroke_width(grams_label, 1, 0);
    lv_obj_align(grams_label, LV_ALIGN_TOP_MID, 0, 143);

    ready_label = lv_label_create(card);
    lv_label_set_text(ready_label, "READY");
    lv_obj_set_style_text_color(ready_label, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(ready_label, &lv_font_montserrat_16, 0);
    lv_obj_align(ready_label, LV_ALIGN_TOP_MID, 0, 145);
    lv_obj_add_flag(ready_label, LV_OBJ_FLAG_HIDDEN);

    // Timer capsule.
    timer_box = lv_obj_create(card);
    lv_obj_set_size(timer_box, 178, 45);
    lv_obj_align(timer_box, LV_ALIGN_TOP_MID, 0, 218);
    lv_obj_set_style_bg_color(timer_box, lv_color_hex(0x080B12), 0);
    lv_obj_set_style_bg_opa(timer_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(timer_box, 2, 0);
    lv_obj_set_style_border_color(timer_box, COLOR_TEXT, 0);
    lv_obj_set_style_radius(timer_box, 23, 0);
    lv_obj_set_style_pad_all(timer_box, 0, 0);
    lv_obj_clear_flag(timer_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(timer_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(timer_box, LV_OBJ_FLAG_HIDDEN);

    const int timer_x[7] = {19, 42, 64, 90, 113, 135, 155};
    const char* init_timer = "00:00.0";
    for (int i = 0; i < 7; i++) {
        timer_chars[i] = lv_label_create(timer_box);
        char c[2] = { init_timer[i], '\0' };
        lv_label_set_text(timer_chars[i], c);
        lv_obj_set_style_text_color(timer_chars[i], COLOR_TEXT, 0);
        lv_obj_set_style_text_font(timer_chars[i], &lv_font_montserrat_28, 0);
        lv_obj_set_width(timer_chars[i], 22);
        lv_obj_set_style_text_align(timer_chars[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(timer_chars[i], LV_ALIGN_LEFT_MID, timer_x[i] - 11, 0);
    }

    // Recipe hold countdown row.
    hold_label = lv_label_create(card);
    lv_label_set_text(hold_label, "HOLD --:--");
    lv_obj_set_style_text_color(hold_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(hold_label, &lv_font_montserrat_28, 0);
    lv_obj_align(hold_label, LV_ALIGN_TOP_MID, 0, 277);
    lv_obj_add_flag(hold_label, LV_OBJ_FLAG_HIDDEN);

    tare_button = lv_button_create(card);
    lv_obj_set_size(tare_button, 125, 86);
    lv_obj_align(tare_button, LV_ALIGN_BOTTOM_LEFT, 8, -10);
    lv_obj_set_style_bg_color(tare_button, lv_color_hex(amoled_theme.colors().button), 0);
    lv_obj_set_style_bg_opa(tare_button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tare_button, 0, 0);
    lv_obj_set_style_radius(tare_button, 29, 0);
    lv_obj_set_style_bg_color(tare_button, COLOR_ORANGE_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(tare_button, 0, 0);
    lv_obj_clear_flag(tare_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(tare_button, tare_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* tare_text = lv_label_create(tare_button);
    lv_label_set_text(tare_text, "TARE");
    lv_obj_set_style_text_color(tare_text, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(tare_text, &lv_font_montserrat_24, 0);
    lv_obj_center(tare_text);

    start_button = lv_button_create(card);
    lv_obj_set_size(start_button, 125, 86);
    lv_obj_align(start_button, LV_ALIGN_BOTTOM_RIGHT, -8, -10);
    lv_obj_set_style_bg_color(start_button, lv_color_hex(amoled_theme.colors().button), 0);
    lv_obj_set_style_bg_opa(start_button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(start_button, 0, 0);
    lv_obj_set_style_radius(start_button, 29, 0);
    lv_obj_set_style_bg_color(start_button, COLOR_ORANGE_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(start_button, 0, 0);
    lv_obj_clear_flag(start_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(start_button, start_button_event_router, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(start_button, start_button_event_router, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(start_button, start_button_event_router, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(start_button, start_button_event_router, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(start_button, start_button_event_router, LV_EVENT_PRESS_LOST, this);

    start_button_label = lv_label_create(start_button);
    lv_label_set_text(start_button_label, "START");
    lv_obj_set_style_text_color(start_button_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(start_button_label, &lv_font_montserrat_24, 0);
    lv_obj_center(start_button_label);

    progress_label = lv_label_create(card);
    lv_label_set_text(progress_label, "0.0 / 320 g");
    lv_obj_set_width(progress_label, 238);
    lv_obj_set_style_text_align(progress_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(progress_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(progress_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_outline_stroke_color(progress_label, COLOR_BG, 0);
    lv_obj_set_style_text_outline_stroke_width(progress_label, 1, 0);
    lv_obj_align(progress_label, LV_ALIGN_TOP_MID, 0, 177);
    lv_obj_add_flag(progress_label, LV_OBJ_FLAG_HIDDEN);

    set_state_colors(false, 0.0f, recipe);
}

void HomeScreen::update(float grams, uint32_t elapsed_ms, bool running, const BrewRecipe& recipe, const BrewStageStatus& stage, int, bool, bool, bool prebrew_pending, bool ready) {
    char buf[64];

    if (stage.holding) {
        lv_label_set_text(weight_label, "STOP");
        if (grams_label) lv_obj_add_flag(grams_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        snprintf(buf, sizeof(buf), "%.1f", grams);
        lv_label_set_text(weight_label, buf);
        if (grams_label) lv_obj_clear_flag(grams_label, LV_OBJ_FLAG_HIDDEN);
    }

    uint32_t minutes = elapsed_ms / 60000UL;
    uint32_t seconds = (elapsed_ms / 1000UL) % 60UL;
    uint32_t tenths = (elapsed_ms / 100UL) % 10UL;
    snprintf(buf, sizeof(buf), "%02lu:%02lu.%lu", minutes, seconds, tenths);
    set_timer_text(buf);

    if (stage.hold_seconds > 0) {
        const uint32_t remaining = stage.holding
            ? stage.hold_remaining_seconds
            : stage.hold_seconds;
        snprintf(buf, sizeof(buf), "HOLD %02lu:%02lu",
                 (unsigned long)(remaining / 60UL),
                 (unsigned long)(remaining % 60UL));
    } else {
        snprintf(buf, sizeof(buf), "HOLD --:--");
    }
    lv_label_set_text(hold_label, buf);
    lv_obj_set_style_text_color(hold_label, COLOR_TEXT, 0);

    float safe_grams = grams < 0 ? 0 : grams;

    const bool brew_active = running || elapsed_ms > 0;
    float pct = 0.0f;
    if (brew_active && stage.recipe_mode && stage.stage_target_g > stage.stage_start_g) {
        pct = constrain((safe_grams - stage.stage_start_g) / (stage.stage_target_g - stage.stage_start_g), 0.0f, 1.0f);
    } else if (brew_active && recipe.water_g > 0.0f) pct = constrain(safe_grams / recipe.water_g, 0.0f, 1.0f);
    const int fill_value = (int)(pct * 1000.0f + 0.5f);
    if (progress_panel) lv_bar_set_value(progress_panel, fill_value, LV_ANIM_OFF);

    if (ready_label) {
        if (ready && !running) lv_obj_clear_flag(ready_label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(ready_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (start_button_label) {
        lv_label_set_text(start_button_label, running ? "PAUSE" : "START");
    }
    if (stage_label) {
        if (brew_active) lv_obj_clear_flag(stage_label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(stage_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (progress_label) {
        if (brew_active) lv_obj_clear_flag(progress_label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(progress_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (timer_box) {
        if (brew_active) lv_obj_clear_flag(timer_box, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(timer_box, LV_OBJ_FLAG_HIDDEN);
    }
    if (hold_label) {
        if (brew_active) lv_obj_clear_flag(hold_label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(hold_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (stage_label) {
        if (stage.recipe_mode) {
            snprintf(buf, sizeof(buf), "POUR %u OF %u",
                     (unsigned)(stage.active_index + 1), (unsigned)stage.stage_count);
        } else {
            snprintf(buf, sizeof(buf), "TARGET");
        }
        lv_label_set_text(stage_label, buf);
    }

    if (prebrew_pending) {
        lv_label_set_text(progress_label, "settling / auto tare");
    } else if (ready && !running) {
        lv_label_set_text(progress_label, "ready to pour");
    } else {
        const float target = stage.recipe_mode ? stage.stage_target_g : recipe.water_g;
        snprintf(buf, sizeof(buf), "%.1f / %.0f g", safe_grams, target);
        lv_label_set_text(progress_label, buf);
    }

    // Always refresh state colors. The pre-brew/post-brew automation can change
    // the display from settling blue -> ready amber/green conditions without a
    // running-state change, so a one-time cached color update can leave the fill
    // blue on the next brew cycle.
    set_state_colors(running, prebrew_pending ? 0.0f : safe_grams, recipe, &stage);
    last_running = running;
}
