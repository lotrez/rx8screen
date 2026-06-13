#pragma once

#include <lvgl.h>

class GearIndicator {
public:
    void create(lv_obj_t *parent);
    void update(int gear);
    lv_obj_t *get_container() { return container; }

private:
    lv_obj_t *container = nullptr;
    lv_obj_t *ghost_label = nullptr;
    lv_obj_t *gear_label = nullptr;
    int cached_gear = -1;
};
