#include "amoled_theme.h"
#include <Preferences.h>

AmoledTheme amoled_theme;

namespace {
constexpr const char* kNamespace = "amoled";
}

void AmoledTheme::begin() {
    Preferences prefs;
    prefs.begin(kNamespace, true);
    current.button = prefs.getUInt("button", current.button);
    current.pour_fill = prefs.getUInt("pour", current.pour_fill);
    current.hold_fill = prefs.getUInt("hold", current.hold_fill);
    current.weight_text = prefs.getUInt("weight", current.weight_text);
    current.timer_text = prefs.getUInt("timer", current.timer_text);
    current.background = prefs.getUInt("bg", current.background);
    prefs.end();
}

void AmoledTheme::save() {
    Preferences prefs;
    prefs.begin(kNamespace, false);
    prefs.putUInt("button", current.button);
    prefs.putUInt("pour", current.pour_fill);
    prefs.putUInt("hold", current.hold_fill);
    prefs.putUInt("weight", current.weight_text);
    prefs.putUInt("timer", current.timer_text);
    prefs.putUInt("bg", current.background);
    prefs.end();
}

void AmoledTheme::set(const AmoledThemeColors& colors) {
    current = colors;
    save();
}

void AmoledTheme::reset() {
    current = AmoledThemeColors{};
    save();
}
