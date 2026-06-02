#pragma once

#include <lvgl.h>
#include "fonts/dseg7_fonts.h"

static const lv_color_t COLOR_BG = lv_color_hex(0x0A0A0E);
static const lv_color_t COLOR_PRIMARY = lv_color_hex(0xE0E4F0);
static const lv_color_t COLOR_DIM = lv_color_hex(0x5A5E6A);
static const lv_color_t COLOR_WARN = lv_color_hex(0xFFB700);
static const lv_color_t COLOR_CRIT = lv_color_hex(0xE53935);
static const lv_color_t COLOR_TRACK = lv_color_hex(0x16161A);
static const lv_color_t COLOR_ACCENT = lv_color_hex(0xE0E4F0);
static const lv_color_t COLOR_BORDER = lv_color_hex(0x1E2030);

static inline void style_container(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 5, 0);
    lv_obj_set_style_pad_all(obj, 8, 0);
}

static inline lv_color_t get_threshold_color(float value, float warn, float crit,
                                              lv_color_t normal_color) {
    if (crit > warn) {
        if (value >= crit) return COLOR_CRIT;
        if (value >= warn) return COLOR_WARN;
    } else {
        if (value <= crit) return COLOR_CRIT;
        if (value <= warn) return COLOR_WARN;
    }
    return normal_color;
}

struct BarGaugeWidgets {
    lv_obj_t *container;
    lv_obj_t *value_label;
    lv_obj_t *bar;
    lv_color_t gauge_color;
};

static inline BarGaugeWidgets create_bar_gauge(lv_obj_t *parent, const char *name,
                                                float min, float max, const char *unit,
                                                lv_color_t gauge_color) {
    BarGaugeWidgets w = {};
    w.gauge_color = gauge_color;

    w.container = lv_obj_create(parent);
    lv_obj_remove_style_all(w.container);
    lv_obj_set_size(w.container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(w.container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(w.container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(w.container, 0, 0);
    lv_obj_clear_flag(w.container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *text_col = lv_obj_create(w.container);
    lv_obj_remove_style_all(text_col);
    lv_obj_set_width(text_col, 120);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(text_col, 0, 0);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name_label = lv_label_create(text_col);
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_color(name_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(name_label, &orbitron_bold_10, 0);

    w.value_label = lv_label_create(text_col);
    lv_label_set_text(w.value_label, "");
    lv_obj_set_style_text_color(w.value_label, gauge_color, 0);
    lv_obj_set_style_text_font(w.value_label, &dseg7_classic_bold_24, 0);

    w.bar = lv_bar_create(w.container);
    lv_obj_set_flex_grow(w.bar, 1);
    lv_obj_set_style_min_width(w.bar, 40, 0);
    lv_obj_set_height(w.bar, 18);
    lv_bar_set_range(w.bar, (int32_t)(min * 10), (int32_t)(max * 10));
    lv_bar_set_value(w.bar, (int32_t)(min * 10), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(w.bar, COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(w.bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(w.bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(w.bar, gauge_color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(w.bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(w.bar, 4, LV_PART_INDICATOR);

    return w;
}

static inline void update_bar_gauge(BarGaugeWidgets &w, float value, float warn, float crit,
                                     const char *unit, float scale = 10.0f) {
    lv_color_t color = get_threshold_color(value, warn, crit, w.gauge_color);
    lv_bar_set_value(w.bar, (int32_t)(value * scale), LV_ANIM_ON);
    lv_obj_set_style_bg_color(w.bar, color, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(w.value_label, color, 0);

    char num_buf[16];
    if (value == (int)value && value >= 100)
        lv_snprintf(num_buf, sizeof(num_buf), "%d", (int)value);
    else if (value >= 100)
        lv_snprintf(num_buf, sizeof(num_buf), "%.0f", value);
    else if (value == (int)value)
        lv_snprintf(num_buf, sizeof(num_buf), "%.0f", value);
    else
        lv_snprintf(num_buf, sizeof(num_buf), "%.1f", value);

    char buf[32];
    lv_snprintf(buf, sizeof(buf), "%s %s", num_buf, unit);
    lv_label_set_text(w.value_label, buf);
}
