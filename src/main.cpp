#include <lvgl.h>
#include <math.h>
#include <src/drivers/sdl/lv_sdl_window.h>
#include "ui/rpm_gauge.h"
#include "ui/speed_gauge.h"
// #include "ui/water_temp_gauge.h"
// #include "ui/oil_temp_gauge.h"
// #include "ui/fuel_gauge.h"
// #include "ui/voltage_gauge.h"

static RpmGauge rpm_gauge;
static SpeedGauge speed_gauge;
// static WaterTempGauge water_temp_gauge;
// static OilTempGauge oil_temp_gauge;
// static FuelGauge fuel_gauge;
// static VoltageGauge voltage_gauge;

static void create_dashboard(lv_obj_t *parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    speed_gauge.create(parent);
    lv_obj_set_size(speed_gauge.get_container(), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(speed_gauge.get_container(), LV_ALIGN_TOP_LEFT, 20, 20);

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

static void update_simulation() {
    sim_tick++;
    float t = sim_tick / 120.0f;

    float rpm = 4500 + sinf(t * 0.5f) * 4000 + sinf(t * 1.3f) * 1000;
    if (rpm < 800) rpm = 800;
    if (rpm > 11000) rpm = 11000;
    rpm_gauge.update(rpm);

    float speed = 80 + sinf(t * 0.3f) * 60 + sinf(t * 0.7f) * 20;
    if (speed < 0) speed = 0;
    speed_gauge.update(speed);

    // float water = 82 + sinf(t * 0.1f) * 15;
    // water_temp_gauge.update(water);
    // float oil = 90 + sinf(t * 0.08f) * 20;
    // oil_temp_gauge.update(oil);
    // float fuel = 72 - sim_tick * 0.001f;
    // if (fuel < 0) fuel = 72;
    // fuel_gauge.update(fuel);
    // float voltage = 13.8 + sinf(t * 2.0f) * 0.4f;
    // voltage_gauge.update(voltage);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    lv_init();

    lv_display_t *disp = lv_sdl_window_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);

    create_dashboard(lv_screen_active());

    while (1) {
        update_simulation();
        uint32_t time_till_next = lv_timer_handler();
        lv_delay_ms(time_till_next);
    }

    return 0;
}
