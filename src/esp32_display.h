#pragma once

#ifdef ESP32

#include <lvgl.h>

void esp32_display_init();
lv_display_t *esp32_display_get();

#endif
