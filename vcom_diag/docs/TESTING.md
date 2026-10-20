# vcom_diag testing

```bash
make -C test test
```

On-target client path (recommended): flash the STM32 example after [BRINGUP.md](../../examples/stm32_h5/BRINGUP.md), then run the dual-bus demo in [DEMO_PT.md](../../examples/stm32_h5/DEMO_PT.md) (`uds_pid_ecu.py` + TUI / `ucp_host.py`).

`diag_server` unit coverage is the host `make -C test test` target above.
