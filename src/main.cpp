#include <lvgl.h>
#include <math.h>
#include <src/drivers/sdl/lv_sdl_window.h>
#include <SDL.h>
#include "ui/rpm_gauge.h"
#include "ui/speed_gauge.h"
#include "ui/gear_indicator.h"
#include "sim_data.h"

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
    lv_obj_align(speed_gauge.get_container(), LV_ALIGN_TOP_LEFT, 20, 20);

    gear_indicator.create(parent);
    lv_obj_align(gear_indicator.get_container(), LV_ALIGN_CENTER, -200, -30);

    rpm_gauge.create(parent);
}

static void update_gauges() {
    sim_update();
    rpm_gauge.update(sim_get_rpm());
    speed_gauge.update(sim_get_speed());
    gear_indicator.update(sim_get_gear());
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    lv_init();

    lv_display_t *disp = lv_sdl_window_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);

    sim_init();
    create_dashboard(lv_screen_active());

    if (argc >= 4 && strcmp(argv[1], "--screenshot") == 0) {
        float screenshot_rpm = (float)atof(argv[2]);
        const char *screenshot_path = argv[3];

        rpm_gauge.update(screenshot_rpm);
        speed_gauge.update(80);
        gear_indicator.update(3);

        for (int frame = 0; frame < 5; frame++) {
            lv_timer_handler();
            lv_delay_ms(10);
        }

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
        update_gauges();
        uint32_t time_till_next = lv_timer_handler();
        lv_delay_ms(time_till_next);
    }

    return 0;
}
