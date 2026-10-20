# vcom_diag

`diag_link` / `diag_server` wrap unmodified [iso14229](third_party/iso14229) **0.11.0** (embeds isotp-c).

| Doc | Contents |
| --- | --- |
| [docs/OVERVIEW.md](docs/OVERVIEW.md) | Stack choice and seams |
| [docs/PORTING.md](docs/PORTING.md) | Poll rate, hooks, compile flags |
| [docs/TESTING.md](docs/TESTING.md) | Host unit + on-target notes |

```bash
make -C test test
```

Example: [examples/stm32_h5/README.md](../examples/stm32_h5/README.md). The board image is a `diag_link` client; the host PID ECU owns `0x7E0` / `0x7E8`.
