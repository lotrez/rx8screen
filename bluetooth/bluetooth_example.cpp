#include <Arduino.h>
#include <NimBLEDevice.h>

#define OBD_TIMEOUT_MS 2000
#define OBD_INIT_RETRIES 3
#define MAX_RESPONSE_LEN 256

static NimBLEAdvertisedDevice *obdDevice = nullptr;
static NimBLEClient *pClient = nullptr;
static NimBLERemoteCharacteristic *pObdWriteChar = nullptr;
static NimBLERemoteCharacteristic *pObdNotifyChar = nullptr;

static String responseBuf;
static bool responseComplete = false;
static bool responseHasData = false;

static void notifyCb(NimBLERemoteCharacteristic *pChar, uint8_t *pData, size_t length, bool isNotify) {
    for (size_t i = 0; i < length; i++) {
        char c = (char)pData[i];
        if (c == '\r' || c == '\n' || c == '\0') continue;
        if (c == '>') {
            responseComplete = true;
            return;
        }
        responseBuf += c;
        if (responseBuf.length() >= 8) {
            responseHasData = true;
        }
    }
}

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient *pClient) override {
        Serial.println("  BLE connected");
    }
    void onDisconnect(NimBLEClient *pClient, int reason) override {
        Serial.printf("  BLE disconnected (%d)\n", reason);
        pObdWriteChar = nullptr;
        pObdNotifyChar = nullptr;
    }
};

static bool connectToObd() {
    Serial.println("Scanning for VEEPEAK...");

    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    NimBLEScanResults results = pScan->getResults(15 * 1000);

    obdDevice = nullptr;
    for (int i = 0; i < results.getCount(); i++) {
        const NimBLEAdvertisedDevice *cdev = results.getDevice(i);
        std::string name = cdev->getName();
        std::string nu = name;
        for (auto &c : nu) c = toupper(c);

        if (nu.find("VEEPEAK") != std::string::npos ||
            nu.find("VEEPEEK") != std::string::npos ||
            nu.find("OBD") != std::string::npos ||
            nu.find("ELM") != std::string::npos) {
            obdDevice = new NimBLEAdvertisedDevice(*cdev);
            Serial.printf("  Found: %s [%s]\n", name.c_str(), cdev->getAddress().toString().c_str());
            break;
        }
    }

    if (!obdDevice) {
        Serial.println("  Not found!");
        pScan->clearResults();
        return false;
    }
    pScan->clearResults();

    Serial.printf("  Connecting to %s...\n", obdDevice->getAddress().toString().c_str());

    if (pClient) {
        if (pClient->isConnected()) pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
    }
    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks());

    if (!pClient->connect(obdDevice)) {
        Serial.println("  Connect failed!");
        return false;
    }

    NimBLERemoteService *pService = nullptr;
    const char *svcs[] = {"fff0", "FFE0", "ffe0"};
    for (auto s : svcs) {
        pService = pClient->getService(s);
        if (pService) break;
    }

    if (!pService) {
        auto all = pClient->getServices(true);
        if (all.size() > 0) pService = all.front();
    }

    if (!pService) {
        Serial.println("  Service not found!");
        pClient->disconnect();
        return false;
    }

    auto chars = pService->getCharacteristics(true);
    for (auto chr : chars) {
        std::string uid = chr->getUUID().toString();
        if (uid == "0xfff2") pObdWriteChar = chr;
        if (uid == "0xfff1") pObdNotifyChar = chr;
    }
    if (!pObdWriteChar) {
        for (auto chr : chars) {
            if (chr->canWrite()) { pObdWriteChar = chr; break; }
        }
    }
    if (!pObdNotifyChar) {
        for (auto chr : chars) {
            if (chr->canNotify()) { pObdNotifyChar = chr; break; }
        }
    }

    if (!pObdWriteChar) {
        Serial.println("  No write char!");
        pClient->disconnect();
        return false;
    }

    if (pObdNotifyChar && pObdNotifyChar->canNotify()) {
        pObdNotifyChar->subscribe(true, notifyCb);
    }

    Serial.printf("  Ready (W:%s N:%s)\n",
        pObdWriteChar->getUUID().toString().c_str(),
        pObdNotifyChar ? pObdNotifyChar->getUUID().toString().c_str() : "-");
    return true;
}

static bool sendObdCmd(const char *cmd, char *response, size_t respLen, bool fast = false) {
    if (!pObdWriteChar || !pClient->isConnected()) return false;

    responseBuf.clear();
    responseComplete = false;
    responseHasData = false;

    String fullCmd = String(cmd) + "\r";
    pObdWriteChar->writeValue((uint8_t *)fullCmd.c_str(), fullCmd.length(), false);

    unsigned long start = micros();
    if (fast) {
        while (!responseComplete && (micros() - start) < OBD_TIMEOUT_MS * 1000ULL) {
            if (responseHasData && !responseComplete) {
                String buf = responseBuf;
                if (buf.length() >= 8) {
                    int spaceCount = 0;
                    for (int i = 0; i < buf.length(); i++) {
                        if (buf[i] == ' ') spaceCount++;
                    }
                    if (spaceCount >= 3) {
                        delay(2);
                        break;
                    }
                }
            }
            yield();
        }
    } else {
        while (!responseComplete && (micros() - start) < OBD_TIMEOUT_MS * 1000ULL) {
            yield();
        }
    }

    strncpy(response, ((String)responseBuf).c_str(), respLen - 1);
    response[respLen - 1] = '\0';
    responseBuf.clear();
    responseComplete = false;
    responseHasData = false;

    return strlen(response) > 0;
}

static bool sendAtCmd(const char *cmd) {
    char resp[MAX_RESPONSE_LEN];
    return sendObdCmd(cmd, resp, sizeof(resp));
}

static bool initObd() {
    Serial.println("Init OBD2...");
    delay(200);

    sendAtCmd("ATE0");
    sendAtCmd("ATH0");
    sendAtCmd("ATL0");

    char resp[MAX_RESPONSE_LEN] = {0};
    sendObdCmd("0100", resp, sizeof(resp));
    String r = String(resp);
    r.trim();
    if (r.length() > 4 && !r.startsWith("SEARCHING") &&
        !r.startsWith("STOPPED") && !r.startsWith("NO DATA") &&
        !r.startsWith("ERROR")) {
        Serial.printf("  ECU ready: \"%s\"\n", resp);
    } else {
        Serial.printf("  Warm failed (\"%s\"), cold reset...\n", resp);
        sendAtCmd("ATZ");
        delay(2000);
        sendAtCmd("ATE0");
        sendAtCmd("ATH0");
        sendAtCmd("ATL0");
        sendAtCmd("ATSP0");
        delay(300);

        bool found = false;
        for (int i = 0; i < 5 && !found; i++) {
            memset(resp, 0, sizeof(resp));
            sendObdCmd("0100", resp, sizeof(resp));
            r = String(resp);
            r.trim();
            if (r.length() > 4 && !r.startsWith("SEARCHING") &&
                !r.startsWith("STOPPED") && !r.startsWith("NO DATA")) {
                found = true;
            }
            if (!found) delay(1000);
        }

        if (!found) {
            const char *protos[] = {"ATSP6", "ATSP7", "ATSP8", "ATSP9"};
            const char *names[]  = {"CAN 11/500", "CAN 29/500", "CAN 11/250", "CAN 29/250"};
            for (int i = 0; i < 4 && !found; i++) {
                sendAtCmd(protos[i]);
                memset(resp, 0, sizeof(resp));
                sendObdCmd("0100", resp, sizeof(resp));
                r = String(resp);
                r.trim();
                found = r.length() > 2 && !r.startsWith("SEARCHING") &&
                        !r.startsWith("STOPPED") && !r.startsWith("NO DATA");
                Serial.printf("  %s: %s %s\n", names[i], resp, found ? "OK" : "");
            }
        }
        if (!found) {
            Serial.println("  ECU connect failed!");
            return false;
        }
    }

    sendAtCmd("ATST10");
    sendAtCmd("ATDPN");
    delay(50);

    Serial.println("  OBD2 ready.");
    return true;
}

static int hexVals(const char *raw, int *out, int maxVals) {
    int count = 0;
    while (*raw && count < maxVals) {
        while (*raw == ' ') raw++;
        if (!*raw) break;
        int val = 0;
        bool found = false;
        while (*raw && *raw != ' ') {
            char c = toupper(*raw);
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

struct ObdPid {
    const char *name;
    const char *cmd;
    const char *unit;
    float (*decoder)(const char *raw);
    bool isAtCmd;
    int expectedBytes;
};

static float decodeRpm(const char *raw) {
    int v[4]; if (hexVals(raw, v, 4) >= 4) return ((v[2] * 256) + v[3]) / 4.0;
    return -1;
}
static float decodeSpeed(const char *raw) {
    int v[3]; if (hexVals(raw, v, 3) >= 3) return (float)v[2];
    return -1;
}
static float decodeTemp(const char *raw) {
    int v[3]; if (hexVals(raw, v, 3) >= 3) return (float)v[2] - 40.0;
    return -999;
}
static float decodePercent(const char *raw) {
    int v[3]; if (hexVals(raw, v, 3) >= 3) return (v[2] * 100.0) / 255.0;
    return -1;
}
static float decodeTimingAdvance(const char *raw) {
    int v[3]; if (hexVals(raw, v, 3) >= 3) return (v[2] / 2.0) - 64.0;
    return -999;
}
static float decodeMaf(const char *raw) {
    int v[4]; if (hexVals(raw, v, 4) >= 4) return ((v[2] * 256) + v[3]) / 100.0;
    return -1;
}
static float decodeVoltage(const char *raw) {
    float v = 0;
    if (sscanf(raw, "%f", &v) == 1 && v > 0) return v;
    return -1;
}

static const ObdPid PIDS[] = {
    {"RPM",     "010C", "rpm",  decodeRpm,          false, 4},
    {"Speed",   "010D", "km/h", decodeSpeed,        false, 3},
    {"Coolant", "0105", "C",    decodeTemp,         false, 3},
    {"Load",    "0104", "%",    decodePercent,      false, 3},
    {"Throttle","0111", "%",    decodePercent,      false, 3},
    {"Timing",  "010E", "deg",  decodeTimingAdvance,false, 3},
    {"MAF",     "0110", "g/s",  decodeMaf,          false, 4},
};
static const int NUM_PIDS = sizeof(PIDS) / sizeof(PIDS[0]);

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println("\n=== ESP32-S3 OBD2 Fast Scanner ===\n");

    NimBLEDevice::init("ESP32-OBD2");

    bool connected = false;
    for (int i = 0; i < 5 && !connected; i++) {
        Serial.printf("Connect %d/5\n", i + 1);
        connected = connectToObd();
        if (!connected) delay(3000);
    }

    if (!connected) {
        Serial.println("FAILED. Restarting...");
        delay(10000);
        ESP.restart();
    }

    bool obdReady = false;
    for (int i = 0; i < OBD_INIT_RETRIES && !obdReady; i++) {
        obdReady = initObd();
        if (!obdReady) { Serial.printf("Retry %d\n", i + 1); delay(2000); }
    }

    if (!obdReady) {
        Serial.println("OBD2 init failed. Restarting...");
        delay(10000);
        ESP.restart();
    }

    Serial.println("\n=== BENCHMARK ===\n");

    unsigned long times[30];
    for (int i = 0; i < 30; i++) {
        char resp[MAX_RESPONSE_LEN] = {0};
        unsigned long t0 = micros();
        sendObdCmd("010C", resp, sizeof(resp), true);
        times[i] = micros() - t0;
    }
    unsigned long sum = 0, mn = 999999, mx = 0;
    for (int i = 0; i < 30; i++) {
        sum += times[i];
        if (times[i] < mn) mn = times[i];
        if (times[i] > mx) mx = times[i];
    }
    Serial.printf("Single PID (fast mode): avg=%luus min=%luus max=%luus -> %.1f Hz\n\n",
        sum / 30, mn, mx, 1000000.0 / (sum / 30.0));

    unsigned long tStart = millis();
    int count = 0;
    while (millis() - tStart < 5000) {
        char resp[MAX_RESPONSE_LEN] = {0};
        sendObdCmd("010C", resp, sizeof(resp), true);
        count++;
    }
    Serial.printf("Sustained 5s: %d req = %.1f Hz\n\n", count, count / 5.0);

    Serial.println("--- Live data ---\n");
}

void loop() {
    if (!pClient || !pClient->isConnected()) {
        Serial.println("BLE lost! Reconnecting...");
        connectToObd();
        if (pClient && pClient->isConnected()) initObd();
        return;
    }

    unsigned long cycleStart = micros();

    for (int i = 0; i < NUM_PIDS; i++) {
        char resp[MAX_RESPONSE_LEN] = {0};

        if (PIDS[i].isAtCmd) {
            sendObdCmd(PIDS[i].cmd, resp, sizeof(resp), false);
        } else {
            sendObdCmd(PIDS[i].cmd, resp, sizeof(resp), true);
        }

        String r = String(resp);
        r.trim();
        if (r.startsWith("NO DATA") || r.startsWith("SEARCHING") ||
            r.startsWith("STOPPED") || r.startsWith("ERROR") ||
            r.startsWith("7F") || r.length() == 0) {
            Serial.printf("%-10s ", "---");
            continue;
        }

        const char *data = resp;
        while (*data == ' ') data++;

        float value = PIDS[i].decoder(data);
        if (value > -900) {
            if (strcmp(PIDS[i].unit, "rpm") == 0) {
                Serial.printf("%-4s:%4.0f ", PIDS[i].name, value);
            } else {
                Serial.printf("%-4s:%5.1f ", PIDS[i].name, value);
            }
        } else {
            Serial.printf("%-10s ", "---");
        }
    }

    unsigned long cycleTime = micros() - cycleStart;
    Serial.printf("| %luus %.0fHz\n", cycleTime, 1000000.0 / cycleTime);
}
