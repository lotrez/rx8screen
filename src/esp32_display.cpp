#ifdef ESP32

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

static void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    int x_start = area->x1;
    int y_start = area->y1;
    int x_end = area->x2 + 1;
    int y_end = area->y2 + 1;
    esp_lcd_panel_draw_bitmap(rgb_panel, x_start, y_start, x_end, y_end, px_map);
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
    panel_config.clk_src = LCD_CLK_SRC_PLL240M;
    panel_config.timings.pclk_hz = 30000000;
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
    panel_config.psram_trans_align = 64;
    panel_config.hsync_gpio_num = 46;
    panel_config.vsync_gpio_num = 3;
    panel_config.de_gpio_num = 5;
    panel_config.pclk_gpio_num = 7;
    panel_config.disp_gpio_num = GPIO_NUM_NC;

    panel_config.data_gpio_nums[0]  = 14;  // B0 (labeled B3)
    panel_config.data_gpio_nums[1]  = 38;  // B1 (labeled B4)
    panel_config.data_gpio_nums[2]  = 18;  // B2 (labeled B5)
    panel_config.data_gpio_nums[3]  = 17;  // B3 (labeled B6)
    panel_config.data_gpio_nums[4]  = 10;  // B4 (labeled B7)
    panel_config.data_gpio_nums[5]  = 39;  // G0 (labeled G2)
    panel_config.data_gpio_nums[6]  = 0;   // G1 (labeled G3)
    panel_config.data_gpio_nums[7]  = 45;  // G2 (labeled G4)
    panel_config.data_gpio_nums[8]  = 48;  // G3 (labeled G5)
    panel_config.data_gpio_nums[9]  = 47;  // G4 (labeled G6)
    panel_config.data_gpio_nums[10] = 21;  // G5 (labeled G7)
    panel_config.data_gpio_nums[11] = 1;   // R0 (labeled R3)
    panel_config.data_gpio_nums[12] = 2;   // R1 (labeled R4)
    panel_config.data_gpio_nums[13] = 42;  // R2 (labeled R5)
    panel_config.data_gpio_nums[14] = 41;  // R3 (labeled R6)
    panel_config.data_gpio_nums[15] = 40;  // R4 (labeled R7)

    panel_config.flags.fb_in_psram = 1;

    esp_err_t result = esp_lcd_new_rgb_panel(&panel_config, &rgb_panel);
    if (result != ESP_OK) {
        Serial.printf("RGB panel init failed: %s\n", esp_err_to_name(result));
        return;
    }

    esp_lcd_panel_reset(rgb_panel);
    esp_lcd_panel_init(rgb_panel);

    // esp_lcd_panel_disp_on_off returns ESP_ERR_NOT_SUPPORTED on RGB panels (ESP-IDF 4.4)
    esp_lcd_panel_disp_on_off(rgb_panel, true);
}

void esp32_display_init() {
    Serial.println("Initializing CH422G IO expander...");
    init_ch422g();

    Serial.println("Initializing RGB LCD panel...");
    init_rgb_panel();

    Serial.println("Creating LVGL display...");
    const uint32_t buf_size = DISPLAY_H_RES * DISPLAY_V_RES * 2;
    uint8_t *draw_buf = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!draw_buf) {
        Serial.println("Failed to allocate LVGL draw buffer from PSRAM!");
        return;
    }
    memset(draw_buf, 0, buf_size);

    lv_disp = lv_display_create(DISPLAY_H_RES, DISPLAY_V_RES);
    lv_display_set_color_format(lv_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(lv_disp, draw_buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(lv_disp, flush_callback);

    Serial.printf("Display initialized: %dx%d RGB565, draw buffer %u bytes in PSRAM\n",
                  DISPLAY_H_RES, DISPLAY_V_RES, (unsigned)buf_size);
}

lv_display_t *esp32_display_get() {
    return lv_disp;
}

#endif
