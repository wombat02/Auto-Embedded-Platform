# ucp porting (summary)

Full RTOS-agnostic guide: [UCP_PORTING_GUIDE.md](UCP_PORTING_GUIDE.md). Time rules: [UCP_TIME_AND_TIMEOUTS.md](UCP_TIME_AND_TIMEOUTS.md).

## Glue must provide

- Monotonic `ucp_port_get_time_ms` (safe tick→ms conversion)
- `ucp_port_send_frame` (or set ops for host tests)
- RX path that feeds `ucp_parse_byte` / session dispatch
- Periodic `ucp_session_tick` + app tick
- App descriptor/registry hooks

## Example

[examples/stm32_h5/README.md](../../examples/stm32_h5/README.md) · [GLUE.md](../../examples/stm32_h5/GLUE.md)

`examples/stm32_h5/Core/Src/aep_ucp_port.c` (time, send, app registry) and `aep_ucp_usb.c` (CDC byte stream).
