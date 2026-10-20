# Auto Embedded Platform

Public portable stack for framed sessions (UCP), signal telemetry, vehicle-comms, and diagnostics. The reusable libraries are MIT.

**Start here:** [docs/QUICKSTART.md](docs/QUICKSTART.md) · [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) · [docs/index.md](docs/index.md)

STM32H523 example (illustration — CubeMX generate required): [examples/stm32_h5/README.md](examples/stm32_h5/README.md) · [BRINGUP.md](examples/stm32_h5/BRINGUP.md) · [GLUE.md](examples/stm32_h5/GLUE.md).

| Directory | Role |
| --- | --- |
| `sig/` | Tier A catalog, TLV codec, slot table — [docs](sig/README.md) |
| `ucp/` | Frame, parser, session, port hooks — [docs](ucp/README.md) |
| `ucp_tlm/` | Telemetry app `0x20` / V1.0 — [docs](ucp_tlm/README.md) |
| `ucp_cfg/` | Config app `0x71` / V1.0: slots, apply, save — [docs](ucp_cfg/README.md) |
| `comp/` | Component descriptors, slots, apply/poll — [docs](comp/README.md) |
| `store/` | Keyed NVS blobs; media via `store_port_*` — [docs](store/README.md) |
| `vcom/` | CAN frame, PHY vtable, TX scheduler — [docs](vcom/README.md) |
| `vcom_diag/` | `diag_link` / `diag_server` + `iso14229` 0.11.0 — [docs](vcom_diag/README.md) |
| `tools/` | Host session, TUI, PT/dash generators, UDS helpers — [requirements](tools/requirements.txt) |

## Host unit tests

```powershell
./tools/run_host_tests.ps1
```

Requires MinGW `gcc` on the PATH.

## License

Portable libraries, host tools, and example glue are MIT — see [LICENSE](LICENSE) and [NOTICE](NOTICE). ST HAL, CMSIS, and Azure RTOS (ThreadX / USBX) come from a CubeMX generate and keep their own licenses. See [docs/LICENSING.md](docs/LICENSING.md).
