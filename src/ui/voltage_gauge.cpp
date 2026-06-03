#include "voltage_gauge.h"
#include "gauge_common.h"

void VoltageGauge::create(lv_obj_t *parent) {
    BarGaugeWidgets w = create_bar_gauge(parent, "BATTERY", 10.0f, 16.0f, "V");
    container = w.container;
    value_label = w.value_label;
    unit_label = w.unit_label;
    bar = w.bar;
}

void VoltageGauge::update(float volts) {
    BarGaugeWidgets w = {container, value_label, unit_label, bar};
    update_bar_gauge(w, volts, 14.5f, 11.5f, "V");
}
