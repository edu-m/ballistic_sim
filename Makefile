CC = gcc
CFLAGS = -Wunused-parameter -Wall -Wextra -O3 -DISOMETRIC=0 $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lm

TARGETS = ballistic spacewar
all: $(TARGETS)
ballistic: ballistic.c
	$(CC) $(CFLAGS) ballistic.c -o ballistic $(LDFLAGS)
spacewar: spacewar.c
	$(CC) $(CFLAGS) spacewar.c -o spacewar $(LDFLAGS)
clean:
	rm -f $(TARGETS)