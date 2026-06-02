#include "rpm_gauge.h"
#include "gauge_common.h"
#include "fonts/dseg7_fonts.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const float RPM_MAX = 11000.0f;
static const lv_color_t COLOR_SEG_OFF = lv_color_hex(0x1A1A1A);
static const lv_color_t COLOR_DIGIT_OFF = lv_color_hex(0x0D1A0D);
static const lv_color_t COLOR_BORDER = lv_color_hex(0x00AA30);

static const int BORDER_THICKNESS = 2;
static const int FILL_INSET = 0;

static lv_color_t gradient_color(float position) {
    uint8_t red, green, blue;
    if (position < 0.5f) {
        float fraction = position / 0.5f;
        red = (uint8_t)(0x00 + (0xFF - 0x00) * fraction);
        green = (uint8_t)(0xFF - (0xFF - 0xAA) * fraction);
        blue = (uint8_t)(0x41 * (1.0f - fraction));
    } else {
        float fraction = (position - 0.5f) / 0.5f;
        red = 0xFF;
        green = (uint8_t)(0xAA - (0xAA - 0x22) * fraction);
        blue = (uint8_t)(0x22 * fraction);
    }
    return lv_color_hex(((uint32_t)red << 16) | ((uint32_t)green << 8) | blue);
}

static const int CARD_RADIUS = 16;

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

    int band_thickness = (int)(card_h * 0.18f);
    int band_y = card_coords.y1 + card_h - band_thickness;

    int fill_y_top = band_y + BORDER_THICKNESS + FILL_INSET - 1;
    int fill_y_bottom = card_coords.y2 - BORDER_THICKNESS - FILL_INSET;

    lv_area_t band_area;
    band_area.x1 = card_coords.x1;
    band_area.x2 = card_coords.x2;
    band_area.y1 = band_y;
    band_area.y2 = card_coords.y2;

    lv_area_t top_section = band_area;
    top_section.y2 = card_coords.y2 - CARD_RADIUS;

    lv_area_t bottom_section = band_area;
    bottom_section.y1 = card_coords.y2 - CARD_RADIUS;

    lv_draw_rect_dsc_t bg_dsc;
    lv_draw_rect_dsc_init(&bg_dsc);
    bg_dsc.bg_color = COLOR_SEG_OFF;

    lv_draw_rect_dsc_t top_bg = bg_dsc;
    top_bg.radius = 0;
    lv_draw_rect(layer, &top_bg, &top_section);

    lv_draw_rect_dsc_t bottom_bg = bg_dsc;
    bottom_bg.radius = CARD_RADIUS;
    lv_draw_rect(layer, &bottom_bg, &bottom_section);

    int num_segments = 40;
    int gap = 3;
    int fill_x_start = (int)(card_coords.x1 + BORDER_THICKNESS + FILL_INSET - 1);
    int fill_x_end = (int)(card_coords.x1 + card_w - BORDER_THICKNESS - FILL_INSET);
    int total_fill_width = fill_x_end - fill_x_start;
    int total_gap = (num_segments - 1) * gap;
    int seg_w = (total_fill_width - total_gap) / num_segments;

    for (int seg = 0; seg < num_segments; seg++) {
        int seg_x1 = fill_x_start + seg * (seg_w + gap);
        int seg_x2 = seg_x1 + seg_w - 1;
        if (seg == num_segments - 1) seg_x2 = fill_x_end - 1;

        float seg_mid = (seg + 0.5f) / num_segments;

        lv_color_t seg_color;
        if (seg_mid <= active_position) {
            seg_color = gradient_color(seg_mid);
        } else {
            seg_color = COLOR_SEG_OFF;
        }

        lv_area_t coords;
        coords.x1 = seg_x1;
        coords.x2 = seg_x2;
        coords.y1 = fill_y_top;
        coords.y2 = fill_y_bottom;

        lv_draw_rect_dsc_t rect_descriptor;
        lv_draw_rect_dsc_init(&rect_descriptor);
        rect_descriptor.bg_color = seg_color;
        rect_descriptor.radius = 2;
        lv_draw_rect(layer, &rect_descriptor, &coords);
    }

    lv_draw_rect_dsc_t border_dsc;
    lv_draw_rect_dsc_init(&border_dsc);
    border_dsc.bg_opa = LV_OPA_TRANSP;
    border_dsc.border_color = COLOR_BORDER;
    border_dsc.border_width = BORDER_THICKNESS;

    lv_draw_rect_dsc_t top_border = border_dsc;
    top_border.radius = 0;
    top_border.border_side = (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT);
    lv_draw_rect(layer, &top_border, &top_section);

    lv_draw_rect_dsc_t bottom_border = border_dsc;
    bottom_border.radius = CARD_RADIUS;
    bottom_border.border_side = (lv_border_side_t)(LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT);
    lv_draw_rect(layer, &bottom_border, &bottom_section);

    lv_draw_rect_dsc_t card_border_dsc;
    lv_draw_rect_dsc_init(&card_border_dsc);
    card_border_dsc.bg_opa = LV_OPA_TRANSP;
    card_border_dsc.border_color = lv_color_hex(0x00AA30);
    card_border_dsc.border_width = 1;
    card_border_dsc.radius = CARD_RADIUS;
    lv_draw_rect(layer, &card_border_dsc, &card_coords);
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
    lv_obj_align(digit_row, LV_ALIGN_CENTER, 0, -20);

    for (int digit = 0; digit < 5; digit++) {
        lv_obj_t *slot = lv_obj_create(digit_row);
        lv_obj_remove_style_all(slot);
        lv_obj_set_size(slot, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);

        ghost_labels[digit] = lv_label_create(slot);
        lv_label_set_text(ghost_labels[digit], "8");
        lv_obj_set_style_text_color(ghost_labels[digit], COLOR_DIGIT_OFF, 0);
        lv_obj_set_style_text_font(ghost_labels[digit], &dseg7_classic_bold_italic_80, 0);

        digit_labels[digit] = lv_label_create(slot);
        lv_label_set_text(digit_labels[digit], "0");
        lv_obj_set_style_text_color(digit_labels[digit], COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(digit_labels[digit], &dseg7_classic_bold_italic_80, 0);
        lv_obj_align(digit_labels[digit], LV_ALIGN_CENTER, 0, 0);
    }

    unit_label = lv_label_create(container);
    lv_label_set_text(unit_label, "RPM");
    lv_obj_set_style_text_color(unit_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(unit_label, &orbitron_bold_20, 0);
    lv_obj_align_to(unit_label, digit_row, LV_ALIGN_OUT_RIGHT_BOTTOM, 12, 0);
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
    lv_color_t active_color = gradient_color(rpm_position);
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
