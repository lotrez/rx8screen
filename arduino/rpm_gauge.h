#pragma once

#include <lvgl.h>

class RpmGauge {
public:
    void create(lv_obj_t *parent);
    void update(float rpm);
    lv_obj_t *get_container() { return container; }
    float get_active_t() { return active_t; }

private:
    lv_obj_t *container = nullptr;
    lv_obj_t *ghost_labels[5] = {};
    lv_obj_t *digit_labels[5] = {};
    lv_obj_t *unit_label = nullptr;
    float active_t = 0.0f;
    int cached_rpm = -1;
    int cached_digits[5] = {-1, -1, -1, -1, -1};
    bool cached_active[5] = {false, false, false, false, false};
};
