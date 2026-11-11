CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS ?=

BUILD   := build
LIB_SRC := src/vm.c src/asm.c
APP_SRC := $(LIB_SRC) src/tui.c src/main.c
LIB_OBJ := $(LIB_SRC:src/%.c=$(BUILD)/%.o)
APP_OBJ := $(APP_SRC:src/%.c=$(BUILD)/%.o)
TESTS   := $(BUILD)/test_vm $(BUILD)/test_asm

.PHONY: all test clean examples sanitize

all: $(BUILD)/mac

$(BUILD)/mac: $(APP_OBJ)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD)/%.o: src/%.c include/*.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_%: tests/test_%.c tests/test.h $(LIB_OBJ) | $(BUILD)
	$(CC) $(CFLAGS) -Itests $< $(LIB_OBJ) $(LDFLAGS) -o $@

$(BUILD):
	mkdir -p $(BUILD)

# Unit tests + end-to-end tests of the CLI against the example programs.
test: $(TESTS) $(BUILD)/mac
	@for t in $(TESTS); do ./$$t || exit 1; done
	@./tests/test_cli.sh $(BUILD)/mac

# Same tests built with AddressSanitizer and UndefinedBehaviorSanitizer.
sanitize:
	$(MAKE) clean
	$(MAKE) test CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
	             LDFLAGS="-fsanitize=address,undefined"

examples: $(BUILD)/mac
	@for f in examples/*.asm; do echo "== $$f"; ./$(BUILD)/mac run $$f; done

clean:
	rm -rf $(BUILD)
