#pragma once

#include <lvgl.h>
#include "gauge_common.h"

class OilTempGauge {
public:
    void create(lv_obj_t *parent);
    void update(float temp);
    lv_obj_t *get_container() { return widgets.container; }

private:
    BarGaugeWidgets widgets;
};
