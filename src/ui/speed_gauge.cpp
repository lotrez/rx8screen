#include "speed_gauge.h"
#include "gauge_common.h"
#include "fonts/dseg7_fonts.h"

static const lv_color_t COLOR_DIGIT_OFF = lv_color_hex(0x0D1A0D);

void SpeedGauge::create(lv_obj_t *parent) {
    // container
    container = lv_obj_create(parent);
    style_container(container);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    // texte
    lv_obj_t *digit_row = lv_obj_create(container);
    lv_obj_remove_style_all(digit_row);
    lv_obj_set_size(digit_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(digit_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(digit_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(digit_row, 8, 0);
    lv_obj_clear_flag(digit_row, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 3; i++) {
        lv_obj_t *slot = lv_obj_create(digit_row);
        lv_obj_remove_style_all(slot);
        lv_obj_set_size(slot, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);

        ghost_labels[i] = lv_label_create(slot);
        lv_label_set_text(ghost_labels[i], "8");
        lv_obj_set_style_text_color(ghost_labels[i], COLOR_DIGIT_OFF, 0);
        lv_obj_set_style_text_font(ghost_labels[i], &dseg7_classic_bold_italic_72, 0);

        digit_labels[i] = lv_label_create(slot);
        lv_label_set_text(digit_labels[i], "0");
        lv_obj_set_style_text_color(digit_labels[i], COLOR_PRIMARY, 0);
        lv_obj_set_style_text_font(digit_labels[i], &dseg7_classic_bold_italic_72, 0);
        lv_obj_align(digit_labels[i], LV_ALIGN_CENTER, 0, 0);
    }

    // texte
    unit_label = lv_label_create(container);
    lv_label_set_text(unit_label, "km/h");
    lv_obj_set_style_text_color(unit_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(unit_label, &orbitron_bold_20, 0);
    lv_obj_align_to(unit_label, digit_row, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, 6);
}

void SpeedGauge::update(float kmh) {
    int val = (int)kmh;
    if (val > 999) val = 999;
    if (val < 0) val = 0;

    int digits[3];
    digits[0] = (val / 100) % 10;
    digits[1] = (val / 10) % 10;
    digits[2] = val % 10;

    char buf[2] = "0";
    for (int i = 0; i < 3; i++) {
        bool active = (i == 2) || (i == 1 && val >= 10) || (i == 0 && val >= 100);
        buf[0] = '0' + digits[i];
        lv_label_set_text(digit_labels[i], buf);
        lv_obj_set_style_text_color(digit_labels[i], active ? COLOR_PRIMARY : COLOR_DIGIT_OFF, 0);
    }
}
