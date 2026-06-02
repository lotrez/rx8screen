#include "voltage_gauge.h"
#include "gauge_common.h"

void VoltageGauge::create(lv_obj_t *parent) {
    gauge_color = lv_color_hex(0xFFB700);
    BarGaugeWidgets w = create_bar_gauge(parent, "BATTERY", 10.0f, 16.0f, "V",
                                          gauge_color);
    container = w.container;
    value_label = w.value_label;
    bar = w.bar;
}

void VoltageGauge::update(float volts) {
    BarGaugeWidgets w = {container, value_label, bar, gauge_color};
    update_bar_gauge(w, volts, 14.5f, 11.5f, "V");
}
