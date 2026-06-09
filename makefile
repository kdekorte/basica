CC = gcc
CFLAGS = -Wall -Wextra -I./src $(shell pkg-config --cflags sdl3 sdl3-ttf sdl3-mixer)
LDFLAGS = $(shell pkg-config --libs sdl3 sdl3-ttf sdl3-mixer) -lm

SRC = src/main.c src/lexer.c src/interpreter.c src/program.c src/graphics.c src/audio.c
OBJ = $(SRC:.c=.o)
TARGET = basica

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: test clean

test: $(TARGET)
	./tests/run_tests.sh

clean:
	rm -f $(OBJ) $(TARGET)
