# ucp_tlm

Telemetry application on UCP: APP `0x20`, version `0x0100` (V1.0).

| Doc | Contents |
| --- | --- |
| [docs/OVERVIEW.md](docs/OVERVIEW.md) | Frames, directory service, stack fit |
| [docs/PORTING.md](docs/PORTING.md) | Registering the vtable from glue |
| [docs/TESTING.md](docs/TESTING.md) | Host checks |
| [../ucp/docs/UCP_APPLICATION_LAYER.md](../ucp/docs/UCP_APPLICATION_LAYER.md) | Version encoding (major/minor) |

Depends on `sig` + `ucp` (including `ucp_xfer` for GET_DIRECTORY offers). Register `g_ucp_tlm_sig_app_vtable` from glue; also register `g_ucp_xfer_app_vtable` for routing (not CAP-advertised).

Example: [examples/stm32_h5/README.md](../examples/stm32_h5/README.md) — registry in `Core/Src/aep_ucp_port.c`.
