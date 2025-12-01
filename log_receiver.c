// log_receiver.c
// Mở FIFO và đọc theo dòng. Nếu dòng nhận vào có timestamp gốc [HH:MM:SS], 
// ghi cả thời gian nhận và timestamp gốc vào log.txt
// Compile: gcc -o log_receiver log_receiver.c

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define FIFO_NAME "logfifo"
#define LOG_FILE "log.txt"
#define LINE_BUF 1024

static volatile sig_atomic_t running = 1;

void on_sigint(int s) {
    (void)s;
    running = 0;
}

long parse_timestamp_seconds(const char *s) {
    int hh, mm, ss;
    if (sscanf(s, " [%d:%d:%d]", &hh, &mm, &ss) == 3) {
        if (hh>=0 && hh<24 && mm>=0 && mm<60 && ss>=0 && ss<60)
            return hh*3600 + mm*60 + ss;
    }
    return -1;
}

int main() {
    // install SIGINT handler
    struct sigaction act;
    act.sa_handler = on_sigint;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    // ensure FIFO exists (created by sender too but safe to mkfifo)
    if (mkfifo(FIFO_NAME, 0666) < 0) {
        if (errno != EEXIST) {
            perror("mkfifo");
            return 1;
        }
    }

    printf("[LOGGER] Đang mở FIFO để đọc log...\n");

    FILE *f_out = fopen(LOG_FILE, "a");
    if (!f_out) {
        perror("fopen log.txt");
        return 1;
    }

    while (running) {
        int fd = open(FIFO_NAME, O_RDONLY);
        if (fd < 0) {
            perror("open fifo read");
            usleep(200000);
            continue;
        }

        // Use FILE* for line oriented reading
        FILE *fp = fdopen(fd, "r");
        if (!fp) {
            perror("fdopen");
            close(fd);
            continue;
        }

        char buffer[LINE_BUF];
        while (running && fgets(buffer, sizeof(buffer), fp) != NULL) {
            // strip trailing newline for processing but keep original content for logging
            char line_copy[LINE_BUF];
            strncpy(line_copy, buffer, LINE_BUF);
            line_copy[LINE_BUF-1] = '\0';

            // current receive time
            time_t now = time(NULL);
            struct tm *t = localtime(&now);

            // check if incoming line has original timestamp prefix [HH:MM:SS]
            long orig_ts = parse_timestamp_seconds(buffer);
            if (orig_ts >= 0) {
                // extract original timestamp string (first token inside brackets)
                char orig_ts_str[16] = {0};
                sscanf(buffer, " [%15[^]]] ]", orig_ts_str); // get inside [...]
                // Write both receive time and original timestamp + raw message (after the bracket)
                // find position after closing ]
                char *after = strchr(buffer, ']');
                const char *msg = (after && *(after+1)) ? after+1 : buffer;
                // ensure msg does not repeat bracket
                fprintf(f_out, "[recv %02d:%02d:%02d][orig %s]%s",
                        t->tm_hour, t->tm_min, t->tm_sec, orig_ts_str, msg);
            } else {
                // no original timestamp; write with receive timestamp (same as before)
                fprintf(f_out, "[%02d:%02d:%02d] %s",
                        t->tm_hour, t->tm_min, t->tm_sec, buffer);
            }
            fflush(f_out);

            // also show on terminal (preserve original behavior)
            printf("[LOGGER] %s", line_copy);
            fflush(stdout);
        }

        // if fgets returned NULL, that means writer closed -> close and loop to reopen
        fclose(fp); // closes fd as well
    }

    printf("\n[LOGGER] Đang tắt... đóng file và unlink fifo (nếu cần)\n");
    fclose(f_out);
    // do not unlink fifo to allow reuse, but you can uncomment next line to remove it:
    // unlink(FIFO_NAME);
    return 0;
}
