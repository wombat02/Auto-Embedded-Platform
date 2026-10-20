# comp overview

`comp` is a portable component runtime: a descriptor registry, a slot table, and apply/poll. It does not parse CAN, open UCP, or touch NVS. Glue owns the descriptors and the context buffers.

`comp_kind_t` (`PARSER`, `PID`, `MODULE`, `UDS_SRV`) is metadata for a host UI. Behavior is `comp_ops_t` plus an opaque cfg blob. There is no `switch (kind)` in this library.

| Op | When |
| --- | --- |
| `start` | Slot is enabled during `comp_runtime_apply` |
| `stop` | Slot is disabled, replaced, or `comp_runtime_stop_all` |
| `poll` | Glue cadence (`comp_runtime_poll`) |
| `on_can_rx` | Optional. Same tick as RX demux, before the soft `poll` path |

Limits (override at compile time): `COMP_MAX_DESCS` 16, `COMP_MAX_SLOTS` 8, `COMP_CFG_MAX` 32.

`materialize_cfg` on `comp_runtime_t` lets glue turn a stored blob into the runtime cfg (pointers to `vcom` core, `sig` table, demux) immediately before `start`.

Design notes: [docs/FUTURE_COMPONENTS_CONFIG.md](../../docs/FUTURE_COMPONENTS_CONFIG.md).
