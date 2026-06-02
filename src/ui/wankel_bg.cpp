#include "wankel_bg.h"

static void wankel_draw_cb(lv_event_t *event) {
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(event);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int cx = coords.x1 + lv_area_get_width(&coords) / 2;
    int cy = coords.y1 + lv_area_get_height(&coords) / 2;

    int v1x = cx;
    int v1y = cy - 210;
    int v2x = cx - 182;
    int v2y = cy + 105;
    int v3x = cx + 182;
    int v3y = cy + 105;

    lv_color_t rotor_color = lv_color_hex(0x2A4A50);
    lv_color_t housing_color = lv_color_hex(0x1A2A30);

    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.width = 10;
    arc_dsc.rounded = 1;
    arc_dsc.opa = LV_OPA_70;
    arc_dsc.color = rotor_color;
    arc_dsc.radius = 260;

    arc_dsc.center.x = v3x;
    arc_dsc.center.y = v3y;
    arc_dsc.start_angle = 2000;
    arc_dsc.end_angle = 3400;
    lv_draw_arc(layer, &arc_dsc);

    arc_dsc.center.x = v2x;
    arc_dsc.center.y = v2y;
    arc_dsc.start_angle = 200;
    arc_dsc.end_angle = 1600;
    lv_draw_arc(layer, &arc_dsc);

    arc_dsc.center.x = v1x;
    arc_dsc.center.y = v1y;
    arc_dsc.start_angle = 2400;
    arc_dsc.end_angle = 1200;
    lv_draw_arc(layer, &arc_dsc);

    lv_draw_arc_dsc_t housing_dsc;
    lv_draw_arc_dsc_init(&housing_dsc);
    housing_dsc.width = 8;
    housing_dsc.rounded = 1;
    housing_dsc.opa = LV_OPA_60;
    housing_dsc.color = housing_color;

    housing_dsc.center.x = cx - 80;
    housing_dsc.center.y = cy - 30;
    housing_dsc.radius = 340;
    housing_dsc.start_angle = 400;
    housing_dsc.end_angle = 2600;
    lv_draw_arc(layer, &housing_dsc);

    housing_dsc.center.x = cx + 80;
    housing_dsc.center.y = cy - 30;
    housing_dsc.start_angle = 1000;
    housing_dsc.end_angle = 3200;
    lv_draw_arc(layer, &housing_dsc);

    housing_dsc.radius = 370;
    housing_dsc.center.x = cx;
    housing_dsc.center.y = cy + 220;
    housing_dsc.start_angle = 3000;
    housing_dsc.end_angle = 600;
    lv_draw_arc(layer, &housing_dsc);

    int dot_r = 10;
    lv_draw_arc_dsc_t dot_dsc;
    lv_draw_arc_dsc_init(&dot_dsc);
    dot_dsc.width = dot_r * 2;
    dot_dsc.rounded = 1;
    dot_dsc.opa = LV_OPA_80;
    dot_dsc.color = lv_color_hex(0xE0E4F0);
    dot_dsc.radius = dot_r;
    dot_dsc.center.x = v1x; dot_dsc.center.y = v1y;
    dot_dsc.start_angle = 0; dot_dsc.end_angle = 3600;
    lv_draw_arc(layer, &dot_dsc);
    dot_dsc.center.x = v2x; dot_dsc.center.y = v2y;
    lv_draw_arc(layer, &dot_dsc);
    dot_dsc.center.x = v3x; dot_dsc.center.y = v3y;
    lv_draw_arc(layer, &dot_dsc);
}

void wankel_bg_create(lv_obj_t *parent) {
    lv_obj_t *bg = lv_obj_create(parent);
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, 1024, 600);
    lv_obj_set_style_bg_opa(bg, LV_OPA_TRANSP, 0);
    lv_obj_align(bg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(bg, wankel_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
}
