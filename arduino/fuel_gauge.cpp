#include "fuel_gauge.h"
#include "gauge_common.h"

void FuelGauge::create(lv_obj_t *parent) {
    BarGaugeWidgets w = create_bar_gauge(parent, "FUEL", 0.0f, 100.0f, "%");
    container = w.container;
    value_label = w.value_label;
    unit_label = w.unit_label;
    bar = w.bar;
}

void FuelGauge::update(float percent) {
    BarGaugeWidgets w = {container, value_label, unit_label, bar};
    update_bar_gauge(w, percent, 20.0f, 15.0f, "%");
}
