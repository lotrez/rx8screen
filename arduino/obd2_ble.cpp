#include "obd2_ble.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

OBD2BLE *OBD2BLE::instance = nullptr;

static class MyClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient *pClient) override {
        Serial.println("[OBD2-CB] onConnect");
    }
    void onDisconnect(NimBLEClient *pClient, int reason) override {
        Serial.printf("[OBD2-CB] onDisconnect reason=%d\n", reason);
    }
} clientCallbacks;

// ─── Lifecycle ─────────────────────────────────────────────────────

void OBD2BLE::begin() {
    instance = this;
    NimBLEDevice::init("ESP32-OBD2");
    status_detail[0] = '\0';
}

bool OBD2BLE::connect_blocking() {
    Serial.println("[OBD2] Scanning for OBD2 adapter...");

    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    NimBLEScanResults results = pScan->getResults(15 * 1000);

    obd_device = nullptr;
    for (int i = 0; i < results.getCount(); i++) {
        const NimBLEAdvertisedDevice *cdev = results.getDevice(i);
        if (!cdev) continue;
        std::string name = cdev->getName();
        std::string nu = name;
        for (auto &c : nu) c = toupper(c);
        if (nu.find("VEEPEAK") != std::string::npos ||
            nu.find("VEEPEEK") != std::string::npos ||
            nu.find("OBD") != std::string::npos ||
            nu.find("ELM") != std::string::npos) {
            obd_device = new NimBLEAdvertisedDevice(*cdev);
            Serial.printf("[OBD2] Found: %s [%s]\n", name.c_str(), cdev->getAddress().toString().c_str());
            break;
        }
    }

    if (!obd_device) {
        Serial.println("[OBD2] Not found!");
        pScan->clearResults();
        return false;
    }
    pScan->clearResults();

    Serial.printf("[OBD2] Connecting to %s...\n", obd_device->getAddress().toString().c_str());

    if (client) {
        if (client->isConnected()) client->disconnect();
        // Don't deleteClient — let BLE stack GC. Prevents heap corruption.
    }
    client = NimBLEDevice::createClient();
    client->setClientCallbacks(&clientCallbacks);

    if (!client->connect(obd_device)) {
        Serial.printf("[OBD2] Connect failed, err=%d\n", client->getLastError());
        client = nullptr;
        return false;
    }

    Serial.println("[OBD2] Connected OK");
    data.connected = true;

    NimBLERemoteService *svc = nullptr;
    const char *svcs[] = {"fff0", "FFE0", "ffe0"};
    for (auto s : svcs) {
        svc = client->getService(s);
        if (svc) break;
    }
    if (!svc) {
        auto all = client->getServices(true);
        if (!all.empty()) svc = all.front();
    }
    if (!svc) {
        Serial.println("[OBD2] No service found");
        disconnect();
        return false;
    }
    Serial.printf("[OBD2] Service: %s\n", svc->getUUID().toString().c_str());
    service = svc;

    auto chars = service->getCharacteristics(true);
    for (auto chr : chars) {
        std::string uid = chr->getUUID().toString();
        if (uid == "0xfff2") write_char = chr;
        if (uid == "0xfff1") notify_char = chr;
    }
    if (!write_char) {
        for (auto chr : chars) {
            if (chr->canWrite()) { write_char = chr; break; }
        }
    }
    if (!notify_char) {
        for (auto chr : chars) {
            if (chr->canNotify()) { notify_char = chr; break; }
        }
    }
    if (!write_char) {
        Serial.println("[OBD2] No write char");
        disconnect();
        return false;
    }
    if (notify_char && notify_char->canNotify()) {
        notify_char->subscribe(true, notify_cb);
    }

    Serial.printf("[OBD2] Ready (W:%s N:%s)\n",
        write_char->getUUID().toString().c_str(),
        notify_char ? notify_char->getUUID().toString().c_str() : "-");

    // ─── Init ELM327 (blocking, same as working example) ─────────
    if (!init_elm_blocking()) {
        Serial.println("[OBD2] Init failed");
        disconnect();
        return false;
    }

    Serial.println("[OBD2] Init OK, starting polling");
    response_buf.clear();
    response_complete = false;
    response_has_data = false;
    current_pid_index = 0;
    set_state(State::POLLING);
    return true;
}

bool OBD2BLE::init_elm_blocking() {
    Serial.println("[OBD2] Init OBD2...");

    // Warm init: echo off, headers off, linefeeds off
    send_cmd_blocking("ATE0", false);
    send_cmd_blocking("ATH0", false);
    send_cmd_blocking("ATL0", false);

    // Check ECU readiness
    char resp[256];
    if (!send_cmd_blocking("0100", false, resp, sizeof(resp))) {
        Serial.println("[OBD2] 0100 timeout, cold reset...");
    }

    String r = String(resp);
    r.trim();
    bool ecu_ready = (r.length() > 4 && !r.startsWith("SEARCHING") &&
                      !r.startsWith("STOPPED") && !r.startsWith("NO DATA") &&
                      !r.startsWith("ERROR"));

    if (!ecu_ready) {
        Serial.printf("[OBD2] Warm init failed (\"%s\"), cold reset...\n", resp);
        send_cmd_blocking("ATZ", false);
        delay(2000);
        send_cmd_blocking("ATE0", false);
        send_cmd_blocking("ATH0", false);
        send_cmd_blocking("ATL0", false);
        send_cmd_blocking("ATSP0", false);
        delay(300);

        // Retry 0100 up to 5 times
        bool found = false;
        for (int i = 0; i < 5 && !found; i++) {
            memset(resp, 0, sizeof(resp));
            if (!send_cmd_blocking("0100", false, resp, sizeof(resp))) {
                delay(1000);
                continue;
            }
            r = String(resp);
            r.trim();
            found = (r.length() > 4 && !r.startsWith("SEARCHING") &&
                     !r.startsWith("STOPPED") && !r.startsWith("NO DATA"));
            if (!found) delay(1000);
        }

        if (!found) {
            // Try different protocols
            const char *protos[] = {"ATSP6", "ATSP7", "ATSP8", "ATSP9"};
            for (int i = 0; i < 4 && !found; i++) {
                send_cmd_blocking(protos[i], false);
                memset(resp, 0, sizeof(resp));
                if (!send_cmd_blocking("0100", false, resp, sizeof(resp))) continue;
                r = String(resp);
                r.trim();
                found = (r.length() > 2 && !r.startsWith("SEARCHING") &&
                         !r.startsWith("STOPPED") && !r.startsWith("NO DATA"));
            }
        }

        if (!found) {
            Serial.println("[OBD2] ECU connect failed!");
            return false;
        }
    }

    send_cmd_blocking("ATST10", false);  // 40ms timeout
    send_cmd_blocking("ATDPN", false);
    delay(50);

    Serial.println("[OBD2] OBD2 ready.");
    return true;
}

bool OBD2BLE::send_cmd_blocking(const char *cmd, bool fast, char *out, size_t out_len) {
    if (!write_char || !client || !client->isConnected()) return false;

    response_buf.clear();
    response_complete = false;
    response_has_data = false;

    String full = String(cmd) + "\r";
    write_char->writeValue((uint8_t *)full.c_str(), full.length(), false);

    unsigned long start = millis();
    while (millis() - start < 2000) {
        if (response_complete) {
            if (out && out_len > 0) {
                strncpy(out, response_buf.c_str(), out_len - 1);
                out[out_len - 1] = '\0';
            }
            bool has_data = response_buf.length() > 0;
            response_buf.clear();
            response_complete = false;
            response_has_data = false;
            return has_data;
        }
        if (fast && response_has_data && response_buf.length() >= 8) {
            int space_count = 0;
            for (int i = 0; i < (int)response_buf.length(); i++) {
                if (response_buf[i] == ' ') space_count++;
            }
            if (space_count >= 3) {
                delay(2);
                if (out && out_len > 0) {
                    strncpy(out, response_buf.c_str(), out_len - 1);
                    out[out_len - 1] = '\0';
                }
                bool has_data = response_buf.length() > 0;
                response_buf.clear();
                response_complete = false;
                response_has_data = false;
                return has_data;
            }
        }
        delay(1);
    }
    return false;
}

void OBD2BLE::set_state(State new_state) {
    state = new_state;
    state_entered_ms = millis();
}

void OBD2BLE::loop() {
    switch (state) {
        case State::IDLE: break;
        case State::POLLING: step_polling(); break;
        case State::RECONNECT: step_reconnect(); break;
    }
}

bool OBD2BLE::is_connected() const {
    return state == State::POLLING && client && client->isConnected();
}

const char *OBD2BLE::get_state_name() const {
    switch (state) {
        case State::IDLE: return "IDLE";
        case State::POLLING: return "LIVE DATA";
        case State::RECONNECT: return "RECONNECTING";
    }
    return "UNKNOWN";
}

// ─── Polling ──────────────────────────────────────────────────────
// Fast PIDs (index 0-1): RPM, Speed — polled every 80ms
// Slow PIDs (index 2-4): Coolant, Battery, Fuel — polled every 1000ms

static char _poll_resp[256];
static uint32_t _poll_cmd_sent_ms = 0;
static bool _poll_cmd_in_flight = false;

void OBD2BLE::step_polling() {
    if (!client || !client->isConnected()) {
        _poll_cmd_in_flight = false;
        set_state(State::RECONNECT);
        return;
    }

    if (_poll_cmd_in_flight) {
        if (read_response(_poll_resp, sizeof(_poll_resp), true)) {
            parse_response(_poll_resp);
            _poll_cmd_in_flight = false;
            _last_pid_time = millis();
        } else if (millis() - _poll_cmd_sent_ms > 500) {
            _poll_cmd_in_flight = false;
            _last_pid_time = millis();
        }
        return;
    }

    // Not in flight — decide what to send next
    uint32_t now = millis();
    if (now - _last_pid_time < 80) return;  // min 80ms gap

    bool is_slow = (current_pid_index >= 2);
    if (is_slow && (now - _last_slow_pid_time < 1000)) {
        // Skip this slow PID — not enough time passed
        current_pid_index++;
        if (current_pid_index >= PID_COUNT) current_pid_index = 0;
        return;
    }

    send_command(PID_TABLE[current_pid_index]);
    if (is_slow) _last_slow_pid_time = now;

    current_pid_index++;
    if (current_pid_index >= PID_COUNT) current_pid_index = 0;
    _poll_cmd_sent_ms = now;
    _poll_cmd_in_flight = true;
}

// ─── Reconnect ───────────────────────────────────────────────────

void OBD2BLE::step_reconnect() {
    data.connected = false;
    if (millis() - state_entered_ms > 3000) {
        status_detail[0] = '\0';
        disconnect();
        set_state(State::IDLE);
    }
}

// ─── Notifications ───────────────────────────────────────────────

void OBD2BLE::notify_cb(NimBLERemoteCharacteristic *pChar,
                         uint8_t *pData, size_t length, bool isNotify) {
    if (instance) instance->on_notify(pData, length);
}

void OBD2BLE::on_notify(uint8_t *pData, size_t length) {
    for (size_t i = 0; i < length; i++) {
        char c = (char)pData[i];
        if (c == '\r' || c == '\n' || c == '\0') continue;
        if (c == '>') {
            response_complete = true;
            return;
        }
        response_buf += c;
        if (response_buf.length() >= 8) {
            response_has_data = true;
        }
    }
}

// ─── Send / Read ─────────────────────────────────────────────────

void OBD2BLE::send_command(const char *cmd) {
    if (!write_char || !client || !client->isConnected()) return;
    response_buf.clear();
    response_complete = false;
    response_has_data = false;
    String full = String(cmd) + "\r";
    write_char->writeValue((uint8_t *)full.c_str(), full.length(), false);
}

bool OBD2BLE::read_response(char *out, size_t out_len, bool fast) {
    if (response_complete) {
        strncpy(out, response_buf.c_str(), out_len - 1);
        out[out_len - 1] = '\0';
        response_buf.clear();
        response_complete = false;
        response_has_data = false;
        return strlen(out) > 0;
    }

    if (fast && response_has_data) {
        if (response_buf.length() >= 8) {
            int space_count = 0;
            for (int i = 0; i < (int)response_buf.length(); i++) {
                if (response_buf[i] == ' ') space_count++;
            }
            if (space_count >= 3) {
                strncpy(out, response_buf.c_str(), out_len - 1);
                out[out_len - 1] = '\0';
                response_buf.clear();
                response_complete = false;
                response_has_data = false;
                return strlen(out) > 0;
            }
        }
    }

    out[0] = '\0';
    return false;
}

// ─── Parse ─────────────────────────────────────────────────────────

static int hex_vals(const char *raw, int *out, int max_vals) {
    int count = 0;
    while (*raw && count < max_vals) {
        while (*raw == ' ') raw++;
        if (!*raw) break;
        int val = 0;
        bool found = false;
        while (*raw && *raw != ' ') {
            char c = toupper((unsigned char)*raw);
            int digit = (c >= '0' && c <= '9') ? c - '0' : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
            if (digit < 0) break;
            val = val * 16 + digit;
            found = true;
            raw++;
        }
        if (found) out[count++] = val;
    }
    return count;
}

void OBD2BLE::parse_response(const char *raw) {
    if (!raw || !raw[0]) return;

    String r = String(raw);
    r.trim();
    if (r.startsWith("NO DATA") || r.startsWith("SEARCHING") ||
        r.startsWith("STOPPED") || r.startsWith("ERROR") ||
        r.startsWith("7F") || r.length() < 4) {
        return;
    }

    const char *data_ptr = raw;
    while (*data_ptr == ' ') data_ptr++;

    int bytes[8];
    int count = hex_vals(data_ptr, bytes, 8);
    if (count < 2) return;

    int pid = bytes[1];
    switch (pid) {
        case 0x0C: if (count >= 4) data.rpm = ((bytes[2] * 256) + bytes[3]) / 4.0f; break;
        case 0x0D: if (count >= 3) data.speed = bytes[2]; break;
        case 0x05: if (count >= 3) data.coolant_temp = bytes[2] - 40.0f; break;
        case 0x2F: if (count >= 3) data.fuel_level = bytes[2] * 100.0f / 255.0f; break;
        case 0x42: if (count >= 3) data.battery_voltage = (bytes[2] * 256 + bytes[3]) / 1000.0f; break;
    }
}

// ─── Disconnect ────────────────────────────────────────────────────

void OBD2BLE::disconnect() {
    data.connected = false;
    obd_device = nullptr;
    if (client) {
        if (client->isConnected()) {
            client->disconnect();
        }
        // Do NOT deleteClient here — causes heap corruption in NimBLE 2.5.0
        // when called from error paths. The BLE stack will GC it.
        client = nullptr;
    }
    service = nullptr;
    write_char = nullptr;
    notify_char = nullptr;
}
