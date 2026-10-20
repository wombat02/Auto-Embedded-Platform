CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wno-unused-parameter -I../include -I../../vcom/include \
	-I../third_party/iso14229 -DUDS_SYS=0 -DUDS_TP_ISOTP_C
BUILD := build

SRCS := ../../vcom/src/vcom_core.c \
	../src/diag_link.c \
	../third_party/iso14229/iso14229.c \
	test_diag_link.c

.PHONY: all test clean

all: $(BUILD)/test_diag_link

test: all
	./$(BUILD)/test_diag_link

$(BUILD)/test_diag_link: $(SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
