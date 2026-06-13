#include "fuel_gauge.h"

void FuelGauge::create(lv_obj_t *parent) {
    widgets = create_bar_gauge(parent, "FUEL", 0.0f, 100.0f, "%");
}

void FuelGauge::update(float percent) {
    update_bar_gauge(widgets, percent, 20.0f, 15.0f, "%");
}
