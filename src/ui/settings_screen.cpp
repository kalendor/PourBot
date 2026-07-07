#include "settings_screen.h"
#include "../config/version.h"

lv_obj_t* SettingsScreen::make_button(lv_obj_t* parent, const char* text, int y, lv_color_t color, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, 240, 58);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(btn, 20, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_center(label);
    return btn;
}

void SettingsScreen::create(lv_event_cb_t calibration_cb, lv_event_cb_t back_cb) {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x020617), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_color(title, lv_color_hex(0xCBD5E1), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t* hint = lv_label_create(scr);
    lv_label_set_text(hint, "Swipe down to go home");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 52);

    make_button(scr, "CALIBRATION", 118, lv_color_hex(0x2563EB), calibration_cb);
    make_button(scr, "BACK", 196, lv_color_hex(0x1E293B), back_cb);

    lv_obj_t* about = lv_label_create(scr);
    lv_label_set_text(about, FIRMWARE_NAME " " FIRMWARE_VERSION);
    lv_obj_set_style_text_color(about, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(about, &lv_font_montserrat_16, 0);
    lv_obj_align(about, LV_ALIGN_BOTTOM_MID, 0, -16);
}
