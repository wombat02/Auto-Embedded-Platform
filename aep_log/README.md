# aep_log

Platform-agnostic log facade for AEP portable modules. Modules emit short
prebuilt messages; product glue implements display.

| Doc | Contents |
| --- | --- |
| [docs/PORTING.md](docs/PORTING.md) | `aep_port_log` contract and compile knobs |

## Levels

| Macro | Level | Default (WARN) |
| --- | --- | --- |
| `AEP_LOGD` | Debug | compiled out |
| `AEP_LOGI` | Info | compiled out |
| `AEP_LOGW` | Warn | emitted |
| `AEP_LOGE` | Error | emitted |

Policy: prefer **Warn/Error** on failure paths so glue does not re-check every
submodule return. Keep **Info** sparse. Debug is for verbose session traces.

## Compile knobs

| Define | Default | Effect |
| --- | --- | --- |
| `AEP_LOG_ENABLE` | `1` | `0` strips all macros to no-ops |
| `AEP_LOG_LEVEL` | `AEP_LOG_LEVEL_WARN` | Only levels >= threshold are compiled in |

Lower the level (e.g. `-DAEP_LOG_LEVEL=AEP_LOG_LEVEL_INFO`) to include Info.

## Sink

```c
void aep_port_log(aep_log_level_t level, const char *msg, size_t len);
```

Weak stub is a no-op. Example: [examples/stm32_h5/GLUE.md](../examples/stm32_h5/GLUE.md).
