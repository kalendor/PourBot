#include "brew_log_store.h"
#include <SD_MMC.h>
#include <Preferences.h>
#include <cctype>
#include <time.h>
#include "config/hardware.h"

BrewLogStore brew_logs;

namespace {
constexpr uint32_t kSampleIntervalMs = 500;
constexpr uint8_t kFlushEveryRows = 10;
constexpr const char* kLogDirectory = "/pourbot";
}

bool BrewLogStore::begin() {
    if (!SD_MMC.setPins(HW_SD_CLK_PIN, HW_SD_CMD_PIN, HW_SD_D0_PIN)) {
        Serial.println("SD: pin assignment failed");
        return false;
    }
    mounted = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5);
    if (!mounted || SD_MMC.cardType() == CARD_NONE) {
        mounted = false;
        Serial.println("SD: no card; brew history disabled");
        return false;
    }
    if (!SD_MMC.exists(kLogDirectory)) SD_MMC.mkdir(kLogDirectory);
    Serial.printf("SD: brew history ready (%llu MB)\n", SD_MMC.cardSize() / (1024ULL * 1024ULL));
    return true;
}

String BrewLogStore::csv_text(const char* text) {
    String out = "\"";
    if (text) {
        for (const char* p = text; *p; ++p) {
            if (*p == '"') out += "\"\"";
            else if (*p == '\r' || *p == '\n') out += ' ';
            else out += *p;
        }
    }
    out += '"';
    return out;
}

void BrewLogStore::start_log(const BrewRecipe& recipe) {
    if (!mounted || active) return;

    char filename[32] = {};
    const time_t now = time(nullptr);
    struct tm local_time = {};
    const bool clock_valid = now >= 1700000000 && localtime_r(&now, &local_time) != nullptr;

    if (clock_valid) {
        // Example: 08292026_14-35.csv
        strftime(filename, sizeof(filename), "%m%d%Y_%H-%M.csv", &local_time);

        // Minute-resolution names can collide if two shots begin quickly. Keep
        // the requested base format, adding _02, _03, etc. only when necessary.
        if (SD_MMC.exists(String(kLogDirectory) + "/" + filename)) {
            char stem[20] = {};
            strftime(stem, sizeof(stem), "%m%d%Y_%H-%M", &local_time);
            for (uint8_t copy = 2; copy < 100; ++copy) {
                snprintf(filename, sizeof(filename), "%s_%02u.csv", stem, copy);
                if (!SD_MMC.exists(String(kLogDirectory) + "/" + filename)) break;
            }
        }
    } else {
        // Preserve reliable logging when the scale has no Internet connection
        // and therefore has not obtained a real clock value yet.
        Preferences prefs;
        prefs.begin("brewlogs", false);
        const uint32_t number = prefs.getUInt("next", 1);
        prefs.putUInt("next", number + 1);
        prefs.end();
        snprintf(filename, sizeof(filename), "brew_%05lu.csv", (unsigned long)number);
        Serial.println("SD: clock unavailable; using fallback brew filename");
    }

    char path[48];
    snprintf(path, sizeof(path), "%s/%s", kLogDirectory, filename);
    log_file = SD_MMC.open(path, FILE_WRITE);
    if (!log_file) {
        Serial.printf("SD: could not create %s\n", path);
        return;
    }

    log_file.println("# PourBot brew log v1");
    log_file.print("# recipe,");
    log_file.println(csv_text(recipe.name));
    log_file.printf("# target_g,%.1f\n", recipe.water_g);
    log_file.println("elapsed_ms,weight_g,flow_gps,stage_index,stage_name,running");
    log_file.flush();
    last_sample_elapsed_ms = UINT32_MAX;
    unflushed_rows = 0;
    active = true;
    Serial.printf("SD: logging %s\n", path);
}

void BrewLogStore::finish_log() {
    if (!active) return;
    if (log_file) {
        log_file.println("# complete");
        log_file.flush();
        log_file.close();
    }
    active = false;
    unflushed_rows = 0;
    Serial.println("SD: brew log saved");
}

void BrewLogStore::update(uint32_t elapsed_ms, bool running, float weight_g, float flow_gps,
                          const BrewRecipe& recipe, const BrewStageStatus& stage) {
    if (!mounted) return;
    if (!active && running) start_log(recipe);
    if (!active) return;
    if (elapsed_ms == 0 && !running) {
        finish_log();
        return;
    }
    if (last_sample_elapsed_ms != UINT32_MAX &&
        (elapsed_ms <= last_sample_elapsed_ms || elapsed_ms - last_sample_elapsed_ms < kSampleIntervalMs)) return;

    log_file.printf("%lu,%.1f,%.2f,%u,", (unsigned long)elapsed_ms, weight_g, flow_gps,
                    (unsigned)(stage.active_index + 1));
    log_file.print(csv_text(stage.stage_name));
    log_file.printf(",%u\n", running ? 1U : 0U);
    last_sample_elapsed_ms = elapsed_ms;
    if (++unflushed_rows >= kFlushEveryRows) {
        log_file.flush();
        unflushed_rows = 0;
    }
}

bool BrewLogStore::valid_name(const String& name) {
    if (!name.endsWith(".csv") || name.length() > 31) return false;
    const bool legacy_name = name.startsWith("brew_");
    const bool dated_name = name.length() >= 18 && isdigit((unsigned char)name[0]);
    if (!legacy_name && !dated_name) return false;
    for (size_t i = 0; i < name.length(); ++i) {
        const char c = name[i];
        if (!isalnum((unsigned char)c) && c != '_' && c != '-' && c != '.') return false;
    }
    return true;
}

File BrewLogStore::open_log(const String& name) {
    if (!mounted || !valid_name(name)) return File();
    return SD_MMC.open(String(kLogDirectory) + "/" + name, FILE_READ);
}

int BrewLogStore::delete_all_logs() {
    if (!mounted) return -1;
    if (active) return -2;

    int deleted = 0;
    while (deleted < 1000) {
        String target;
        File dir = SD_MMC.open(kLogDirectory);
        if (!dir) return deleted > 0 ? deleted : -3;

        File file;
        while ((file = dir.openNextFile())) {
            if (!file.isDirectory()) {
                String full = file.name();
                const int slash = full.lastIndexOf('/');
                const String name = slash >= 0 ? full.substring(slash + 1) : full;
                if (valid_name(name)) target = String(kLogDirectory) + "/" + name;
            }
            file.close();
            if (target.length()) break;
        }
        dir.close();

        if (!target.length()) break;
        if (!SD_MMC.remove(target)) return -3;
        deleted++;
    }
    Serial.printf("SD: deleted %d brew log(s)\n", deleted);
    return deleted;
}

String BrewLogStore::list_json() {
    String json = "{\"ok\":";
    json += mounted ? "true" : "false";
    json += ",\"logging\":";
    json += active ? "true" : "false";
    json += ",\"files\":[";
    if (mounted) {
        File dir = SD_MMC.open(kLogDirectory);
        File file;
        bool first = true;
        uint8_t count = 0;
        while ((file = dir.openNextFile()) && count < 60) {
            if (!file.isDirectory()) {
                String full = file.name();
                int slash = full.lastIndexOf('/');
                String name = slash >= 0 ? full.substring(slash + 1) : full;
                if (valid_name(name)) {
                    if (!first) json += ',';
                    json += "{\"name\":\"" + name + "\",\"bytes\":" + String((uint32_t)file.size()) + "}";
                    first = false;
                    count++;
                }
            }
            file.close();
        }
        dir.close();
    }
    json += "]}";
    return json;
}
