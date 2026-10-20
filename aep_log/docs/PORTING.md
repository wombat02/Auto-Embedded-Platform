# aep_log porting

Implement one strong symbol in glue:

```c
void aep_port_log(aep_log_level_t level, const char *msg, size_t len);
```

- `msg` is not necessarily NUL-terminated beyond `len`; copy at most `len` bytes.
- May be called from RTOS threads (and potentially ISR in some ports) — keep it non-blocking if your sink is shared.
- Map `AEP_LOG_INFO` / `WARN` / `ERROR` / `DEBUG` to your ring, UART, or USB log path.

## Compile-time control

Pass defines on the compiler command line or in a project header before includes:

```text
-DAEP_LOG_ENABLE=1
-DAEP_LOG_LEVEL=AEP_LOG_LEVEL_WARN
```

Release builds that want silence: `-DAEP_LOG_ENABLE=0`.

## Include / link

- Include path: `aep_log/include`
- Source: `aep_log/src/aep_log_port.c` (weak stub; omit if glue always provides a strong `aep_port_log`)

## Example (stm32_h5)

[examples/stm32_h5/Core/Src/aep_ucp_port.c](../../examples/stm32_h5/Core/Src/aep_ucp_port.c) maps levels onto the USB log ring (`logi` / `logw` / `loge`).
