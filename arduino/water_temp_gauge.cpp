#include "water_temp_gauge.h"

void WaterTempGauge::create(lv_obj_t *parent) {
    widgets = create_bar_gauge(parent, "WATER TEMP", 60.0f, 130.0f, "\xC2\xB0""C");
}

void WaterTempGauge::update(float temp) {
    update_bar_gauge(widgets, temp, 100.0f, 105.0f, "\xC2\xB0""C");
}
