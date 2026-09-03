#pragma once

// Waveshare ESP32-S3 Touch AMOLED 1.64 known-good display/touch settings
#define HW_TOUCH_I2C_SDA_PIN 47
#define HW_TOUCH_I2C_SCL_PIN 48
#define HW_TOUCH_I2C_ADDRESS  0x38

#define HW_DISPLAY_WIDTH_PX 280
#define HW_DISPLAY_HEIGHT_PX 456
#define HW_DISPLAY_OFFSET_X_PX 0
#define HW_DISPLAY_ROTATION_DEG 0
#define HW_DISPLAY_IPS_INVERT_X 180
#define HW_DISPLAY_IPS_INVERT_Y 24
#define HW_DISPLAY_COLOR_ORDER 20

#define HW_DISPLAY_CS_PIN 9
#define HW_DISPLAY_SCK_PIN 10
#define HW_DISPLAY_D0_PIN 11
#define HW_DISPLAY_D1_PIN 12
#define HW_DISPLAY_D2_PIN 13
#define HW_DISPLAY_D3_PIN 14
#define HW_DISPLAY_RESET_PIN 21

// Keep the working HX711 pins from your existing project
#define HW_LOADCELL_DOUT_PIN 3
#define HW_LOADCELL_SCK_PIN  2

// Capacitive touch modules drive LOW while idle and HIGH while touched.
#define HW_START_PAUSE_BUTTON_PIN 5
#define HW_TARE_SLEEP_BUTTON_PIN  6
#define HW_BUTTON_ACTIVE_LOW      0
#define HW_BUTTON_DEBOUNCE_MS     10
#define HW_TOUCH_RELEASE_SETTLE_MS 200
#define HW_RESET_HOLD_MS          1000
#define HW_SLEEP_HOLD_MS          2000

#define HW_SERIAL_BAUD_RATE 115200
#define DEBUG_SUPPRESS_TOUCH_I2C_ERRORS 1

// Wi-Fi dashboard.
// Leave these blank for user-configured WiFi through the web setup page.
// If no saved WiFi exists, the scale starts the fallback setup access point.
#define HW_WIFI_STA_SSID ""
#define HW_WIFI_STA_PASSWORD ""
#define HW_WIFI_HOSTNAME "pourbot"
#define HW_WIFI_CONNECT_TIMEOUT_MS 15000

// Local clock used for timestamped SD brew-log filenames. This POSIX timezone
// observes Eastern daylight/standard time automatically.
#define HW_TIMEZONE "EST5EDT,M3.2.0,M11.1.0"
#define HW_NTP_SERVER_1 "pool.ntp.org"
#define HW_NTP_SERVER_2 "time.nist.gov"

// Fallback mode: if home Wi-Fi fails, the ESP32 creates this access point.
#define HW_WIFI_AP_SSID "pourbot"
#define HW_WIFI_AP_PASSWORD ""

// Battery / power monitor using the Waveshare ADC voltage sense path.
// The Waveshare wiki ADC demo uses ADC1 channel 3 and reports raw ~1900
// as system voltage about 4.9V when USB powered.
#define HW_BATTERY_ADC_ENABLED 1
#define HW_BATTERY_ADC_PIN 4
#define HW_BATTERY_ADC_SAMPLES 16
#define HW_BATTERY_READ_INTERVAL_MS 1000

// TP4057 CHRG is an open-drain, active-low output. GPIO7 has an external
// 10 kOhm pull-up to 3.3V: LOW means the battery is actively charging.
#define HW_CHARGE_STATUS_ENABLED 1
#define HW_CHARGE_STATUS_PIN 7
#define HW_CHARGE_STATUS_ACTIVE_LOW 1

// Onboard TF card in 1-bit SDMMC mode.
#define HW_SD_CLK_PIN 41
#define HW_SD_CMD_PIN 39
#define HW_SD_D0_PIN  40

// 4.9V / 1900 raw counts ≈ 0.00258 V per raw count.
#define HW_BATTERY_ADC_RAW_TO_SYSTEM_V 0.00258f

// USB power usually reads above this level; LiPo battery reads 3.3–4.2V.
// This threshold is used only to avoid interpreting the USB/system rail as
// battery percentage. Charging state comes from the TP4057 CHRG input above.
#define HW_USB_POWER_ESTIMATE_V 4.45f
