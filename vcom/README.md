# vcom

CAN frame type, RX dispatch, TX scheduler, PHY vtables. No HAL / FDCAN handles.

| Doc | Contents |
| --- | --- |
| [docs/OVERVIEW.md](docs/OVERVIEW.md) | Types, scheduler, RX |
| [docs/PORTING.md](docs/PORTING.md) | Implementing PHY vtables |
| [docs/TESTING.md](docs/TESTING.md) | Host unit tests |

Diagnostics sit in [`vcom_diag`](../vcom_diag/README.md).

Example PHYs: [examples/stm32_h5/README.md](../examples/stm32_h5/README.md) — `aep_fdcan_phy.c`, `aep_kline_phy.c`.
