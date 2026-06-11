CC = gcc
CFLAGS = -Wall -Wextra -I./src $(shell pkg-config --cflags sdl3 sdl3-ttf sdl3-mixer)
LDFLAGS = $(shell pkg-config --libs sdl3 sdl3-ttf sdl3-mixer) -lm

PREFIX ?= /usr/local
DESTDIR ?=

SRC = src/main.c src/lexer.c src/interpreter.c src/program.c src/graphics.c src/audio.c
OBJ = $(SRC:.c=.o)
TARGET = basica

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: test clean install uninstall

test: $(TARGET)
	./tests/run_tests.sh

clean:
	rm -f $(OBJ) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -d $(DESTDIR)$(PREFIX)/share/basica/fonts
	install -m 644 fonts/ModernDOS8x16.ttf $(DESTDIR)$(PREFIX)/share/basica/fonts/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	rm -rf $(DESTDIR)$(PREFIX)/share/basica
