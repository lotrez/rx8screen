#include "esp32_display.h"
#include <Arduino.h>
#include <Wire.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_heap_caps.h"

static const int DISPLAY_H_RES = 1024;
static const int DISPLAY_V_RES = 600;

static esp_lcd_panel_handle_t rgb_panel = nullptr;
static lv_display_t *lv_disp = nullptr;
static void *panel_fb = nullptr;

// Partial mode render buffer: 200 lines in PSRAM (~410KB for 1024-wide RGB565).
// Must be large enough to fit the tallest card (RPM card ~216px) in ≤2 chunks.
// 50 lines caused the bottom half of large cards to drop — LVGL couldn't
// finish all 5 partial flushes before the next invalidation.
static const int RENDER_BUF_LINES = 200;
static void *render_buf = nullptr;

static void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    if (area->x2 < 0 || area->y2 < 0 || area->x1 >= DISPLAY_H_RES || area->y1 >= DISPLAY_V_RES) {
        lv_display_flush_ready(disp);
        return;
    }

    // In PARTIAL mode, LVGL rendered dirty pixels into px_map (render_buf).
    // Copy just that rectangle to the panel's framebuffer.
    // The RGB panel driver handles cache coherency.
    esp_err_t result = esp_lcd_panel_draw_bitmap(
        rgb_panel,
        area->x1,
        area->y1,
        area->x2 + 1,
        area->y2 + 1,
        px_map
    );

    if (result != ESP_OK) {
        Serial.printf("draw_bitmap failed: %s\n", esp_err_to_name(result));
    }

    lv_display_flush_ready(disp);
}

static void init_ch422g() {
    Wire.begin(8, 9);
    Wire.beginTransmission(0x24);
    Wire.write(0x02);
    Wire.write(0xFF);
    Wire.endTransmission();
    Wire.beginTransmission(0x24);
    Wire.write(0x03);
    Wire.write(0xFF);
    Wire.endTransmission();
}

static void init_rgb_panel() {
    esp_lcd_rgb_panel_config_t panel_config = {};
    panel_config.clk_src = LCD_CLK_SRC_DEFAULT;
    panel_config.timings.pclk_hz = 15000000;   // 15MHz — reference uses this, prevents drift
    panel_config.timings.h_res = DISPLAY_H_RES;
    panel_config.timings.v_res = DISPLAY_V_RES;
    panel_config.timings.hsync_pulse_width = 162;
    panel_config.timings.hsync_back_porch = 152;
    panel_config.timings.hsync_front_porch = 48;
    panel_config.timings.vsync_pulse_width = 45;
    panel_config.timings.vsync_back_porch = 13;
    panel_config.timings.vsync_front_porch = 3;
    panel_config.timings.flags.pclk_active_neg = 1;
    panel_config.data_width = 16;
    panel_config.bits_per_pixel = 16;
    panel_config.num_fbs = 1;                  // Single framebuffer for the panel
    // Bounce buffer: 10 lines in SRAM (reference uses this)
    panel_config.bounce_buffer_size_px = 10 * DISPLAY_H_RES;
    panel_config.psram_trans_align = 64;
    panel_config.hsync_gpio_num = 46;
    panel_config.vsync_gpio_num = 3;
    panel_config.de_gpio_num = 5;
    panel_config.pclk_gpio_num = 7;
    panel_config.disp_gpio_num = GPIO_NUM_NC;

    panel_config.data_gpio_nums[0]  = 14;
    panel_config.data_gpio_nums[1]  = 38;
    panel_config.data_gpio_nums[2]  = 18;
    panel_config.data_gpio_nums[3]  = 17;
    panel_config.data_gpio_nums[4]  = 10;
    panel_config.data_gpio_nums[5]  = 39;
    panel_config.data_gpio_nums[6]  = 0;
    panel_config.data_gpio_nums[7]  = 45;
    panel_config.data_gpio_nums[8]  = 48;
    panel_config.data_gpio_nums[9]  = 47;
    panel_config.data_gpio_nums[10] = 21;
    panel_config.data_gpio_nums[11] = 1;
    panel_config.data_gpio_nums[12] = 2;
    panel_config.data_gpio_nums[13] = 42;
    panel_config.data_gpio_nums[14] = 41;
    panel_config.data_gpio_nums[15] = 40;

    panel_config.flags.fb_in_psram = 1;

    esp_err_t result = esp_lcd_new_rgb_panel(&panel_config, &rgb_panel);
    if (result != ESP_OK || !rgb_panel) {
        Serial.printf("RGB panel init failed: %s\n", esp_err_to_name(result));
        return;
    }

    esp_lcd_panel_reset(rgb_panel);
    esp_lcd_panel_init(rgb_panel);
    // esp_lcd_panel_disp_on_off not supported on RGB panels, skip it

    Serial.printf("RGB panel OK: PCLK=15MHz, bounce=10 lines, num_fbs=1\n");
}

void esp32_display_init() {
    Serial.println("Init CH422G...");
    init_ch422g();

    Serial.println("Init RGB panel...");
    init_rgb_panel();
    if (!rgb_panel) {
        Serial.println("FATAL: RGB panel init failed");
        return;
    }

    Serial.println("Getting panel framebuffer...");
    esp_err_t result = esp_lcd_rgb_panel_get_frame_buffer(rgb_panel, 1, &panel_fb);
    if (result != ESP_OK || !panel_fb) {
        Serial.printf("FATAL: get_frame_buffer failed: %s\n", esp_err_to_name(result));
        return;
    }
    Serial.printf("Panel framebuffer: %p\n", panel_fb);

    Serial.println("Allocating LVGL render buffer...");
    size_t render_buf_size = DISPLAY_H_RES * RENDER_BUF_LINES * 2;
    render_buf = heap_caps_malloc(render_buf_size, MALLOC_CAP_SPIRAM);
    if (!render_buf) {
        Serial.println("FATAL: render buffer alloc failed");
        return;
    }
    Serial.printf("Render buffer: %p size=%u bytes (%d lines)\n", render_buf, (unsigned)render_buf_size, RENDER_BUF_LINES);

    Serial.println("Creating LVGL display...");
    lv_disp = lv_display_create(DISPLAY_H_RES, DISPLAY_V_RES);
    lv_display_set_color_format(lv_disp, LV_COLOR_FORMAT_RGB565);

    lv_display_set_buffers(lv_disp, render_buf, nullptr, render_buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lv_disp, flush_callback);

    Serial.println("Display initialized OK (PARTIAL mode)");
}

lv_display_t *esp32_display_get() {
    return lv_disp;
}
