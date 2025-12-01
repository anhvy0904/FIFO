CC = gcc
CFLAGS = -Wall -O2
TARGETS = log_sender log_receiver

all: $(TARGETS)

log_sender: log_sender.c
	$(CC) $(CFLAGS) log_sender.c -o log_sender

log_receiver: log_receiver.c
	$(CC) $(CFLAGS) log_receiver.c -o log_receiver

clean:
	rm -f $(TARGETS) logfifo log.txt

