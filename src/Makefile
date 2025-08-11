CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=gnu11 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lbsd

SOURCES = fdiff.c ignore.c store.c murmurhash.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = fdiff

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

