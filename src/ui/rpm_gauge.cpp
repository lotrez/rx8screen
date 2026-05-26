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

static const int SCREEN_WIDTH = DISPLAY_WIDTH;
static const int SCREEN_HEIGHT = DISPLAY_HEIGHT;
static const int BAND_THICKNESS = 50;
static const float HALF_BAND = BAND_THICKNESS / 2.0f;
static const float CORNER_RADIUS = 100.0f;
static const int MARGIN = 10;

static const float ARC_CENTER_X = (SCREEN_WIDTH - MARGIN) - HALF_BAND - CORNER_RADIUS + 1;
static const float ARC_CENTER_Y = (SCREEN_HEIGHT - MARGIN) - HALF_BAND - CORNER_RADIUS + 2;

static const float PATH_BOTTOM_LEN = ARC_CENTER_X - MARGIN;
static const float PATH_ARC_LEN = CORNER_RADIUS * (float)M_PI / 2.0f;
static const float PATH_RIGHT_LEN = ARC_CENTER_Y - MARGIN;
static const float PATH_TOTAL_LEN = PATH_BOTTOM_LEN + PATH_ARC_LEN + PATH_RIGHT_LEN;

static const int BORDER_THICKNESS = 3;
static const int FILL_INSET = 2;

static const lv_color_t COLOR_INNER_BORDER = lv_color_hex(0x000000);

// Returns gradient color from green (position=0) through amber (0.5) to red (1)
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

// Draw the entire band + border using LVGL draw primitives on the layer.
// Called every frame via LV_EVENT_DRAW_MAIN.
static void band_draw_cb(lv_event_t *event) {
    lv_layer_t *layer = lv_event_get_layer(event);
    RpmGauge *gauge = (RpmGauge *)lv_event_get_user_data(event);
    float active_position = gauge->get_active_t();

    // --- Draw bottom flat band as vertical strips ---
    int num_bottom_strips = 200;
    float bottom_strip_width = PATH_BOTTOM_LEN / num_bottom_strips;

    for (int strip = 0; strip < num_bottom_strips; strip++) {
        float position_start = (strip * bottom_strip_width) / PATH_TOTAL_LEN;
        float position_end = ((strip + 1) * bottom_strip_width) / PATH_TOTAL_LEN;
        float position_mid = (position_start + position_end) * 0.5f;

        lv_color_t strip_color = (position_mid <= active_position) ? gradient_color(position_mid) : COLOR_SEG_OFF;

        lv_area_t coords;
        lv_coord_t bottom_x2 = (lv_coord_t)(MARGIN + (strip + 1) * bottom_strip_width);
        float bottom_end_x = ARC_CENTER_X - CORNER_RADIUS * 0.3f;
        if (bottom_x2 > (lv_coord_t)bottom_end_x) bottom_x2 = (lv_coord_t)bottom_end_x;
        coords.x1 = (lv_coord_t)(MARGIN + strip * bottom_strip_width);
        coords.x2 = bottom_x2;
        if (coords.x1 >= coords.x2) continue;
        coords.y1 = SCREEN_HEIGHT - MARGIN - BAND_THICKNESS + FILL_INSET;
        coords.y2 = SCREEN_HEIGHT - MARGIN - FILL_INSET;

        lv_draw_rect_dsc_t rect_descriptor;
        lv_draw_rect_dsc_init(&rect_descriptor);
        rect_descriptor.bg_color = strip_color;
        lv_draw_rect(layer, &rect_descriptor, &coords);
    }

    // --- Draw right flat band as horizontal strips ---
    int num_right_strips = 200;
    float right_strip_height = PATH_RIGHT_LEN / num_right_strips;

    for (int strip = 0; strip < num_right_strips; strip++) {
        float distance_from_top = strip * right_strip_height;
        float strip_y_pos = ARC_CENTER_Y - distance_from_top - right_strip_height;
        float path_distance = PATH_BOTTOM_LEN + PATH_ARC_LEN + distance_from_top;
        float position_start = path_distance / PATH_TOTAL_LEN;
        float position_end = (path_distance + right_strip_height) / PATH_TOTAL_LEN;
        float position_mid = (position_start + position_end) * 0.5f;

        lv_color_t strip_color = (position_mid <= active_position) ? gradient_color(position_mid) : COLOR_SEG_OFF;

        lv_area_t coords;
        lv_coord_t right_y1 = (lv_coord_t)strip_y_pos;
        lv_coord_t right_y2 = (lv_coord_t)(strip_y_pos + right_strip_height);
        float right_end_y = ARC_CENTER_Y + CORNER_RADIUS * 0.3f;
        if (right_y1 > (lv_coord_t)right_end_y) continue;
        if (right_y2 > (lv_coord_t)right_end_y) right_y2 = (lv_coord_t)right_end_y;
        coords.x1 = SCREEN_WIDTH - MARGIN - BAND_THICKNESS + FILL_INSET;
        coords.x2 = SCREEN_WIDTH - MARGIN - FILL_INSET;
        coords.y1 = right_y1;
        coords.y2 = right_y2;

        lv_draw_rect_dsc_t rect_descriptor;
        lv_draw_rect_dsc_init(&rect_descriptor);
        rect_descriptor.bg_color = strip_color;
        lv_draw_rect(layer, &rect_descriptor, &coords);
    }

    // --- Draw corner arc band using overlapping thick arcs for smooth edges ---
    int num_arc_segments = 45;
    float arc_segment_angle = 90.0f / num_arc_segments;

    for (int seg = 0; seg < num_arc_segments; seg++) {
        float angle_start = seg * arc_segment_angle - arc_segment_angle * 0.5f;
        float angle_end = (seg + 1) * arc_segment_angle + arc_segment_angle * 0.5f;
        if (angle_start < -15) angle_start = -15;
        if (angle_end > 105) angle_end = 105;
        float angle_mid = (seg + 0.5f) * arc_segment_angle;

        float arc_fraction = 1.0f - angle_mid / 90.0f;
        float gradient_position = (PATH_BOTTOM_LEN + arc_fraction * PATH_ARC_LEN) / PATH_TOTAL_LEN;

        lv_color_t seg_color = (gradient_position <= active_position) ? gradient_color(gradient_position) : COLOR_SEG_OFF;

        lv_draw_arc_dsc_t arc_fill;
        lv_draw_arc_dsc_init(&arc_fill);
        arc_fill.color = seg_color;
        arc_fill.width = BAND_THICKNESS - FILL_INSET * 2;
        arc_fill.center.x = (int32_t)ARC_CENTER_X;
        arc_fill.center.y = (int32_t)ARC_CENTER_Y;
        arc_fill.radius = (uint16_t)(CORNER_RADIUS + HALF_BAND);
        arc_fill.start_angle = (lv_value_precise_t)angle_start;
        arc_fill.end_angle = (lv_value_precise_t)angle_end;
        lv_draw_arc(layer, &arc_fill);
    }

    // --- Border: lines for flat sides, arcs for corner ---

    lv_draw_line_dsc_t line_descriptor;
    lv_draw_line_dsc_init(&line_descriptor);
    line_descriptor.color = COLOR_BORDER;
    line_descriptor.width = BORDER_THICKNESS;

    // Start cap: vertical line at the left end of the bottom band
    line_descriptor.p1.x = MARGIN;
    line_descriptor.p1.y = SCREEN_HEIGHT - MARGIN - BAND_THICKNESS;
    line_descriptor.p2.x = MARGIN;
    line_descriptor.p2.y = SCREEN_HEIGHT - MARGIN;
    lv_draw_line(layer, &line_descriptor);

    // End cap: horizontal line at the top end of the right band
    line_descriptor.p1.x = SCREEN_WIDTH - MARGIN - BAND_THICKNESS;
    line_descriptor.p1.y = MARGIN;
    line_descriptor.p2.x = SCREEN_WIDTH - MARGIN;
    line_descriptor.p2.y = MARGIN;
    lv_draw_line(layer, &line_descriptor);

    // Inner bottom line
    line_descriptor.p1.x = MARGIN;
    line_descriptor.p1.y = SCREEN_HEIGHT - MARGIN - BAND_THICKNESS;
    line_descriptor.p2.x = (lv_value_precise_t)ARC_CENTER_X;
    line_descriptor.p2.y = SCREEN_HEIGHT - MARGIN - BAND_THICKNESS;
    lv_draw_line(layer, &line_descriptor);

    // Inner right line
    line_descriptor.p1.x = SCREEN_WIDTH - MARGIN - BAND_THICKNESS;
    line_descriptor.p1.y = (lv_value_precise_t)ARC_CENTER_Y;
    line_descriptor.p2.x = SCREEN_WIDTH - MARGIN - BAND_THICKNESS;
    line_descriptor.p2.y = MARGIN;
    lv_draw_line(layer, &line_descriptor);

    // Outer bottom line
    line_descriptor.p1.x = MARGIN;
    line_descriptor.p1.y = SCREEN_HEIGHT - MARGIN;
    line_descriptor.p2.x = (lv_value_precise_t)ARC_CENTER_X;
    line_descriptor.p2.y = SCREEN_HEIGHT - MARGIN;
    lv_draw_line(layer, &line_descriptor);

    // Outer right line
    line_descriptor.p1.x = SCREEN_WIDTH - MARGIN;
    line_descriptor.p1.y = (lv_value_precise_t)ARC_CENTER_Y;
    line_descriptor.p2.x = SCREEN_WIDTH - MARGIN;
    line_descriptor.p2.y = MARGIN;
    lv_draw_line(layer, &line_descriptor);

    // Inner corner arc
    lv_draw_arc_dsc_t inner_arc_border;
    lv_draw_arc_dsc_init(&inner_arc_border);
    inner_arc_border.color = COLOR_BORDER;
    inner_arc_border.width = BORDER_THICKNESS;
    inner_arc_border.center.x = (int32_t)ARC_CENTER_X;
    inner_arc_border.center.y = (int32_t)ARC_CENTER_Y;
    inner_arc_border.radius = (uint16_t)(CORNER_RADIUS - HALF_BAND);
    inner_arc_border.start_angle = 0;
    inner_arc_border.end_angle = 90;
    lv_draw_arc(layer, &inner_arc_border);

    // Outer corner arc
    lv_draw_arc_dsc_t outer_arc_border;
    lv_draw_arc_dsc_init(&outer_arc_border);
    outer_arc_border.color = COLOR_BORDER;
    outer_arc_border.width = BORDER_THICKNESS;
    outer_arc_border.center.x = (int32_t)ARC_CENTER_X;
    outer_arc_border.center.y = (int32_t)ARC_CENTER_Y;
    outer_arc_border.radius = (uint16_t)(CORNER_RADIUS + HALF_BAND);
    outer_arc_border.start_angle = 0;
    outer_arc_border.end_angle = 90;
    lv_draw_arc(layer, &outer_arc_border);

    // --- Inner black border (gap between green border and colored fill) ---

    lv_draw_line_dsc_t inner_line;
    lv_draw_line_dsc_init(&inner_line);
    inner_line.color = COLOR_INNER_BORDER;
    inner_line.width = FILL_INSET;

    // Inner bottom black line
    inner_line.p1.x = MARGIN;
    inner_line.p1.y = SCREEN_HEIGHT - MARGIN - BAND_THICKNESS + FILL_INSET;
    inner_line.p2.x = (lv_value_precise_t)ARC_CENTER_X;
    inner_line.p2.y = SCREEN_HEIGHT - MARGIN - BAND_THICKNESS + FILL_INSET;
    lv_draw_line(layer, &inner_line);

    // Inner right black line
    inner_line.p1.x = SCREEN_WIDTH - MARGIN - BAND_THICKNESS + FILL_INSET;
    inner_line.p1.y = (lv_value_precise_t)ARC_CENTER_Y;
    inner_line.p2.x = SCREEN_WIDTH - MARGIN - BAND_THICKNESS + FILL_INSET;
    inner_line.p2.y = MARGIN;
    lv_draw_line(layer, &inner_line);

    // Outer bottom black line
    inner_line.p1.x = MARGIN;
    inner_line.p1.y = SCREEN_HEIGHT - MARGIN - FILL_INSET;
    inner_line.p2.x = (lv_value_precise_t)ARC_CENTER_X;
    inner_line.p2.y = SCREEN_HEIGHT - MARGIN - FILL_INSET;
    lv_draw_line(layer, &inner_line);

    // Outer right black line
    inner_line.p1.x = SCREEN_WIDTH - MARGIN - FILL_INSET;
    inner_line.p1.y = (lv_value_precise_t)ARC_CENTER_Y;
    inner_line.p2.x = SCREEN_WIDTH - MARGIN - FILL_INSET;
    inner_line.p2.y = MARGIN;
    lv_draw_line(layer, &inner_line);

    // Inner corner black arc
    lv_draw_arc_dsc_t inner_black_arc;
    lv_draw_arc_dsc_init(&inner_black_arc);
    inner_black_arc.color = COLOR_INNER_BORDER;
    inner_black_arc.width = FILL_INSET;
    inner_black_arc.center.x = (int32_t)ARC_CENTER_X;
    inner_black_arc.center.y = (int32_t)ARC_CENTER_Y;
    inner_black_arc.radius = (uint16_t)(CORNER_RADIUS - HALF_BAND + FILL_INSET);
    inner_black_arc.start_angle = 0;
    inner_black_arc.end_angle = 90;
    lv_draw_arc(layer, &inner_black_arc);

    // Outer corner black arc
    lv_draw_arc_dsc_t outer_black_arc;
    lv_draw_arc_dsc_init(&outer_black_arc);
    outer_black_arc.color = COLOR_INNER_BORDER;
    outer_black_arc.width = FILL_INSET;
    outer_black_arc.center.x = (int32_t)ARC_CENTER_X;
    outer_black_arc.center.y = (int32_t)ARC_CENTER_Y;
    outer_black_arc.radius = (uint16_t)(CORNER_RADIUS + HALF_BAND - FILL_INSET);
    outer_black_arc.start_angle = 0;
    outer_black_arc.end_angle = 90;
    lv_draw_arc(layer, &outer_black_arc);
}

void RpmGauge::create(lv_obj_t *parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(container, band_draw_cb, LV_EVENT_DRAW_MAIN, this);

    // Flex row container for the 5 RPM digit slots
    lv_obj_t *digit_row = lv_obj_create(container);
    lv_obj_remove_style_all(digit_row);
    lv_obj_set_size(digit_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(digit_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(digit_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(digit_row, 6, 0);
    lv_obj_clear_flag(digit_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(digit_row, LV_ALIGN_BOTTOM_LEFT, 30, -(lv_coord_t)(BAND_THICKNESS + MARGIN + 20));

    for (int digit = 0; digit < 5; digit++) {
        lv_obj_t *slot = lv_obj_create(digit_row);
        lv_obj_remove_style_all(slot);
        lv_obj_set_size(slot, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);

        ghost_labels[digit] = lv_label_create(slot);
        lv_label_set_text(ghost_labels[digit], "8");
        lv_obj_set_style_text_color(ghost_labels[digit], COLOR_DIGIT_OFF, 0);
        lv_obj_set_style_text_font(ghost_labels[digit], &dseg7_classic_bold_italic_72, 0);

        digit_labels[digit] = lv_label_create(slot);
        lv_label_set_text(digit_labels[digit], "0");
        lv_obj_set_style_text_color(digit_labels[digit], COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(digit_labels[digit], &dseg7_classic_bold_italic_72, 0);
        lv_obj_align(digit_labels[digit], LV_ALIGN_CENTER, 0, 0);
    }

    unit_label = lv_label_create(container);
    lv_label_set_text(unit_label, "RPM");
    lv_obj_set_style_text_color(unit_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(unit_label, &orbitron_bold_20, 0);
    lv_obj_align_to(unit_label, digit_row, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -14);
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
