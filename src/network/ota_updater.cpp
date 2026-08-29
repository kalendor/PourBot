#include "ota_updater.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <time.h>
#include "config/version.h"

OtaUpdater ota_updater;

namespace {
constexpr const char* kOtaManifestUrl = "https://kalendor.github.io/PourBot/ota.json";

// ISRG Root X1, issued by Internet Security Research Group and valid through
// 2035. GitHub Pages currently serves PourBot through a chain rooted here.
constexpr const char* kRootCa = R"CERT(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)CERT";
}

String OtaUpdater::json_escape(const String& value) {
    String out;
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        if (c == '\\' || c == '"') out += '\\';
        if (c == '\r' || c == '\n') out += ' ';
        else out += c;
    }
    return out;
}

String OtaUpdater::json_value(const String& json, const char* key) {
    const String token = String("\"") + key + "\"";
    int pos = json.indexOf(token);
    if (pos < 0) return "";
    pos = json.indexOf(':', pos + token.length());
    if (pos < 0) return "";
    pos = json.indexOf('"', pos + 1);
    if (pos < 0) return "";
    const int end = json.indexOf('"', pos + 1);
    return end < 0 ? "" : json.substring(pos + 1, end);
}

int OtaUpdater::compare_versions(const String& left, const String& right) {
    // Legacy vNN releases are intentionally below the 1.1.0 semantic-version
    // baseline so installed v73/v74 units can migrate into the new scheme.
    const bool left_semver = left.indexOf('.') >= 0;
    const bool right_semver = right.indexOf('.') >= 0;
    if (left_semver != right_semver) return left_semver ? 1 : -1;

    int left_pos = left.startsWith("v") ? 1 : 0;
    int right_pos = right.startsWith("v") ? 1 : 0;
    for (int segment = 0; segment < 3; ++segment) {
        int left_value = 0;
        int right_value = 0;
        while (left_pos < static_cast<int>(left.length()) && isDigit(left[left_pos])) {
            left_value = left_value * 10 + (left[left_pos++] - '0');
        }
        while (right_pos < static_cast<int>(right.length()) && isDigit(right[right_pos])) {
            right_value = right_value * 10 + (right[right_pos++] - '0');
        }
        if (left_value != right_value) return left_value > right_value ? 1 : -1;
        while (left_pos < static_cast<int>(left.length()) && left[left_pos] != '.') ++left_pos;
        while (right_pos < static_cast<int>(right.length()) && right[right_pos] != '.') ++right_pos;
        if (left_pos < static_cast<int>(left.length())) ++left_pos;
        if (right_pos < static_cast<int>(right.length())) ++right_pos;
    }
    return 0;
}

bool OtaUpdater::fetch_manifest(String& error) {
    if (WiFi.status() != WL_CONNECTED) {
        error = "WiFi is not connected";
        return false;
    }
    if (time(nullptr) < 1700000000) {
        error = "Clock is still synchronizing; try again shortly";
        return false;
    }

    WiFiClientSecure client;
    client.setCACert(kRootCa);
    client.setTimeout(15000);
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    if (!http.begin(client, kOtaManifestUrl)) {
        error = "Could not open update server";
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = String("Update server returned HTTP ") + code;
        http.end();
        return false;
    }
    const String body = http.getString();
    http.end();
    latest_version = json_value(body, "version");
    firmware_url = json_value(body, "url");
    if (!latest_version.length() || !firmware_url.startsWith("https://kalendor.github.io/PourBot/")) {
        error = "Update metadata is invalid";
        return false;
    }
    update_available = compare_versions(latest_version, FIRMWARE_VERSION) > 0;
    message = update_available ? String("Update ") + latest_version + " is available" : "PourBot is up to date";
    return true;
}

String OtaUpdater::check_json() {
    String error;
    const bool ok = fetch_manifest(error);
    if (!ok) message = error;
    String json = String("{\"ok\":") + (ok ? "true" : "false");
    json += ",\"current\":\"" + String(FIRMWARE_VERSION) + "\"";
    json += ",\"latest\":\"" + json_escape(latest_version) + "\"";
    json += ",\"available\":" + String(update_available ? "true" : "false");
    json += ",\"message\":\"" + json_escape(message) + "\"}";
    return json;
}

String OtaUpdater::status_json() const {
    String json = "{\"ok\":true,\"current\":\"" + String(FIRMWARE_VERSION) + "\"";
    json += ",\"latest\":\"" + json_escape(latest_version) + "\"";
    json += ",\"available\":" + String(update_available ? "true" : "false");
    json += ",\"installing\":" + String((installing || install_pending) ? "true" : "false");
    json += ",\"progress\":" + String(progress_percent);
    json += ",\"message\":\"" + json_escape(message) + "\"}";
    return json;
}

bool OtaUpdater::schedule_install(String& error) {
    if (installing || install_pending) {
        error = "An update is already in progress";
        return false;
    }
    if (!fetch_manifest(error)) return false;
    if (!update_available) {
        error = "PourBot is already up to date";
        return false;
    }
    install_pending = true;
    install_at_ms = millis() + 1000;
    progress_percent = 0;
    message = String("Preparing ") + latest_version;
    return true;
}

void OtaUpdater::update() {
    if (install_pending && (int32_t)(millis() - install_at_ms) >= 0) perform_install();
}

void OtaUpdater::perform_install() {
    install_pending = false;
    installing = true;
    message = String("Installing ") + latest_version;
    Serial.printf("OTA: downloading %s\n", firmware_url.c_str());

    WiFiClientSecure client;
    client.setCACert(kRootCa);
    client.setTimeout(30000);
    httpUpdate.rebootOnUpdate(true);
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpUpdate.onProgress([this](int current, int total) {
        if (total > 0) progress_percent = constrain((current * 100) / total, 0, 100);
    });

    const t_httpUpdate_return result = httpUpdate.update(client, firmware_url);
    installing = false;
    if (result == HTTP_UPDATE_FAILED) {
        message = String("Update failed: ") + httpUpdate.getLastErrorString();
        Serial.printf("OTA: %s\n", message.c_str());
    } else if (result == HTTP_UPDATE_NO_UPDATES) {
        message = "No update was installed";
    }
}
