CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -I../include
BUILD := build

SRCS := ../src/sig_catalog.c ../src/sig_tlv.c ../src/sig_table.c test_sig.c
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(SRCS)))

.PHONY: all test clean

all: $(BUILD)/test_sig

test: all
	./$(BUILD)/test_sig

$(BUILD)/test_sig: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/%.o: ../src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/test_sig.o: test_sig.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
