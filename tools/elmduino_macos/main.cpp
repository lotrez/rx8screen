#include "Arduino.h"
#include "ELMduino.h"

// Define the global Serial instance
BluetoothSerial Serial;

#include <cstdio>
#include <chrono>
#include <thread>

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);

    const char* port = "/dev/cu.VEEPEAK";
    int baud = 115200;
    bool live_mode = false;

    for (int index = 1; index < argc; index++) {
        std::string arg = argv[index];
        if (arg == "--port" && index + 1 < argc) port = argv[++index];
        else if (arg == "--baud" && index + 1 < argc) baud = std::atoi(argv[++index]);
        else if (arg == "--live") live_mode = true;
        else if (arg == "--help") {
            printf("Usage: %s [--port /dev/cu.XXX] [--baud 115200] [--live]\n", argv[0]);
            return 0;
        }
    }

    printf("=== RX-8 OBD2 Test using ELMduino ===\n");
    printf("Port: %s @ %d baud\n\n", port, baud);

    // Open Bluetooth serial
    printf("Opening Bluetooth serial...\n");
    if (!Serial.begin(port, baud)) {
        printf("[FAIL] Could not open serial port\n");
        return 1;
    }

    // Initialize ELMduino
    ELM327 myELM327;
    printf("Initializing ELM327 (this may take up to 30 seconds for protocol search)...\n");

    bool elm_ok = myELM327.begin(Serial, true, 2000, '0', 128, 0);
    if (!elm_ok) {
        printf("[FAIL] ELM327 initialization failed\n");
        printf("Make sure:\n");
        printf("  1. ELM327 is plugged into OBD2 port\n");
        printf("  2. Ignition is ON\n");
        printf("  3. Bluetooth is paired and connected\n");
        Serial.end();
        return 1;
    }

    printf("[OK] ELM327 initialized!\n");
    printf("Connected: %s\n", myELM327.connected ? "YES" : "NO");

    if (live_mode) {
        printf("\n--- Live Mode (Ctrl+C to stop) ---\n\n");
        while (true) {
            printf("\033[2J\033[H");
            printf("=== RX-8 Live Data ===\n\n");

            float rpm_val = myELM327.rpm();
            if (myELM327.nb_rx_state == ELM_SUCCESS) {
                printf("  RPM:        %.0f\n", rpm_val);
            } else {
                printf("  RPM:        ---\n");
            }

            float speed_val = myELM327.kph();
            if (myELM327.nb_rx_state == ELM_SUCCESS) {
                printf("  Speed:      %.0f km/h\n", speed_val);
            } else {
                printf("  Speed:      ---\n");
            }

            float coolant_val = myELM327.engineCoolantTemp();
            if (myELM327.nb_rx_state == ELM_SUCCESS) {
                printf("  Coolant:    %.1f C", coolant_val);
                if (coolant_val > 105.0f) printf("  *** OVERHEAT ***");
                printf("\n");
            } else {
                printf("  Coolant:    ---\n");
            }

            float intake_val = myELM327.intakeAirTemp();
            if (myELM327.nb_rx_state == ELM_SUCCESS) {
                printf("  Intake Air: %.1f C\n", intake_val);
            } else {
                printf("  Intake Air: ---\n");
            }

            float throttle_val = myELM327.throttle();
            if (myELM327.nb_rx_state == ELM_SUCCESS) {
                printf("  Throttle:   %.1f %%\n", throttle_val);
            } else {
                printf("  Throttle:   ---\n");
            }

            float load_val = myELM327.engineLoad();
            if (myELM327.nb_rx_state == ELM_SUCCESS) {
                printf("  Engine Load:%.1f %%\n", load_val);
            } else {
                printf("  Engine Load:---\n");
            }

            float map_val = myELM327.manifoldPressure();
            if (myELM327.nb_rx_state == ELM_SUCCESS) {
                printf("  MAP:        %.0f kPa\n", map_val);
            } else {
                printf("  MAP:        ---\n");
            }

            float voltage_val = myELM327.ctrlModVoltage();
            if (myELM327.nb_rx_state == ELM_SUCCESS) {
                printf("  Battery:    %.2f V", voltage_val);
                if (voltage_val < 11.0f) printf("  *** LOW VOLT ***");
                printf("\n");
            } else {
                printf("  Battery:    ---\n");
            }

            fflush(stdout);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    } else {
        printf("\n--- Single Read ---\n\n");

        float rpm_val = myELM327.rpm();
        if (myELM327.nb_rx_state == ELM_SUCCESS) {
            printf("  Engine RPM:      %.0f\n", rpm_val);
        } else {
            printf("  Engine RPM:      NO DATA\n");
        }

        float speed_val = myELM327.kph();
        if (myELM327.nb_rx_state == ELM_SUCCESS) {
            printf("  Vehicle Speed:   %.0f km/h\n", speed_val);
        } else {
            printf("  Vehicle Speed:   NO DATA\n");
        }

        float coolant_val = myELM327.engineCoolantTemp();
        if (myELM327.nb_rx_state == ELM_SUCCESS) {
            printf("  Coolant Temp:    %.1f C\n", coolant_val);
        } else {
            printf("  Coolant Temp:    NO DATA\n");
        }

        float intake_val = myELM327.intakeAirTemp();
        if (myELM327.nb_rx_state == ELM_SUCCESS) {
            printf("  Intake Air Temp: %.1f C\n", intake_val);
        } else {
            printf("  Intake Air Temp: NO DATA\n");
        }

        float throttle_val = myELM327.throttle();
        if (myELM327.nb_rx_state == ELM_SUCCESS) {
            printf("  Throttle Pos:    %.1f %%\n", throttle_val);
        } else {
            printf("  Throttle Pos:    NO DATA\n");
        }

        float load_val = myELM327.engineLoad();
        if (myELM327.nb_rx_state == ELM_SUCCESS) {
            printf("  Engine Load:     %.1f %%\n", load_val);
        } else {
            printf("  Engine Load:     NO DATA\n");
        }

        float map_val = myELM327.manifoldPressure();
        if (myELM327.nb_rx_state == ELM_SUCCESS) {
            printf("  MAP:             %.0f kPa\n", map_val);
        } else {
            printf("  MAP:             NO DATA\n");
        }

        float fuel_val = myELM327.fuelLevel();
        if (myELM327.nb_rx_state == ELM_SUCCESS) {
            printf("  Fuel Level:      %.1f %%\n", fuel_val);
        } else {
            printf("  Fuel Level:      NO DATA\n");
        }

        float voltage_val = myELM327.ctrlModVoltage();
        if (myELM327.nb_rx_state == ELM_SUCCESS) {
            printf("  Battery Voltage: %.2f V\n", voltage_val);
        } else {
            printf("  Battery Voltage: NO DATA\n");
        }
    }

    Serial.end();
    printf("\nDone.\n");
    return 0;
}
