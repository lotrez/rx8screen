#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

struct Obd2Data {
    float rpm = 0.0f;
    float speed = 0.0f;
    float coolant_temp = 0.0f;
    float fuel_level = 0.0f;
    float battery_voltage = 0.0f;
    bool connected = false;
};

class OBD2BLE {
public:
    void begin();
    bool connect_blocking();
    void loop();
    bool is_connected() const;
    const char *get_state_name() const;
    const char *get_status_detail() const { return status_detail; }
    const Obd2Data &get_data() const { return data; }

private:
    void disconnect();

    enum class State {
        IDLE,
        SCAN_START,
        POLLING,
        RECONNECT
    };

    State state = State::IDLE;
    uint32_t state_entered_ms = 0;
    int current_pid_index = 0;
    uint32_t _last_pid_time = 0;
    uint32_t _last_slow_pid_time = 0;

    NimBLEAdvertisedDevice *obd_device = nullptr;
    NimBLEClient *client = nullptr;
    NimBLERemoteService *service = nullptr;
    NimBLERemoteCharacteristic *write_char = nullptr;
    NimBLERemoteCharacteristic *notify_char = nullptr;

    String response_buf;
    bool response_complete = false;
    bool response_has_data = false;

    char status_detail[64];
    Obd2Data data;

    void set_state(State new_state);
    bool init_elm_blocking();
    bool send_cmd_blocking(const char *cmd, bool fast, char *out = nullptr, size_t out_len = 0);
    void step_scan_start();
    void step_polling();
    void step_reconnect();

    void send_command(const char *cmd);
    void on_notify(uint8_t *pData, size_t length);
    bool read_response(char *out, size_t out_len, bool fast);
    void parse_response(const char *response);

    static void notify_cb(NimBLERemoteCharacteristic *pChar,
                          uint8_t *pData, size_t length, bool isNotify);

    static OBD2BLE *instance;

    // Only PIDs displayed on the dashboard
    static constexpr const char *PID_TABLE[] = {
        "010C",   // RPM
        "010D",   // Vehicle Speed
        "0105",   // Engine Coolant Temp
        "0142",   // Control Module Voltage (battery)
        "012F",   // Fuel Tank Level
    };
    static constexpr int PID_COUNT = sizeof(PID_TABLE) / sizeof(PID_TABLE[0]);
};
