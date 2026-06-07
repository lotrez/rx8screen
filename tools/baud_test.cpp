#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>

void set_baud(int fd, int baud) {
    speed_t baud_rate;
    switch (baud) {
        case 9600:   baud_rate = B9600; break;
        case 38400:  baud_rate = B38400; break;
        case 115200: baud_rate = B115200; break;
        default:     baud_rate = B115200; break;
    }
    termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, baud_rate);
    cfsetospeed(&options, baud_rate);
    tcsetattr(fd, TCSANOW, &options);
    tcflush(fd, TCIOFLUSH);
}

std::string try_cmd(int fd, const char* cmd, int timeout_ms) {
    write(fd, cmd, strlen(cmd));
    tcdrain(fd);

    std::string result;
    auto start = std::chrono::steady_clock::now();
    char buf[512];
    while (true) {
        int bytes = read(fd, buf, sizeof(buf) - 1);
        if (bytes > 0) {
            buf[bytes] = '\0';
            result += buf;
            if (result.find('>') != std::string::npos) break;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return result;
}

void test_baud(const char* port, int baud) {
    printf("\n--- Trying %d baud ---\n", baud);
    int fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) { perror("open"); return; }
    fcntl(fd, F_SETFL, 0);

    termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, (baud == 9600) ? B9600 : (baud == 38400) ? B38400 : B115200);
    cfsetospeed(&options, (baud == 9600) ? B9600 : (baud == 38400) ? B38400 : B115200);
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

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Try AT Z
    std::string resp = try_cmd(fd, "AT Z\r", 3000);
    printf("  AT Z response: [%s]\n", resp.c_str());

    if (resp.find("ELM") != std::string::npos) {
        printf("  *** SUCCESS at %d baud ***\n", baud);

        // Try a few more commands
        resp = try_cmd(fd, "AT E0\r", 1000);
        printf("  AT E0: [%s]\n", resp.c_str());

        resp = try_cmd(fd, "AT I\r", 1000);
        printf("  AT I: [%s]\n", resp.c_str());

        resp = try_cmd(fd, "01 0C\r", 5000);
        printf("  01 0C (RPM): [%s]\n", resp.c_str());
    }

    close(fd);
}

int main() {
    setbuf(stdout, NULL);
    const char* port = "/dev/cu.VEEPEAK";

    printf("Testing multiple baud rates on %s...\n", port);
    printf("Make sure ignition is ON and ELM327 is plugged in.\n");

    test_baud(port, 115200);
    test_baud(port, 38400);
    test_baud(port, 9600);

    printf("\nDone.\n");
    return 0;
}
