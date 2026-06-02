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

static void band_draw_cb(lv_event_t *event) {
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_obj_t *container = (lv_obj_t *)lv_event_get_target(event);
    RpmGauge *gauge = (RpmGauge *)lv_event_get_user_data(event);
    float active_position = gauge->get_active_t();

    lv_area_t container_coords;
    lv_obj_get_coords(container, &container_coords);
    lv_coord_t card_x = container_coords.x1;
    lv_coord_t card_y = container_coords.y1;
    lv_coord_t card_w = lv_area_get_width(&container_coords);
    lv_coord_t card_h = lv_area_get_height(&container_coords);

    int band_thickness = (int)(card_h * 0.18f);
    int band_y = card_y + card_h - band_thickness;

    int fill_y_top = band_y + BORDER_THICKNESS + FILL_INSET - 1;
    int fill_y_bottom = card_y + card_h - BORDER_THICKNESS - FILL_INSET;

    int num_strips = 200;
    float fill_x_start = card_x + BORDER_THICKNESS + FILL_INSET - 1;
    float fill_x_end = card_x + card_w - BORDER_THICKNESS - FILL_INSET;
    float total_width = fill_x_end - fill_x_start;
    float strip_width = total_width / num_strips;

    for (int strip = 0; strip < num_strips; strip++) {
        float raw_x1 = fill_x_start + strip * strip_width;
        float raw_x2 = fill_x_start + (strip + 1) * strip_width;
        if (strip == num_strips - 1) raw_x2 = fill_x_end;

        float position_mid = (raw_x1 + raw_x2) * 0.5f - fill_x_start;
        position_mid /= total_width;
        lv_color_t strip_color = (position_mid <= active_position) ? gradient_color(position_mid) : COLOR_SEG_OFF;

        lv_area_t coords;
        coords.x1 = (lv_coord_t)floorf(raw_x1);
        coords.x2 = (lv_coord_t)ceilf(raw_x2);
        coords.y1 = fill_y_top;
        coords.y2 = fill_y_bottom;

        lv_draw_rect_dsc_t rect_descriptor;
        lv_draw_rect_dsc_init(&rect_descriptor);
        rect_descriptor.bg_color = strip_color;
        lv_draw_rect(layer, &rect_descriptor, &coords);
    }

    lv_draw_line_dsc_t line_descriptor;
    lv_draw_line_dsc_init(&line_descriptor);
    line_descriptor.color = COLOR_BORDER;
    line_descriptor.width = BORDER_THICKNESS;

    line_descriptor.p1.x = card_x;
    line_descriptor.p1.y = band_y;
    line_descriptor.p2.x = card_x + card_w;
    line_descriptor.p2.y = band_y;
    lv_draw_line(layer, &line_descriptor);

    line_descriptor.p1.x = card_x;
    line_descriptor.p1.y = card_y + card_h;
    line_descriptor.p2.x = card_x + card_w;
    line_descriptor.p2.y = card_y + card_h;
    lv_draw_line(layer, &line_descriptor);

    line_descriptor.p1.x = card_x;
    line_descriptor.p1.y = band_y;
    line_descriptor.p2.x = card_x;
    line_descriptor.p2.y = card_y + card_h;
    lv_draw_line(layer, &line_descriptor);

    line_descriptor.p1.x = card_x + card_w;
    line_descriptor.p1.y = band_y;
    line_descriptor.p2.x = card_x + card_w;
    line_descriptor.p2.y = card_y + card_h;
    lv_draw_line(layer, &line_descriptor);
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
