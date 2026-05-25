#pragma once

#include <lvgl.h>

class BarGauge {
public:
    void create(lv_obj_t *parent, const char *name, float min, float max,
                float warn, float crit, const char *unit);
    void update(float value);
    lv_obj_t *get_container() { return container; }

private:
    lv_obj_t *container = nullptr;
    lv_obj_t *bar = nullptr;
    lv_obj_t *name_label = nullptr;
    lv_obj_t *value_label = nullptr;
    float min_val, max_val;
    float warn_val, crit_val;

    lv_color_t get_color(float value);
};

class ArcGauge {
public:
    void create(lv_obj_t *parent, const char *unit, float min, float max,
                float warn, float crit, uint32_t tick_count);
    void update(float value);
    lv_obj_t *get_container() { return container; }

private:
    lv_obj_t *container = nullptr;
    lv_obj_t *arc = nullptr;
    lv_obj_t *value_label = nullptr;
    lv_obj_t *unit_label = nullptr;
    lv_obj_t *name_label = nullptr;
    float min_val, max_val;
    float warn_val, crit_val;

    lv_color_t get_color(float value);
};

class DigitalReadout {
public:
    void create(lv_obj_t *parent, const char *name, const char *unit);
    void update(float value);
    lv_obj_t *get_container() { return container; }

private:
    lv_obj_t *container = nullptr;
    lv_obj_t *value_label = nullptr;
    lv_obj_t *name_label = nullptr;
    lv_obj_t *unit_label = nullptr;
};
