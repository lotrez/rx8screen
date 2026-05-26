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

static const int W = DISPLAY_WIDTH;
static const int H = DISPLAY_HEIGHT;
static const int BAND_PX = 50;
static const float HALF_BAND = BAND_PX / 2.0f;
static const float R = 100.0f;
// Margin from screen edges so the band doesn't touch the borders
static const int MARGIN = 10;

// Band outer edges are inset by MARGIN from screen edges
// Band center-line arc center
static const float CL_X = (W - MARGIN) - HALF_BAND - R + 1;
static const float CL_Y = (H - MARGIN) - HALF_BAND - R + 2;

// Path lengths along the center line
static const float PATH_BOTTOM = CL_X - MARGIN;
static const float PATH_ARC = R * (float)M_PI / 2.0f;
static const float PATH_RIGHT = CL_Y - MARGIN;
static const float PATH_TOTAL = PATH_BOTTOM + PATH_ARC + PATH_RIGHT;

static const int BORDER_W = 3;

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

// Draw the entire band + border using LVGL draw primitives on the layer.
// Called every frame via LV_EVENT_DRAW_MAIN.
static void band_draw_cb(lv_event_t *e) {
    lv_layer_t *layer = lv_event_get_layer(e);
    RpmGauge *gauge = (RpmGauge *)lv_event_get_user_data(e);
    float active_t = gauge->get_active_t();

    // --- Draw bottom flat band as vertical strips ---
    int num_strips = 200;
    float strip_w = PATH_BOTTOM / num_strips;

    for (int i = 0; i < num_strips; i++) {
        float t0 = (i * strip_w) / PATH_TOTAL;
        float t1 = ((i + 1) * strip_w) / PATH_TOTAL;
        float t_mid = (t0 + t1) * 0.5f;

        lv_color_t color = (t_mid <= active_t) ? seg_gradient(t_mid) : COLOR_SEG_OFF;

        lv_area_t coords;
        coords.x1 = (lv_coord_t)(MARGIN + i * strip_w);
        coords.x2 = (lv_coord_t)(MARGIN + (i + 1) * strip_w);
        coords.y1 = H - MARGIN - BAND_PX;
        coords.y2 = H - MARGIN;

        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = color;
        lv_draw_rect(layer, &rect_dsc, &coords);
    }

    // --- Draw right flat band as horizontal strips ---
    int num_strips_r = 200;
    float strip_h = PATH_RIGHT / num_strips_r;

    for (int i = 0; i < num_strips_r; i++) {
        float dist_from_top = i * strip_h;
        float y_pos = CL_Y - dist_from_top - strip_h;
        float path_pos = PATH_BOTTOM + PATH_ARC + dist_from_top;
        float t0 = path_pos / PATH_TOTAL;
        float t1 = (path_pos + strip_h) / PATH_TOTAL;
        float t_mid = (t0 + t1) * 0.5f;

        lv_color_t color = (t_mid <= active_t) ? seg_gradient(t_mid) : COLOR_SEG_OFF;

        lv_area_t coords;
        coords.x1 = W - MARGIN - BAND_PX;
        coords.x2 = W - MARGIN;
        coords.y1 = (lv_coord_t)y_pos;
        coords.y2 = (lv_coord_t)(y_pos + strip_h);

        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = color;
        lv_draw_rect(layer, &rect_dsc, &coords);
    }

    // --- Draw corner arc band as angular segments ---
    int num_arc_segs = 60;
    float d_angle = 90.0f / num_arc_segs;

    for (int i = 0; i < num_arc_segs; i++) {
        float a0 = i * d_angle;
        float a1 = (i + 1) * d_angle;
        float a_mid = (a0 + a1) * 0.5f;

        float arc_frac = 1.0f - a_mid / 90.0f;
        float t = (PATH_BOTTOM + arc_frac * PATH_ARC) / PATH_TOTAL;

        lv_color_t color = (t <= active_t) ? seg_gradient(t) : COLOR_SEG_OFF;

        float rad_mid = a_mid * (float)M_PI / 180.0f;

        // Draw this arc segment as a small filled trapezoid (approximated as rect)
        float inner_r = R - HALF_BAND;
        float outer_r = R + HALF_BAND;
        float mid_r = R;
        float mx = CL_X + mid_r * cosf(rad_mid);
        float my = CL_Y + mid_r * sinf(rad_mid);
        float seg_arc_len = d_angle * (float)M_PI / 180.0f * mid_r;
        float half_len = seg_arc_len * 0.5f;

        float dx = -sinf(rad_mid);
        float dy = cosf(rad_mid);

        lv_point_precise_t pts[4] = {
            { mx + dx * half_len + cosf(rad_mid) * inner_r - mx, my + dy * half_len + sinf(rad_mid) * inner_r - my },
            { mx - dx * half_len + cosf(rad_mid) * inner_r - mx, my - dy * half_len + sinf(rad_mid) * inner_r - my },
            { mx - dx * half_len + cosf(rad_mid) * outer_r - mx, my - dy * half_len + sinf(rad_mid) * outer_r - my },
            { mx + dx * half_len + cosf(rad_mid) * outer_r - mx, my + dy * half_len + sinf(rad_mid) * outer_r - my },
        };

        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = color;
        // Use a small rect approximation centered on the arc segment
        lv_area_t coords;
        float rx = mx - half_len;
        float ry = my - HALF_BAND;
        coords.x1 = (lv_coord_t)(CL_X + inner_r * cosf(a0 * (float)M_PI / 180.0f));
        coords.y1 = (lv_coord_t)(CL_Y + inner_r * sinf(a0 * (float)M_PI / 180.0f));
        coords.x2 = (lv_coord_t)(CL_X + outer_r * cosf(a1 * (float)M_PI / 180.0f));
        coords.y2 = (lv_coord_t)(CL_Y + outer_r * sinf(a1 * (float)M_PI / 180.0f));
        // Normalize area
        if (coords.x1 > coords.x2) { lv_coord_t tmp = coords.x1; coords.x1 = coords.x2; coords.x2 = tmp; }
        if (coords.y1 > coords.y2) { lv_coord_t tmp = coords.y1; coords.y1 = coords.y2; coords.y2 = tmp; }
        lv_draw_rect(layer, &rect_dsc, &coords);
    }

    // --- Border: lines for flat sides, arcs for corner ---

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = COLOR_BORDER;
    line_dsc.width = BORDER_W;

    // Inner bottom line
    line_dsc.p1.x = MARGIN; line_dsc.p1.y = H - MARGIN - BAND_PX;
    line_dsc.p2.x = (lv_value_precise_t)CL_X; line_dsc.p2.y = H - MARGIN - BAND_PX;
    lv_draw_line(layer, &line_dsc);

    // Inner right line
    line_dsc.p1.x = W - MARGIN - BAND_PX; line_dsc.p1.y = (lv_value_precise_t)CL_Y;
    line_dsc.p2.x = W - MARGIN - BAND_PX; line_dsc.p2.y = MARGIN;
    lv_draw_line(layer, &line_dsc);

    // Outer bottom line
    line_dsc.p1.x = MARGIN; line_dsc.p1.y = H - MARGIN;
    line_dsc.p2.x = (lv_value_precise_t)CL_X; line_dsc.p2.y = H - MARGIN;
    lv_draw_line(layer, &line_dsc);

    // Outer right line
    line_dsc.p1.x = W - MARGIN; line_dsc.p1.y = (lv_value_precise_t)CL_Y;
    line_dsc.p2.x = W - MARGIN; line_dsc.p2.y = MARGIN;
    lv_draw_line(layer, &line_dsc);

    // Inner corner arc
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = COLOR_BORDER;
    arc_dsc.width = BORDER_W;
    arc_dsc.center.x = (int32_t)CL_X;
    arc_dsc.center.y = (int32_t)CL_Y;
    arc_dsc.radius = (uint16_t)(R - HALF_BAND);
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 90;
    lv_draw_arc(layer, &arc_dsc);

    // Outer corner arc
    lv_draw_arc_dsc_t oarc_dsc;
    lv_draw_arc_dsc_init(&oarc_dsc);
    oarc_dsc.color = COLOR_BORDER;
    oarc_dsc.width = BORDER_W;
    oarc_dsc.center.x = (int32_t)CL_X;
    oarc_dsc.center.y = (int32_t)CL_Y;
    oarc_dsc.radius = (uint16_t)(R + HALF_BAND);
    oarc_dsc.start_angle = 0;
    oarc_dsc.end_angle = 90;
    lv_draw_arc(layer, &oarc_dsc);
}

void RpmGauge::create(lv_obj_t *parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, W, H);
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
    lv_obj_align(digit_row, LV_ALIGN_BOTTOM_LEFT, 30, -(lv_coord_t)(BAND_PX + MARGIN + 20));

    for (int d = 0; d < 5; d++) {
        lv_obj_t *slot = lv_obj_create(digit_row);
        lv_obj_remove_style_all(slot);
        lv_obj_set_size(slot, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);

        ghost_labels[d] = lv_label_create(slot);
        lv_label_set_text(ghost_labels[d], "8");
        lv_obj_set_style_text_color(ghost_labels[d], COLOR_DIGIT_OFF, 0);
        lv_obj_set_style_text_font(ghost_labels[d], &dseg7_classic_bold_italic_72, 0);

        digit_labels[d] = lv_label_create(slot);
        lv_label_set_text(digit_labels[d], "0");
        lv_obj_set_style_text_color(digit_labels[d], COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(digit_labels[d], &dseg7_classic_bold_italic_72, 0);
        lv_obj_align(digit_labels[d], LV_ALIGN_CENTER, 0, 0);
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

    int val = (int)rpm;
    if (val > 99999) val = 99999;
    if (val < 0) val = 0;

    int digits[5];
    digits[0] = (val / 10000) % 10;
    digits[1] = (val / 1000) % 10;
    digits[2] = (val / 100) % 10;
    digits[3] = (val / 10) % 10;
    digits[4] = val % 10;

    char buf[2] = "0";
    float rpm_t = rpm / RPM_MAX;
    if (rpm_t > 1.0f) rpm_t = 1.0f;
    lv_color_t val_color = seg_gradient(rpm_t);
    for (int d = 0; d < 5; d++) {
        bool on = (d == 4) || (d == 3 && val >= 10) || (d == 2 && val >= 100)
                || (d == 1 && val >= 1000) || (d == 0 && val >= 10000);
        buf[0] = '0' + digits[d];
        lv_label_set_text(digit_labels[d], buf);
        lv_obj_set_style_text_color(digit_labels[d], on ? val_color : COLOR_DIGIT_OFF, 0);
    }

    lv_obj_invalidate(container);
}
