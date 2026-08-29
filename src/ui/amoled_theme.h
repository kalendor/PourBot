#pragma once

#include <Arduino.h>

struct AmoledThemeColors {
    uint32_t button = 0xCC5000;
    uint32_t pour_fill = 0xCC5000;
    uint32_t hold_fill = 0xD32F2F;
    uint32_t weight_text = 0xF8FAFC;
    uint32_t timer_text = 0xF8FAFC;
    uint32_t background = 0x020617;
};

class AmoledTheme {
public:
    void begin();
    const AmoledThemeColors& colors() const { return current; }
    void set(const AmoledThemeColors& colors);
    void reset();

private:
    AmoledThemeColors current;
    void save();
};

extern AmoledTheme amoled_theme;
