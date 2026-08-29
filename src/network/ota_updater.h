#pragma once

#include <Arduino.h>

class OtaUpdater {
public:
    String check_json();
    String status_json() const;
    bool schedule_install(String& error);
    void update();

private:
    String latest_version;
    String firmware_url;
    String message = "Not checked";
    bool update_available = false;
    bool install_pending = false;
    bool installing = false;
    uint32_t install_at_ms = 0;
    int progress_percent = 0;

    bool fetch_manifest(String& error);
    void perform_install();
    static String json_value(const String& json, const char* key);
    static int version_number(const String& version);
    static String json_escape(const String& value);
};

extern OtaUpdater ota_updater;
