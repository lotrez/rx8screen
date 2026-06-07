#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <sstream>
#include <functional>

class SerialPort {
    int port_fd = -1;
    friend class ELM327Client;
public:
    bool open_port(const char* path, int baud = 115200) {
        port_fd = ::open(path, O_RDWR | O_NOCTTY | O_NDELAY);
        if (port_fd < 0) {
            perror("  [ERROR] open");
            return false;
        }
        fcntl(port_fd, F_SETFL, 0);

        termios options;
        tcgetattr(port_fd, &options);
        cfsetispeed(&options, baud);
        cfsetospeed(&options, baud);
        options.c_cflag |= (CLOCAL | CREAD);
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;
        options.c_cflag &= ~CRTSCTS;
        options.c_iflag &= ~(IXON | IXOFF | IXANY);
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        options.c_oflag &= ~OPOST;
        options.c_cc[VMIN] = 0;
        options.c_cc[VTIME] = 5;
        tcsetattr(port_fd, TCSANOW, &options);
        tcflush(port_fd, TCIOFLUSH);
        return true;
    }

    void close_port() {
        if (port_fd >= 0) ::close(port_fd);
        port_fd = -1;
    }

    bool send_cmd(const std::string& cmd) {
        std::string full = cmd + "\r";
        ssize_t written = write(port_fd, full.c_str(), full.size());
        tcdrain(port_fd);
        return written == (ssize_t)full.size();
    }

    std::string read_until_prompt(int timeout_ms = 1000) {
        std::string result;
        auto start = std::chrono::steady_clock::now();
        char buf[512];
        while (true) {
            ssize_t bytes = read(port_fd, buf, sizeof(buf) - 1);
            if (bytes > 0) {
                buf[bytes] = '\0';
                result += buf;
                if (result.find('>') != std::string::npos) break;
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return result;
    }

    ~SerialPort() { close_port(); }
};

struct OBDReading {
    std::string name;
    std::string raw_response;
    float value;
    std::string unit;
    bool valid;
};

class ELM327Client {
    SerialPort serial;
    bool is_connected = false;

    std::string send_at(const std::string& cmd, int timeout_ms = 1000) {
        tcflush(serial.port_fd, TCIOFLUSH);
        if (!serial.send_cmd(cmd)) {
            printf("  [ERROR] send failed: %s\n", cmd.c_str());
            return "";
        }
        std::string raw = serial.read_until_prompt(timeout_ms);
        return clean_response(raw);
    }

    std::string clean_response(const std::string& raw) {
        std::string clean;
        for (char ch : raw) {
            if (ch == '\r') clean += ' ';
            else if (ch == '\n') clean += ' ';
            else if (ch == '>') {}
            else clean += ch;
        }
        while (clean.size() > 1 && clean.front() == ' ') clean.erase(0, 1);
        while (clean.size() > 1 && clean.back() == ' ') clean.pop_back();
        while (clean.find("  ") != std::string::npos)
            clean.replace(clean.find("  "), 2, " ");
        return clean;
    }

    std::vector<int> parse_hex_bytes(const std::string& response) {
        std::vector<std::string> tokens;
        std::istringstream stream(response);
        std::string token;
        while (stream >> token) tokens.push_back(token);

        std::vector<int> bytes;
        for (const auto& tok : tokens) {
            for (size_t pos = 0; pos + 1 < tok.size(); pos += 2) {
                std::string hex_pair = tok.substr(pos, 2);
                char* end;
                long val = strtol(hex_pair.c_str(), &end, 16);
                if (end == hex_pair.c_str() + 2) bytes.push_back((int)val);
            }
        }
        return bytes;
    }

public:
    bool connect(const char* port_path, int baud = 115200) {
        printf("Connecting to ELM327 on %s @ %d baud...\n", port_path, baud);

        if (!serial.open_port(port_path, baud)) {
            printf("[FAIL] Could not open serial port\n");
            return false;
        }

        printf("  Waiting for Bluetooth RFCOMM to settle...\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        tcflush(serial.port_fd, TCIOFLUSH);

        printf("  [1/7] Set defaults (AT D)...\n");
        std::string resp = send_at("AT D", 1000);
        printf("         [%s]\n", resp.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        printf("  [2/7] Reset (AT Z)...\n");
        resp = send_at("AT Z", 3000);
        printf("         [%s]\n", resp.c_str());

        if (resp.find("ELM") == std::string::npos) {
            printf("  No ELM response, retrying...\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            resp = send_at("AT Z", 3000);
            printf("         [%s]\n", resp.c_str());
        }

        if (resp.find("ELM") == std::string::npos) {
            printf("  [WARN] Trying 38400 baud...\n");
            serial.close_port();
            if (!serial.open_port(port_path, 38400)) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            resp = send_at("AT Z", 3000);
            printf("         [%s]\n", resp.c_str());
            if (resp.find("ELM") == std::string::npos) {
                printf("[FAIL] Cannot communicate with ELM327\n");
                return false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        printf("  [3/7] Echo off (AT E0)...\n");
        resp = send_at("AT E0", 500);
        printf("         [%s]\n", resp.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        printf("  [4/7] Spaces off (AT S0)...\n");
        resp = send_at("AT S0", 500);
        printf("         [%s]\n", resp.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        printf("  [5/7] Allow long messages (AT AL)...\n");
        resp = send_at("AT AL", 500);
        printf("         [%s]\n", resp.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        printf("  [6/7] Set auto protocol (AT SP 0)...\n");
        resp = send_at("AT SP 0", 1000);
        printf("         [%s]\n", resp.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        printf("  [7/7] Protocol search with 0100 (30s timeout)...\n");
        resp = send_at("01 00", 30000);
        printf("         [%s]\n", resp.c_str());

        if (resp.find("41") == std::string::npos) {
            printf("  [WARN] Protocol search may not have locked.\n");
        }

        printf("  Protocol: ");
        resp = send_at("AT DP", 1000);
        printf("[%s]\n", resp.c_str());

        printf("[OK] ELM327 connected!\n\n");
        is_connected = true;
        return true;
    }

    OBDReading query_pid(const std::string& pid_hex, const std::string& pid_name,
                         std::function<float(const std::vector<int>&)> parser,
                         const std::string& unit) {
        OBDReading reading;
        reading.name = pid_name;
        reading.unit = unit;
        reading.valid = false;

        if (!is_connected) return reading;

        std::string resp = send_at("01 " + pid_hex, 3000);
        reading.raw_response = resp;

        if (resp.find("SEARCHING") != std::string::npos ||
            resp.find("NO DATA") != std::string::npos ||
            resp.find("STOPPED") != std::string::npos ||
            resp.empty()) {
            return reading;
        }

        auto bytes = parse_hex_bytes(resp);
        if (bytes.size() < 2) return reading;

        std::vector<int> data(bytes.begin() + 2, bytes.end());
        if (data.empty()) return reading;

        reading.value = parser(data);
        reading.valid = true;
        return reading;
    }

    void run_single_read() {
        printf("--- Single OBD2 PID Read ---\n\n");

        auto queries = std::vector<std::tuple<std::string, std::string,
            std::function<float(const std::vector<int>&)>, std::string>>{
            {"0C", "Engine RPM", [](const std::vector<int>& b) {
                return ((b[0] * 256.0f) + b[1]) / 4.0f; }, "rpm"},
            {"0D", "Vehicle Speed", [](const std::vector<int>& b) {
                return (float)b[0]; }, "km/h"},
            {"05", "Coolant Temp", [](const std::vector<int>& b) {
                return (float)b[0] - 40.0f; }, "°C"},
            {"0F", "Intake Air Temp", [](const std::vector<int>& b) {
                return (float)b[0] - 40.0f; }, "°C"},
            {"11", "Throttle Position", [](const std::vector<int>& b) {
                return (float)b[0] * 100.0f / 255.0f; }, "%"},
            {"04", "Engine Load", [](const std::vector<int>& b) {
                return (float)b[0] * 100.0f / 255.0f; }, "%"},
            {"0B", "MAP", [](const std::vector<int>& b) {
                return (float)b[0]; }, "kPa"},
            {"2F", "Fuel Level", [](const std::vector<int>& b) {
                return (float)b[0] * 100.0f / 255.0f; }, "%"},
            {"42", "Battery Voltage", [](const std::vector<int>& b) {
                return ((b[0] * 256.0f) + b[1]) / 1000.0f; }, "V"},
        };

        for (const auto& [pid, name, parser, unit] : queries) {
            auto reading = query_pid(pid, name, parser, unit);
            if (reading.valid) {
                printf("  %-20s: %.1f %s\n", name.c_str(), reading.value, unit.c_str());
            } else {
                printf("  %-20s: NO DATA (raw: '%s')\n", name.c_str(), reading.raw_response.c_str());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void run_live_loop() {
        struct Query {
            const char* pid;
            const char* name;
            std::function<float(const std::vector<int>&)> parser;
            const char* unit;
            const char* format;
        };

        std::vector<Query> queries = {
            {"0C", "RPM", [](const std::vector<int>& b) {
                return ((b[0] * 256.0f) + b[1]) / 4.0f;
            }, "rpm", "%.0f"},
            {"0D", "Speed", [](const std::vector<int>& b) {
                return (float)b[0];
            }, "km/h", "%.0f"},
            {"05", "Coolant", [](const std::vector<int>& b) {
                return (float)b[0] - 40.0f;
            }, "C", "%.1f"},
            {"0F", "Intake", [](const std::vector<int>& b) {
                return (float)b[0] - 40.0f;
            }, "C", "%.1f"},
            {"11", "Throttle", [](const std::vector<int>& b) {
                return (float)b[0] * 100.0f / 255.0f;
            }, "%", "%.1f"},
            {"04", "Load", [](const std::vector<int>& b) {
                return (float)b[0] * 100.0f / 255.0f;
            }, "%", "%.1f"},
            {"0B", "MAP", [](const std::vector<int>& b) {
                return (float)b[0];
            }, "kPa", "%.0f"},
            {"42", "Battery", [](const std::vector<int>& b) {
                return ((b[0] * 256.0f) + b[1]) / 1000.0f;
            }, "V", "%.2f"},
        };

        printf("Live mode - press Ctrl+C to stop\n\n");

        while (true) {
            printf("\033[2J\033[H");
            printf("=== RX-8 OBD2 Live ===\n\n");

            for (const auto& q : queries) {
                auto reading = query_pid(q.pid, q.name, q.parser, q.unit);
                if (reading.valid) {
                    char val_str[32];
                    snprintf(val_str, sizeof(val_str), q.format, reading.value);
                    printf("  %-12s: %s %s", q.name, val_str, q.unit);

                    if (std::string(q.name) == "Coolant" && reading.value > 105.0f)
                        printf("  *** OVERHEAT ***");
                    if (std::string(q.name) == "RPM" && reading.value > 8500.0f)
                        printf("  *** REDLINE ***");
                    if (std::string(q.name) == "Battery" && reading.value < 11.0f)
                        printf("  *** LOW VOLT ***");
                    printf("\n");
                } else {
                    printf("  %-12s: ---\n", q.name);
                }
            }

            fflush(stdout);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
};

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);

    const char* port = "/dev/cu.VEEPEAK";
    int baud = 115200;
    bool live = false;

    for (int index = 1; index < argc; index++) {
        std::string arg = argv[index];
        if (arg == "--port" && index + 1 < argc) port = argv[++index];
        else if (arg == "--baud" && index + 1 < argc) baud = std::atoi(argv[++index]);
        else if (arg == "--live") live = true;
        else if (arg == "--help") {
            printf("Usage: %s [--port /dev/cu.XXX] [--baud 115200] [--live]\n", argv[0]);
            return 0;
        }
    }

    ELM327Client elm;
    if (!elm.connect(port, baud)) {
        printf("\nFailed to connect. Make sure:\n");
        printf("  1. ELM327 is plugged into OBD2 port\n");
        printf("  2. Ignition is ON\n");
        printf("  3. Bluetooth paired (check /dev/cu.*)\n");
        return 1;
    }

    if (live) elm.run_live_loop();
    else elm.run_single_read();

    return 0;
}
