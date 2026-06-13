#include "oil_temp_gauge.h"
#include "gauge_common.h"

void OilTempGauge::create(lv_obj_t *parent) {
    BarGaugeWidgets w = create_bar_gauge(parent, "OIL TEMP", 60.0f, 150.0f, "\xC2\xB0""C");
    container = w.container;
    value_label = w.value_label;
    unit_label = w.unit_label;
    bar = w.bar;
}

void OilTempGauge::update(float temp) {
    BarGaugeWidgets w = {container, value_label, unit_label, bar};
    update_bar_gauge(w, temp, 110.0f, 120.0f, "\xC2\xB0""C");
}
