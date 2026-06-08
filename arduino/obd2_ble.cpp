#include "obd2_ble.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

OBD2BLE *OBD2BLE::instance = nullptr;

void OBD2BLE::begin() {
    instance = this;
    NimBLEDevice::init("RX8-Dash");
    set_state(State::SCAN_START);
}

void OBD2BLE::set_state(State new_state) {
    state = new_state;
    state_entered_ms = millis();
}

void OBD2BLE::loop() {
    switch (state) {
        case State::IDLE:
            break;
        case State::SCAN_START:
            step_scan_start();
            break;
        case State::SCAN_WAIT:
            step_scan_wait();
            break;
        case State::CONNECT:
            step_connect();
            break;
        case State::DISCOVER:
            step_discover();
            break;
        case State::SUBSCRIBE:
            step_subscribe();
            break;
        case State::INIT_ELM_SEND:
            step_init_elm_send();
            break;
        case State::INIT_ELM_WAIT:
            step_init_elm_wait();
            break;
        case State::POLLING_IDLE:
            step_polling_idle();
            break;
        case State::POLLING_WAIT:
            step_polling_wait();
            break;
        case State::RECONNECT:
            step_reconnect();
            break;
    }
}

// ─── Scan ───────────────────────────────────────────────────────────

void OBD2BLE::step_scan_start() {
    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(nullptr, false);
    // Start non-blocking scan for 5 seconds
    if (pScan->start(5000, false, false)) {
        Serial.println("[OBD2] Scan started");
        set_state(State::SCAN_WAIT);
    } else {
        Serial.println("[OBD2] Scan start failed");
        set_state(State::RECONNECT);
    }
}

void OBD2BLE::step_scan_wait() {
    NimBLEScan *pScan = NimBLEDevice::getScan();
    if (pScan->isScanning()) {
        // Still scanning — do nothing, return immediately
        return;
    }

    // Scan done. Check results.
    NimBLEScanResults results = pScan->getResults();
    found_target = false;
    for (int i = 0; i < results.getCount(); i++) {
        const NimBLEAdvertisedDevice *dev = results.getDevice(i);
        if (dev && dev->getName() == DEVICE_NAME) {
            target_addr = dev->getAddress();
            found_target = true;
            Serial.printf("[OBD2] Found %s at %s\n", DEVICE_NAME, target_addr.toString().c_str());
            break;
        }
    }

    if (!found_target) {
        Serial.println("[OBD2] VEEPEAK not found");
        set_state(State::RECONNECT);
        return;
    }

    set_state(State::CONNECT);
}

// ─── Connect ──────────────────────────────────────────────────────────

void OBD2BLE::step_connect() {
    client = NimBLEDevice::createClient();
    if (client->connect(target_addr)) {
        Serial.println("[OBD2] Connected");
        data.connected = true;
        set_state(State::DISCOVER);
    } else {
        Serial.println("[OBD2] Connect failed");
        NimBLEDevice::deleteClient(client);
        client = nullptr;
        set_state(State::RECONNECT);
    }
}

// ─── Discover ───────────────────────────────────────────────────────

void OBD2BLE::step_discover() {
    if (!client || !client->isConnected()) {
        set_state(State::RECONNECT);
        return;
    }

    service = client->getService(SERVICE_UUID);
    if (!service) {
        Serial.println("[OBD2] Service FFF0 not found");
        disconnect();
        set_state(State::RECONNECT);
        return;
    }

    write_char = service->getCharacteristic(WRITE_CHAR_UUID);
    notify_char = service->getCharacteristic(NOTIFY_CHAR_UUID);

    if (!write_char || !notify_char) {
        Serial.println("[OBD2] Characteristics not found");
        disconnect();
        set_state(State::RECONNECT);
        return;
    }

    Serial.println("[OBD2] Discovered OK");
    set_state(State::SUBSCRIBE);
}

// ─── Subscribe ──────────────────────────────────────────────────────

void OBD2BLE::step_subscribe() {
    if (!notify_char) {
        set_state(State::RECONNECT);
        return;
    }

    response_len = 0;
    response_complete = false;

    if (notify_char->subscribe(true, notify_callback)) {
        Serial.println("[OBD2] Subscribed");
        init_step = 0;
        set_state(State::INIT_ELM_SEND);
    } else {
        Serial.println("[OBD2] Subscribe failed");
        disconnect();
        set_state(State::RECONNECT);
    }
}

// ─── ELM327 Init ─────────────────────────────────────────────────────

void OBD2BLE::step_init_elm_send() {
    if (init_step >= INIT_CMD_COUNT) {
        Serial.println("[OBD2] Init complete");
        current_pid_index = 0;
        set_state(State::POLLING_IDLE);
        return;
    }

    send_command(INIT_CMDS[init_step]);
    set_state(State::INIT_ELM_WAIT);
}

void OBD2BLE::step_init_elm_wait() {
    uint32_t elapsed = millis() - last_tx_ms;

    if (response_complete) {
        Serial.printf("[OBD2] RX: %s\n", response_buf);
        init_step++;
        set_state(State::INIT_ELM_SEND);
        return;
    }

    if (elapsed > 3000) {
        Serial.printf("[OBD2] Init timeout (cmd %d)\n", init_step);
        init_step++;
        set_state(State::INIT_ELM_SEND);
        return;
    }
    // Otherwise: response not yet received, return and check again next loop()
}

// ─── Polling ─────────────────────────────────────────────────────────

void OBD2BLE::step_polling_idle() {
    if (!client || !client->isConnected()) {
        set_state(State::RECONNECT);
        return;
    }

    send_command(PID_TABLE[current_pid_index]);
    set_state(State::POLLING_WAIT);
}

void OBD2BLE::step_polling_wait() {
    uint32_t elapsed = millis() - last_tx_ms;

    if (response_complete) {
        parse_response(response_buf);
        current_pid_index++;
        if (current_pid_index >= PID_COUNT) {
            current_pid_index = 0;
        }
        set_state(State::POLLING_IDLE);
        return;
    }

    if (elapsed > 500) {
        Serial.printf("[OBD2] PID timeout: %s\n", PID_TABLE[current_pid_index]);
        current_pid_index++;
        if (current_pid_index >= PID_COUNT) {
            current_pid_index = 0;
        }
        set_state(State::POLLING_IDLE);
        return;
    }
    // Otherwise: response not yet received, return and check again next loop()
}

// ─── Reconnect ──────────────────────────────────────────────────────

void OBD2BLE::step_reconnect() {
    data.connected = false;
    if (millis() - state_entered_ms > 3000) {
        disconnect();
        set_state(State::SCAN_START);
    }
}

// ─── Notifications ───────────────────────────────────────────────────

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

// ─── Send ────────────────────────────────────────────────────────────

void OBD2BLE::send_command(const char *cmd) {
    if (!write_char || !client || !client->isConnected()) return;

    response_len = 0;
    response_complete = false;
    last_tx_ms = millis();

    char cmd_buf[32];
    int n = snprintf(cmd_buf, sizeof(cmd_buf), "%s\r", cmd);
    if (n > 0 && n < (int)sizeof(cmd_buf)) {
        write_char->writeValue((uint8_t *)cmd_buf, (size_t)n, false);  // no ACK wait
        Serial.printf("[OBD2] TX: %s\n", cmd);
    }
}

// ─── Parse ───────────────────────────────────────────────────────────

static int hex_byte_to_int(const char *hex) {
    return (int)strtol(hex, nullptr, 16);
}

static int parse_hex_response(const char *response, int *out_bytes, int max_bytes) {
    char buf[256];
    strncpy(buf, response, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *gt = strchr(buf, '>');
    if (gt) *gt = '\0';

    if (strstr(buf, "NO DATA") != nullptr) return 0;
    if (strstr(buf, "SEARCHING") != nullptr) return 0;
    if (strstr(buf, "STOPPED") != nullptr) return 0;
    if (strstr(buf, "ERROR") != nullptr) return 0;
    if (strstr(buf, "7F") != nullptr) return 0;

    int count = 0;
    char *token = strtok(buf, " \r\n\t");
    while (token && count < max_bytes) {
        if (strlen(token) >= 2 && isxdigit((unsigned char)token[0]) && isxdigit((unsigned char)token[1])) {
            out_bytes[count++] = hex_byte_to_int(token);
        }
        token = strtok(nullptr, " \r\n\t");
    }
    return count;
}

void OBD2BLE::parse_response(const char *response) {
    Serial.printf("[OBD2] RX: %s\n", response);

    int bytes[16];
    int byte_count = parse_hex_response(response, bytes, 16);
    if (byte_count < 2) return;

    int pid = bytes[1];
    switch (pid) {
        case 0x0C:  // RPM
            if (byte_count >= 4) data.rpm = ((bytes[2] * 256) + bytes[3]) / 4.0f;
            break;
        case 0x0D:  // Speed
            if (byte_count >= 3) data.speed = bytes[2];
            break;
        case 0x05:  // Coolant temp
            if (byte_count >= 3) data.coolant_temp = bytes[2] - 40.0f;
            break;
        case 0x0F:  // Intake air temp
            if (byte_count >= 3) data.intake_temp = bytes[2] - 40.0f;
            break;
        case 0x04:  // Engine load
            if (byte_count >= 3) data.engine_load = bytes[2] * 100.0f / 255.0f;
            break;
        case 0x11:  // Throttle position
            if (byte_count >= 3) data.throttle_pos = bytes[2] * 100.0f / 255.0f;
            break;
        case 0x2F:  // Fuel level
            if (byte_count >= 3) data.fuel_level = bytes[2] * 100.0f / 255.0f;
            break;
        case 0x0E:  // Timing advance
            if (byte_count >= 3) data.timing_advance = bytes[2] / 2.0f - 64.0f;
            break;
        case 0x10:  // MAF rate
            if (byte_count >= 4) data.maf_rate = (bytes[2] * 256 + bytes[3]) / 100.0f;
            break;
    }
}

// ─── Disconnect ─────────────────────────────────────────────────────

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
