CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g -Icompiler -Iruntime
LDFLAGS =

SRC_DIR = compiler
BUILD_DIR = build
SRC_FILES = $(wildcard $(SRC_DIR)/*/*.c) $(wildcard $(SRC_DIR)/*/*/*.c) \
            $(wildcard runtime/*/*.c) $(wildcard runtime/*/*/*.c)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter $(SRC_DIR)/%,$(SRC_FILES))) \
            $(patsubst runtime/%.c,$(BUILD_DIR)/runtime/%.o,$(filter runtime/%,$(SRC_FILES)))

TARGET = nuruc

.PHONY: all clean test test-native test-ast test-safe-io test-negative test-nir-opt test-opt-compare test-cli test-gc test-pe

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/runtime/%.o: runtime/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

test: test-native test-ast test-codegen-e2e test-safe-io test-negative test-nir-opt test-opt-compare test-cli test-gc test-pe

test-native: $(TARGET)
	@echo "Running native execution tests..."
	./tests/native/run_native_tests.sh

test-codegen-e2e: $(TARGET)
	@echo "Running self-hosting end-to-end check..."
	./tests/native/run_codegen_e2e.sh

test-ast: $(TARGET)
	@echo "Running AST tests..."
	./$(TARGET) -a examples/hello/hello.nuru

test-safe-io: $(TARGET)
	@echo "Running safe external-tool invocation tests..."
	./tests/native/run_safe_io_tests.sh

test-negative: $(TARGET)
	@echo "Running compile-time diagnostic tests..."
	./tests/native/run_negative_tests.sh

test-nir-opt: $(TARGET)
	@echo "Running NIR optimizer unit tests..."
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_optimizer tests/nir/test_optimizer.c \
		$(BUILD_DIR)/nir/nuru_nir.o $(BUILD_DIR)/optimizer/nuru_optimizer.o
	./$(BUILD_DIR)/test_optimizer

test-opt-compare: $(TARGET)
	@echo "Running optimized vs unoptimized behavioral comparison..."
	./tests/native/run_opt_compare.sh

test-cli: $(TARGET)
	@echo "Running CLI integration tests..."
	./tests/integration/run_cli_tests.sh

test-gc:
	@echo "Running mark-and-sweep GC tests..."
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_gc tests/nir/test_gc.c
	./$(BUILD_DIR)/test_gc

test-pe:
	@echo "Running PE/COFF container tests..."
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_pe tests/nir/test_pe.c $(BUILD_DIR)/object/nuru_pe.o
	./$(BUILD_DIR)/test_pe
