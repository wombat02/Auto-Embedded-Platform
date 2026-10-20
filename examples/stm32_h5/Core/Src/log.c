/*
 * Example log ring (USB CDC drain). Glue only — not a portable library.
 * Drained by USBX App while UCP is idle; see examples/stm32_h5/GLUE.md.
 */
#include "tx_api.h"
#include "log.h"
#include <string.h>

log_ring_t log_ring;

void log_ring_push(const char *msg)
{
    UINT old_posture = tx_interrupt_control(TX_INT_DISABLE);

    uint16_t next_head = (uint16_t)((log_ring.head + 1u) % LOG_MSG_COUNT);

    if (next_head == log_ring.tail)
    {
        log_ring.tail = (uint16_t)((log_ring.tail + 1u) % LOG_MSG_COUNT);
        log_ring.dropped++;
    }

    strncpy(log_ring.buffer[log_ring.head].msg, msg, LOG_MSG_SIZE - 1u);
    log_ring.buffer[log_ring.head].msg[LOG_MSG_SIZE - 1u] = '\0';
    log_ring.head = next_head;

    tx_interrupt_control(old_posture);
}

void log_write(log_level_t level, const char *fmt, ...)
{
    char final[LOG_MSG_SIZE];

    const char *prefix =
        (level == LOG_LEVEL_INFO)  ? "[I] " :
        (level == LOG_LEVEL_WARN)  ? "[W] " :
                                     "[E] ";

    size_t prefix_len = strlen(prefix);
    if (prefix_len >= LOG_MSG_SIZE)
    {
        return;
    }

    memcpy(final, prefix, prefix_len);

    size_t remaining = LOG_MSG_SIZE - prefix_len - 3u;
    if ((int)remaining <= 0)
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    int msg_len = vsnprintf(&final[prefix_len], remaining + 1u, fmt, args);
    va_end(args);

    if (msg_len < 0)
    {
        return;
    }
    if ((size_t)msg_len > remaining)
    {
        msg_len = (int)remaining;
    }

    size_t total_len = prefix_len + (size_t)msg_len;
    final[total_len++] = '\r';
    final[total_len++] = '\n';
    final[total_len] = '\0';

    log_ring_push(final);
}
