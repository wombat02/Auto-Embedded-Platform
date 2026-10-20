# UCP Generic RTOS Porting Guide

This guide is RTOS-agnostic and MCU-family-agnostic. The portable core should remain unchanged across targets.

## Reuse As-Is

- `ucp_protocol.*`
- `ucp_parser.*`
- `ucp_session.*`
- `ucp_port.h` + weak defaults in `ucp_port.c`
- `ucp_log.h`
- `ucp_types.h`

## Platform Glue Responsibilities

- Time source (`ucp_port_get_time_ms`) — monotonic ms; 64-bit tick conversion required (see `UCP_TIME_AND_TIMEOUTS.md`)
- UART frame enqueue TX path (`ucp_port_uart_send_frame`)
- RX byte feed loop (call `ucp_parse_byte`)
- Session packet dispatch (`ucp_session_on_packet`)
- Session periodic tick (`ucp_session_tick`)
- App descriptor/registry hooks
- Logging sink

## Worker Loop Template

1. Drain RX bytes and dispatch complete packets.
2. Capture fresh monotonic `now_ms`.
3. Call `ucp_session_tick(now_ms)`.
4. Run app tick and TX pump.

## Must-Have Guardrails

1. Do not pass temporary/stack buffers to asynchronous DMA TX APIs.
2. Keep one parser path and deterministic packet order.
3. Reset parser/CAP transport guard state on restart paths.
4. Enable verbose tracing only when needed; default to INFO-level logs.
5. **Time source:** `ucp_port_get_time_ms()` must be monotonic. Convert ticks with a 64-bit multiply (`(uint64_t)ticks * 1000 / ticks_per_sec`) unless the tick is already 1 ms. A 32-bit `ticks * 1000` overflows at ~71.6 minutes when the tick is 1 ms and forces a false session TIMEOUT. See `UCP_TIME_AND_TIMEOUTS.md`.
6. Session timeouts must use unsigned elapsed time (`now - then`), not `now >= last + limit` in a way that assumes no wrap, and must not treat a backward jump as “still fresh”.
