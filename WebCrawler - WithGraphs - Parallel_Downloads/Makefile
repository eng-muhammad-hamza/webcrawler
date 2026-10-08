CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lcurl -lpthread

SRC_DIR = src
INC_DIR = include
BIN_DIR = bin

SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/workers.c $(SRC_DIR)/thread_pool.c $(SRC_DIR)/metrics.c
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BIN_DIR)/%.o)
EXECUTABLE = $(BIN_DIR)/web_crawler

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean