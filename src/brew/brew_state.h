#pragma once
#include <Arduino.h>

class BrewState {
public:
    void start() {
        if (!running) {
            running = true;
            start_ms = millis() - paused_elapsed_ms;
        }
    }
    void pause() {
        if (running) {
            running = false;
            paused_elapsed_ms = millis() - start_ms;
        }
    }
    void toggle() {
        if (!running) {
            start();
        } else {
            pause();
        }
    }
    void reset() {
        running = false;
        start_ms = 0;
        paused_elapsed_ms = 0;
    }
    bool is_running() const { return running; }
    uint32_t elapsed_ms() const { return running ? millis() - start_ms : paused_elapsed_ms; }
private:
    bool running = false;
    uint32_t start_ms = 0;
    uint32_t paused_elapsed_ms = 0;
};
