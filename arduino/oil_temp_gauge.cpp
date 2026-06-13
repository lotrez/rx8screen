#include "oil_temp_gauge.h"

void OilTempGauge::create(lv_obj_t *parent) {
    widgets = create_bar_gauge(parent, "OIL TEMP", 60.0f, 150.0f, "\xC2\xB0""C");
}

void OilTempGauge::update(float temp) {
    update_bar_gauge(widgets, temp, 110.0f, 120.0f, "\xC2\xB0""C");
}
