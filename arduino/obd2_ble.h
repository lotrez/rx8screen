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
    void loop();  // Never blocks. One quick state step per call.
    bool is_connected() const { return state == State::POLLING_IDLE || state == State::POLLING_WAIT; }
    const Obd2Data &get_data() const { return data; }

private:
    void disconnect();

    enum class State {
        IDLE,
        SCAN_START,
        SCAN_WAIT,
        CONNECT,
        DISCOVER,
        SUBSCRIBE,
        INIT_ELM_SEND,
        INIT_ELM_WAIT,
        POLLING_IDLE,
        POLLING_WAIT,
        RECONNECT
    };

    static constexpr const char *DEVICE_NAME = "VEEPEAK";
    static constexpr const char *SERVICE_UUID = "FFF0";
    static constexpr const char *WRITE_CHAR_UUID = "FFF2";
    static constexpr const char *NOTIFY_CHAR_UUID = "FFF1";

    State state = State::IDLE;
    uint32_t state_entered_ms = 0;
    uint32_t last_tx_ms = 0;
    int init_step = 0;
    int current_pid_index = 0;
    bool found_target = false;
    NimBLEAddress target_addr;

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
    void step_scan_start();
    void step_scan_wait();
    void step_connect();
    void step_discover();
    void step_subscribe();
    void step_init_elm_send();
    void step_init_elm_wait();
    void step_polling_idle();
    void step_polling_wait();
    void step_reconnect();

    void send_command(const char *cmd);
    void on_notify(const uint8_t *data, size_t len);
    void parse_response(const char *response);

    static void notify_callback(NimBLERemoteCharacteristic *pChar,
                                 uint8_t *pData, size_t length, bool isNotify);

    static OBD2BLE *instance;

    static constexpr const char *INIT_CMDS[] = {
        "ATZ",
        "ATE0",
        "ATH0",
        "ATL0",
        "ATSP0",
        "ATST10",
        "0100",
    };
    static constexpr int INIT_CMD_COUNT = sizeof(INIT_CMDS) / sizeof(INIT_CMDS[0]);

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
