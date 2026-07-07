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

#define HW_SERIAL_BAUD_RATE 115200
#define DEBUG_SUPPRESS_TOUCH_I2C_ERRORS 1

// Wi-Fi dashboard.
// Leave these blank for user-configured WiFi through the web setup page.
// If no saved WiFi exists, the scale starts the fallback setup access point.
#define HW_WIFI_STA_SSID ""
#define HW_WIFI_STA_PASSWORD ""
#define HW_WIFI_HOSTNAME "pouroverscale"
#define HW_WIFI_CONNECT_TIMEOUT_MS 15000

// Fallback mode: if home Wi-Fi fails, the ESP32 creates this access point.
#define HW_WIFI_AP_SSID "PouroverScale"
#define HW_WIFI_AP_PASSWORD ""

// Battery / power monitor using the Waveshare ADC voltage sense path.
// The Waveshare wiki ADC demo uses ADC1 channel 3 and reports raw ~1900
// as system voltage about 4.9V when USB powered.
#define HW_BATTERY_ADC_ENABLED 1
#define HW_BATTERY_ADC_PIN 4
#define HW_BATTERY_ADC_SAMPLES 16
#define HW_BATTERY_READ_INTERVAL_MS 1000

// 4.9V / 1900 raw counts ≈ 0.00258 V per raw count.
#define HW_BATTERY_ADC_RAW_TO_SYSTEM_V 0.00258f

// The board exposes system voltage, not a dedicated charge-status pin.
// USB power usually reads above this level; LiPo battery reads 3.3–4.2V.
#define HW_USB_POWER_ESTIMATE_V 4.45f
