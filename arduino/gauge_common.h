#pragma once

#include <lvgl.h>
#include "dseg7_fonts.h"

static const lv_color_t COLOR_BG = lv_color_hex(0x0A0A0A);
static const lv_color_t COLOR_PRIMARY = lv_color_hex(0x00FF41);
static const lv_color_t COLOR_DIM = lv_color_hex(0x00AA30);
static const lv_color_t COLOR_WARN = lv_color_hex(0xFFAA00);
static const lv_color_t COLOR_CRIT = lv_color_hex(0xFF2222);
static const lv_color_t COLOR_TRACK = lv_color_hex(0x1A1A1A);
static const lv_color_t COLOR_ACCENT = lv_color_hex(0x00DDFF);
static const lv_color_t COLOR_GRID_BORDER = lv_color_hex(0xFF0000);

static inline void style_container(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 5, 0);
    lv_obj_set_style_pad_all(obj, 8, 0);
}

static inline lv_color_t get_threshold_color(float value, float warn, float crit) {
    if (crit > warn) {
        if (value >= crit) return COLOR_CRIT;
        if (value >= warn) return COLOR_WARN;
    } else {
        if (value <= crit) return COLOR_CRIT;
        if (value <= warn) return COLOR_WARN;
    }
    return COLOR_PRIMARY;
}

struct BarGaugeWidgets {
    lv_obj_t *container;
    lv_obj_t *value_label;
    lv_obj_t *unit_label;
    lv_obj_t *bar;
    int cached_display_val = -99999;
    uint32_t cached_color_full = 0;
};

static inline BarGaugeWidgets create_bar_gauge(lv_obj_t *parent, const char *name,
                                                 float min, float max, const char *unit) {
    BarGaugeWidgets w = {};

    w.container = lv_obj_create(parent);
    lv_obj_remove_style_all(w.container);
    lv_obj_set_size(w.container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(w.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(w.container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(w.container, 20, 0);
    lv_obj_clear_flag(w.container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name_label = lv_label_create(w.container);
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_color(name_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(name_label, &orbitron_bold_14, 0);

    lv_obj_t *value_row = lv_obj_create(w.container);
    lv_obj_remove_style_all(value_row);
    lv_obj_set_size(value_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(value_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(value_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(value_row, 6, 0);
    lv_obj_clear_flag(value_row, LV_OBJ_FLAG_SCROLLABLE);

    w.value_label = lv_label_create(value_row);
    lv_label_set_text(w.value_label, "");
    lv_obj_set_style_text_color(w.value_label, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(w.value_label, &dseg7_classic_bold_20, 0);

    w.unit_label = lv_label_create(value_row);
    lv_label_set_text(w.unit_label, unit);
    lv_obj_set_style_text_color(w.unit_label, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(w.unit_label, &orbitron_bold_14, 0);

    w.bar = lv_bar_create(w.container);
    lv_obj_set_size(w.bar, LV_PCT(90), 10);
    lv_bar_set_range(w.bar, (int32_t)(min * 10), (int32_t)(max * 10));
    lv_bar_set_value(w.bar, (int32_t)(min * 10), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(w.bar, COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(w.bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(w.bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(w.bar, COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(w.bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(w.bar, 4, LV_PART_INDICATOR);

    return w;
}

static inline void update_bar_gauge(BarGaugeWidgets &w, float value, float warn, float crit,
                                     const char *unit, float scale = 10.0f) {
    lv_color_t color = get_threshold_color(value, warn, crit);

    // Granularity of what's actually displayed: integer for >=100, 0.1-step for <100
    int display_val = (value >= 100.0f) ? (int)value : (int)(value * 10.0f);
    uint32_t color_u16 = lv_color_to_u16(color);
    if (display_val == w.cached_display_val && color_u16 == w.cached_color_full) return;
    w.cached_display_val = display_val;
    w.cached_color_full = color_u16;

    lv_bar_set_value(w.bar, (int32_t)(value * scale), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(w.bar, color, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(w.value_label, color, 0);
    lv_obj_set_style_text_color(w.unit_label, color, 0);

    char num_buf[16];
    if (value == (int)value && value >= 100)
        lv_snprintf(num_buf, sizeof(num_buf), "%d", (int)value);
    else if (value >= 100)
        lv_snprintf(num_buf, sizeof(num_buf), "%.0f", value);
    else if (value == (int)value)
        lv_snprintf(num_buf, sizeof(num_buf), "%.0f", value);
    else
        lv_snprintf(num_buf, sizeof(num_buf), "%.1f", value);

    lv_label_set_text(w.value_label, num_buf);
}
