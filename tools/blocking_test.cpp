#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>

int main() {
    setbuf(stdout, NULL);
    const char* port = "/dev/cu.VEEPEAK";

    printf("Opening %s (BLOCKING mode)...\n", port);
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0) { perror("open"); return 1; }

    termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~CRTSCTS;
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    options.c_cc[VMIN] = 1;   // block until at least 1 byte
    options.c_cc[VTIME] = 50; // 5 second timeout per read
    tcsetattr(fd, TCSANOW, &options);
    tcflush(fd, TCIOFLUSH);

    printf("Sleeping 3s for BT to settle...\n");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    tcflush(fd, TCIOFLUSH);

    // send AT Z with \r
    printf("Sending 'AT Z\\r'...\n");
    const char* cmd = "AT Z\r";
    write(fd, cmd, strlen(cmd));
    tcdrain(fd);

    printf("Reading (blocking, 5s timeout per byte)...\n");
    char buf[512];
    int total = 0;
    auto start = std::chrono::steady_clock::now();
    while (true) {
        int bytes = read(fd, buf + total, 1); // read 1 byte at a time
        if (bytes > 0) {
            total += bytes;
            if (buf[total - 1] == '>') {
                printf("  Got prompt '>'\n");
                break;
            }
        } else {
            printf("  read() returned %d\n", bytes);
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > 10000) {
            printf("  Timeout after %lldms\n", elapsed);
            break;
        }
    }

    if (total > 0) {
        buf[total] = '\0';
        printf("Response (%d bytes): [", total);
        for (int i = 0; i < total; i++) {
            if (buf[i] >= 32 && buf[i] < 127)
                printf("%c", buf[i]);
            else
                printf("\\x%02x", (unsigned char)buf[i]);
        }
        printf("]\n");
    } else {
        printf("NO RESPONSE\n");
    }

    close(fd);
    return 0;
}
