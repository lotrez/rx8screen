#include "gear_indicator.h"
#include "gauge_common.h"

void GearIndicator::create(lv_obj_t *parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    gear_label = lv_label_create(container);
    lv_label_set_text(gear_label, "N");
    lv_obj_set_style_text_color(gear_label, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(gear_label, &orbitron_extrabold_48, 0);
}

void GearIndicator::update(int gear) {
    if (gear < 0 || gear > 6) {
        lv_label_set_text(gear_label, "N");
    } else {
        static char gear_buf[2] = "1";
        gear_buf[0] = '0' + gear + 1;
        lv_label_set_text(gear_label, gear_buf);
    }
}
