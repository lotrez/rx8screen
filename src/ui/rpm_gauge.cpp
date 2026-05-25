#include "rpm_gauge.h"
#include "gauge_common.h"
#include "fonts/dseg7_fonts.h"

void RpmGauge::create(lv_obj_t *parent) {
    container = lv_obj_create(parent);
    style_container(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    arc = lv_arc_create(container);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 9000);
    lv_arc_set_value(arc, 0);
    lv_obj_set_size(arc, LV_PCT(85), LV_PCT(70));
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_color(arc, COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    value_label = lv_label_create(container);
    lv_label_set_text(value_label, "0");
    lv_obj_set_style_text_color(value_label, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(value_label, &dseg7_classic_bold_36, 0);
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
    lv_label_set_text(unit_label, "RPM");
    lv_obj_set_style_text_color(unit_label, COLOR_DIM, 0);
    lv_obj_set_style_text_font(unit_label, &orbitron_bold_14, 0);
}

void RpmGauge::update(float rpm) {
    lv_color_t color = get_threshold_color(rpm, 7500.0f, 8500.0f);
    lv_arc_set_value(arc, (int32_t)rpm);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(value_label, color, 0);

    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%.0f", rpm);
    lv_label_set_text(value_label, buf);
}
