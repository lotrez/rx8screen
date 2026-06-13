#pragma once

#include <lvgl.h>
#include <math.h>

// Time-based exponential value tween for smooth gauge animations.
// Moves `current` toward `target` each step(), producing a visible
// "counting up/down" effect on digit displays instead of instant jumps.
struct ValueTween {
    float current = 0.0f;
    float target = 0.0f;
    uint32_t last_ms = 0;
    bool first = true;

    void set(float value) { target = value; }

    // Advances current toward target at the given rate (higher = faster).
    // Uses measured frame time so the animation speed is independent of FPS.
    // Returns true if current changed and the gauge should redraw.
    bool step(float rate) {
        uint32_t now = lv_tick_get();
        if (first) {
            current = target;
            last_ms = now;
            first = false;
            return true;
        }

        float dt = (now - last_ms) / 1000.0f;
        last_ms = now;
        if (dt <= 0.0f) return false;

        float diff = target - current;
        if (fabsf(diff) < 0.5f) {
            bool changed = (current != target);
            current = target;
            return changed;
        }

        float move = diff * rate * dt;
        if (fabsf(move) < 1.0f) move = (diff > 0) ? 1.0f : -1.0f;
        current += move;
        if ((diff > 0 && current > target) || (diff < 0 && current < target))
            current = target;
        return true;
    }
};
