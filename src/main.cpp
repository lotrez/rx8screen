#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <src/drivers/sdl/lv_sdl_window.h>
#include "ui/gauges.h"

static ArcGauge rpm_gauge;
static DigitalReadout speed_readout;
static BarGauge water_temp_gauge;
static BarGauge oil_temp_gauge;
static BarGauge fuel_gauge;
static BarGauge voltage_gauge;

static void create_dashboard(lv_obj_t *parent) {
    printf("[RX8] create_dashboard start\n"); fflush(stdout);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    lv_obj_t *top_right;

    static lv_coord_t col_dsc[] = {LV_PCT(50), LV_PCT(50), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_PCT(58), LV_PCT(21), LV_PCT(21), LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(parent);
    printf("[RX8] grid created\n"); fflush(stdout);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_all(grid, 4, 0);
    lv_obj_set_style_pad_row(grid, 4, 0);
    lv_obj_set_style_pad_column(grid, 4, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    printf("[RX8] grid configured\n"); fflush(stdout);

    rpm_gauge.create(grid, "RPM", 0, 9000, 7500, 8500, 10);
    printf("[RX8] rpm_gauge created\n"); fflush(stdout);
    lv_obj_set_grid_cell(rpm_gauge.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1,
                         LV_GRID_ALIGN_STRETCH, 0, 1);
    printf("[RX8] rpm_gauge placed\n"); fflush(stdout);

    top_right = lv_obj_create(grid);
    printf("[RX8] top_right created\n"); fflush(stdout);
    lv_obj_set_grid_cell(top_right, LV_GRID_ALIGN_STRETCH, 1, 1,
                         LV_GRID_ALIGN_STRETCH, 0, 1);
    printf("[RX8] top_right grid cell set\n"); fflush(stdout);
    lv_obj_set_style_bg_opa(top_right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_right, 0, 0);
    lv_obj_set_style_pad_all(top_right, 0, 0);
    lv_obj_clear_flag(top_right, LV_OBJ_FLAG_SCROLLABLE);
    printf("[RX8] top_right styled\n"); fflush(stdout);

    speed_readout.create(top_right, "SPEED", "km/h");
    printf("[RX8] speed_readout created\n"); fflush(stdout);

    water_temp_gauge.create(top_right, "WATER TEMP", 60, 130, 100, 105, "\xC2\xB0""C");
    printf("[RX8] water_temp_gauge created\n"); fflush(stdout);
    water_temp_gauge.update(60);

    oil_temp_gauge.create(grid, "OIL TEMP", 60, 150, 110, 120, "\xC2\xB0""C");
    printf("[RX8] oil_temp created\n"); fflush(stdout);
    lv_obj_set_grid_cell(oil_temp_gauge.get_container(), LV_GRID_ALIGN_STRETCH, 0, 1,
                         LV_GRID_ALIGN_STRETCH, 1, 1);
    oil_temp_gauge.update(60);

    fuel_gauge.create(grid, "FUEL", 0, 100, 20, 15, "%");
    printf("[RX8] fuel created\n"); fflush(stdout);
    lv_obj_set_grid_cell(fuel_gauge.get_container(), LV_GRID_ALIGN_STRETCH, 1, 1,
                         LV_GRID_ALIGN_STRETCH, 1, 1);
    fuel_gauge.update(50);

    voltage_gauge.create(grid, "BATTERY", 10, 16, 14.5, 11.5, "V");
    printf("[RX8] voltage created\n"); fflush(stdout);
    lv_obj_set_grid_cell(voltage_gauge.get_container(), LV_GRID_ALIGN_STRETCH, 0, 2,
                         LV_GRID_ALIGN_STRETCH, 2, 1);
    voltage_gauge.update(12.0);
    printf("[RX8] dashboard complete\n"); fflush(stdout);
}

static uint32_t sim_tick = 0;

static void update_simulation() {
    sim_tick++;
    float t = sim_tick / 30.0f;

    float rpm = 3500 + sinf(t * 0.5f) * 3000 + sinf(t * 1.3f) * 500;
    if (rpm < 800) rpm = 800;
    if (rpm > 9000) rpm = 9000;
    rpm_gauge.update(rpm);

    float speed = 80 + sinf(t * 0.3f) * 60 + sinf(t * 0.7f) * 20;
    if (speed < 0) speed = 0;
    speed_readout.update(speed);

    float water = 82 + sinf(t * 0.1f) * 15;
    water_temp_gauge.update(water);

    float oil = 90 + sinf(t * 0.08f) * 20;
    oil_temp_gauge.update(oil);

    float fuel = 72 - sim_tick * 0.001f;
    if (fuel < 0) fuel = 72;
    fuel_gauge.update(fuel);

    float voltage = 13.8 + sinf(t * 2.0f) * 0.4f;
    voltage_gauge.update(voltage);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("[RX8] Starting...\n");
    fflush(stdout);

    lv_init();
    printf("[RX8] LVGL initialized\n");
    fflush(stdout);

    lv_display_t *disp = lv_sdl_window_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    printf("[RX8] SDL window created\n");
    fflush(stdout);

    create_dashboard(lv_screen_active());
    printf("[RX8] Dashboard created, entering main loop\n");
    fflush(stdout);

    while (1) {
        update_simulation();
        uint32_t time_till_next = lv_timer_handler();
        lv_delay_ms(time_till_next);
    }

    return 0;
}
