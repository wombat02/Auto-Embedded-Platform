CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -I../include
BUILD := build

.PHONY: all test clean

all: $(BUILD)/test_vcom

test: all
	./$(BUILD)/test_vcom

$(BUILD)/test_vcom: ../src/vcom_core.c test_vcom.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ ../src/vcom_core.c test_vcom.c

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
