#include "voltage_gauge.h"

void VoltageGauge::create(lv_obj_t *parent) {
    widgets = create_bar_gauge(parent, "BATTERY", 10.0f, 16.0f, "V");
}

void VoltageGauge::update(float volts) {
    update_bar_gauge(widgets, volts, 14.5f, 11.5f, "V");
}
