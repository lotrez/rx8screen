// RX-8 ESP32-S3 OBD2 Dashboard — Arduino entry point
// Shows connecting screen until BLE OBD2 adapter is linked, then real data only.

#define DISPLAY_WIDTH 1024
#define DISPLAY_HEIGHT 600

#include <Arduino.h>
#include <lvgl.h>
#include <Wire.h>
#include "esp32_display.h"
#include "rpm_gauge.h"
#include "speed_gauge.h"
#include "gear_indicator.h"
#include "gauge_common.h"
#include "water_temp_gauge.h"
#include "fuel_gauge.h"
#include "voltage_gauge.h"
#include "connecting_screen.h"
#include "obd2_ble.h"

static ConnectingScreen connecting_screen;
static lv_obj_t *dashboard_screen = nullptr;
static bool showing_dashboard = false;

static RpmGauge rpm_gauge;
static SpeedGauge speed_gauge;
static GearIndicator gear_indicator;
static WaterTempGauge water_temp_gauge;
static FuelGauge fuel_gauge;
static VoltageGauge voltage_gauge;
static OBD2BLE obd2;

static lv_obj_t *card_rpm;
static lv_obj_t *card_speed;
static lv_obj_t *card_gear;

static lv_obj_t *create_card(lv_obj_t *parent) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, COLOR_GRID_BORDER, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static void create_dashboard(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    static lv_coord_t col_dsc[] = {LV_GRID_FR(24), LV_GRID_FR(24), LV_GRID_FR(24), LV_GRID_FR(28), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_FR(38), LV_GRID_FR(24), LV_GRID_FR(38), LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_all(grid, 8, 0);
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_set_style_pad_column(grid, 8, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    card_rpm = create_card(grid);
    lv_obj_set_style_pad_all(card_rpm, 0, 0);
    lv_obj_set_style_clip_corner(card_rpm, true, 0);
    lv_obj_set_grid_cell(card_rpm, LV_GRID_ALIGN_STRETCH, 0, 3, LV_GRID_ALIGN_STRETCH, 0, 1);

    lv_obj_t *card_water = create_card(grid);
    lv_obj_set_grid_cell(card_water, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    water_temp_gauge.create(card_water);

    lv_obj_t *card_voltage = create_card(grid);
    lv_obj_set_grid_cell(card_voltage, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    voltage_gauge.create(card_voltage);

    lv_obj_t *card_fuel = create_card(grid);
    lv_obj_set_grid_cell(card_fuel, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    fuel_gauge.create(card_fuel);

    card_speed = create_card(grid);
    lv_obj_set_grid_cell(card_speed, LV_GRID_ALIGN_STRETCH, 0, 3, LV_GRID_ALIGN_STRETCH, 2, 1);

    card_gear = create_card(grid);
    lv_obj_set_grid_cell(card_gear, LV_GRID_ALIGN_STRETCH, 3, 1, LV_GRID_ALIGN_STRETCH, 0, 3);

    rpm_gauge.create(card_rpm);
    lv_obj_set_size(rpm_gauge.get_container(), LV_PCT(100), LV_PCT(100));

    speed_gauge.create(card_speed);

    gear_indicator.create(card_gear);
    lv_obj_align(gear_indicator.get_container(), LV_ALIGN_CENTER, 0, 0);
}

static const int NUM_GEARS = 6;
static const float GEAR_RATIO[NUM_GEARS] = {3.542f, 2.242f, 1.625f, 1.250f, 1.000f, 0.818f};
static const float FINAL_DRIVE = 4.777f;
static const float TIRE_CIRCUMFERENCE_M = 2.05f;

static float rpm_for_gear_speed(int gear, float speed_kmh) {
    if (gear < 0 || gear >= NUM_GEARS) return 800.0f;
    float wheel_rpm = (speed_kmh / 3.6f) / TIRE_CIRCUMFERENCE_M * 60.0f;
    return wheel_rpm * GEAR_RATIO[gear] * FINAL_DRIVE;
}

static void show_connecting_screen() {
    if (showing_dashboard) {
        lv_screen_load(connecting_screen.get_screen());
        showing_dashboard = false;
    }
}

static void show_dashboard() {
    if (!showing_dashboard) {
        lv_screen_load(dashboard_screen);
        showing_dashboard = true;
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }
    delay(1000);
    Serial.println("====================");
    Serial.println("RX-8 Dashboard v3.1");
    Serial.println("====================");

    Serial.println("lv_init()...");
    lv_init();

    Serial.println("esp32_display_init()...");
    esp32_display_init();

    if (!esp32_display_get()) {
        Serial.println("FATAL: display init failed, halting");
        while (1) { delay(1000); }
    }

    Serial.println("create connecting screen...");
    lv_obj_t *connecting_root = lv_obj_create(NULL);
    connecting_screen.create(connecting_root);
    connecting_screen.start_animation();
    lv_screen_load(connecting_root);
    showing_dashboard = false;

    Serial.println("create dashboard (hidden)...");
    dashboard_screen = lv_obj_create(NULL);
    create_dashboard(dashboard_screen);

    Serial.println("obd2.begin()...");
    obd2.begin();

    Serial.println("=== READY ===");
}

void loop() {
    static uint32_t last_tick = millis();
    uint32_t now = millis();
    uint32_t elapsed = now - last_tick;
    last_tick = now;

    obd2.loop();

    if (obd2.is_connected()) {
        show_dashboard();

        const Obd2Data &d = obd2.get_data();
        rpm_gauge.update(d.rpm);
        speed_gauge.update(d.speed);

        int inferred_gear = -1;
        if (d.speed > 5.0f && d.rpm > 1000.0f) {
            float best_diff = 999999.0f;
            for (int g = 0; g < NUM_GEARS; g++) {
                float expected_rpm = rpm_for_gear_speed(g, d.speed);
                float diff = fabsf(expected_rpm - d.rpm);
                if (diff < best_diff) {
                    best_diff = diff;
                    inferred_gear = g;
                }
            }
        }
        gear_indicator.update(inferred_gear);

        water_temp_gauge.update(d.coolant_temp);
        voltage_gauge.update(d.battery_voltage);
        fuel_gauge.update(d.fuel_level);
    } else {
        show_connecting_screen();
    }

    lv_timer_handler();
    lv_tick_inc(elapsed);
}
