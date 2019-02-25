src = $(wildcard src/*.c)

CFLAGS = -std=gnu99 -Wall -Isrc -fshort-enums

receiver: $(src) receiver.c
	$(CC) $(CFLAGS) receiver.c $(filter-out %sender.c, $(src)) -o $@

sender: $(src) sender.c
	$(CC) $(CFLAGS) sender.c $(filter-out %receiver.c, $(src)) -o $@

all: receiver sender

clean:
		rm receiver;
		rm sender;
