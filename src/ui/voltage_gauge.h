#pragma once

#include <lvgl.h>

class VoltageGauge {
public:
    void create(lv_obj_t *parent);
    void update(float volts);
    lv_obj_t *get_container() { return container; }

private:
    lv_obj_t *container = nullptr;
    lv_obj_t *value_label = nullptr;
    lv_obj_t *bar = nullptr;
};
