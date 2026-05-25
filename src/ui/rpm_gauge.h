#pragma once

#include <lvgl.h>

static const int RPM_NUM_SEGMENTS = 50;

class RpmGauge {
public:
    void create(lv_obj_t *parent);
    void update(float rpm);
    lv_obj_t *get_container() { return container; }

private:
    lv_obj_t *container = nullptr;
    lv_obj_t *segments[RPM_NUM_SEGMENTS] = {};
    lv_obj_t *ghost_labels[4] = {};
    lv_obj_t *digit_labels[4] = {};
    lv_obj_t *unit_label = nullptr;
};
