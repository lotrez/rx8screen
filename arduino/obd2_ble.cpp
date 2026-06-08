#include "obd2_ble.h"
#include <string.h>
#include <stdlib.h>

static OBD2BLE *instance = nullptr;

void OBD2BLE::begin() {
    instance = this;
    NimBLEDevice::init("RX8-Dash");
    set_state(State::SCANNING);
}

void OBD2BLE::set_state(State new_state) {
    state = new_state;
    state_entered_ms = millis();
    Serial.printf("[OBD2] State: %d\n", (int)state);
}

void OBD2BLE::loop() {
    switch (state) {
        case State::IDLE:
            break;

        case State::SCANNING:
            if (scan_and_connect()) {
                set_state(State::DISCOVERING);
            } else {
                if (millis() - state_entered_ms > 10000) {
                    Serial.println("[OBD2] Scan timeout, retrying...");
                    set_state(State::SCANNING);
                }
            }
            break;

        case State::CONNECTING:
            // Handled in scan_and_connect
            break;

        case State::DISCOVERING:
            if (discover_service()) {
                set_state(State::SUBSCRIBING);
            } else {
                if (millis() - state_entered_ms > 5000) {
                    Serial.println("[OBD2] Discovery timeout");
                    disconnect();
                    set_state(State::RECONNECTING);
                }
            }
            break;

        case State::SUBSCRIBING:
            if (subscribe_notifications()) {
                set_state(State::INIT_ELM);
                init_step = 0;
            } else {
                if (millis() - state_entered_ms > 5000) {
                    Serial.println("[OBD2] Subscribe timeout");
                    disconnect();
                    set_state(State::RECONNECTING);
                }
            }
            break;

        case State::INIT_ELM:
            init_elm327();
            break;

        case State::CONNECTED:
            if (!client || !client->isConnected()) {
                Serial.println("[OBD2] Connection lost");
                data.connected = false;
                disconnect();
                set_state(State::RECONNECTING);
                break;
            }
            if (millis() - last_poll_ms >= 250) {  // Poll every 250ms = 4 Hz per PID
                poll_next_pid();
                last_poll_ms = millis();
            }
            break;

        case State::RECONNECTING:
            if (millis() - state_entered_ms > 3000) {
                Serial.println("[OBD2] Reconnecting...");
                set_state(State::SCANNING);
            }
            break;
    }
}

bool OBD2BLE::scan_and_connect() {
    NimBLEScan *pScan = NimBLEDevice::getScan();
    NimBLEScanResults results = pScan->getResults(5000);  // 5 second blocking scan

    NimBLEAddress target_addr;
    bool found = false;

    for (int i = 0; i < results.getCount(); i++) {
        const NimBLEAdvertisedDevice *dev = results.getDevice(i);
        if (dev && dev->getName() == DEVICE_NAME) {
            target_addr = dev->getAddress();
            found = true;
            Serial.printf("[OBD2] Found %s at %s\n", DEVICE_NAME, target_addr.toString().c_str());
            break;
        }
    }

    if (!found) {
        Serial.println("[OBD2] VEEPEAK not found");
        return false;
    }

    client = NimBLEDevice::createClient();
    if (!client->connect(target_addr)) {
        Serial.println("[OBD2] Connect failed");
        NimBLEDevice::deleteClient(client);
        client = nullptr;
        return false;
    }

    Serial.println("[OBD2] Connected to VEEPEAK");
    data.connected = true;
    return true;
}

bool OBD2BLE::discover_service() {
    if (!client || !client->isConnected()) return false;

    service = client->getService(SERVICE_UUID);
    if (!service) {
        Serial.println("[OBD2] Service FFF0 not found");
        return false;
    }

    write_char = service->getCharacteristic(WRITE_CHAR_UUID);
    notify_char = service->getCharacteristic(NOTIFY_CHAR_UUID);

    if (!write_char || !notify_char) {
        Serial.println("[OBD2] Characteristics not found");
        return false;
    }

    Serial.println("[OBD2] Service discovered OK");
    return true;
}

bool OBD2BLE::subscribe_notifications() {
    if (!notify_char) return false;

    response_len = 0;
    response_complete = false;

    if (notify_char->subscribe(true, notify_callback)) {
        Serial.println("[OBD2] Subscribed to notifications");
        return true;
    }

    Serial.println("[OBD2] Subscribe failed");
    return false;
}

void OBD2BLE::notify_callback(NimBLERemoteCharacteristic *pChar,
                               uint8_t *pData, size_t length, bool isNotify) {
    if (instance) {
        instance->on_notify(pData, length);
    }
}

void OBD2BLE::on_notify(const uint8_t *notify_data, size_t len) {
    for (size_t i = 0; i < len && response_len < RESPONSE_BUF_SIZE - 1; i++) {
        char c = (char)notify_data[i];
        response_buf[response_len++] = c;
        if (c == '>') {
            response_buf[response_len] = '\0';
            response_complete = true;
            break;
        }
    }
}

void OBD2BLE::send_command(const char *cmd) {
    if (!write_char || !client || !client->isConnected()) return;

    response_len = 0;
    response_complete = false;

    char cmd_buf[32];
    snprintf(cmd_buf, sizeof(cmd_buf), "%s\r", cmd);
    write_char->writeValue((uint8_t *)cmd_buf, strlen(cmd_buf), false);  // no ACK wait
    Serial.printf("[OBD2] TX: %s\n", cmd);
}

bool OBD2BLE::wait_for_response(uint32_t timeout_ms) {
    uint32_t start = millis();
    while (!response_complete && (millis() - start) < timeout_ms) {
        yield();
    }
    return response_complete;
}

void OBD2BLE::init_elm327() {
    static const char *init_cmds[] = {
        "ATZ",      // Reset
        "ATE0",     // Echo off
        "ATH0",     // Headers off
        "ATL0",     // Linefeeds off
        "ATSP0",    // Auto protocol
        "ATST10",   // 64ms timeout
        "0100",     // Test connection
    };
    static constexpr int INIT_CMD_COUNT = sizeof(init_cmds) / sizeof(init_cmds[0]);

    if (init_step >= INIT_CMD_COUNT) {
        Serial.println("[OBD2] ELM327 init complete");
        current_pid_index = 0;
        last_poll_ms = millis();
        set_state(State::CONNECTED);
        return;
    }

    // Send command and wait for response
    send_command(init_cmds[init_step]);
    if (wait_for_response(3000)) {
        Serial.printf("[OBD2] RX: %s\n", response_buf);
        // For ATZ, we might get "ELM327..." banner then prompt
        // For 0100, we might get SEARCHING... then response
        // Just advance to next step
        init_step++;
    } else {
        if (millis() - state_entered_ms > 15000) {
            Serial.println("[OBD2] Init timeout");
            disconnect();
            set_state(State::RECONNECTING);
        }
    }
}

void OBD2BLE::poll_next_pid() {
    send_command(PID_TABLE[current_pid_index]);

    if (wait_for_response(500)) {
        parse_response(response_buf);
    } else {
        Serial.printf("[OBD2] Timeout waiting for %s\n", PID_TABLE[current_pid_index]);
    }

    current_pid_index++;
    if (current_pid_index >= PID_COUNT) {
        current_pid_index = 0;
    }
}

static int hex_byte_to_int(const char *hex) {
    return (int)strtol(hex, nullptr, 16);
}

static int parse_hex_response(const char *response, int *out_bytes, int max_bytes) {
    // Response looks like: "41 0C 12 08 >" or "NO DATA>" or "SEARCHING...>"
    // We need to extract space-separated hex values before '>'
    // Returns: number of hex bytes parsed, or 0 for special responses/errors

    char buf[256];
    strncpy(buf, response, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Remove '>' and any trailing whitespace
    char *gt = strchr(buf, '>');
    if (gt) *gt = '\0';

    // Check for special responses
    if (strstr(buf, "NO DATA") != nullptr) return 0;
    if (strstr(buf, "SEARCHING") != nullptr) return 0;
    if (strstr(buf, "STOPPED") != nullptr) return 0;
    if (strstr(buf, "ERROR") != nullptr) return 0;
    if (strstr(buf, "7F") != nullptr) return 0;

    // Tokenize by spaces
    int count = 0;
    char *token = strtok(buf, " \r\n\t");
    while (token && count < max_bytes) {
        // Skip empty tokens and "ELM327" banner
        if (strlen(token) >= 2 && isxdigit((unsigned char)token[0]) && isxdigit((unsigned char)token[1])) {
            out_bytes[count++] = hex_byte_to_int(token);
        }
        token = strtok(nullptr, " \r\n\t");
    }

    return count;  // Need at least 2 for header (41) + PID
}

void OBD2BLE::parse_response(const char *response) {
    Serial.printf("[OBD2] RX: %s\n", response);

    int bytes[16];
    int byte_count = parse_hex_response(response, bytes, 16);

    if (byte_count < 2) return;  // Special response or parse error

    // bytes[0] = 0x41 (mode 01 response)
    // bytes[1] = PID
    int pid = bytes[1];

    switch (pid) {
        case 0x0C:  // RPM
            if (byte_count >= 4) {
                data.rpm = ((bytes[2] * 256) + bytes[3]) / 4.0f;
            }
            break;
        case 0x0D:  // Speed
            if (byte_count >= 3) {
                data.speed = bytes[2];
            }
            break;
        case 0x05:  // Coolant temp
            if (byte_count >= 3) {
                data.coolant_temp = bytes[2] - 40.0f;
            }
            break;
        case 0x0F:  // Intake air temp
            if (byte_count >= 3) {
                data.intake_temp = bytes[2] - 40.0f;
            }
            break;
        case 0x04:  // Engine load
            if (byte_count >= 3) {
                data.engine_load = bytes[2] * 100.0f / 255.0f;
            }
            break;
        case 0x11:  // Throttle position
            if (byte_count >= 3) {
                data.throttle_pos = bytes[2] * 100.0f / 255.0f;
            }
            break;
        case 0x2F:  // Fuel level
            if (byte_count >= 3) {
                data.fuel_level = bytes[2] * 100.0f / 255.0f;
            }
            break;
        case 0x0E:  // Timing advance
            if (byte_count >= 3) {
                data.timing_advance = bytes[2] / 2.0f - 64.0f;
            }
            break;
        case 0x10:  // MAF rate
            if (byte_count >= 4) {
                data.maf_rate = (bytes[2] * 256 + bytes[3]) / 100.0f;
            }
            break;
    }
}

void OBD2BLE::disconnect() {
    data.connected = false;
    if (client) {
        if (client->isConnected()) {
            client->disconnect();
        }
        NimBLEDevice::deleteClient(client);
        client = nullptr;
    }
    service = nullptr;
    write_char = nullptr;
    notify_char = nullptr;
}
