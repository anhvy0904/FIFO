#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int main() {
    const char *fifo = "logfifo";
    mkfifo(fifo, 0666); // Tạo FIFO nếu chưa tồn tại

    printf("=== LOG_SENDER (GỬI LOG QUA FIFO) ===\n");
    printf("Chọn chế độ hoạt động:\n");
    printf("1. Nhập log thủ công\n");
    printf("2. Đọc log từ file\n");
    printf("Lựa chọn của bạn (1/2): ");

    int choice;
    if (scanf("%d", &choice) != 1) { // kiểm tra dữ liệu nhập
        fprintf(stderr, "Lỗi: không đọc được lựa chọn.\n");
        return 1;
    }
    getchar(); // bỏ ký tự newline sau khi nhập số

    // Mở FIFO để ghi log
    int fd = open(fifo, O_WRONLY);
    if (fd < 0) {
        perror("Không mở được FIFO để ghi");
        exit(1);
    }

    // -------------------------------
    // CHẾ ĐỘ 1: Nhập log thủ công
    // -------------------------------
    if (choice == 1) {
        printf("\n[LOG_SENDER] Nhập log (gõ 'exit' để thoát):\n");
        char line[256];
        while (1) {
            printf(">>> ");
            if (fgets(line, sizeof(line), stdin) == NULL)
                break; // EOF hoặc lỗi

            if (strncmp(line, "exit", 4) == 0)
                break;

            if (write(fd, line, strlen(line)) < 0)
                perror("write");
        }
    }

    // -------------------------------
    // CHẾ ĐỘ 2: Đọc log từ file
    // -------------------------------
    else if (choice == 2) {
        char path[256];
        printf("\nNhập đường dẫn tới file cần đọc (ví dụ: input.txt): ");

        if (fgets(path, sizeof(path), stdin) == NULL) {
            fprintf(stderr, "Lỗi: không đọc được đường dẫn.\n");
            close(fd);
            return 1;
        }
        path[strcspn(path, "\n")] = '\0'; // xóa ký tự newline

        FILE *fin = fopen(path, "r");
        if (!fin) {
            perror("Không mở được file đầu vào");
            close(fd);
            exit(1);
        }

        printf("[LOG_SENDER] Đang gửi nội dung từ file '%s'...\n", path);
        char line[256];
        while (fgets(line, sizeof(line), fin)) {
            if (write(fd, line, strlen(line)) < 0)
                perror("write");
            printf("[LOG_SENDER] Gửi: %s", line);
            usleep(300000); // chờ 0.3 giây để logger dễ bắt kịp
        }

        fclose(fin);
        printf("[LOG_SENDER] Gửi file hoàn tất.\n");
    }

    // -------------------------------
    // Lựa chọn không hợp lệ
    // -------------------------------
    else {
        printf("Lựa chọn không hợp lệ. Thoát chương trình.\n");
    }

    close(fd);
    printf("[LOG_SENDER] Kết thúc.\n");
    return 0;
}

