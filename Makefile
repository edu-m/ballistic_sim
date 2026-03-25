CC = gcc
# Use -D to define ISOMETRIC here
CFLAGS = -Wall -Wextra -O2 -DISOMETRIC=0 $(shell sdl2-config --cflags) -Wunused-parameter
LDFLAGS = $(shell sdl2-config --libs) -lm

TARGET = engine
SRC = main.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
