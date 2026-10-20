# UCP Transport/Session Architecture

UCP is split into a portable core and platform glue.

## Layering

1. **UART/PHY driver layer** (platform-specific)
2. **Portable transport/session layer** (`ucp_protocol`, `ucp_parser`, `ucp_session`, `ucp_port`)
3. **Application/service protocol layer** (app registry and handlers)

## Portable Core Contract

- Portable core never depends on vendor HAL, MCU family, or RTOS APIs.
- Platform glue provides time, UART TX/RX hooks, logging sink, and app registry hooks.

## TX/RX Design Requirements

- TX enqueue API should be non-blocking and owned by a worker/task/thread context.
- RX bytes are fed through one parser path (`ucp_parse_byte`) in deterministic order.
- Session tick runs after RX dispatch with fresh loop timestamp.

## Critical Bring-up Lessons

1. **Long-frame corruption diagnosis:** if HELLO/LINK_READY succeed but CAP fails checksum, first inspect transport scheduling and TX buffer ownership.
2. **DMA safety:** never start DMA from stack-local frame memory.
3. **CAP recovery:** parser/session must support CAP checksum-fail resync/retry without deadlock.
4. **Restart determinism:** clear transport-level CAP guard windows on reconnect paths.
5. **Monotonic time:** port `ucp_port_get_time_ms()` with 64-bit tick conversion. See `UCP_TIME_AND_TIMEOUTS.md`.
