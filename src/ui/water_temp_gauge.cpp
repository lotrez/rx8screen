#include "water_temp_gauge.h"
#include "gauge_common.h"

void WaterTempGauge::create(lv_obj_t *parent) {
    BarGaugeWidgets w = create_bar_gauge(parent, "WATER TEMP", 60.0f, 130.0f, "\xC2\xB0""C");
    container = w.container;
    value_label = w.value_label;
    bar = w.bar;
}

void WaterTempGauge::update(float temp) {
    BarGaugeWidgets w = {container, value_label, bar};
    update_bar_gauge(w, temp, 100.0f, 105.0f, "\xC2\xB0""C");
}
