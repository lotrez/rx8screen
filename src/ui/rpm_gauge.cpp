#include "rpm_gauge.h"
#include "gauge_common.h"
#include "fonts/dseg7_fonts.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const float RPM_MAX = 11000.0f;
static const lv_color_t COLOR_SEG_OFF = lv_color_hex(0x141418);
static const lv_color_t COLOR_DIGIT_OFF = lv_color_hex(0x16161E);

static const int BORDER_THICKNESS = 2;
static const int FILL_INSET = 0;

static lv_color_t zone_color(float position) {
    if (position < 0.75f) {
        return lv_color_hex(0xE0E4F0);
    } else if (position < 0.85f) {
        return lv_color_hex(0xFFB700);
    } else {
        return lv_color_hex(0xE53935);
    }
}

static const int CARD_RADIUS = 12;

static void band_draw_cb(lv_event_t *event) {
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_obj_t *container = (lv_obj_t *)lv_event_get_target(event);
    RpmGauge *gauge = (RpmGauge *)lv_event_get_user_data(event);
    float active_position = gauge->get_active_t();

    lv_obj_t *card = lv_obj_get_parent(container);
    lv_area_t card_coords;
    lv_obj_get_coords(card, &card_coords);
    lv_coord_t card_w = lv_area_get_width(&card_coords);
    lv_coord_t card_h = lv_area_get_height(&card_coords);

    int band_thickness = (int)(card_h * 0.55f);
    int band_y = card_coords.y1 + card_h - band_thickness;

    int inset = 2;
    int fill_x1 = card_coords.x1 + inset;
    int fill_x2 = card_coords.x2 - inset;
    int fill_y1 = band_y + inset;
    int fill_y2 = card_coords.y2 - inset;
    int fill_w = fill_x2 - fill_x1;
    int radius = CARD_RADIUS - inset;

    lv_area_t track_area = {fill_x1, fill_y1, fill_x2, fill_y2};
    lv_draw_rect_dsc_t track_dsc;
    lv_draw_rect_dsc_init(&track_dsc);
    track_dsc.bg_color = lv_color_hex(0x16161A);
    track_dsc.radius = radius;
    lv_draw_rect(layer, &track_dsc, &track_area);

    if (active_position > 0.0f) {
        int active_x = fill_x1 + (int)(fill_w * active_position);

        lv_color_t fill_color;
        if (active_position < 0.75f) {
            fill_color = lv_color_hex(0xE0E4F0);
        } else if (active_position < 0.85f) {
            fill_color = lv_color_hex(0xFFB700);
        } else {
            fill_color = lv_color_hex(0xE53935);
        }

        lv_area_t fill_area = {(lv_coord_t)fill_x1, fill_y1, (lv_coord_t)active_x, fill_y2};
        lv_draw_rect_dsc_t fill_dsc;
        lv_draw_rect_dsc_init(&fill_dsc);
        fill_dsc.bg_color = fill_color;
        fill_dsc.radius = radius;
        lv_draw_rect(layer, &fill_dsc, &fill_area);
    }
}

void RpmGauge::create(lv_obj_t *parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(container, band_draw_cb, LV_EVENT_DRAW_MAIN, this);

    lv_obj_t *digit_row = lv_obj_create(container);
    lv_obj_remove_style_all(digit_row);
    lv_obj_set_size(digit_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(digit_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(digit_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(digit_row, 6, 0);
    lv_obj_clear_flag(digit_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(digit_row, LV_ALIGN_TOP_MID, -40, 4);

    for (int digit = 0; digit < 5; digit++) {
        lv_obj_t *slot = lv_obj_create(digit_row);
        lv_obj_remove_style_all(slot);
        lv_obj_set_size(slot, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);

        ghost_labels[digit] = lv_label_create(slot);
        lv_label_set_text(ghost_labels[digit], "8");
        lv_obj_set_style_text_color(ghost_labels[digit], COLOR_DIGIT_OFF, 0);
        lv_obj_set_style_text_font(ghost_labels[digit], &dseg7_classic_bold_italic_64, 0);

        digit_labels[digit] = lv_label_create(slot);
        lv_label_set_text(digit_labels[digit], "0");
        lv_obj_set_style_text_color(digit_labels[digit], COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(digit_labels[digit], &dseg7_classic_bold_italic_64, 0);
        lv_obj_align(digit_labels[digit], LV_ALIGN_CENTER, 0, 0);
    }

    unit_label = lv_label_create(container);
    lv_label_set_text(unit_label, "RPM");
    lv_obj_set_style_text_color(unit_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(unit_label, &orbitron_bold_10, 0);
    lv_obj_align_to(unit_label, digit_row, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, 0);
}

void RpmGauge::update(float rpm) {
    active_t = rpm / RPM_MAX;
    if (active_t > 1.0f) active_t = 1.0f;
    if (active_t < 0.0f) active_t = 0.0f;

    int rpm_value = (int)rpm;
    if (rpm_value > 99999) rpm_value = 99999;
    if (rpm_value < 0) rpm_value = 0;

    int digits[5];
    digits[0] = (rpm_value / 10000) % 10;
    digits[1] = (rpm_value / 1000) % 10;
    digits[2] = (rpm_value / 100) % 10;
    digits[3] = (rpm_value / 10) % 10;
    digits[4] = rpm_value % 10;

    char digit_buf[2] = "0";
    float rpm_position = rpm / RPM_MAX;
    if (rpm_position > 1.0f) rpm_position = 1.0f;
    lv_color_t active_color = zone_color(rpm_position);
    for (int digit = 0; digit < 5; digit++) {
        bool is_active = (digit == 4) || (digit == 3 && rpm_value >= 10)
                       || (digit == 2 && rpm_value >= 100)
                       || (digit == 1 && rpm_value >= 1000)
                       || (digit == 0 && rpm_value >= 10000);
        digit_buf[0] = '0' + digits[digit];
        lv_label_set_text(digit_labels[digit], digit_buf);
        lv_obj_set_style_text_color(digit_labels[digit], is_active ? active_color : COLOR_DIGIT_OFF, 0);
    }

    lv_obj_invalidate(container);
}
