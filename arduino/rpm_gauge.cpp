#include "rpm_gauge.h"
#include "gauge_common.h"
#include "dseg7_fonts.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const float RPM_MAX = 11000.0f;
static const lv_color_t COLOR_SEG_OFF = lv_color_hex(0x0D1A0D);
static const lv_color_t COLOR_DIGIT_OFF = lv_color_hex(0x0D1A0D);
static const lv_color_t COLOR_BORDER = COLOR_GRID_BORDER;

static const int BORDER_THICKNESS = 3;
static const int FILL_INSET = 0;

static lv_color_t gradient_lut[256];
static bool gradient_lut_ready = false;

static void build_gradient_lut() {
    if (gradient_lut_ready) return;
    for (int i = 0; i < 256; i++) {
        float position = i / 255.0f;
        uint8_t red, green, blue;
        if (position < 0.5f) {
            float fraction = position / 0.5f;
            red   = (uint8_t)((0xFF) * fraction);
            green = (uint8_t)(0xFF - (0xFF - 0xAA) * fraction);
            blue  = (uint8_t)(0x41 * (1.0f - fraction));
        } else {
            float fraction = (position - 0.5f) / 0.5f;
            red   = 0xFF;
            green = (uint8_t)(0xAA - (0xAA - 0x22) * fraction);
            blue  = (uint8_t)(0x22 * fraction);
        }
        gradient_lut[i] = lv_color_hex(((uint32_t)red << 16) | ((uint32_t)green << 8) | blue);
    }
    gradient_lut_ready = true;
}

static inline lv_color_t gradient_color(float position) {
    if (!gradient_lut_ready) build_gradient_lut();
    int idx = (int)(position * 255.0f);
    if (idx < 0) idx = 0;
    if (idx > 255) idx = 255;
    return gradient_lut[idx];
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

    int fill_x_start = (int)(card_coords.x1 + BORDER_THICKNESS + FILL_INSET - 1);
    int fill_x_end = (int)(card_coords.x1 + card_w - BORDER_THICKNESS - FILL_INSET);
    int total_fill_width = fill_x_end - fill_x_start;

    lv_area_t fill_area;
    fill_area.x1 = fill_x_start;
    fill_area.x2 = fill_x_end;
    fill_area.y1 = fill_y_top;
    fill_area.y2 = fill_y_bottom;

    lv_draw_rect_dsc_t off_dsc;
    lv_draw_rect_dsc_init(&off_dsc);
    off_dsc.bg_color = COLOR_SEG_OFF;
    off_dsc.bg_opa = LV_OPA_COVER;
    off_dsc.radius = 0;
    lv_draw_rect(layer, &off_dsc, &fill_area);

    int active_width = (int)(total_fill_width * active_position);
    if (active_width > 0) {
        lv_area_t active_area = fill_area;
        active_area.x2 = fill_x_start + active_width - 1;
        if (active_area.x2 > fill_x_end) active_area.x2 = fill_x_end;

        lv_draw_rect_dsc_t active_dsc;
        lv_draw_rect_dsc_init(&active_dsc);
        active_dsc.bg_color = gradient_color(active_position);
        active_dsc.bg_opa = LV_OPA_COVER;
        active_dsc.radius = 0;
        lv_draw_rect(layer, &active_dsc, &active_area);
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
    card_border_dsc.border_color = COLOR_GRID_BORDER;
    card_border_dsc.border_width = 2;
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
    tween.set(rpm);
}

void RpmGauge::tick() {
    if (!tween.step(10.0f)) return;

    float rpm = tween.current;
    float new_active_t = rpm / RPM_MAX;
    if (new_active_t > 1.0f) new_active_t = 1.0f;
    if (new_active_t < 0.0f) new_active_t = 0.0f;

    int rpm_value = (int)rpm;
    if (rpm_value > 99999) rpm_value = 99999;
    if (rpm_value < 0) rpm_value = 0;

    bool active_t_changed = (fabsf(active_t - new_active_t) > 0.001f);
    active_t = new_active_t;

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

    bool any_digit_changed = false;
    for (int digit = 0; digit < 5; digit++) {
        bool is_active = (digit == 4) || (digit == 3 && rpm_value >= 10)
                       || (digit == 2 && rpm_value >= 100)
                       || (digit == 1 && rpm_value >= 1000)
                       || (digit == 0 && rpm_value >= 10000);

        if (digits[digit] != cached_digits[digit] || is_active != cached_active[digit]) {
            cached_digits[digit] = digits[digit];
            cached_active[digit] = is_active;
            digit_buf[0] = '0' + digits[digit];
            lv_label_set_text(digit_labels[digit], digit_buf);
            lv_obj_set_style_text_color(digit_labels[digit], is_active ? active_color : COLOR_DIGIT_OFF, 0);
            any_digit_changed = true;
        }
    }

    if (active_t_changed || any_digit_changed) {
        lv_obj_invalidate(container);
    }
}
