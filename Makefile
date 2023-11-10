NAME = video_stream

CC = gcc
CFLAGS = -g -Wall -I./include/image_processing.h
LDFLAGS = -lm -lSDL2 -lv4l2

SRC_DIR = ./src
BUILD_DIR = ./build
BIN_DIR = ./bin
INCLUDE = ./include

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SOURCES))

TARGET = $(BIN_DIR)/$(NAME)

.PHONY	:	all clean re

all: $(TARGET)

$(TARGET): $(OBJECTS) $(HEADER)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET)

re		:	clean all
