#include <lvgl.h>
#include <math.h>
#include <src/drivers/sdl/lv_sdl_window.h>
#include <SDL.h>
#include "ui/rpm_gauge.h"
#include "ui/speed_gauge.h"
#include "ui/gear_indicator.h"
// #include "ui/water_temp_gauge.h"
// #include "ui/oil_temp_gauge.h"
// #include "ui/fuel_gauge.h"
// #include "ui/voltage_gauge.h"

#include <string.h>
#include <stdio.h>

static RpmGauge rpm_gauge;
static SpeedGauge speed_gauge;
static GearIndicator gear_indicator;

static void create_dashboard(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    speed_gauge.create(parent);
    lv_obj_set_size(speed_gauge.get_container(), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(speed_gauge.get_container(), LV_ALIGN_TOP_LEFT, 26, 25);

    gear_indicator.create(parent);
    lv_obj_align(gear_indicator.get_container(), LV_ALIGN_CENTER, 256, -38);

    rpm_gauge.create(parent);

    // --- Old grid layout (preserved for when other gauges return) ---
    // static lv_coord_t col_dsc[] = {LV_PCT(50), LV_PCT(50), LV_GRID_TEMPLATE_LAST};
    // static lv_coord_t row_dsc[] = {LV_PCT(58), LV_PCT(21), LV_PCT(21), LV_GRID_TEMPLATE_LAST};
    // lv_obj_t *grid = lv_obj_create(parent);
    // lv_obj_remove_style_all(grid);
    // lv_obj_set_size(grid, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    // lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    // lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    // lv_obj_set_style_pad_all(grid, 4, 0);
    // lv_obj_set_style_pad_row(grid, 4, 0);
    // lv_obj_set_style_pad_column(grid, 4, 0);
    // lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    // rpm_gauge.create(grid);
    // lv_obj_set_grid_cell(rpm_gauge.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    // lv_obj_t *top_right = lv_obj_create(grid);
    // lv_obj_set_grid_cell(top_right, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    // lv_obj_set_style_bg_opa(top_right, LV_OPA_TRANSP, 0);
    // lv_obj_set_style_border_width(top_right, 0, 0);
    // lv_obj_set_style_pad_all(top_right, 0, 0);
    // lv_obj_clear_flag(top_right, LV_OBJ_FLAG_SCROLLABLE);
    // speed_gauge.create(top_right);
    // water_temp_gauge.create(top_right);
    // water_temp_gauge.update(60);
    // oil_temp_gauge.create(grid);
    // lv_obj_set_grid_cell(oil_temp_gauge.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    // oil_temp_gauge.update(60);
    // fuel_gauge.create(grid);
    // lv_obj_set_grid_cell(fuel_gauge.get_container(), LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    // fuel_gauge.update(50);
    // voltage_gauge.create(grid);
    // lv_obj_set_grid_cell(voltage_gauge.get_container(), LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 2, 1);
    // voltage_gauge.update(12.0);
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
            if (sim_rpm > 6000.0f) {
                rpm_gain *= 0.7f;
            }
            if (sim_rpm > 8000.0f) {
                rpm_gain *= 0.5f;
            }
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
            if (sim_speed < 60.0f) {
                sim_speed -= 20.0f * dt;
            }
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
                if (sim_rpm < IDLE_RPM) {
                    sim_rpm = engine_jitter(IDLE_RPM, sim_tick);
                }
            }
            break;
        }
    }

    if (sim_rpm < 500.0f) sim_rpm = 500.0f;
    if (sim_rpm > REDLINE + 500.0f) sim_rpm = REDLINE + 500.0f;

    rpm_gauge.update(sim_rpm);
    speed_gauge.update(sim_speed);
    gear_indicator.update(sim_current_gear);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    lv_init();

    lv_display_t *disp = lv_sdl_window_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);

    create_dashboard(lv_screen_active());

    // Screenshot mode: --screenshot <rpm> <output.bmp>
    if (argc >= 4 && strcmp(argv[1], "--screenshot") == 0) {
        float screenshot_rpm = (float)atof(argv[2]);
        const char *screenshot_path = argv[3];

        // Set RPM to the requested value
        rpm_gauge.update(screenshot_rpm);
        speed_gauge.update(80);

        // Render a few frames to make sure everything is drawn
        for (int frame = 0; frame < 5; frame++) {
            lv_timer_handler();
            lv_delay_ms(10);
        }

        // Capture the SDL window
        SDL_Window *window = lv_sdl_window_get_window(disp);
        SDL_Renderer *renderer = (SDL_Renderer *)lv_sdl_window_get_renderer(disp);

        int window_width, window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);

        SDL_Surface *surface = SDL_CreateRGBSurface(0, window_width, window_height, 32,
            0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
            surface->pixels, surface->pitch);
        SDL_SaveBMP(surface, screenshot_path);
        SDL_FreeSurface(surface);

        printf("Screenshot saved: %s (RPM=%.0f)\n", screenshot_path, screenshot_rpm);
        return 0;
    }

    while (1) {
        update_simulation();
        uint32_t time_till_next = lv_timer_handler();
        lv_delay_ms(time_till_next);
    }

    return 0;
}
