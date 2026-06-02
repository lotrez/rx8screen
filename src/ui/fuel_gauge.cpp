#include "fuel_gauge.h"
#include "gauge_common.h"

void FuelGauge::create(lv_obj_t *parent) {
    gauge_color = lv_color_hex(0xE53935);
    BarGaugeWidgets w = create_bar_gauge(parent, "Fuel", 0.0f, 100.0f, "%",
                                          gauge_color);
    container = w.container;
    value_label = w.value_label;
    bar = w.bar;
}

void FuelGauge::update(float percent) {
    BarGaugeWidgets w = {container, value_label, bar, gauge_color};
    update_bar_gauge(w, percent, 20.0f, 15.0f, "%");
}
