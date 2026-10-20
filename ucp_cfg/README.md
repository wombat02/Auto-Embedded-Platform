# ucp_cfg

UCP config application: APP `0x71`, version `0x0100` (V1.0), role `REQ_RSP`. Lists component descriptors, edits slot specs, applies them in RAM, and saves or loads them through `store`. Also serves pack advertisements.

| Doc | Contents |
| --- | --- |
| [docs/OVERVIEW.md](docs/OVERVIEW.md) | Opcodes and stack fit |
| [docs/PORTING.md](docs/PORTING.md) | Bind the vtable from glue |
| [../ucp/docs/UCP_APPLICATION_LAYER.md](../ucp/docs/UCP_APPLICATION_LAYER.md) | Version encoding (major/minor) |

Depends on `comp`, `store`, and `ucp`. Host client: `tools/ucp_cfg.py`.

Example: [examples/stm32_h5/README.md](../examples/stm32_h5/README.md) — `g_ucp_cfg_app_vtable` in `Core/Src/aep_ucp_port.c`, bind in `aep_app.c`.
