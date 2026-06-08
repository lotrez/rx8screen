#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

struct Obd2Data {
    float rpm = 0.0f;
    float speed = 0.0f;
    float coolant_temp = 0.0f;
    float intake_temp = 0.0f;
    float engine_load = 0.0f;
    float throttle_pos = 0.0f;
    float fuel_level = 0.0f;
    float timing_advance = 0.0f;
    float maf_rate = 0.0f;
    float battery_voltage = 0.0f;
    bool connected = false;
};

class OBD2BLE {
public:
    void begin();
    void loop();
    bool is_connected() const { return state == State::CONNECTED; }
    const Obd2Data &get_data() const { return data; }

private:
    enum class State {
        IDLE,
        SCANNING,
        CONNECTING,
        DISCOVERING,
        SUBSCRIBING,
        INIT_ELM,
        CONNECTED,
        RECONNECTING
    };

    static constexpr const char *DEVICE_NAME = "VEEPEAK";
    static constexpr const char *SERVICE_UUID = "FFF0";
    static constexpr const char *WRITE_CHAR_UUID = "FFF2";
    static constexpr const char *NOTIFY_CHAR_UUID = "FFF1";

    State state = State::IDLE;
    uint32_t state_entered_ms = 0;
    uint32_t last_poll_ms = 0;
    int init_step = 0;
    int current_pid_index = 0;

    NimBLEClient *client = nullptr;
    NimBLERemoteService *service = nullptr;
    NimBLERemoteCharacteristic *write_char = nullptr;
    NimBLERemoteCharacteristic *notify_char = nullptr;

    static constexpr size_t RESPONSE_BUF_SIZE = 256;
    char response_buf[RESPONSE_BUF_SIZE];
    size_t response_len = 0;
    bool response_complete = false;

    Obd2Data data;

    void set_state(State new_state);
    bool scan_and_connect();
    bool discover_service();
    bool subscribe_notifications();
    void init_elm327();
    void poll_next_pid();
    void send_command(const char *cmd);
    void on_notify(const uint8_t *data, size_t len);
    bool wait_for_response(uint32_t timeout_ms);
    void parse_response(const char *response);

    static void notify_callback(NimBLERemoteCharacteristic *pChar,
                                 uint8_t *pData, size_t length, bool isNotify);

    // PID rotation table
    static constexpr const char *PID_TABLE[] = {
        "010C",  // RPM
        "010D",  // Speed
        "0105",  // Coolant temp
        "010F",  // Intake air temp
        "0104",  // Engine load
        "0111",  // Throttle position
        "012F",  // Fuel level
        "010E",  // Timing advance
        "0110",  // MAF rate
    };
    static constexpr int PID_COUNT = sizeof(PID_TABLE) / sizeof(PID_TABLE[0]);
};
