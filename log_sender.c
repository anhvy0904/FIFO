// log_sender.c
// Giữ lại chế độ nhập tay & đọc file, thêm chế độ 3: gửi theo thời gian (mô phỏng real-time)
// Compile: gcc -o log_sender log_sender.c

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>

#define FIFO_NAME "logfifo"
#define BUF_SIZE 512

/* Thay thế hoàn toàn hàm parse_timestamp_seconds hiện tại bằng hàm sau */
long parse_timestamp_seconds(const char *s) {
    int hh, mm, ss;
    const char *p = s;
    /* tìm ký tự '[' đầu tiên trong dòng */
    while (*p && *p != '[') p++;
    if (*p == '[') {
        if (sscanf(p, "[%d:%d:%d]", &hh, &mm, &ss) == 3) {
            if (hh>=0 && hh<24 && mm>=0 && mm<60 && ss>=0 && ss<60)
                return hh*3600 + mm*60 + ss;
        }
    }
    return -1;
}


int main() {
    // ensure FIFO exists
    if (mkfifo(FIFO_NAME, 0666) < 0) {
        if (errno != EEXIST) {
            perror("mkfifo");
            return 1;
        }
    }

    printf("=== LOG_SENDER (GỬI LOG QUA FIFO) ===\n");
    printf("Chọn chế độ hoạt động:\n");
    printf("1. Nhập log thủ công\n");
    printf("2. Đọc log từ file (theo dòng, giữ tốc độ mặc định 0.3s)\n");
    printf("3. Gửi mô phỏng real-time từ file (dò timestamp hoặc theo interval)\n");
    printf("Lựa chọn của bạn (1/2/3): ");

    int choice;
    if (scanf("%d", &choice) != 1) {
        fprintf(stderr, "Lỗi: không đọc được lựa chọn.\n");
        return 1;
    }
    getchar(); // bỏ newline

    // open FIFO for writing (will block until a reader opens FIFO for reading)
    int fd = open(FIFO_NAME, O_WRONLY);
    if (fd < 0) {
        perror("Không mở được FIFO để ghi");
        exit(1);
    }

    if (choice == 1) {
        // Manual input
        printf("\n[LOG_SENDER] Nhập log (gõ 'exit' để thoát):\n");
        char line[256];
        while (1) {
            printf(">>> ");
            if (fgets(line, sizeof(line), stdin) == NULL)
                break;
            if (strncmp(line, "exit", 4) == 0)
                break;
            if (write(fd, line, strlen(line)) < 0)
                perror("write");
        }
    }
    else if (choice == 2) {
        // Simple file send (current behavior)
        char path[256];
        printf("\nNhập đường dẫn tới file cần đọc (ví dụ: input.txt): ");
        if (fgets(path, sizeof(path), stdin) == NULL) {
            fprintf(stderr, "Lỗi: không đọc được đường dẫn.\n");
            close(fd);
            return 1;
        }
        path[strcspn(path, "\n")] = '\0';
        FILE *fin = fopen(path, "r");
        if (!fin) { perror("Không mở được file đầu vào"); close(fd); exit(1); }

        printf("[LOG_SENDER] Đang gửi nội dung từ file '%s'...\n", path);
        char line[256];
        while (fgets(line, sizeof(line), fin)) {
            if (write(fd, line, strlen(line)) < 0) perror("write");
            printf("[LOG_SENDER] Gửi: %s", line);
            usleep(300000); // 0.3s như cũ
        }
        fclose(fin);
        printf("[LOG_SENDER] Gửi file hoàn tất.\n");
    }
    else if (choice == 3) {
        // Real-time simulation
        char path[256];
        printf("\nNhập đường dẫn tới file cần gửi theo thời gian (ví dụ: input_ts.txt): ");
        if (fgets(path, sizeof(path), stdin) == NULL) {
            fprintf(stderr, "Lỗi: không đọc được đường dẫn.\n");
            close(fd);
            return 1;
        }
        path[strcspn(path, "\n")] = '\0';

        printf("Nếu file có timestamp ở đầu mỗi dòng dạng [HH:MM:SS], chương trình sẽ mô phỏng theo khoảng cách thời gian thật.\n");
        printf("Nếu không có timestamp, bạn có thể nhập interval (ms) giữa các dòng. Nhập 0 để tự động dùng 300ms: ");
        long interval_ms = 0;
        if (scanf("%ld", &interval_ms) != 1) interval_ms = 0;
        getchar();

        printf("Bạn có muốn dùng multiplier (tăng tốc, ví dụ 2.0 = nhanh gấp 2)? Nhập float (1.0 mặc định): ");
        double multiplier = 1.0;
        if (scanf("%lf", &multiplier) != 1) multiplier = 1.0;
        getchar();

        FILE *fin = fopen(path, "r");
        if (!fin) { perror("Không mở được file đầu vào"); close(fd); exit(1); }

        char line[512];
        long prev_ts = -1; // seconds-of-day of previous timestamp
        struct timespec tprev_real;
        int first = 1;

        while (fgets(line, sizeof(line), fin)) {
            // strip trailing newline? keep it when sending to receiver so it behaves like lines
            long ts = parse_timestamp_seconds(line);
            if (ts >= 0) {
                if (first) {
                    // send immediately
                    if (write(fd, line, strlen(line)) < 0) perror("write");
                    printf("[RT SEND] %s", line);
                    first = 0;
                } else {
                    long delta = ts - prev_ts;
                    if (delta < 0) delta = 0; // malformed or day wrap - send immediately
                    // apply multiplier: effective sleep = delta / multiplier
                    double wait_seconds = delta / multiplier;
                    // sleep with usleep (microseconds)
                    if (wait_seconds > 0) {
                        long usec = (long)(wait_seconds * 1e6);
                        usleep(usec);
                    }
                    if (write(fd, line, strlen(line)) < 0) perror("write");
                    printf("[RT SEND] %s", line);
                }
                prev_ts = ts;
            } else {
                // no timestamp in line -> use interval_ms
                long use_ms = (interval_ms <= 0) ? 300 : interval_ms; // default 300ms
                use_ms = (long)(use_ms / multiplier);
                if (use_ms > 0) usleep(use_ms * 1000);
                if (write(fd, line, strlen(line)) < 0) perror("write");
                printf("[RT SEND] %s", line);
            }
        }

        fclose(fin);
        printf("[LOG_SENDER] Gửi real-time hoàn tất.\n");
    }
    else {
        printf("Lựa chọn không hợp lệ. Thoát chương trình.\n");
    }

    close(fd);
    printf("[LOG_SENDER] Kết thúc.\n");
    return 0;
}
