# Border Queue Optimizer — Makefile
# Requires: C11 compiler, make

CC       ?= cc
CFLAGS   := -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS  := -lm
BUILD    := build

SRCS     := src/heap.c src/stats.c src/queue_model.c src/simulator.c \
            src/optimizer.c src/predictor.c src/arrival.c
OBJS     := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

TEST_SRCS := tests/test_main.c tests/test_queue_model.c tests/test_heap.c \
             tests/test_simulator.c tests/test_optimizer.c
TEST_OBJS := $(patsubst tests/%.c,$(BUILD)/%.o,$(TEST_SRCS))

BIN      := $(BUILD)/border-queue-optimizer
TEST_BIN := $(BUILD)/bqo_tests

.PHONY: all clean test simulate optimize predict

all: $(BIN)

# ── Core objects ─────────────────────────────────────────────────────
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -Iinclude -Isrc -c $< -o $@

# ── Test objects ─────────────────────────────────────────────────────
$(BUILD)/%.o: tests/%.c | $(BUILD)
	$(CC) $(CFLAGS) -Iinclude -Isrc -c $< -o $@

# ── Main binary ─────────────────────────────────────────────────────
$(BIN): $(BUILD)/main.o $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# ── Test binary ─────────────────────────────────────────────────────
$(TEST_BIN): $(TEST_OBJS) $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD):
	mkdir -p $(BUILD)

# ── Convenience targets ─────────────────────────────────────────────
test: $(TEST_BIN)
	./$(TEST_BIN)

simulate: $(BIN)
	./$(BIN) simulate --lanes 4 --duration 28800 --rate 120 --service-rate 40

optimize: $(BIN)
	./$(BIN) optimize --rate 120 --service-rate 40 --max-lanes 16

predict: $(BIN)
	./$(BIN) predict --hour 8 --minute 30

clean:
	rm -rf $(BUILD)
