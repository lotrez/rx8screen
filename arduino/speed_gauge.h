#pragma once

#include <lvgl.h>

class SpeedGauge {
public:
    void create(lv_obj_t *parent);
    void update(float kmh);
    lv_obj_t *get_container() { return container; }

private:
    lv_obj_t *container = nullptr;
    lv_obj_t *ghost_labels[3] = {};
    lv_obj_t *digit_labels[3] = {};
    lv_obj_t *unit_label = nullptr;
    int cached_speed = -1;
    int cached_digits[3] = {-1, -1, -1};
    bool cached_active[3] = {false, false, false};
};
