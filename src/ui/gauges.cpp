#include "gauges.h"

static const lv_color_t COLOR_BG = lv_color_hex(0x0A0A0A);
static const lv_color_t COLOR_PRIMARY = lv_color_hex(0x00FF41);
static const lv_color_t COLOR_DIM = lv_color_hex(0x00AA30);
static const lv_color_t COLOR_WARN = lv_color_hex(0xFFAA00);
static const lv_color_t COLOR_CRIT = lv_color_hex(0xFF2222);
static const lv_color_t COLOR_TRACK = lv_color_hex(0x1A1A1A);
static const lv_color_t COLOR_ACCENT = lv_color_hex(0x00DDFF);

static void style_container(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x222222), 0);
    lv_obj_set_style_radius(obj, 4, 0);
    lv_obj_set_style_pad_all(obj, 6, 0);
}

lv_color_t BarGauge::get_color(float value) {
    if (crit_val > warn_val) {
        if (value >= crit_val) return COLOR_CRIT;
        if (value >= warn_val) return COLOR_WARN;
    } else {
        if (value <= crit_val) return COLOR_CRIT;
        if (value <= warn_val) return COLOR_WARN;
    }
    return COLOR_PRIMARY;
}

void BarGauge::create(lv_obj_t *parent, const char *name, float min, float max,
                      float warn, float crit, const char *unit) {
    min_val = min;
    max_val = max;
    warn_val = warn;
    crit_val = crit;

    container = lv_obj_create(parent);
    style_container(container);
    lv_obj_set_size(container, 300, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(container, 2, 0);

    lv_obj_t *header = lv_obj_create(container);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, 4, 0);

    name_label = lv_label_create(header);
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_color(name_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);

    char buf[64];
    lv_snprintf(buf, sizeof(buf), "%.0f %s", min, unit);
    value_label = lv_label_create(header);
    lv_label_set_text(value_label, buf);
    lv_obj_set_style_text_color(value_label, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_14, 0);

    bar = lv_bar_create(container);
    lv_obj_set_size(bar, LV_PCT(100), 12);
    lv_bar_set_range(bar, (int32_t)(min * 10), (int32_t)(max * 10));
    lv_bar_set_value(bar, (int32_t)(min * 10), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
}
}

void BarGauge::update(float value) {
    lv_color_t color = get_color(value);
    lv_bar_set_value(bar, (int32_t)(value * 10), LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(value_label, color, 0);

    const char *fmt;
    if (value == (int)value && value >= 100)
        fmt = "%d";
    else if (value >= 100)
        fmt = "%.0f";
    else if (value == (int)value)
        fmt = "%.0f";
    else
        fmt = "%.1f";

    char buf[32];
    lv_snprintf(buf, sizeof(buf), fmt, value);
    lv_label_set_text(value_label, buf);
}

lv_color_t ArcGauge::get_color(float value) {
    if (value >= crit_val) return COLOR_CRIT;
    if (value >= warn_val) return COLOR_WARN;
    return COLOR_PRIMARY;
}

void ArcGauge::create(lv_obj_t *parent, const char *unit, float min, float max,
                      float warn, float crit, uint32_t tick_count) {
    min_val = min;
    max_val = max;
    warn_val = warn;
    crit_val = crit;

    container = lv_obj_create(parent);
    style_container(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    arc = lv_arc_create(container);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, (int32_t)min, (int32_t)max);
    lv_arc_set_value(arc, 0);
    lv_obj_set_size(arc, LV_PCT(85), LV_PCT(70));
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_color(arc, COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    value_label = lv_label_create(container);
    lv_label_set_text(value_label, "0");
    lv_obj_set_style_text_color(value_label, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_14, 0);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *unit_row = lv_obj_create(container);
    lv_obj_set_style_bg_opa(unit_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(unit_row, 0, 0);
    lv_obj_set_style_pad_all(unit_row, 0, 0);
    lv_obj_set_size(unit_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(unit_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(unit_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(unit_row, 2, 0);
    lv_obj_align(unit_row, LV_ALIGN_CENTER, 0, 10);

    unit_label = lv_label_create(unit_row);
    lv_label_set_text(unit_label, unit);
    lv_obj_set_style_text_color(unit_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(unit_label, &lv_font_montserrat_14, 0);
}

void ArcGauge::update(float value) {
    lv_color_t color = get_color(value);
    lv_arc_set_value(arc, (int32_t)value);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(value_label, color, 0);

    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%.0f", value);
    lv_label_set_text(value_label, buf);
}

void DigitalReadout::create(lv_obj_t *parent, const char *name, const char *unit) {
    container = lv_obj_create(parent);
    style_container(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    name_label = lv_label_create(container);
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_color(name_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);

    value_label = lv_label_create(container);
    lv_label_set_text(value_label, "0");
    lv_obj_set_style_text_color(value_label, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_14, 0);

    unit_label = lv_label_create(container);
    lv_label_set_text(unit_label, unit);
    lv_obj_set_style_text_color(unit_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(unit_label, &lv_font_montserrat_14, 0);
}

void DigitalReadout::update(float value) {
    char buf[32];
    if (value >= 100)
        lv_snprintf(buf, sizeof(buf), "%.0f", value);
    else
        lv_snprintf(buf, sizeof(buf), "%.1f", value);
    lv_label_set_text(value_label, buf);
}
