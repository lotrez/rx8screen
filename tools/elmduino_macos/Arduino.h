#pragma once

// Minimal Arduino compatibility layer for macOS
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

// Arduino types
typedef uint8_t byte;

// F() macro - just pass through on desktop
#define F(x) x

// String class
typedef std::string String;

// Timing
inline unsigned long millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline void delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Stream base class
class Stream {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual size_t write(uint8_t byte) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;

    size_t print(const char* str) {
        return write((const uint8_t*)str, strlen(str));
    }

    size_t println(const char* str) {
        size_t n = write((const uint8_t*)str, strlen(str));
        n += write((const uint8_t*)"\r\n", 2);
        return n;
    }

    size_t print(int val) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", val);
        return print(buf);
    }

    size_t println(int val) {
        size_t n = print(val);
        n += write((const uint8_t*)"\r\n", 2);
        return n;
    }

    size_t print(float val, int decimals = 2) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        return print(buf);
    }

    size_t println(float val, int decimals = 2) {
        size_t n = print(val, decimals);
        n += write((const uint8_t*)"\r\n", 2);
        return n;
    }

    size_t print(double val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", val);
        return print(buf);
    }

    size_t println(double val) {
        size_t n = print(val);
        n += write((const uint8_t*)"\r\n", 2);
        return n;
    }

    size_t print(unsigned long val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lu", val);
        return print(buf);
    }

    size_t println(unsigned long val) {
        size_t n = print(val);
        n += write((const uint8_t*)"\r\n", 2);
        return n;
    }

    size_t print(long val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", val);
        return print(buf);
    }

    size_t println(long val) {
        size_t n = print(val);
        n += write((const uint8_t*)"\r\n", 2);
        return n;
    }

    size_t print(uint32_t val) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u", val);
        return print(buf);
    }

    size_t println(uint32_t val) {
        size_t n = print(val);
        n += write((const uint8_t*)"\r\n", 2);
        return n;
    }

    size_t println() {
        return write((const uint8_t*)"\r\n", 2);
    }
};

// BluetoothSerial - wraps POSIX serial for macOS Bluetooth SPP
class BluetoothSerial : public Stream {
    int fd = -1;
    bool connected = false;

public:
    bool begin(const char* port, int baud = 115200) {
        fd = ::open(port, O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd < 0) {
            perror("  [ERROR] open serial port");
            return false;
        }
        fcntl(fd, F_SETFL, 0);

        termios options;
        tcgetattr(fd, &options);
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
        options.c_cc[VTIME] = 1;
        tcsetattr(fd, TCSANOW, &options);
        tcflush(fd, TCIOFLUSH);

        // Wait for BT RFCOMM to fully establish
        delay(3000);
        tcflush(fd, TCIOFLUSH);

        connected = true;
        return true;
    }

    int available() override {
        if (fd < 0) return 0;
        int count = 0;
        if (ioctl(fd, FIONREAD, &count) < 0) return 0;
        return count;
    }

    int read() override {
        if (fd < 0) return -1;
        uint8_t byte_val;
        ssize_t n = ::read(fd, &byte_val, 1);
        return (n > 0) ? (int)byte_val : -1;
    }

    size_t write(uint8_t byte_val) override {
        if (fd < 0) return 0;
        return ::write(fd, &byte_val, 1);
    }

    size_t write(const uint8_t* buffer, size_t size) override {
        if (fd < 0) return 0;
        ssize_t written = ::write(fd, buffer, size);
        tcdrain(fd);
        return (written > 0) ? (size_t)written : 0;
    }

    void end() {
        if (fd >= 0) ::close(fd);
        fd = -1;
        connected = false;
    }

    bool hasClient() { return connected; }
};

// Global Serial instance (declared extern, defined in main.cpp)
extern BluetoothSerial Serial;
