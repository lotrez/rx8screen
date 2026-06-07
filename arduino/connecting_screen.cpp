#include "connecting_screen.h"
#include "gauge_common.h"
#include <math.h>

#ifndef DISPLAY_WIDTH
#define DISPLAY_WIDTH 1024
#define DISPLAY_HEIGHT 600
#endif

static const int TRIANGLE_RADIUS = 55;
static const float ROTATION_SPEED = 3.5f;
static const int ARC_WIDTH = 5;

static void rotate_point(float angle_rad, float x_in, float y_in, float &x_out, float &y_out) {
    float cos_angle = cosf(angle_rad);
    float sin_angle = sinf(angle_rad);
    x_out = x_in * cos_angle - y_in * sin_angle;
    y_out = x_in * sin_angle + y_in * cos_angle;
}

static void draw_rotor_arc(lv_layer_t *layer, int center_x, int center_y, float angle_rad,
                           float arc_center_dx, float arc_center_dy,
                           float arc_start_deg, float arc_end_deg,
                           uint16_t arc_radius, lv_color_t color) {
    float rot_cx, rot_cy;
    rotate_point(angle_rad, arc_center_dx, arc_center_dy, rot_cx, rot_cy);

    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = color;
    arc_dsc.width = ARC_WIDTH;
    arc_dsc.start_angle = arc_start_deg;
    arc_dsc.end_angle = arc_end_deg;
    arc_dsc.center.x = center_x + (int)rot_cx;
    arc_dsc.center.y = center_y + (int)rot_cy;
    arc_dsc.radius = arc_radius;
    arc_dsc.opa = LV_OPA_COVER;
    arc_dsc.rounded = 0;
    lv_draw_arc(layer, &arc_dsc);
}

void ConnectingScreen::draw_cb(lv_event_t *event) {
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(event);
    ConnectingScreen *screen_ptr = (ConnectingScreen *)lv_event_get_user_data(event);
    float angle_deg = screen_ptr->current_angle;
    float angle_rad = angle_deg * (float)M_PI / 180.0f;

    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);
    int center_x = (int)(lv_area_get_width(&obj_coords) / 2) + obj_coords.x1;
    int center_y = (int)(lv_area_get_height(&obj_coords) / 2) + obj_coords.y1 - 20;

    float side_offset = TRIANGLE_RADIUS * 0.8660254f;
    float half_height = TRIANGLE_RADIUS * 0.5f;
    uint16_t arc_radius = (uint16_t)(TRIANGLE_RADIUS * 1.73205f);

    // Rotate arc angles together with the triangle so the shape spins as a rigid body
    float arc_angle_offset = angle_deg;

    // Three arcs of a Reuleaux triangle, each centered at one vertex
    // Arc 1: centered at top vertex, spans 60° to 120°
    draw_rotor_arc(layer, center_x, center_y, angle_rad,
                   0.0f, -TRIANGLE_RADIUS,
                   60.0f + arc_angle_offset, 120.0f + arc_angle_offset,
                   arc_radius, COLOR_CRIT);

    // Arc 2: centered at bottom-left vertex, spans 300° to 360°
    draw_rotor_arc(layer, center_x, center_y, angle_rad,
                   -side_offset, half_height,
                   300.0f + arc_angle_offset, 360.0f + arc_angle_offset,
                   arc_radius, COLOR_CRIT);

    // Arc 3: centered at bottom-right vertex, spans 180° to 240°
    draw_rotor_arc(layer, center_x, center_y, angle_rad,
                   side_offset, half_height,
                   180.0f + arc_angle_offset, 240.0f + arc_angle_offset,
                   arc_radius, COLOR_CRIT);

    // Inner bearing circle
    lv_draw_arc_dsc_t bearing_dsc;
    lv_draw_arc_dsc_init(&bearing_dsc);
    bearing_dsc.color = COLOR_CRIT;
    bearing_dsc.width = ARC_WIDTH;
    bearing_dsc.start_angle = 0.0f;
    bearing_dsc.end_angle = 360.0f;
    bearing_dsc.center.x = center_x;
    bearing_dsc.center.y = center_y;
    bearing_dsc.radius = 10;
    bearing_dsc.opa = LV_OPA_COVER;
    bearing_dsc.rounded = 1;
    lv_draw_arc(layer, &bearing_dsc);
}

void ConnectingScreen::timer_cb(lv_timer_t *timer) {
    ConnectingScreen *screen_ptr = (ConnectingScreen *)lv_timer_get_user_data(timer);
    screen_ptr->current_angle += ROTATION_SPEED;
    if (screen_ptr->current_angle >= 360.0f) {
        screen_ptr->current_angle -= 360.0f;
    }
    lv_obj_invalidate(screen_ptr->triangle_obj);
}

void ConnectingScreen::create(lv_obj_t *parent) {
    screen = lv_obj_create(parent);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    triangle_obj = lv_obj_create(screen);
    lv_obj_remove_style_all(triangle_obj);
    lv_obj_set_size(triangle_obj, TRIANGLE_RADIUS * 3, TRIANGLE_RADIUS * 3);
    lv_obj_align(triangle_obj, LV_ALIGN_CENTER, 0, -30);
    lv_obj_clear_flag(triangle_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(triangle_obj, draw_cb, LV_EVENT_DRAW_MAIN, this);

    label = lv_label_create(screen);
    lv_label_set_text(label, "CONNECTING");
    lv_obj_set_style_text_color(label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(label, &orbitron_bold_24, 0);
    lv_obj_align_to(label, triangle_obj, LV_ALIGN_OUT_BOTTOM_MID, 0, 30);
}

void ConnectingScreen::start_animation() {
    if (timer) return;
    timer = lv_timer_create(timer_cb, 16, this);
}

void ConnectingScreen::stop_animation() {
    if (timer) {
        lv_timer_delete(timer);
        timer = nullptr;
    }
}
