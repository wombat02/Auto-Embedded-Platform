# comp

Portable component runtime: descriptor registry, slot table, apply/poll. `comp_kind_t` is metadata for a host UI. Behavior is `comp_ops_t` plus an opaque cfg blob.

| Doc | Contents |
| --- | --- |
| [docs/OVERVIEW.md](docs/OVERVIEW.md) | Ops, limits, materialize hook |
| [docs/PORTING.md](docs/PORTING.md) | What glue registers and when it polls |
| [docs/FUTURE_COMPONENTS_CONFIG.md](../docs/FUTURE_COMPONENTS_CONFIG.md) | Slots, sig tiers, and how `ucp_cfg` / `store` sit on top |

Ops: `start` / `stop` / `poll` / optional `on_can_rx` (same-tick RX demux path).

Example: [examples/stm32_h5/README.md](../examples/stm32_h5/README.md) — PT parser, PID client, cluster, and dash modules. Demo procedure: [DEMO_PT.md](../examples/stm32_h5/DEMO_PT.md).
