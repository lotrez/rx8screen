#pragma once

#include <lvgl.h>
#include "gauge_common.h"

class VoltageGauge {
public:
    void create(lv_obj_t *parent);
    void update(float volts);
    lv_obj_t *get_container() { return widgets.container; }

private:
    BarGaugeWidgets widgets;
};
