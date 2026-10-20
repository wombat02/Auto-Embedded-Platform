# comp porting

## Glue responsibilities

1. Define one `comp_desc_t` per linked component (`id`, `ops`, `ctx_size`, `name`). Ids are stable; they are what `ucp_cfg` and `store` persist.
2. `comp_runtime_init` + `comp_runtime_register` for each descriptor.
3. Allocate ctx storage. `comp_runtime_apply` receives `ctx_ptrs` / `ctx_sizes` from glue; this library does not malloc.
4. Optional: `comp_runtime_set_materialize` so an NVM blob becomes a runtime cfg with stack pointers.
5. On the comms loop: `comp_runtime_poll`. If a descriptor implements `on_can_rx`, feed frames from `vcom_rx_demux` (or call `comp_runtime_on_can_rx`).
6. Drive apply from your config path. The example uses [`ucp_cfg`](../../ucp_cfg/docs/PORTING.md).

Keep vehicle decode maps in the product (or in the example's `aep_*` modules). Do not add them under `comp/`.

## Example

[examples/stm32_h5/README.md](../../examples/stm32_h5/README.md) · [GLUE.md](../../examples/stm32_h5/GLUE.md) · [DEMO_PT.md](../../examples/stm32_h5/DEMO_PT.md)

| Id | Module | Role |
| --- | --- | --- |
| `0x0001` | `aep_pt_parser.c` | CAN0 PT-A/B → Tier A signals |
| `0x0002` | `aep_pid_client.c` | `diag_link` client → extension signals |
| `0x0003` | `aep_cluster_mod.c` | Signals → CAN1 `0x301` |
| `0x0004` | `aep_dash_mod.c` | CAN1 dash → Tier B signals + pack ad |

Ids are `AEP_COMP_ID_*` in `Core/Inc/aep_demo_pt.h`.
