#include "rpm_gauge.h"
#include "gauge_common.h"
#include "fonts/dseg7_fonts.h"
#include <math.h>

static const lv_color_t COLOR_SEG_OFF = lv_color_hex(0x1A1A1A);
static const lv_color_t COLOR_DIGIT_OFF = lv_color_hex(0x0D1A0D);

static const float RPM_MAX = 11000.0f;

static const float ARC_CX = 400.0f;
static const float ARC_CY = 120.0f;
static const float ARC_R  = 340.0f;
static const float BX0 = 20.0f, BY0 = 460.0f;
static const float BX1 = 400.0f, BY1 = 460.0f;

static lv_color_t seg_gradient(float t) {
    uint8_t r, g, b;
    if (t < 0.5f) {
        float f = t / 0.5f;
        r = (uint8_t)(0x00 + (0xFF - 0x00) * f);
        g = (uint8_t)(0xFF - (0xFF - 0xAA) * f);
        b = (uint8_t)(0x41 * (1.0f - f));
    } else {
        float f = (t - 0.5f) / 0.5f;
        r = 0xFF;
        g = (uint8_t)(0xAA - (0xAA - 0x22) * f);
        b = (uint8_t)(0x22 * f);
    }
    return lv_color_hex(((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
}

static void path_point(float t, float *x, float *y, float *outward_angle) {
    float bottom_len = BX1 - BX0;
    float arc_len = (float)M_PI / 2.0f * ARC_R;
    float total = bottom_len + arc_len;
    float fb = bottom_len / total;

    if (t < fb) {
        float bt = t / fb;
        *x = BX0 + (BX1 - BX0) * bt;
        *y = BY0;
        *outward_angle = (float)M_PI / 2.0f;
    } else {
        float at = (t - fb) / (1.0f - fb);
        float theta = at * (float)M_PI / 2.0f;
        *x = ARC_CX + ARC_R * sinf(theta);
        *y = ARC_CY + ARC_R * cosf(theta);
        *outward_angle = (float)M_PI / 2.0f - theta;
    }
}

void RpmGauge::create(lv_obj_t *parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_pos(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < RPM_NUM_SEGMENTS; i++) {
        float t = (float)i / (RPM_NUM_SEGMENTS - 1);
        float px, py, oa;
        path_point(t, &px, &py, &oa);

        int32_t rot = (int32_t)((oa - (float)M_PI / 2.0f) * 1800.0f / (float)M_PI);

        segments[i] = lv_obj_create(container);
        lv_obj_remove_style_all(segments[i]);
        lv_obj_set_size(segments[i], 6, 20);
        lv_obj_set_pos(segments[i], (lv_coord_t)(px - 3), (lv_coord_t)(py - 10));
        lv_obj_set_style_bg_opa(segments[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(segments[i], COLOR_SEG_OFF, 0);
        lv_obj_set_style_radius(segments[i], 2, 0);
        lv_obj_set_style_transform_rotation(segments[i], rot, 0);
        lv_obj_set_style_transform_pivot_x(segments[i], LV_PCT(50), 0);
        lv_obj_set_style_transform_pivot_y(segments[i], LV_PCT(50), 0);
    }

    lv_obj_t *digit_row = lv_obj_create(container);
    lv_obj_remove_style_all(digit_row);
    lv_obj_set_size(digit_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(digit_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(digit_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(digit_row, 6, 0);
    lv_obj_clear_flag(digit_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(digit_row, LV_ALIGN_BOTTOM_LEFT, 30, -80);

    for (int i = 0; i < 4; i++) {
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
        lv_obj_set_style_text_color(digit_labels[i], COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(digit_labels[i], &dseg7_classic_bold_italic_72, 0);
        lv_obj_align(digit_labels[i], LV_ALIGN_CENTER, 0, 0);
    }

    unit_label = lv_label_create(container);
    lv_label_set_text(unit_label, "RPM");
    lv_obj_set_style_text_color(unit_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(unit_label, &orbitron_bold_20, 0);
    lv_obj_align_to(unit_label, digit_row, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -14);
}

void RpmGauge::update(float rpm) {
    int active = (int)(rpm / RPM_MAX * RPM_NUM_SEGMENTS);
    if (active > RPM_NUM_SEGMENTS) active = RPM_NUM_SEGMENTS;
    if (active < 0) active = 0;

    for (int i = 0; i < RPM_NUM_SEGMENTS; i++) {
        float t = (float)i / (RPM_NUM_SEGMENTS - 1);
        lv_obj_set_style_bg_color(segments[i], i < active ? seg_gradient(t) : COLOR_SEG_OFF, 0);
    }

    int val = (int)rpm;
    if (val > 9999) val = 9999;
    if (val < 0) val = 0;

    int digits[4];
    digits[0] = (val / 1000) % 10;
    digits[1] = (val / 100) % 10;
    digits[2] = (val / 10) % 10;
    digits[3] = val % 10;

    char buf[2] = "0";
    float rpm_t = rpm / RPM_MAX;
    if (rpm_t > 1.0f) rpm_t = 1.0f;
    lv_color_t val_color = seg_gradient(rpm_t);
    for (int i = 0; i < 4; i++) {
        bool on = (i == 3) || (i == 2 && val >= 10) || (i == 1 && val >= 100) || (i == 0 && val >= 1000);
        buf[0] = '0' + digits[i];
        lv_label_set_text(digit_labels[i], buf);
        lv_obj_set_style_text_color(digit_labels[i], on ? val_color : COLOR_DIGIT_OFF, 0);
    }
}
