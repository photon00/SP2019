CC = gcc
DEBUG_FLAG =
CFLAGS = -Wall -O2 -DROUNDS=10

all: bidding_system host player

bidding_system: bidding_system.c
	$(CC) bidding_system.c -o bidding_system $(CFLAGS) $(DEBUG_FLAG)

host: host.c
	$(CC) host.c -o host $(CFLAGS) $(DEBUG_FLAG)

player: player.c
	$(CC) player.c -o player $(CFLAGS) $(DEBUG_FLAG)

rm:
	rm *FIFO
clean:
	rm bidding_system host player