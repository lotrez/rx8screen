#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>

int main() {
    setbuf(stdout, NULL);
    const char* port = "/dev/tty.VEEPEAK";

    printf("Opening %s...\n", port);
    int fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) { perror("open failed"); return 1; }
    fcntl(fd, F_SETFL, 0);

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
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 5;
    tcsetattr(fd, TCSANOW, &options);
    tcflush(fd, TCIOFLUSH);

    printf("Port open, fd=%d. Sleeping 2s...\n", fd);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // drain anything buffered
    char drain[256];
    int drained = 0;
    while (true) {
        int n = read(fd, drain, sizeof(drain));
        if (n <= 0) break;
        drained += n;
    }
    printf("Drained %d bytes from buffer\n", drained);

    // send AT Z with \r
    const char* cmd = "AT Z\r";
    printf("Sending: '%s'\n", "AT Z");
    ssize_t written = write(fd, cmd, strlen(cmd));
    printf("Written %zd bytes\n", written);
    tcdrain(fd);

    printf("Reading response (5s timeout)...\n");
    auto start = std::chrono::steady_clock::now();
    char buf[512];
    int total = 0;
    while (true) {
        int bytes = read(fd, buf + total, sizeof(buf) - total - 1);
        if (bytes > 0) {
            total += bytes;
            buf[total] = '\0';
            printf("  Read %d bytes (total %d): [", bytes, total);
            for (int i = 0; i < bytes; i++) {
                if (buf[total - bytes + i] >= 32 && buf[total - bytes + i] < 127)
                    printf("%c", buf[total - bytes + i]);
                else
                    printf("\\x%02x", (unsigned char)buf[total - bytes + i]);
            }
            printf("]\n");
            if (buf[total - 1] == '>') break;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > 5000) {
            printf("  Timeout after %lldms, total bytes: %d\n", elapsed, total);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (total > 0) {
        buf[total] = '\0';
        printf("Full response (%d bytes): [", total);
        for (int i = 0; i < total; i++) {
            if (buf[i] >= 32 && buf[i] < 127)
                printf("%c", buf[i]);
            else
                printf("\\x%02x", (unsigned char)buf[i]);
        }
        printf("]\n");
    } else {
        printf("NO RESPONSE AT ALL\n");
    }

    close(fd);
    return 0;
}
