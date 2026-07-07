#include "web_dashboard.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <cstring>
#include "config/hardware.h"
#include "config/version.h"

namespace {
Preferences wifiPrefs;
constexpr const char* kPrefsNamespace = "pourover";
constexpr const char* kWifiSsidKey = "wifi_ssid";
constexpr const char* kWifiPassKey = "wifi_pass";
constexpr uint32_t kWifiRestartDelayMs = 900;
}

WebDashboard::WebDashboard() : server(80) {}

void WebDashboard::begin(ActionCallback tare_cb, ActionCallback start_cb, ActionCallback reset_cb, TargetCallback target_cb, ActionCallback cal_tare_cb, CalibrateCallback calibrate_cb, RecipeCallback recipe_cb) {
    on_tare = tare_cb;
    on_start = start_cb;
    on_reset = reset_cb;
    on_target = target_cb;
    on_cal_tare = cal_tare_cb;
    on_calibrate = calibrate_cb;
    on_recipe = recipe_cb;

    ap_mode = false;
    wifiPrefs.begin(kPrefsNamespace, false);
    String sta_ssid = wifiPrefs.getString(kWifiSsidKey, HW_WIFI_STA_SSID);
    String sta_pass = wifiPrefs.getString(kWifiPassKey, HW_WIFI_STA_PASSWORD);

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HW_WIFI_HOSTNAME);
    WiFi.setSleep(false);

    if (sta_ssid.length() > 0) {
        WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
        Serial.printf("Connecting to Wi-Fi SSID: %s", sta_ssid.c_str());
        const uint32_t start_ms = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start_ms) < HW_WIFI_CONNECT_TIMEOUT_MS) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();
    } else {
        Serial.println("No saved Wi-Fi SSID. Starting setup AP.");
    }

    if (WiFi.status() == WL_CONNECTED) {
        dashboard_ip = WiFi.localIP();
        Serial.printf("Connected to Wi-Fi. IP: %s\n", dashboard_ip.toString().c_str());
        if (MDNS.begin(HW_WIFI_HOSTNAME)) {
            MDNS.addService("http", "tcp", 80);
            Serial.printf("mDNS: http://%s.local/\n", HW_WIFI_HOSTNAME);
        } else {
            Serial.println("mDNS failed to start");
        }
    } else {
        ap_mode = true;
        Serial.println("Wi-Fi connect failed. Starting fallback AP.");
        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_AP_STA);
        const char* ap_pass = HW_WIFI_AP_PASSWORD;
        if (ap_pass && std::strlen(ap_pass) >= 8) {
            WiFi.softAP(HW_WIFI_AP_SSID, ap_pass);
        } else {
            WiFi.softAP(HW_WIFI_AP_SSID);
        }
        dashboard_ip = WiFi.softAPIP();
    }

    // Read-only routes stay GET. Everything that changes state is POST-only so a
    // page merely linking/loading an image (e.g. <img src=".../api/reset">) from
    // another tab on the same network can't silently trigger an action.
    server.on("/", HTTP_GET, [this]() { handle_root(); });
    server.on("/settings", HTTP_GET, [this]() { handle_settings(); });
    server.on("/recipes", HTTP_GET, [this]() { handle_recipes(); });
    server.on("/wifi", HTTP_GET, [this]() { handle_wifi(); });
    server.on("/api/status", HTTP_GET, [this]() { handle_status(); });
    server.on("/api/tare", HTTP_POST, [this]() { handle_tare(); });
    server.on("/api/start", HTTP_POST, [this]() { handle_start(); });
    server.on("/api/reset", HTTP_POST, [this]() { handle_reset(); });
    server.on("/api/target", HTTP_POST, [this]() { handle_target(); });
    server.on("/api/cal_tare", HTTP_POST, [this]() { handle_cal_tare(); });
    server.on("/api/calibrate", HTTP_POST, [this]() { handle_calibrate(); });
    server.on("/api/recipe", HTTP_POST, [this]() { handle_recipe(); });
    server.on("/api/wifi", HTTP_POST, [this]() { handle_wifi_save(); });
    server.on("/api/wifi_scan", HTTP_GET, [this]() { handle_wifi_scan(); });
    server.on("/api/wifi_clear", HTTP_POST, [this]() { handle_wifi_clear(); });
    server.onNotFound([this]() { server.send(404, "text/plain", "Not found"); });
    server.begin();

    if (ap_mode) {
        Serial.printf("Web dashboard fallback AP: %s\n", HW_WIFI_AP_SSID);
        Serial.printf("Open: http://%s/\n", dashboard_ip.toString().c_str());
    } else {
        Serial.printf("Open: http://%s/ or http://%s.local/\n", dashboard_ip.toString().c_str(), HW_WIFI_HOSTNAME);
    }
}

void WebDashboard::update() {
    server.handleClient();
    if (wifi_restart_pending && millis() >= wifi_restart_at_ms) {
        delay(100);
        ESP.restart();
    }
}

void WebDashboard::set_state(const WebDashboardState& new_state) {
    state = new_state;
}

void WebDashboard::handle_root() {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", build_html());
}

void WebDashboard::handle_settings() {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", build_settings_html());
}

void WebDashboard::handle_recipes() {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", build_recipes_html());
}

void WebDashboard::handle_wifi() {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", build_wifi_html());
}

void WebDashboard::handle_status() {
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Connection", "close");
    server.send(200, "application/json", build_json());
}

void WebDashboard::handle_tare() {
    if (on_tare) on_tare();
    send_action_ok("tare");
}

void WebDashboard::handle_start() {
    if (on_start) on_start();
    send_action_ok("start");
}

void WebDashboard::handle_reset() {
    if (on_reset) on_reset();
    send_action_ok("reset");
}

void WebDashboard::handle_target() {
    float grams = state.target_g;
    if (server.hasArg("g")) {
        grams = server.arg("g").toFloat();
    } else if (server.hasArg("plain")) {
        grams = server.arg("plain").toFloat();
    }

    if (grams < 10.0f) grams = 10.0f;
    if (grams > 5000.0f) grams = 5000.0f;
    if (on_target) on_target(grams);

    String json = "{\"ok\":true,\"action\":\"target\",\"target_g\":";
    json += String(grams, 1);
    json += "}";
    server.sendHeader("Connection", "close");
    server.send(200, "application/json", json);
}

void WebDashboard::handle_cal_tare() {
    if (on_cal_tare) on_cal_tare();
    send_action_ok("cal_tare");
}

void WebDashboard::handle_calibrate() {
    float grams = 100.0f;
    if (server.hasArg("g")) {
        grams = server.arg("g").toFloat();
    } else if (server.hasArg("plain")) {
        grams = server.arg("plain").toFloat();
    }

    if (grams < 1.0f) grams = 1.0f;
    if (grams > 5000.0f) grams = 5000.0f;

    bool ok = false;
    if (on_calibrate) ok = on_calibrate(grams);

    String json = "{\"ok\":";
    json += ok ? "true" : "false";
    json += ",\"action\":\"calibrate\",\"known_g\":";
    json += String(grams, 1);
    json += ",\"calibration_factor\":";
    json += String(state.calibration_factor, 3);
    json += "}";
    server.sendHeader("Connection", "close");
    server.send(ok ? 200 : 500, "application/json", json);
}


void WebDashboard::handle_recipe() {
    BrewRecipe r = state.recipe;
    if (server.hasArg("name")) {
        strncpy(r.name, server.arg("name").c_str(), sizeof(r.name) - 1);
        r.name[sizeof(r.name) - 1] = '\0';
    }
    uint8_t count = server.hasArg("count") ? (uint8_t)server.arg("count").toInt() : r.stage_count;
    if (count < 1) count = 1;
    if (count > BREW_MAX_STAGES) count = BREW_MAX_STAGES;
    r.stage_count = count;

    for (uint8_t i = 0; i < BREW_MAX_STAGES; i++) {
        String suffix = String(i + 1);
        String name_key = "s" + suffix + "name";
        String target_key = "s" + suffix + "target";
        String hold_key = "s" + suffix + "hold";
        r.stages[i].enabled = i < count;
        if (server.hasArg(name_key)) {
            strncpy(r.stages[i].name, server.arg(name_key).c_str(), sizeof(r.stages[i].name) - 1);
            r.stages[i].name[sizeof(r.stages[i].name) - 1] = '\0';
        }
        if (server.hasArg(target_key)) r.stages[i].target_g = server.arg(target_key).toFloat();
        if (server.hasArg(hold_key)) r.stages[i].hold_seconds = (uint32_t)server.arg(hold_key).toInt();
    }

    bool ok = false;
    if (on_recipe) ok = on_recipe(r);
    server.sendHeader("Connection", "close");
    server.send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true,\"action\":\"recipe\"}" : "{\"ok\":false,\"action\":\"recipe\"}");
}


void WebDashboard::handle_wifi_save() {
    String ssid = server.hasArg("ssid") ? server.arg("ssid") : "";
    String pass = server.hasArg("pass") ? server.arg("pass") : "";
    ssid.trim();

    if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64) {
        server.sendHeader("Connection", "close");
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_wifi_credentials\"}");
        return;
    }

    wifiPrefs.putString(kWifiSsidKey, ssid);
    wifiPrefs.putString(kWifiPassKey, pass);
    wifi_restart_pending = true;
    wifi_restart_at_ms = millis() + kWifiRestartDelayMs;

    server.sendHeader("Connection", "close");
    server.send(200, "application/json", "{\"ok\":true,\"action\":\"wifi_saved\",\"restarting\":true}");
}


static String wifi_json_escape(const String& value) {
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i) {
        char c = value[i];
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else if ((uint8_t)c < 0x20) {
            out += ' ';
        } else {
            out += c;
        }
    }
    return out;
}

void WebDashboard::handle_wifi_scan() {
    server.sendHeader("Cache-Control", "no-store");
    const int count = WiFi.scanNetworks(false, true);
    String json;
    json.reserve(1200);
    json += "{\"ok\":";
    json += count >= 0 ? "true" : "false";
    json += ",\"networks\":[";
    if (count > 0) {
        for (int i = 0; i < count; ++i) {
            if (i) json += ",";
            json += "{\"ssid\":\"";
            json += wifi_json_escape(WiFi.SSID(i));
            json += "\",\"rssi\":";
            json += String(WiFi.RSSI(i));
            json += ",\"secure\":";
            json += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true";
            json += "}";
        }
    }
    json += "]}";
    WiFi.scanDelete();
    server.send(count >= 0 ? 200 : 500, "application/json", json);
}

void WebDashboard::handle_wifi_clear() {
    wifiPrefs.remove(kWifiSsidKey);
    wifiPrefs.remove(kWifiPassKey);
    wifi_restart_pending = true;
    wifi_restart_at_ms = millis() + kWifiRestartDelayMs;

    server.sendHeader("Connection", "close");
    server.send(200, "application/json", "{\"ok\":true,\"action\":\"wifi_cleared\",\"restarting\":true}");
}

void WebDashboard::send_action_ok(const char* action) {
    String json = "{\"ok\":true,\"action\":\"";
    json += action;
    json += "\"}";
    server.sendHeader("Connection", "close");
    server.send(200, "application/json", json);
}

String WebDashboard::build_json() const {
    String json;
    json.reserve(900);
    json += "{";
    json += "\"weight_g\":" + String(state.weight_g, 1) + ",";
    json += "\"flow_gps\":" + String(state.flow_gps, 2) + ",";
    json += "\"target_g\":" + String(state.target_g, 1) + ",";
    json += "\"elapsed_ms\":" + String(state.elapsed_ms) + ",";
    json += "\"running\":" + String(state.running ? "true" : "false") + ",";
    json += "\"battery_percent\":" + String(state.battery_percent) + ",";
    json += "\"battery_voltage_v\":" + String(state.battery_voltage_v, 3) + ",";
    json += "\"battery_raw_adc\":" + String(state.battery_raw_adc) + ",";
    json += "\"battery_pin_voltage_v\":" + String(state.battery_pin_voltage_v, 3) + ",";
    json += "\"battery_valid\":" + String(state.battery_valid ? "true" : "false") + ",";
    json += "\"battery_percent_valid\":" + String(state.battery_percent_valid ? "true" : "false") + ",";
    json += "\"charging\":" + String(state.charging ? "true" : "false") + ",";
    json += "\"prebrew_pending\":" + String(state.prebrew_pending ? "true" : "false") + ",";
    json += "\"pour_ready\":" + String(state.pour_ready ? "true" : "false") + ",";
    json += "\"calibration_factor\":" + String(state.calibration_factor, 3) + ",";
    json += "\"screen\":\"" + String(state.screen_name ? state.screen_name : "") + "\",";
    json += "\"ip\":\"" + dashboard_ip.toString() + "\",";
    json += "\"wifi_mode\":\"" + String(ap_mode ? "AP" : "STA") + "\",";
    json += "\"recipe_name\":\"" + String(state.recipe.name) + "\",";
    json += "\"stage_name\":\"" + String(state.stage.stage_name ? state.stage.stage_name : "") + "\",";
    json += "\"stage_index\":" + String(state.stage.active_index) + ",";
    json += "\"stage_count\":" + String(state.stage.stage_count) + ",";
    json += "\"stage_start_g\":" + String(state.stage.stage_start_g, 1) + ",";
    json += "\"stage_target_g\":" + String(state.stage.stage_target_g, 1) + ",";
    json += "\"stage_hold_remaining_s\":" + String(state.stage.hold_remaining_seconds) + ",";
    json += "\"stage_holding\":" + String(state.stage.holding ? "true" : "false") + ",";
    json += "\"stages\":[";
    for (uint8_t i = 0; i < state.recipe.stage_count && i < BREW_MAX_STAGES; i++) {
        if (i) json += ",";
        json += "{\"name\":\"" + String(state.recipe.stages[i].name) + "\",\"target_g\":" + String(state.recipe.stages[i].target_g, 1) + ",\"hold_s\":" + String(state.recipe.stages[i].hold_seconds) + "}";
    }
    json += "]";
    json += "}";
    return json;
}

String WebDashboard::build_html() const {
    return R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
  <title>)HTML" FIRMWARE_NAME " " FIRMWARE_VERSION R"HTML(</title>
  <style>
    :root {
      color-scheme: dark;
      --bg:#020617;
      --card:#0F172A;
      --panel:#080B12;
      --border:#1E293B;
      --muted:#94A3B8;
      --dim:#64748B;
      --text:#FFFFFF;
      --amber:#FBBF24;
      --blue:#38BDF8;
      --green:#22C55E;
      --red:#EF4444;
      --track:#1F2937;
      font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,sans-serif;
    }
    * { box-sizing:border-box; -webkit-tap-highlight-color:transparent; }
    html {
      height:100%;
      overflow:hidden;
    }
    body {
      margin:0;
      width:100%;
      height:100vh;
      height:100dvh;
      background:
        radial-gradient(circle at 50% 0%, rgba(56,189,248,.12), transparent 30%),
        radial-gradient(circle at 50% 100%, rgba(251,191,36,.10), transparent 34%),
        var(--bg);
      color:var(--text);
      display:grid;
      place-items:center;
      overflow:hidden;
      overscroll-behavior:none;
    }
    .app {
      width:min(420px, 96vw);
      max-height:100dvh;
      padding:0 8px;
      display:flex;
      align-items:center;
      justify-content:center;
    }
    .card {
      position:relative;
      min-height:auto;
      padding:14px 14px 14px;
      border:1px solid var(--border);
      border-radius:28px;
      background:linear-gradient(180deg, rgba(15,23,42,.98), rgba(8,11,18,.98));
      box-shadow:0 20px 70px rgba(0,0,0,.52), inset 0 1px 0 rgba(255,255,255,.04);
      overflow:hidden;
    }
    .status {
      display:grid;
      grid-template-columns:1fr auto 1fr;
      align-items:center;
      gap:10px;
      color:var(--muted);
      font-size:14px;
      font-weight:700;
      letter-spacing:.01em;
      margin:0 2px 8px;
    }
    .wifi { display:flex; align-items:center; gap:8px; min-width:0; }
    .dot { width:7px; height:7px; border-radius:50%; background:var(--blue); box-shadow:0 0 14px rgba(56,189,248,.85); flex:0 0 auto; }
    .live { color:var(--dim); text-align:center; }
    .pwr { display:flex; justify-content:flex-end; align-items:center; gap:8px; white-space:nowrap; }
    .battery { width:28px; height:14px; border:1px solid currentColor; border-radius:3px; padding:2px; position:relative; color:var(--muted); }
    .battery:after { content:""; position:absolute; right:-5px; top:4px; width:3px; height:6px; border-radius:1px; background:currentColor; }
    .battery-fill { display:block; height:100%; width:0%; border-radius:1px; background:currentColor; transition:width .18s linear; }
    .settings-link {
      display:flex;
      align-items:center;
      justify-content:center;
      width:100%;
      min-height:36px;
      margin-top:8px;
      color:var(--dim);
      text-decoration:none;
      font-size:12px;
      font-weight:800;
      letter-spacing:.09em;
      text-transform:uppercase;
      border:1px solid rgba(148,163,184,.25);
      padding:8px 11px;
      border-radius:999px;
      background:rgba(2,6,23,.45);
    }
    .dial {
      position:relative;
      width:min(304px, 74vw);
      aspect-ratio:1;
      margin:6px auto 4px;
      border-radius:50%;
      display:grid;
      place-items:center;
      --ring: var(--amber);
      --ringGlow: rgba(251,191,36,.10);
      background:
        conic-gradient(from 210deg, var(--ring) 0deg, var(--ring) var(--arc,0deg), var(--track) var(--arc,0deg), var(--track) 300deg, transparent 300deg 360deg);
      filter:drop-shadow(0 0 24px var(--ringGlow));
    }
    .dial.complete {
      --ring: var(--green);
      --ringGlow: rgba(34,197,94,.28);
    }
    .dial.holding {
      animation: holdPulse 1.7s ease-in-out infinite;
    }
    @keyframes holdPulse {
      0%, 100% { --ring:#FDBA4B; --ringGlow:rgba(253,186,75,.44); }
      50% { --ring:#4ADE80; --ringGlow:rgba(34,197,94,.22); }
    }
    .dial:before {
      content:"";
      position:absolute;
      inset:8px;
      border-radius:50%;
      background:linear-gradient(180deg, #101827, #080B12 76%);
      box-shadow:inset 0 0 0 1px rgba(255,255,255,.03);
    }
    .readout { position:relative; z-index:1; text-align:center; width:90%; transform:translateY(5px); }
    .target-line { color:var(--muted); font-size:22px; font-weight:750; font-variant-numeric:tabular-nums; margin-bottom:8px; }
    .weight { font-size:66px; line-height:.9; font-weight:850; letter-spacing:-4px; font-variant-numeric:tabular-nums; }
    .grams { color:var(--muted); font-size:15px; font-weight:700; margin-top:8px; }
    .ready-pill { opacity:0; transform:translateY(4px); color:var(--green); font-size:16px; font-weight:900; letter-spacing:.16em; margin-top:8px; transition:opacity .15s ease, transform .15s ease; }
    .ready-pill.show { opacity:1; transform:translateY(0); }
    .timer-box {
      width:190px;
      height:46px;
      display:flex;
      align-items:center;
      justify-content:center;
      margin:8px auto 8px;
      background:var(--panel);
      border:2px solid var(--amber);
      border-radius:999px;
      box-shadow:0 0 24px rgba(251,191,36,.08);
    }
    .timer { color:var(--amber); font-size:29px; font-weight:800; letter-spacing:1px; font-variant-numeric:tabular-nums; }
    .flow-row { display:flex; align-items:center; justify-content:center; gap:10px; margin:4px 0 6px; }
    .flow-dot { width:11px; height:11px; border-radius:50%; background:var(--amber); box-shadow:0 0 15px rgba(251,191,36,.72); }
    .flow { font-size:25px; font-weight:750; font-variant-numeric:tabular-nums; }
    .progress { text-align:center; color:var(--dim); font-size:15px; font-weight:750; margin:6px 0 10px; }
    .controls { display:grid; grid-template-columns:1fr 1fr 1fr; gap:8px; margin-top:6px; }
    button {
      appearance:none;
      border:0;
      border-radius:16px;
      min-height:46px;
      color:#fff;
      font-size:15px;
      font-weight:850;
      letter-spacing:.02em;
      background:#334155;
      box-shadow:0 8px 24px rgba(0,0,0,.22), inset 0 1px 0 rgba(255,255,255,.06);
      touch-action:manipulation;
    }
    button:active { transform:translateY(1px); filter:brightness(1.12); }
    button.primary { background:#16A34A; }
    button.warn { background:#F59E0B; color:#111827; }
    .target { margin-top:8px; display:grid; grid-template-columns:1fr auto; gap:10px; align-items:center; }
    .target input {
      width:100%;
      min-height:44px;
      border:1px solid #334155;
      background:#020617;
      color:#fff;
      border-radius:15px;
      padding:8px 12px;
      font-size:18px;
      font-weight:850;
      text-align:center;
      font-variant-numeric:tabular-nums;
      outline:none;
    }
    .target button { min-width:96px; background:#475569; }
    .meta { color:var(--dim); text-align:center; margin-top:7px; font-size:11px; font-weight:650; }
    @media (max-width:380px) {
      .card { padding-left:12px; padding-right:12px; }
      .weight { font-size:58px; }
      .target-line { font-size:19px; }
      .timer { font-size:25px; }
      .flow { font-size:23px; }
      .controls { gap:8px; }
      button { font-size:14px; }
    }

    @media (max-height:720px) {
      .app { padding:0 6px; }
      .card { padding-top:10px; padding-bottom:10px; }
      .status { margin-bottom:6px; font-size:13px; }
      .dial { width:min(282px, 70vw); margin-top:4px; }
      .weight { font-size:60px; }
      .target-line { font-size:20px; margin-bottom:6px; }
      .grams { margin-top:6px; }
      .timer-box { height:40px; width:172px; margin:6px auto; }
      .timer { font-size:26px; }
      .flow { font-size:23px; }
      .progress { margin:4px 0 8px; }
      button { min-height:42px; }
      .target input { min-height:42px; }
      .settings-link { min-height:32px; margin-top:6px; }
      .meta { display:none; }
    }

    @media (max-height:640px) {
      .card { padding:9px 12px; border-radius:24px; }
      .status { margin-bottom:4px; font-size:12px; }
      .settings-link { min-height:30px; padding:6px 10px; margin-top:5px; }
      .dial { width:min(260px, 68vw); margin:4px auto 2px; }
      .dial:before { inset:7px; }
      .readout { transform:translateY(3px); }
      .weight { font-size:54px; }
      .target-line { font-size:18px; margin-bottom:5px; }
      .grams { font-size:13px; margin-top:5px; }
      .timer-box { height:38px; width:164px; margin:5px auto; }
      .timer { font-size:24px; }
      .flow-row { margin:3px 0 4px; }
      .flow { font-size:21px; }
      .progress { font-size:13px; margin:3px 0 6px; }
      .controls { gap:7px; margin-top:4px; }
      button { min-height:38px; font-size:13px; border-radius:14px; }
      .target { margin-top:6px; gap:8px; }
      .target input { min-height:38px; font-size:16px; }
      .target button { min-width:86px; }
    }

  </style>
</head>
<body>
  <main class="app">
    <section class="card">
      <div class="status">
        <div class="wifi"><span id="wifiLabel">WiFi</span><span class="dot" id="wifiDot"></span></div>
        <div class="live" id="liveLabel">LIVE</div>
        <div class="pwr"><span class="battery" id="battery"><span class="battery-fill" id="batteryFill"></span></span><span id="powerText">PWR --</span></div>
      </div>

      <div class="dial" id="dial">
        <div class="readout">
          <div class="target-line" id="targetLine">0 / 320</div>
          <div class="weight" id="weight">0.0</div>
          <div class="grams">grams</div>
          <div class="ready-pill" id="readyPill">READY</div>
        </div>
      </div>

      <div class="timer-box"><div class="timer" id="timer">00:00.0</div></div>

      <div class="flow-row"><span class="flow-dot"></span><div class="flow" id="flow">0.0 g/s</div></div>
      <div class="progress" id="progress">0% to target</div>

      <div class="controls">
        <button class="primary" id="startBtn" onclick="cmd('start')">START</button>
        <button onclick="cmd('tare')">TARE</button>
        <button onclick="cmd('reset')">RESET</button>
      </div>

      <div class="target">
        <input id="targetInput" type="number" min="10" max="5000" step="1" value="320" aria-label="Target water grams">
        <button onclick="setTarget()">SET</button>
      </div>

      <div style="display:grid;grid-template-columns:1fr 1fr;gap:8px;"><a class="settings-link" href="/settings" onclick="stopDashboardPolling()">Settings</a><a class="settings-link" href="/recipes" onclick="stopDashboardPolling()">Recipes</a></div>
    </section>
    <div class="meta" id="meta">Connecting...</div>
  </main>
<script>
function fmtTime(ms) {
  const m = Math.floor(ms / 60000);
  const s = Math.floor(ms / 1000) % 60;
  const t = Math.floor(ms / 100) % 10;
  return String(m).padStart(2,'0') + ':' + String(s).padStart(2,'0') + '.' + t;
}
function setBattery(d) {
  const battery = el.battery;
  const fill = el.batteryFill;
  const text = el.powerText;
  const pct = Math.max(0, Math.min(100, Number(d.battery_percent || 0)));
  const valid = !!d.battery_valid;
  const pctValid = !!d.battery_percent_valid;
  let color = 'var(--dim)';
  let label = 'PWR --';
  let width = '0%';
  if (valid) {
    if (d.charging && !pctValid) {
      label = 'USB';
      color = 'var(--green)';
      width = '55%';
    } else if (d.charging) {
      label = 'USB ' + pct + '%';
      color = 'var(--green)';
      width = pct + '%';
    } else {
      label = 'BAT ' + pct + '%';
      color = pct <= 10 ? 'var(--red)' : (pct <= 20 ? 'var(--amber)' : 'var(--muted)');
      width = pct + '%';
    }
  }
  battery.style.color = color;
  text.style.color = color;
  text.textContent = label;
  fill.style.width = width;
}

const el = {
  battery: document.getElementById('battery'),
  batteryFill: document.getElementById('batteryFill'),
  powerText: document.getElementById('powerText'),
  timer: document.getElementById('timer'),
  weight: document.getElementById('weight'),
  targetLine: document.getElementById('targetLine'),
  flow: document.getElementById('flow'),
  progress: document.getElementById('progress'),
  readyPill: document.getElementById('readyPill'),
  dial: document.getElementById('dial'),
  targetInput: document.getElementById('targetInput'),
  startBtn: document.getElementById('startBtn'),
  liveLabel: document.getElementById('liveLabel'),
  wifiLabel: document.getElementById('wifiLabel'),
  meta: document.getElementById('meta')
};

let latestElapsedMs = 0;
let latestRunning = false;
let timerAnchorPerf = performance.now();
let lastTimerText = '';
let statusInFlight = false;
let statusAbort = null;
let statusTimer = null;
let timerRenderTimer = null;

function stopDashboardPolling() {
  if (statusTimer) clearInterval(statusTimer);
  if (timerRenderTimer) clearTimeout(timerRenderTimer);
  if (statusAbort) { try { statusAbort.abort(); } catch(e) {} }
  statusInFlight = false;
}

function syncTimer(elapsedMs, running) {
  latestElapsedMs = Math.max(0, Number(elapsedMs || 0));
  latestRunning = !!running;
  timerAnchorPerf = performance.now();
  renderTimerOnce();
}

function renderTimerOnce() {
  let shownMs = latestElapsedMs;
  if (latestRunning) shownMs += performance.now() - timerAnchorPerf;
  const text = fmtTime(shownMs);
  if (text !== lastTimerText) {
    el.timer.textContent = text;
    lastTimerText = text;
  }
}

function scheduleTimerRender() {
  renderTimerOnce();
  timerRenderTimer = setTimeout(scheduleTimerRender, latestRunning ? 100 : 250);
}

async function update() {
  if (statusInFlight || document.hidden) return;
  statusInFlight = true;
  try {
    statusAbort = new AbortController();
    const r = await fetch('/api/status', {cache:'no-store', signal: statusAbort.signal});
    const d = await r.json();
    const w = Number(d.weight_g || 0);
    const target = Math.max(1, Number(d.target_g || 320));
    const prebrewPending = !!d.prebrew_pending;
    const pourReady = !!d.pour_ready;
    const safeWeight = Math.max(0, w);
    const recipeMode = Number(d.stage_count || 0) > 1;
    const stageStart = recipeMode ? Number(d.stage_start_g || 0) : 0;
    const stageTarget = recipeMode ? Math.max(stageStart + 1, Number(d.stage_target_g || target)) : target;

    // Match the AMOLED behavior: in recipe mode the ring represents the active
    // stage, not the whole final target. This makes the web ring turn green at
    // the same stage target as the scale screen.
    const activeStart = recipeMode ? stageStart : 0;
    const activeTarget = recipeMode ? stageTarget : target;
    const complete = !prebrewPending && activeTarget > 0 && safeWeight >= (activeTarget - 0.5);
    const pct = prebrewPending ? 0 : Math.max(0, Math.min(100, ((safeWeight - activeStart) / Math.max(1, activeTarget - activeStart)) * 100));
    const arc = Math.round((pct / 100) * 300);
    el.weight.textContent = w.toFixed(1);
    el.targetLine.textContent = recipeMode
      ? (d.stage_name || 'Stage') + '  ' + safeWeight.toFixed(0) + ' / ' + activeTarget.toFixed(0)
      : safeWeight.toFixed(0) + ' / ' + target.toFixed(0);
    syncTimer(Number(d.elapsed_ms || 0), !!d.running);
    el.flow.textContent = Number(d.flow_gps || 0).toFixed(1) + ' g/s';
    if (prebrewPending) el.progress.textContent = 'settling / auto tare';
    else if (pourReady) el.progress.textContent = 'ready to pour';
    else if (recipeMode) {
      if (!!d.stage_holding) el.progress.textContent = 'HOLD · ' + Number(d.stage_hold_remaining_s || 0) + 's';
      else el.progress.textContent = 'stage ' + (Number(d.stage_index || 0) + 1) + '/' + Number(d.stage_count || 0) + ' · ' + Math.round(pct) + '%';
    } else el.progress.textContent = Math.round(pct) + '% to target';
    el.readyPill.classList.toggle('show', pourReady && !d.running);
    el.dial.style.setProperty('--arc', arc + 'deg');
    const holding = !!d.stage_holding;
    el.dial.classList.toggle('complete', complete);
    el.dial.classList.toggle('holding', holding);
    if (!holding) {
      el.dial.style.setProperty('--ring', complete ? '#4ADE80' : '#FDBA4B');
      el.dial.style.setProperty('--ringGlow', complete ? 'rgba(34,197,94,.28)' : 'rgba(251,191,36,.10)');
    }
    if (document.activeElement !== el.targetInput) el.targetInput.value = target.toFixed(0);
    el.startBtn.textContent = d.running ? 'PAUSE' : 'START';
    el.startBtn.className = d.running ? 'warn' : 'primary';
    el.liveLabel.textContent = d.running ? 'BREW' : 'LIVE';
    el.wifiLabel.textContent = d.wifi_mode === 'AP' ? 'AP' : 'WiFi';
    setBattery(d);
    el.meta.textContent = (d.running ? 'BREWING' : (pourReady ? 'READY TO POUR' : 'IDLE')) + ' · IP ' + (d.ip || '--');
  } catch(e) {
    el.liveLabel.textContent = 'OFFLINE';
    el.meta.textContent = 'Connection lost';
  } finally {
    statusInFlight = false;
    statusAbort = null;
  }
}
async function cmd(name) {
  try { await fetch('/api/' + name, {method:'POST'}); } catch(e) {}
  update();
}
async function setTarget() {
  const v = Number(el.targetInput.value || 320);
  try { await fetch('/api/target?g=' + encodeURIComponent(v), {method:'POST'}); } catch(e) {}
  update();
}
document.addEventListener('visibilitychange', () => { if (!document.hidden) update(); });
scheduleTimerRender();
statusTimer = setInterval(update, 650);
update();
</script>
</body>
</html>
)HTML";
}

String WebDashboard::build_settings_html() const {
    return R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
  <title>Pourover Scale Settings</title>
  <style>
    :root { color-scheme: dark; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif; }
    * { box-sizing:border-box; -webkit-tap-highlight-color:transparent; }
    html { background:#020617; }
    body { margin:0; background:radial-gradient(circle at 50% -10%, rgba(56,189,248,.12), transparent 34%), #020617; color:#fff; min-height:100vh; min-height:100dvh; }
    .app { width:min(520px, 100%); margin:0 auto; padding:16px; }
    .card { background:linear-gradient(180deg, rgba(15,23,42,.98), rgba(8,13,24,.98)); border:1px solid #1e293b; border-radius:30px; padding:22px 18px; box-shadow:0 24px 70px rgba(0,0,0,.45), inset 0 1px 0 rgba(255,255,255,.04); }
    .top { display:grid; grid-template-columns:1fr; gap:12px; color:#94a3b8; font-size:14px; margin-bottom:18px; }
    .top div { display:grid; grid-template-columns:1fr 1fr 1fr; gap:8px; }
    .top a { color:#cbd5e1; text-align:center; text-decoration:none; border:1px solid #334155; padding:10px 8px; border-radius:999px; font-weight:850; background:rgba(2,6,23,.42); }
    .section { margin-top:18px; padding-top:18px; border-top:1px solid #1e293b; }
    .section:first-of-type { border-top:0; padding-top:0; }
    h1 { margin:0; font-size:24px; }
    h2 { margin:0 0 12px; font-size:14px; color:#94a3b8; letter-spacing:.08em; text-transform:uppercase; }
    label { display:block; color:#94a3b8; font-size:13px; font-weight:700; margin:0 0 7px; }
    .grid2 { display:grid; grid-template-columns:1fr 1fr; gap:12px; }
    .target { display:grid; grid-template-columns:1fr auto; gap:10px; align-items:end; }
    input, select { width:100%; border:1px solid #334155; background:#020617; color:#fff; border-radius:16px; padding:15px 14px; font-size:18px; font-weight:700; text-align:center; font-variant-numeric:tabular-nums; min-height:54px; }
    select { appearance:none; text-align-last:center; }
    button { border:0; border-radius:15px; padding:16px 14px; color:#fff; font-size:16px; font-weight:800; background:#334155; min-height:54px; }
    button.primary { background:#16a34a; }
    button.secondary { background:#475569; }
    .result { margin-top:12px; border:1px solid #1e293b; background:#020617; border-radius:18px; padding:14px; display:flex; justify-content:space-between; align-items:center; gap:12px; }
    .result span:first-child { color:#94a3b8; font-size:13px; font-weight:700; text-transform:uppercase; letter-spacing:.06em; }
    .result span:last-child { font-size:26px; font-weight:900; font-variant-numeric:tabular-nums; }
    .preset { display:grid; grid-template-columns:repeat(4,1fr); gap:8px; margin-top:10px; }
    .preset button { min-height:42px; padding:10px 6px; font-size:14px; background:#1e293b; }
    .hint { color:#64748b; font-size:13px; line-height:1.35; margin-top:8px; }
    .small { color:#64748b; text-align:center; margin-top:14px; font-size:12px; }

    @media (min-width:430px) { .top { grid-template-columns:1fr auto; align-items:center; } .top div { grid-template-columns:auto auto auto; } .top a { min-width:92px; } }
    @media (max-width:390px), (max-height:720px) {
      .app { padding:10px 8px; width:100%; }
      .card { border-radius:24px; padding:18px 14px; }
      .section { margin-top:15px; padding-top:15px; }
      input, select, button { min-height:48px; font-size:16px; padding:12px 10px; }
      .result span:last-child { font-size:23px; }
    }
  </style>
</head>
<body>
  <main class="app">
    <section class="card">
      <div class="top"><h1>Settings</h1><div><a href="/recipes">Recipes</a> <a href="/wifi">WiFi</a> <a href="/">Dashboard</a></div></div>

      <div class="section">
        <h2>Target Weight</h2>
        <div class="target">
          <div>
            <label for="targetInput">Manual target water</label>
            <input id="targetInput" type="number" min="10" max="5000" step="1" value="320" aria-label="Target water grams">
          </div>
          <button class="primary" onclick="applyManualTarget()">APPLY</button>
        </div>
        <div class="hint">Use this exactly like the current target entry. Brew ratio below is optional.</div>
      </div>

      <div class="section">
        <h2>Brew Ratio</h2>
        <div class="grid2">
          <div>
            <label for="doseInput">Coffee dose</label>
            <input id="doseInput" type="number" min="1" max="250" step="0.1" value="20" aria-label="Coffee dose grams" oninput="updateRatioTarget()">
          </div>
          <div>
            <label for="ratioSelect">Ratio</label>
            <select id="ratioSelect" onchange="ratioChanged()" aria-label="Brew ratio">
              <option value="15">15:1</option>
              <option value="16" selected>16:1</option>
              <option value="17">17:1</option>
              <option value="18">18:1</option>
              <option value="custom">Custom</option>
            </select>
          </div>
        </div>
        <div id="customRatioWrap" style="display:none;margin-top:12px;">
          <label for="customRatioInput">Custom ratio</label>
          <input id="customRatioInput" type="number" min="1" max="40" step="0.1" value="16" aria-label="Custom brew ratio" oninput="updateRatioTarget()">
        </div>
        <div class="preset">
          <button onclick="setRatio(15)">15:1</button>
          <button onclick="setRatio(16)">16:1</button>
          <button onclick="setRatio(17)">17:1</button>
          <button onclick="setRatio(18)">18:1</button>
        </div>
        <div class="result"><span>Calculated target</span><span id="ratioTargetText">320g</span></div>
        <button class="primary" style="width:100%;margin-top:12px;" onclick="applyRatioTarget()">USE CALCULATED TARGET</button>
        <div class="hint">Formula: coffee dose × brew ratio = target water weight.</div>
      </div>

      <div class="section">
        <h2>Calibration</h2>
        <div class="hint">Empty the scale, tap CAL TARE, place a known weight, enter its grams, then tap CALIBRATE.</div>
        <div class="target" style="margin-top:10px;">
          <div>
            <label for="knownInput">Known weight</label>
            <input id="knownInput" type="number" min="1" max="5000" step="1" value="100" aria-label="Known calibration weight grams">
          </div>
          <button class="primary" onclick="calibrateScale()">CALIBRATE</button>
        </div>
        <div class="grid2" style="margin-top:12px;">
          <button class="secondary" onclick="calTare()">CAL TARE</button>
          <button class="secondary" onclick="el.knownInput.value='100'">100g</button>
        </div>
      </div>
    </section>
    <div class="small" id="meta">Connecting...</div>
  </main>
<script>
const el = {
  targetInput: document.getElementById('targetInput'),
  doseInput: document.getElementById('doseInput'),
  ratioSelect: document.getElementById('ratioSelect'),
  customRatioWrap: document.getElementById('customRatioWrap'),
  customRatioInput: document.getElementById('customRatioInput'),
  ratioTargetText: document.getElementById('ratioTargetText'),
  knownInput: document.getElementById('knownInput'),
  meta: document.getElementById('meta')
};
let settingsStatusInFlight = false;
let lastTargetFromScale = 320;

function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }
function activeRatio() {
  if (el.ratioSelect.value === 'custom') return clamp(Number(el.customRatioInput.value || 16), 1, 40);
  return Number(el.ratioSelect.value || 16);
}
function calculatedTarget() {
  const dose = clamp(Number(el.doseInput.value || 0), 1, 250);
  return clamp(dose * activeRatio(), 10, 5000);
}
function updateRatioTarget() {
  const target = calculatedTarget();
  el.ratioTargetText.textContent = Math.round(target) + 'g';
}
function ratioChanged() {
  el.customRatioWrap.style.display = (el.ratioSelect.value === 'custom') ? 'block' : 'none';
  updateRatioTarget();
}
function setRatio(v) {
  el.ratioSelect.value = String(v);
  ratioChanged();
}
async function setTarget(v) {
  const grams = clamp(Number(v || 320), 10, 5000);
  try { await fetch('/api/target?g=' + encodeURIComponent(grams), {method:'POST'}); } catch(e) {}
  lastTargetFromScale = grams;
  el.targetInput.value = Math.round(grams);
  update();
}
function applyManualTarget() { setTarget(el.targetInput.value); }
function applyRatioTarget() { setTarget(Math.round(calculatedTarget())); }

async function update() {
  if (settingsStatusInFlight) return;
  settingsStatusInFlight = true;
  try {
    const r = await fetch('/api/status', {cache:'no-store'});
    const d = await r.json();
    const target = Number(d.target_g || lastTargetFromScale || 320);
    lastTargetFromScale = target;
    if (document.activeElement !== el.targetInput) el.targetInput.value = Math.round(target);
    el.meta.textContent = 'Target ' + Math.round(target) + 'g · Weight ' + Number(d.weight_g || 0).toFixed(1) + 'g · ' + (d.running ? 'BREWING' : 'READY');
  } catch(e) {
    el.meta.textContent = 'Connection lost';
  } finally {
    settingsStatusInFlight = false;
  }
}
async function calTare() {
  el.meta.textContent = 'Calibrating: taring...';
  try { await fetch('/api/cal_tare', {method:'POST'}); } catch(e) {}
  update();
}
async function calibrateScale() {
  const v = Number(el.knownInput.value || 100);
  el.meta.textContent = 'Calibrating with ' + v + 'g...';
  try {
    const r = await fetch('/api/calibrate?g=' + encodeURIComponent(v), {method:'POST'});
    if (!r.ok) el.meta.textContent = 'Calibration failed. Tare empty, then place known weight.';
  } catch(e) {
    el.meta.textContent = 'Calibration request failed';
  }
  update();
}
setInterval(update, 1500);
ratioChanged();
update();
</script>
</body>
</html>
)HTML";
}



String WebDashboard::build_wifi_html() const {
    String saved = wifiPrefs.getString(kWifiSsidKey, HW_WIFI_STA_SSID);
    String mode = ap_mode ? "Setup AP" : "Home WiFi";
    String ip = dashboard_ip.toString();
    String current = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String(HW_WIFI_AP_SSID);
    String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
  <title>Pourover Scale WiFi Setup</title>
  <style>
    :root {
      color-scheme: dark;
      --bg:#020617;
      --card:#0f172a;
      --card2:#111c31;
      --border:#1e293b;
      --border2:#334155;
      --text:#f8fafc;
      --muted:#94a3b8;
      --dim:#64748b;
      --green:#16a34a;
      --blue:#38bdf8;
      --red:#b91c1c;
      font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,sans-serif;
    }
    * { box-sizing:border-box; -webkit-tap-highlight-color:transparent; }
    html { min-height:100%; background:var(--bg); }
    body {
      margin:0;
      min-height:100vh;
      min-height:100dvh;
      background:
        radial-gradient(circle at 50% -10%, rgba(56,189,248,.14), transparent 34%),
        radial-gradient(circle at 100% 100%, rgba(34,197,94,.10), transparent 35%),
        var(--bg);
      color:var(--text);
    }
    .app { width:min(520px,100%); margin:0 auto; padding:16px; }
    .card {
      background:linear-gradient(180deg, rgba(15,23,42,.98), rgba(8,13,24,.98));
      border:1px solid var(--border);
      border-radius:30px;
      padding:20px;
      box-shadow:0 24px 70px rgba(0,0,0,.45), inset 0 1px 0 rgba(255,255,255,.04);
    }
    .top { display:grid; grid-template-columns:1fr; gap:14px; margin-bottom:18px; }
    .nav { display:grid; grid-template-columns:1fr 1fr; gap:8px; }
    .top a {
      color:#cbd5e1;
      text-decoration:none;
      text-align:center;
      border:1px solid var(--border2);
      padding:11px 10px;
      border-radius:999px;
      font-size:13px;
      font-weight:900;
      background:rgba(2,6,23,.42);
    }
    h1 { margin:0; font-size:28px; letter-spacing:-.03em; }
    h2 { margin:20px 0 12px; font-size:13px; color:var(--muted); letter-spacing:.10em; text-transform:uppercase; }
    label { display:block; color:var(--muted); font-size:13px; font-weight:800; margin:0 0 8px; }
    input {
      width:100%;
      border:1px solid var(--border2);
      background:#020617;
      color:#fff;
      border-radius:17px;
      padding:15px 14px;
      font-size:17px;
      font-weight:750;
      min-height:54px;
      margin-bottom:12px;
      outline:none;
    }
    input:focus { border-color:var(--blue); box-shadow:0 0 0 3px rgba(56,189,248,.15); }
    button {
      width:100%;
      border:0;
      border-radius:17px;
      padding:15px 14px;
      color:#fff;
      font-size:15px;
      font-weight:900;
      background:var(--green);
      min-height:54px;
      margin:0;
      box-shadow:0 10px 24px rgba(0,0,0,.23), inset 0 1px 0 rgba(255,255,255,.07);
      touch-action:manipulation;
    }
    button:active { transform:translateY(1px); filter:brightness(1.12); }
    .action-grid { display:grid; grid-template-columns:1fr; gap:10px; margin-top:2px; }
    .network-list { display:grid; gap:10px; margin:12px 0 16px; }
    .net {
      width:100%;
      text-align:left;
      background:rgba(2,6,23,.76);
      border:1px solid var(--border2);
      border-radius:18px;
      padding:13px 14px;
      color:#e2e8f0;
      font-weight:900;
      min-height:58px;
      margin:0;
      box-shadow:none;
    }
    .net small { display:block; color:var(--muted); font-weight:700; margin-top:4px; line-height:1.25; }
    button.secondary { background:#334155; }
    button.danger { background:var(--red); }
    .status {
      border:1px solid var(--border);
      background:rgba(2,6,23,.72);
      border-radius:20px;
      padding:15px;
      color:#cbd5e1;
      line-height:1.6;
      font-size:14px;
    }
    .status b { color:#fff; }
    .hint { color:var(--dim); font-size:13px; line-height:1.45; margin-top:12px; }
    .msg {
      color:#cbd5e1;
      text-align:center;
      margin-top:14px;
      min-height:20px;
      font-size:13px;
      font-weight:750;
    }
    @media (min-width:430px) {
      .top { grid-template-columns:1fr auto; align-items:center; }
      .nav { grid-template-columns:auto auto; }
      .top a { min-width:108px; }
      .action-grid { grid-template-columns:1fr 1fr; }
      .action-grid .full { grid-column:1 / -1; }
    }
    @media (max-width:390px), (max-height:720px) {
      .app{padding:10px 8px;}
      .card{border-radius:24px;padding:17px 13px;}
      h1{font-size:25px;}
      input,button{min-height:50px;font-size:16px;padding:12px 11px;}
      .top a{padding:10px 8px;}
      .status{padding:13px;}
    }
  </style>
</head>
<body>
  <main class="app">
    <section class="card">
      <div class="top"><h1>WiFi Setup</h1><div class="nav"><a href="/settings">Settings</a><a href="/">Dashboard</a></div></div>
      <div class="status">
        <div><b>Mode:</b> )HTML" + mode + R"HTML(</div>
        <div><b>Current network:</b> )HTML" + current + R"HTML(</div>
        <div><b>IP:</b> )HTML" + ip + R"HTML(</div>
      </div>
      <h2>Available Networks</h2>
      <div class="action-grid"><button class="secondary full" onclick="scanWifi()">SCAN NETWORKS</button></div>
      <div class="network-list" id="networkList"></div>
      <h2>Join Home WiFi</h2>
      <label for="ssid">Network name</label>
      <input id="ssid" maxlength="32" autocomplete="off" autocapitalize="none" value=")HTML" + saved + R"HTML(">
      <label for="pass">Password</label>
      <input id="pass" type="password" maxlength="64" autocomplete="current-password" placeholder="Leave blank for open network">
      <div class="action-grid">
        <button class="full" onclick="saveWifi()">SAVE & RESTART</button>
        <button class="secondary" onclick="togglePass()">SHOW / HIDE PASSWORD</button>
        <button class="danger" onclick="clearWifi()">FORGET SAVED WIFI</button>
      </div>
      <div class="hint">Scanning can take a few seconds. Manual entry still works for hidden networks.<br><br>If the saved network fails, the scale starts its setup hotspot: <b>)HTML" HW_WIFI_AP_SSID R"HTML(</b>. Connect to it, then open <b>192.168.4.1/wifi</b>.</div>
      <div class="msg" id="msg"></div>
    </section>
  </main>
<script>
function el(id){return document.getElementById(id)}
function togglePass(){ el('pass').type = el('pass').type === 'password' ? 'text' : 'password'; }
function signalLabel(rssi){
  if(rssi >= -55) return 'Excellent';
  if(rssi >= -67) return 'Good';
  if(rssi >= -75) return 'Fair';
  return 'Weak';
}
async function scanWifi(){
  const list = el('networkList');
  list.innerHTML = '';
  el('msg').textContent = 'Scanning nearby WiFi networks...';
  try {
    const r = await fetch('/api/wifi_scan', {cache:'no-store'});
    if(!r.ok) throw new Error('scan failed');
    const d = await r.json();
    const seen = new Set();
    const networks = (d.networks || []).filter(n => n.ssid && !seen.has(n.ssid) && seen.add(n.ssid));
    if(!networks.length){ el('msg').textContent = 'No networks found. You can still enter the name manually.'; return; }
    networks.forEach(n => {
      const b = document.createElement('button');
      b.className = 'net';
      b.type = 'button';
      b.innerHTML = `${n.ssid}<small>${signalLabel(n.rssi)} signal · ${n.secure ? 'Password required' : 'Open network'}</small>`;
      b.onclick = () => { el('ssid').value = n.ssid; el('pass').focus(); el('msg').textContent = 'Selected ' + n.ssid + '. Enter the password and save.'; };
      list.appendChild(b);
    });
    el('msg').textContent = 'Tap a network to select it.';
  } catch(e) {
    el('msg').textContent = 'Scan failed. You can still enter the network name manually.';
  }
}
async function saveWifi(){
  const ssid = el('ssid').value.trim();
  const pass = el('pass').value;
  if(!ssid){ el('msg').textContent='Enter a network name first.'; return; }
  el('msg').textContent='Saving WiFi and restarting...';
  const params = new URLSearchParams({ssid, pass});
  try { const r = await fetch('/api/wifi?' + params.toString(), {method:'POST'}); el('msg').textContent = r.ok ? 'Saved. The scale is restarting now.' : 'Save failed.'; }
  catch(e){ el('msg').textContent='Saved request sent. The scale may be restarting.'; }
}
async function clearWifi(){
  if(!confirm('Forget saved WiFi and restart into setup AP?')) return;
  el('msg').textContent='Clearing saved WiFi and restarting...';
  try { await fetch('/api/wifi_clear', {method:'POST'}); } catch(e) {}
}
</script>
</body>
</html>
)HTML";
    return html;
}

String WebDashboard::build_recipes_html() const {
    return R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
  <title>Pourover Recipes</title>
  <style>
    :root { color-scheme: dark; font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif; }
    * { box-sizing:border-box; -webkit-tap-highlight-color:transparent; }
    html { background:#020617; }
    body { margin:0; background:radial-gradient(circle at 50% -10%, rgba(56,189,248,.12), transparent 34%), #020617; color:#fff; min-height:100vh; min-height:100dvh; }
    .app { width:min(540px,100%); margin:0 auto; padding:16px; }
    .card { background:linear-gradient(180deg, rgba(15,23,42,.98), rgba(8,13,24,.98)); border:1px solid #1e293b; border-radius:30px; padding:22px 18px; box-shadow:0 24px 70px rgba(0,0,0,.45), inset 0 1px 0 rgba(255,255,255,.04); }
    .top { display:grid; grid-template-columns:1fr; gap:12px; color:#94a3b8; font-size:14px; margin-bottom:16px; }
    .top div { display:grid; grid-template-columns:1fr 1fr 1fr; gap:8px; }
    .top a { color:#cbd5e1; text-align:center; text-decoration:none; border:1px solid #334155; padding:10px 8px; border-radius:999px; font-weight:850; background:rgba(2,6,23,.42); }
    h1 { margin:0; font-size:24px; }
    label { display:block; color:#94a3b8; font-size:12px; font-weight:800; margin:0 0 6px; letter-spacing:.05em; text-transform:uppercase; }
    input, select { width:100%; border:1px solid #334155; background:#020617; color:#fff; border-radius:14px; padding:12px 10px; font-size:16px; font-weight:750; text-align:center; min-height:46px; }
    .grid2 { display:grid; grid-template-columns:1fr 110px; gap:10px; align-items:end; margin-bottom:12px; }
    .stage { display:grid; grid-template-columns:1.2fr .8fr .8fr; gap:8px; margin-top:10px; padding-top:10px; border-top:1px solid #1e293b; }
    .stage:first-of-type { border-top:0; padding-top:0; }
    button { width:100%; border:0; border-radius:15px; padding:15px 12px; color:#fff; font-size:16px; font-weight:900; background:#16a34a; min-height:52px; margin-top:14px; }
    .secondary { background:#334155; }
    .preset-row { display:grid; grid-template-columns:1fr 1fr; gap:8px; margin:10px 0 14px; }
    .preset-row button { margin-top:0; padding:12px 8px; min-height:46px; font-size:13px; background:#1e293b; }
    .hint { color:#64748b; font-size:13px; line-height:1.35; margin-top:10px; }
    .meta { color:#64748b; text-align:center; margin-top:12px; font-size:12px; }
    @media (min-width:430px) { .top { grid-template-columns:1fr auto; align-items:center; } .top div { grid-template-columns:auto auto auto; } .top a { min-width:92px; } }
    @media (max-width:390px) { .app{padding:10px 8px}.card{border-radius:24px;padding:18px 12px}.stage{grid-template-columns:1fr 82px 82px;gap:6px}.grid2{grid-template-columns:1fr 92px} input,select{font-size:14px;padding:10px 6px;} }
  </style>
</head>
<body>
  <main class="app">
    <section class="card">
      <div class="top"><h1>Recipes</h1><div><a href="/settings">Settings</a> <a href="/wifi">WiFi</a> <a href="/">Dashboard</a></div></div>
      <div class="grid2">
        <div><label for="recipeName">Recipe name</label><input id="recipeName" maxlength="22" value="Generic Pour Over"></div>
        <div><label for="finalTarget">Final g</label><input id="finalTarget" type="number" min="50" max="5000" step="1" value="320"></div>
      </div>
      <label>Built-in recipes</label>
      <div class="preset-row">
        <button onclick="loadPreset('generic')">Generic</button>
        <button onclick="loadPreset('hoffmann')">Hoffmann V60</button>
        <button onclick="loadPreset('fourSix')">4:6 Method</button>
        <button onclick="loadPreset('kalita')">Kalita Wave</button>
        <button onclick="loadPreset('chemex')">Chemex</button>
        <button onclick="loadPreset('custom')">Custom V60</button>
      </div>
      <div class="hint">Presets scale from the final target weight. You can still edit any stage manually before saving.</div>
      <div class="stage">
        <div><label>Stage 1</label><input id="s1name" value="Bloom"></div>
        <div><label>Target g</label><input id="s1target" type="number" min="1" max="5000" step="1" value="64"></div>
        <div><label>Hold s</label><input id="s1hold" type="number" min="0" max="900" step="1" value="45"></div>
      </div>
      <div class="stage">
        <div><label>Stage 2</label><input id="s2name" value="Main Pour"></div>
        <div><label>Target g</label><input id="s2target" type="number" min="1" max="5000" step="1" value="192"></div>
        <div><label>Hold s</label><input id="s2hold" type="number" min="0" max="900" step="1" value="0"></div>
      </div>
      <div class="stage">
        <div><label>Stage 3</label><input id="s3name" value="Finish"></div>
        <div><label>Target g</label><input id="s3target" type="number" min="1" max="5000" step="1" value="320"></div>
        <div><label>Hold s</label><input id="s3hold" type="number" min="0" max="900" step="1" value="0"></div>
      </div>
      <div class="stage">
        <div><label>Stage 4</label><input id="s4name" value=""></div>
        <div><label>Target g</label><input id="s4target" type="number" min="0" max="5000" step="1" value="0"></div>
        <div><label>Hold s</label><input id="s4hold" type="number" min="0" max="900" step="1" value="0"></div>
      </div>
      <div class="stage">
        <div><label>Stage 5</label><input id="s5name" value=""></div>
        <div><label>Target g</label><input id="s5target" type="number" min="0" max="5000" step="1" value="0"></div>
        <div><label>Hold s</label><input id="s5hold" type="number" min="0" max="900" step="1" value="0"></div>
      </div>
      <button onclick="saveRecipe()">SAVE RECIPE</button>
      <button class="secondary" onclick="update()">RELOAD ACTIVE</button>
    </section>
    <div class="meta" id="meta">Connecting...</div>
  </main>
<script>
const ids = ['s1','s2','s3','s4','s5'];
const presets = {
  generic: {name:'Generic Pour Over', rows:[['Bloom',0.20,45],['Main Pour',0.60,0],['Finish',1.00,0]]},
  hoffmann: {name:'Hoffmann V60', rows:[['Bloom',0.188,45],['Main Pour',0.60,0],['Finish',1.00,0]]},
  fourSix: {name:'4:6 Method', rows:[['Pour 1',0.20,45],['Pour 2',0.40,0],['Pour 3',0.60,0],['Pour 4',0.80,0],['Finish',1.00,0]]},
  kalita: {name:'Kalita Wave', rows:[['Bloom',0.20,45],['Pour 2',0.50,0],['Pour 3',0.75,0],['Finish',1.00,0]]},
  chemex: {name:'Chemex', rows:[['Bloom',0.15,45],['Pour 2',0.40,0],['Pour 3',0.70,0],['Finish',1.00,0]]},
  custom: {name:'Custom V60', rows:[['Bloom',0.1875,45],['Pour 2',0.5625,0],['Pour 3',0.8125,0],['Finish',1.00,0]]}
};
function el(id){ return document.getElementById(id); }
function clamp(v,lo,hi){ return Math.max(lo, Math.min(hi, Number(v||0))); }
function finalTarget(){ return clamp(el('finalTarget').value,50,5000); }
function clearRows(){ ids.forEach(id=>{ el(id+'name').value=''; el(id+'target').value=0; el(id+'hold').value=0; }); }
function loadPreset(key){
  const p=presets[key]||presets.generic;
  const target=finalTarget();
  el('recipeName').value=p.name;
  clearRows();
  p.rows.forEach((r,i)=>{ if(i<ids.length){ el(ids[i]+'name').value=r[0]; el(ids[i]+'target').value=Math.round(target*r[1]); el(ids[i]+'hold').value=r[2]; }});
  el('meta').textContent='Loaded '+p.name+' · scaled to '+Math.round(target)+'g';
}
async function update(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'}); const d=await r.json();
    if(document.activeElement && document.activeElement.tagName==='INPUT') return;
    el('recipeName').value=d.recipe_name||'Generic Pour Over';
    el('finalTarget').value=Math.round(Number(d.target_g||320));
    clearRows();
    const stages=d.stages||[];
    stages.forEach((s,i)=>{ if(i<ids.length){ el(ids[i]+'name').value=s.name||('Stage '+(i+1)); el(ids[i]+'target').value=Math.round(Number(s.target_g||0)); el(ids[i]+'hold').value=Number(s.hold_s||0); }});
    el('meta').textContent='Active: '+(d.stage_name||'Target')+' · Final '+Math.round(Number(d.target_g||0))+'g';
  }catch(e){ el('meta').textContent='Connection lost'; }
}
async function saveRecipe(){
  const params=new URLSearchParams();
  const active=ids.filter(id => Number(el(id+'target').value||0) > 0 && (el(id+'name').value||'').trim().length > 0);
  params.set('name', el('recipeName').value || 'Recipe'); params.set('count', String(Math.max(1, active.length)));
  active.forEach((id,i)=>{ const key='s'+(i+1); params.set(key+'name', el(id+'name').value || ('Stage '+(i+1))); params.set(key+'target', clamp(el(id+'target').value,1,5000)); params.set(key+'hold', clamp(el(id+'hold').value,0,900)); });
  el('meta').textContent='Saving recipe...';
  try{ const r=await fetch('/api/recipe?'+params.toString(),{method:'POST'}); el('meta').textContent=r.ok?'Recipe saved':'Recipe save failed'; }
  catch(e){ el('meta').textContent='Recipe save failed'; }
  setTimeout(update,500);
}
el('finalTarget').addEventListener('change',()=>{ const name=el('recipeName').value; const matched=Object.keys(presets).find(k=>presets[k].name===name); if(matched) loadPreset(matched); });
update();
</script>
</body>
</html>
)HTML";
}
