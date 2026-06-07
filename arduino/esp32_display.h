#pragma once

#ifdef ESP32

#include <lvgl.h>

void esp32_display_init();
lv_display_t *esp32_display_get();

// Call after lv_timer_handler() to push the composited frame to the display.
// Returns true if a frame was actually sent.
bool esp32_display_flush_frame();

uint32_t esp32_display_get_flush_count();

#endif
