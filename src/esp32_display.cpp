#ifdef ESP32

#include "esp32_display.h"
#include <Arduino.h>
#include <Wire.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_heap_caps.h"
#include "freertos/semphr.h"

static const int DISPLAY_H_RES = 1024;
static const int DISPLAY_V_RES = 600;

static esp_lcd_panel_handle_t rgb_panel = nullptr;
static lv_display_t *lv_disp = nullptr;
static SemaphoreHandle_t vsync_sem = nullptr;

static bool on_frame_trans_done(esp_lcd_panel_handle_t panel, esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_awoken = pdFALSE;
    xSemaphoreGiveFromISR(vsync_sem, &high_task_awoken);
    return (high_task_awoken == pdTRUE);
}

static void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    esp_lcd_panel_draw_bitmap(rgb_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    xSemaphoreTake(vsync_sem, portMAX_DELAY);
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
    panel_config.timings.pclk_hz = 16000000;
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
    panel_config.on_frame_trans_done = on_frame_trans_done;

    vsync_sem = xSemaphoreCreateBinary();

    esp_err_t result = esp_lcd_new_rgb_panel(&panel_config, &rgb_panel);
    if (result != ESP_OK) {
        Serial.printf("RGB panel init failed: %s\n", esp_err_to_name(result));
        return;
    }

    esp_lcd_panel_reset(rgb_panel);
    esp_lcd_panel_init(rgb_panel);
    esp_lcd_panel_disp_on_off(rgb_panel, true);

    Serial.printf("RGB panel initialized (PCLK=16MHz, under 22MHz PSRAM limit)\n");
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
