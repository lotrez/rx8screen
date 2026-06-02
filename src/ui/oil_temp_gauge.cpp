#include "oil_temp_gauge.h"
#include "gauge_common.h"

void OilTempGauge::create(lv_obj_t *parent) {
    BarGaugeWidgets w = create_bar_gauge(parent, "OIL TEMP", 60.0f, 150.0f, "\xC2\xB0""C",
                                          lv_color_hex(0xE53935));
    container = w.container;
    value_label = w.value_label;
    bar = w.bar;
}

void OilTempGauge::update(float temp) {
    BarGaugeWidgets w = {container, value_label, bar, lv_color_hex(0xE53935)};
    update_bar_gauge(w, temp, 110.0f, 120.0f, "\xC2\xB0""C");
}
