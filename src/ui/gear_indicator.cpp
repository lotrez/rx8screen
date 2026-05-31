#include "gear_indicator.h"
#include "gauge_common.h"
#include "fonts/dseg7_fonts.h"

static const lv_color_t COLOR_DIGIT_OFF = lv_color_hex(0x0D1A0D);

void GearIndicator::create(lv_obj_t *parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *slot = lv_obj_create(container);
    lv_obj_remove_style_all(slot);
    lv_obj_set_size(slot, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);

    ghost_label = lv_label_create(slot);
    lv_label_set_text(ghost_label, "8");
    lv_obj_set_style_text_color(ghost_label, COLOR_DIGIT_OFF, 0);
    lv_obj_set_style_text_font(ghost_label, &dseg7_classic_bold_italic_112, 0);

    gear_label = lv_label_create(slot);
    lv_label_set_text(gear_label, "N");
    lv_obj_set_style_text_color(gear_label, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(gear_label, &dseg7_classic_bold_italic_112, 0);
    lv_obj_align(gear_label, LV_ALIGN_CENTER, 0, 0);
}

void GearIndicator::update(int gear) {
    if (gear < 0 || gear > 5) {
        lv_label_set_text(gear_label, "N");
        lv_obj_set_style_text_color(gear_label, COLOR_DIM, 0);
    } else {
        static char gear_buf[2] = "1";
        gear_buf[0] = '1' + gear;
        lv_label_set_text(gear_label, gear_buf);
        lv_obj_set_style_text_color(gear_label, COLOR_ACCENT, 0);
    }
}
