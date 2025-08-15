CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=gnu11 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lbsd

SRCDIR = src
SOURCES = $(SRCDIR)/fdiff.c $(SRCDIR)/ignore.c $(SRCDIR)/store.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = fdiff

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)

uninstall:
	rm -f $(BINDIR)/$(TARGET)
