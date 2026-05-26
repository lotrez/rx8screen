#include "sim_data.h"
#include <math.h>
#include <stdint.h>

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

static uint32_t sim_tick = 0;
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

void sim_init() {
    sim_tick = 0;
    sim_phase = SIM_IDLE;
    sim_phase_tick = 0;
    sim_current_gear = 0;
    sim_rpm = IDLE_RPM;
    sim_speed = 0.0f;
}

void sim_update() {
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
}

float sim_get_rpm() { return sim_rpm; }
float sim_get_speed() { return sim_speed; }
int sim_get_gear() { return sim_current_gear; }
