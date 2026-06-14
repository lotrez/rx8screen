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
#include <math.h>

// Uncomment to skip BLE OBD2 and run with simulated driving data.
#define SIMULATE_DATA

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

// 7 gap rectangles for the high-RPM alarm blink. Each covers a strip of the
// grid padding/gaps between cards. Blinking only invalidates these small
// areas instead of the full screen.
static const int ALARM_GAP_COUNT = 7;
static lv_obj_t *alarm_gaps[ALARM_GAP_COUNT] = {};
static uint32_t alarm_last_toggle_ms = 0;
static bool alarm_visible = false;

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

    // High-RPM alarm blink: red rectangles in the gaps between cards.
    // Laid out to match the 8px padding/gaps of the grid below.
    // Column math: 1024 - 16 pad - 24 gaps = 984 content; cols 0-2 = 236 each, col 3 = 276.
    // Row math: 600 - 16 pad - 16 gaps = 568 content; rows 0,2 = 216 each, row 1 = 136.
    struct GapSpec { int x; int y; int w; int h; };
    static const GapSpec gap_specs[ALARM_GAP_COUNT] = {
        {0,   0,   1024, 8},   // top padding
        {0,   592, 1024, 8},   // bottom padding
        {0,   8,   8,    584}, // left padding
        {1016, 8,   8,    584}, // right padding
        {732, 8,   8,    584}, // vertical divider between col 2 and col 3
        {8,   224, 724,  8},   // horizontal divider between row 0 and row 1 (cols 0-2)
        {8,   368, 724,  8},   // horizontal divider between row 1 and row 2 (cols 0-2)
    };
    for (int gap_index = 0; gap_index < ALARM_GAP_COUNT; gap_index++) {
        lv_obj_t *gap = lv_obj_create(parent);
        lv_obj_remove_style_all(gap);
        lv_obj_set_pos(gap, gap_specs[gap_index].x, gap_specs[gap_index].y);
        lv_obj_set_size(gap, gap_specs[gap_index].w, gap_specs[gap_index].h);
        lv_obj_set_style_bg_color(gap, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_bg_opa(gap, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(gap, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(gap, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_background(gap);
        alarm_gaps[gap_index] = gap;
    }

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
// RX-8 Series 1 6-speed manual gear ratios
static const float GEAR_RATIO[NUM_GEARS] = {3.760f, 2.269f, 1.520f, 1.133f, 0.892f, 0.744f};
static const float FINAL_DRIVE = 4.300f;
// 225/45R18 stock tire: diameter ≈ 25.9 in → circumference ≈ 2.067 m
static const float TIRE_CIRCUMFERENCE_M = 2.067f;

static float rpm_for_gear_speed(int gear, float speed_kmh) {
    if (gear < 0 || gear >= NUM_GEARS) return 800.0f;
    float wheel_rpm = (speed_kmh / 3.6f) / TIRE_CIRCUMFERENCE_M * 60.0f;
    return wheel_rpm * GEAR_RATIO[gear] * FINAL_DRIVE;
}

#ifdef SIMULATE_DATA

// ─── Driving simulation (mock OBD2 data) ─────────────────────────
// Reuses the gear constants above. Time-based via lv_tick_get().
static const float SIM_SHIFT_RPM   = 10500.0f;
static const float SIM_IDLE_RPM    = 800.0f;
static const float SIM_REDLINE     = 11000.0f;

enum SimPhase { SP_IDLE, SP_REV_UP, SP_SHIFT, SP_ACCEL, SP_CRUISE, SP_DECEL, SP_BRAKE };

static SimPhase sim_phase = SP_IDLE;
static float    sim_phase_time = 0.0f;
static int      sim_gear = 0;
static float    sim_rpm = SIM_IDLE_RPM;
static float    sim_speed = 0.0f;
static float    sim_water = 82.0f;
static float    sim_fuel = 72.0f;
static uint32_t sim_last_ms = 0;
static uint32_t sim_jitter_tick = 0;

// OBD2 polling rate simulation.
// Fast PIDs (RPM, Speed) arrive every ~50ms; slow PIDs (coolant, battery, fuel) every ~1s.
// The driving sim below runs at full resolution; these hold the last "sampled" values
// so the gauges see the same discrete update cadence as real OBD2 data.
static const uint32_t SIM_FAST_PID_MS  = 50;
static const uint32_t SIM_SLOW_PID_MS  = 1000;
static uint32_t sim_last_fast_ms = 0;
static uint32_t sim_last_slow_ms = 0;
static float    sim_out_rpm = 0.0f;
static float    sim_out_speed = 0.0f;
static float    sim_out_water = 82.0f;
static float    sim_out_fuel = 72.0f;
static float    sim_out_voltage = 12.8f;

static float sim_speed_for_rpm(int gear, float rpm_val) {
    float wheel_rpm = rpm_val / (GEAR_RATIO[gear] * FINAL_DRIVE);
    return wheel_rpm * TIRE_CIRCUMFERENCE_M * 3.6f / 60.0f;
}

static float sim_jitter(float base_rpm) {
    sim_jitter_tick++;
    return base_rpm
         + sinf(sim_jitter_tick * 0.7f) * 15.0f
         + sinf(sim_jitter_tick * 1.3f) * 10.0f
         + sinf(sim_jitter_tick * 2.9f) * 5.0f;
}

static Obd2Data simulate_obd() {
    uint32_t now = lv_tick_get();
    if (sim_last_ms == 0) sim_last_ms = now;
    float dt = (now - sim_last_ms) / 1000.0f;
    sim_last_ms = now;
    if (dt > 0.1f) dt = 0.1f;
    sim_phase_time += dt;

    switch (sim_phase) {
        case SP_IDLE:
            sim_rpm = sim_jitter(SIM_IDLE_RPM);
            sim_speed = 0.0f;
            if (sim_phase_time > 1.5f) { sim_phase = SP_REV_UP; sim_phase_time = 0; }
            break;

        case SP_REV_UP: {
            float progress = fminf(sim_phase_time / 0.75f, 1.0f);
            sim_rpm = SIM_IDLE_RPM + (3000.0f - SIM_IDLE_RPM) * progress
                    + sinf(sim_jitter_tick * 3.0f) * 20.0f;
            sim_speed = 0.0f;
            if (sim_phase_time > 0.75f) {
                sim_phase = SP_ACCEL; sim_phase_time = 0;
                sim_gear = 0;
                sim_rpm = rpm_for_gear_speed(0, 5.0f);
            }
            break;
        }

        case SP_SHIFT: {
            float progress = sim_phase_time / 0.15f;
            sim_rpm = sim_rpm * (1.0f - progress * 0.4f)
                    + sinf(sim_jitter_tick * 8.0f) * 30.0f;
            if (sim_phase_time > 0.15f) { sim_phase = SP_ACCEL; sim_phase_time = 0; }
            break;
        }

        case SP_ACCEL: {
            float gain = 2800.0f * dt;
            if (sim_rpm > 6000.0f) gain *= 0.7f;
            if (sim_rpm > 8000.0f) gain *= 0.5f;
            sim_rpm += gain + sinf(sim_jitter_tick * 6.0f) * 8.0f;
            sim_speed = sim_speed_for_rpm(sim_gear, sim_rpm);
            if (sim_speed < 3.0f) sim_speed = 3.0f;
            if (sim_rpm >= SIM_SHIFT_RPM) {
                sim_phase = SP_SHIFT; sim_phase_time = 0;
                sim_gear++;
                if (sim_gear >= NUM_GEARS) {
                    sim_gear = NUM_GEARS - 1;
                    sim_phase = SP_CRUISE; sim_phase_time = 0;
                } else {
                    sim_rpm = rpm_for_gear_speed(sim_gear, sim_speed);
                    if (sim_rpm < SIM_IDLE_RPM) sim_rpm = SIM_IDLE_RPM;
                }
            }
            break;
        }

        case SP_CRUISE:
            sim_rpm = sim_jitter(sim_rpm);
            if (sim_phase_time > 2.5f) { sim_phase = SP_DECEL; sim_phase_time = 0; }
            break;

        case SP_DECEL:
            sim_speed -= 40.0f * dt;
            if (sim_speed < 60.0f) sim_speed -= 20.0f * dt;
            if (sim_speed < 20.0f) { sim_phase = SP_BRAKE; sim_phase_time = 0; }
            sim_rpm = rpm_for_gear_speed(sim_gear, sim_speed);
            if (sim_rpm < SIM_IDLE_RPM) {
                sim_rpm = sim_jitter(SIM_IDLE_RPM);
                if (sim_gear > 0) sim_gear--;
            }
            break;

        case SP_BRAKE:
            sim_speed -= 80.0f * dt;
            if (sim_speed < 0.0f) {
                sim_speed = 0.0f;
                sim_rpm = sim_jitter(SIM_IDLE_RPM);
                if (sim_phase_time > 0.5f) { sim_phase = SP_IDLE; sim_phase_time = 0; }
            } else {
                sim_rpm = rpm_for_gear_speed(0, sim_speed);
                if (sim_rpm < SIM_IDLE_RPM) sim_rpm = sim_jitter(SIM_IDLE_RPM);
            }
            break;
    }

    if (sim_rpm < 500.0f) sim_rpm = 500.0f;
    if (sim_rpm > SIM_REDLINE + 500.0f) sim_rpm = SIM_REDLINE + 500.0f;

    sim_water += (92.0f - sim_water) * 0.001f;
    sim_fuel -= 0.002f * dt;
    if (sim_fuel < 5.0f) sim_fuel = 5.0f;

    float sim_voltage = 12.8f + (sim_rpm > 1000.0f ? 1.4f : 0.0f)
                       + sinf(sim_jitter_tick * 0.05f) * 0.2f;

    // Sample at OBD2 polling rates — gauges see discrete jumps, not continuous values.
    if (now - sim_last_fast_ms >= SIM_FAST_PID_MS || sim_last_fast_ms == 0) {
        sim_out_rpm = sim_rpm;
        sim_out_speed = sim_speed;
        sim_last_fast_ms = now;
    }
    if (now - sim_last_slow_ms >= SIM_SLOW_PID_MS || sim_last_slow_ms == 0) {
        sim_out_water = sim_water;
        sim_out_fuel = sim_fuel;
        sim_out_voltage = sim_voltage;
        sim_last_slow_ms = now;
    }

    Obd2Data data;
    data.rpm = sim_out_rpm;
    data.speed = sim_out_speed;
    data.coolant_temp = sim_out_water;
    data.fuel_level = sim_out_fuel;
    data.battery_voltage = sim_out_voltage;
    data.connected = true;
    return data;
}

#endif // SIMULATE_DATA

static void update_alarm(float rpm) {
    static const float ALARM_RPM = 9000.0f;
    static const uint32_t BLINK_PERIOD_MS = 200;

    if (rpm > ALARM_RPM) {
        uint32_t now = lv_tick_get();
        if (now - alarm_last_toggle_ms >= BLINK_PERIOD_MS) {
            alarm_last_toggle_ms = now;
            alarm_visible = !alarm_visible;
            for (int gap_index = 0; gap_index < ALARM_GAP_COUNT; gap_index++) {
                if (alarm_gaps[gap_index]) {
                    lv_obj_set_style_bg_opa(alarm_gaps[gap_index], alarm_visible ? LV_OPA_70 : LV_OPA_TRANSP, 0);
                }
            }
        }
    } else if (alarm_visible) {
        alarm_visible = false;
        for (int gap_index = 0; gap_index < ALARM_GAP_COUNT; gap_index++) {
            if (alarm_gaps[gap_index]) {
                lv_obj_set_style_bg_opa(alarm_gaps[gap_index], LV_OPA_TRANSP, 0);
            }
        }
    }
}

static void update_all_gauges(const Obd2Data &d) {
    rpm_gauge.update(d.rpm);
    speed_gauge.update(d.speed);
    update_alarm(d.rpm);

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
}

static void show_connecting_screen() {
    if (showing_dashboard) {
        lv_screen_load(connecting_screen.get_screen());
        connecting_screen.start_animation();
        showing_dashboard = false;
    }
}

static void show_dashboard() {
    if (!showing_dashboard) {
        connecting_screen.stop_animation();
        lv_screen_load(dashboard_screen);
        showing_dashboard = true;
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }
    delay(1000);
    Serial.println("====================");
#ifdef SIMULATE_DATA
    Serial.println("RX-8 Dashboard [SIM]");
#else
    Serial.println("RX-8 Dashboard v3.1");
#endif
    Serial.println("====================");

    Serial.println("lv_init()...");
    lv_init();

    Serial.println("esp32_display_init()...");
    esp32_display_init();

    if (!esp32_display_get()) {
        Serial.println("FATAL: display init failed, halting");
        while (1) { delay(1000); }
    }

    dashboard_screen = lv_obj_create(NULL);
    create_dashboard(dashboard_screen);

#ifdef SIMULATE_DATA
    lv_screen_load(dashboard_screen);
    showing_dashboard = true;
    Serial.println("=== READY [SIM] ===");
#else
    Serial.println("create connecting screen...");
    lv_obj_t *connecting_root = lv_obj_create(NULL);
    connecting_screen.create(connecting_root);
    connecting_screen.start_animation();
    lv_screen_load(connecting_root);
    showing_dashboard = false;

    // Flush to display so user sees the screen before BLE blocks
    for (int i = 0; i < 3; i++) {
        lv_timer_handler();
        lv_tick_inc(5);
        delay(5);
    }

    Serial.println("obd2.begin()...");
    obd2.begin();

    for (int attempt = 0; attempt < 3; attempt++) {
        Serial.printf("Connect attempt %d/3\n", attempt + 1);
        if (obd2.connect_blocking()) break;
        delay(3000);
    }

    Serial.println("=== READY ===");
#endif
}

// Uncomment to print loop FPS + OBD2 state every 5 seconds via Serial.
// #define DEBUG_TIMING

void loop() {
    static uint32_t last_tick = millis();
    uint32_t now = millis();
    uint32_t elapsed = now - last_tick;
    last_tick = now;

#ifdef DEBUG_TIMING
    static uint32_t frame_count = 0;
    static uint32_t last_fps_ms = now;
    frame_count++;
    if (now - last_fps_ms >= 5000) {
#ifdef SIMULATE_DATA
        Serial.printf("[TIMING] %.0f fps [SIM]\n",
                      (double)frame_count * 1000.0 / (now - last_fps_ms));
#else
        Serial.printf("[TIMING] %.0f fps, state=%s\n",
                      (double)frame_count * 1000.0 / (now - last_fps_ms),
                      obd2.get_state_name());
#endif
        frame_count = 0;
        last_fps_ms = now;
    }
#endif

#ifdef SIMULATE_DATA
    show_dashboard();
    update_all_gauges(simulate_obd());
#else
    obd2.loop();

    if (obd2.is_connected()) {
        show_dashboard();
        update_all_gauges(obd2.get_data());
    } else {
        show_connecting_screen();
        const char *detail = obd2.get_status_detail();
        if (detail && detail[0]) {
            char buf[80];
            snprintf(buf, sizeof(buf), "%s | %s", obd2.get_state_name(), detail);
            connecting_screen.set_status(buf);
        } else {
            connecting_screen.set_status(obd2.get_state_name());
        }
    }
#endif

    rpm_gauge.tick();
    speed_gauge.tick();

    lv_timer_handler();
    lv_tick_inc(elapsed);
}
