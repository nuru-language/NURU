CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g -Icompiler
LDFLAGS =

SRC_DIR = compiler
BUILD_DIR = build
SRC_FILES = $(wildcard $(SRC_DIR)/*/*.c) $(wildcard $(SRC_DIR)/*/*/*.c)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))

TARGET = nuruc

.PHONY: all clean test test-native test-ast

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

test: test-native test-ast test-codegen-e2e

test-native: $(TARGET)
	@echo "Running native execution tests..."
	./tests/native/run_native_tests.sh

test-codegen-e2e: $(TARGET)
	@echo "Running self-hosting end-to-end check..."
	./tests/native/run_codegen_e2e.sh

test-ast: $(TARGET)
	@echo "Running AST tests..."
	./$(TARGET) -a examples/hello/hello.nuru
