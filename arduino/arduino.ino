// RX-8 ESP32-S3 OBD2 Dashboard — Arduino entry point

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

#include <string.h>
#include <stdio.h>

static ConnectingScreen connecting_screen;
static lv_obj_t *dashboard_screen = nullptr;
static bool dashboard_active = false;

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

static uint32_t sim_tick = 0;

static const int NUM_GEARS = 6;
static const float GEAR_RATIO[NUM_GEARS] = {3.542f, 2.242f, 1.625f, 1.250f, 1.000f, 0.818f};
static const float FINAL_DRIVE = 4.777f;
static const float TIRE_CIRCUMFERENCE_M = 2.05f;
static const float SHIFT_RPM = 10500.0f;
static const float IDLE_RPM = 800.0f;
static const float REDLINE = 11000.0f;

enum SimPhase {
    SIM_IDLE,
    SIM_REV_UP,
    SIM_SHIFT,
    SIM_ACCEL,
    SIM_CRUISE,
    SIM_DECEL,
    SIM_BRAKE
};

static SimPhase sim_phase = SIM_IDLE;
static int sim_phase_tick = 0;
static int sim_current_gear = 0;
static float sim_rpm = IDLE_RPM;
static float sim_speed = 0.0f;

static float rpm_for_gear_speed(int gear, float speed_kmh) {
    if (gear < 0 || gear >= NUM_GEARS) return IDLE_RPM;
    float wheel_rpm = (speed_kmh / 3.6f) / TIRE_CIRCUMFERENCE_M * 60.0f;
    return wheel_rpm * GEAR_RATIO[gear] * FINAL_DRIVE;
}

static float speed_for_gear_rpm(int gear, float rpm_val) {
    if (gear < 0 || gear >= NUM_GEARS) return 0.0f;
    float wheel_rpm = rpm_val / (GEAR_RATIO[gear] * FINAL_DRIVE);
    return wheel_rpm * TIRE_CIRCUMFERENCE_M * 3.6f / 60.0f;
}

static float engine_jitter(float base_rpm, uint32_t tick) {
    float jitter = sinf(tick * 0.7f) * 15.0f
                 + sinf(tick * 1.3f) * 10.0f
                 + sinf(tick * 2.9f) * 5.0f
                 + sinf(tick * 4.1f) * 3.0f;
    return base_rpm + jitter;
}

static void update_simulation() {
    sim_tick++;
    sim_phase_tick++;

    float dt = 1.0f / 120.0f;

    switch (sim_phase) {
        case SIM_IDLE: {
            sim_rpm = engine_jitter(IDLE_RPM, sim_tick);
            sim_speed = 0.0f;
            if (sim_phase_tick > 180) {
                sim_phase = SIM_REV_UP;
                sim_phase_tick = 0;
            }
            break;
        }
        case SIM_REV_UP: {
            float progress = sim_phase_tick / 90.0f;
            float target_rpm = IDLE_RPM + (3000.0f - IDLE_RPM) * fminf(progress, 1.0f);
            sim_rpm = target_rpm + sinf(sim_tick * 3.0f) * 20.0f;
            sim_speed = 0.0f;
            if (sim_phase_tick > 90) {
                sim_phase = SIM_ACCEL;
                sim_phase_tick = 0;
                sim_current_gear = 0;
                sim_rpm = rpm_for_gear_speed(0, 5.0f);
            }
            break;
        }
        case SIM_SHIFT: {
            float progress = sim_phase_tick / 18.0f;
            float rpm_drop = sim_rpm * (1.0f - progress * 0.4f);
            sim_rpm = rpm_drop + sinf(sim_tick * 8.0f) * 30.0f;
            if (sim_phase_tick > 18) {
                sim_phase = SIM_ACCEL;
                sim_phase_tick = 0;
            }
            break;
        }
        case SIM_ACCEL: {
            float rpm_gain = 2800.0f * dt;
            if (sim_rpm > 6000.0f) rpm_gain *= 0.7f;
            if (sim_rpm > 8000.0f) rpm_gain *= 0.5f;
            sim_rpm += rpm_gain + sinf(sim_tick * 6.0f) * 8.0f;
            sim_speed = speed_for_gear_rpm(sim_current_gear, sim_rpm);
            if (sim_speed < 3.0f) sim_speed = 3.0f;

            if (sim_rpm >= SHIFT_RPM) {
                sim_phase = SIM_SHIFT;
                sim_phase_tick = 0;
                sim_current_gear++;
                if (sim_current_gear >= NUM_GEARS) {
                    sim_current_gear = NUM_GEARS - 1;
                    sim_phase = SIM_CRUISE;
                    sim_phase_tick = 0;
                } else {
                    sim_rpm = rpm_for_gear_speed(sim_current_gear, sim_speed);
                    if (sim_rpm < IDLE_RPM) sim_rpm = IDLE_RPM;
                }
            }
            break;
        }
        case SIM_CRUISE: {
            sim_rpm = engine_jitter(sim_rpm, sim_tick);
            if (sim_phase_tick > 300) {
                sim_phase = SIM_DECEL;
                sim_phase_tick = 0;
            }
            break;
        }
        case SIM_DECEL: {
            sim_speed -= 40.0f * dt;
            if (sim_speed < 60.0f) sim_speed -= 20.0f * dt;
            if (sim_speed < 20.0f) {
                sim_phase = SIM_BRAKE;
                sim_phase_tick = 0;
            }
            sim_rpm = rpm_for_gear_speed(sim_current_gear, sim_speed);
            if (sim_rpm < IDLE_RPM) {
                sim_rpm = engine_jitter(IDLE_RPM, sim_tick);
                if (sim_current_gear > 0) sim_current_gear--;
            }
            break;
        }
        case SIM_BRAKE: {
            sim_speed -= 80.0f * dt;
            if (sim_speed < 0.0f) {
                sim_speed = 0.0f;
                sim_rpm = engine_jitter(IDLE_RPM, sim_tick);
                if (sim_phase_tick > 60) {
                    sim_phase = SIM_IDLE;
                    sim_phase_tick = 0;
                }
            } else {
                sim_rpm = rpm_for_gear_speed(0, sim_speed);
                if (sim_rpm < IDLE_RPM) sim_rpm = engine_jitter(IDLE_RPM, sim_tick);
            }
            break;
        }
    }

    if (sim_rpm < 500.0f) sim_rpm = 500.0f;
    if (sim_rpm > REDLINE + 500.0f) sim_rpm = REDLINE + 500.0f;

    rpm_gauge.update(sim_rpm);
    speed_gauge.update(sim_speed);
    gear_indicator.update(sim_current_gear);

    float sim_water_temp = 82.0f + sim_phase_tick * 0.02f;
    if (sim_water_temp > 105.0f) sim_water_temp = 105.0f - (sim_water_temp - 105.0f) * 0.5f;
    if (sim_water_temp > 110.0f) sim_water_temp = 110.0f;
    water_temp_gauge.update(sim_water_temp);

    float sim_voltage = 12.8f + (sim_rpm > 1000.0f ? 1.4f : 0.0f) + sinf(sim_tick * 0.05f) * 0.2f;
    voltage_gauge.update(sim_voltage);

    float sim_fuel = 72.0f - sim_tick * 0.005f;
    if (sim_fuel < 5.0f) sim_fuel = 5.0f;
    fuel_gauge.update(sim_fuel);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }  // Wait for serial port
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

    Serial.println("create_dashboard()...");
    dashboard_screen = lv_obj_create(NULL);
    create_dashboard(dashboard_screen);
    lv_screen_load(dashboard_screen);
    dashboard_active = true;

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

    if (dashboard_active) {
        if (obd2.is_connected()) {
            const Obd2Data &d = obd2.get_data();
            rpm_gauge.update(d.rpm);
            speed_gauge.update(d.speed);
            // Infer gear from speed and RPM
            int inferred_gear = 0;
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
            } else {
                inferred_gear = -1;  // Neutral
            }
            gear_indicator.update(inferred_gear);
            water_temp_gauge.update(d.coolant_temp);
            voltage_gauge.update(d.battery_voltage);
            fuel_gauge.update(d.fuel_level);
        } else {
            update_simulation();
        }
    }

    lv_timer_handler();
    lv_tick_inc(elapsed);
}
