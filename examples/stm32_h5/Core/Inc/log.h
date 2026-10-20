/*
 * Example log ring (USB CDC drain). Glue only — not a portable library.
 */
#ifndef AEP_LOG_H_
#define AEP_LOG_H_

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#define LOG_MSG_SIZE   128
#define LOG_MSG_COUNT  96

typedef struct {
    char msg[LOG_MSG_SIZE];
} log_slot_t;

typedef struct {
    log_slot_t buffer[LOG_MSG_COUNT];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint32_t dropped;
} log_ring_t;

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

extern log_ring_t log_ring;

void log_ring_push(const char *msg);
void log_write(log_level_t level, const char *fmt, ...);

#define logi(...) log_write(LOG_LEVEL_INFO, __VA_ARGS__)
#define logw(...) log_write(LOG_LEVEL_WARN, __VA_ARGS__)
#define loge(...) log_write(LOG_LEVEL_ERROR, __VA_ARGS__)

#endif /* AEP_LOG_H_ */
