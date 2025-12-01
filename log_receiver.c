#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

int main() {
    const char *fifo = "logfifo";
    mkfifo(fifo, 0666); // tạo FIFO nếu chưa tồn tại

    printf("[LOGGER] Đang mở FIFO để đọc log...\n");
    int fd = open(fifo, O_RDONLY);
    if (fd < 0) { perror("open"); exit(1); }

    FILE *f = fopen("log.txt", "a");
    if (!f) { perror("fopen"); exit(1); }

    char buffer[512];
    while (1) {
        ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            // Ghi timestamp vào file log
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            fprintf(f, "[%02d:%02d:%02d] %s",
                    t->tm_hour, t->tm_min, t->tm_sec, buffer);
            fflush(f);

            // Hiển thị lên terminal
            printf("[LOGGER] %s", buffer);
            fflush(stdout);
        }
        else if (n == 0) {
            // Không có writer -> block chờ writer mới
            close(fd);
            fd = open(fifo, O_RDONLY);
        }
    }

    close(fd);
    fclose(f);
    unlink(fifo);
    return 0;
}

