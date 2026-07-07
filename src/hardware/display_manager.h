#pragma once
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "touch_driver.h"
#include "../config/hardware.h"

class DisplayManager {
public:
    void init();
    void update();
    void set_brightness(float brightness);
    bool is_initialized() const { return initialized; }
    TouchData get_touch_data() const { return touch_driver.get_touch_data(); }
    uint32_t width() const { return screen_width; }
    uint32_t height() const { return screen_height; }

private:
    Arduino_DataBus* bus = nullptr;
    Arduino_GFX* gfx_device = nullptr;
    lv_display_t* lvgl_display = nullptr;
    lv_indev_t* lvgl_input = nullptr;
    lv_color_t* draw_buffer = nullptr;
    uint16_t* dma_staging_buffer = nullptr;
    TouchDriver touch_driver;
    uint16_t dma_staging_rows = 16;
    uint32_t screen_width = 0;
    uint32_t screen_height = 0;
    uint32_t buffer_size = 0;
    bool initialized = false;

    static void display_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
    static void display_rounder_cb(lv_event_t* e);
    static void touchpad_read_cb(lv_indev_t* indev, lv_indev_data_t* data);
    static uint32_t millis_cb();
};

extern DisplayManager* g_display_manager;
