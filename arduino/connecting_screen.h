#pragma once

#include <lvgl.h>

class ConnectingScreen {
public:
    void create(lv_obj_t *parent);
    void start_animation();
    void stop_animation();
    void set_status(const char *text);
    lv_obj_t *get_screen() { return screen; }

private:
    lv_obj_t *screen = nullptr;
    lv_obj_t *triangle_obj = nullptr;
    lv_obj_t *label = nullptr;
    lv_obj_t *status_label = nullptr;
    lv_timer_t *timer = nullptr;
    float current_angle = 0.0f;

    static void draw_cb(lv_event_t *event);
    static void timer_cb(lv_timer_t *timer);
};
