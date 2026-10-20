CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -I../include -DUCP_LOG_ENABLE=0
BUILD := build

UCP_SRCS := ../src/ucp_protocol.c ../src/ucp_parser.c ../src/ucp_port.c \
	../src/ucp_session.c ../src/ucp_session_debug.c ../src/ucp_xfer.c

.PHONY: all test clean

all: $(BUILD)/test_ucp_roles $(BUILD)/test_ucp_xfer

test: all
	./$(BUILD)/test_ucp_roles
	./$(BUILD)/test_ucp_xfer

$(BUILD)/test_ucp_roles: $(UCP_SRCS) test_ucp_roles.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(UCP_SRCS) test_ucp_roles.c

$(BUILD)/test_ucp_xfer: $(UCP_SRCS) test_ucp_xfer.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(UCP_SRCS) test_ucp_xfer.c

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
