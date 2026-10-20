# Components, profiles, and config

Current model for parsers / PID / modules: portable `comp` runtime, UCP config app `0x71`, and `store`. The filename is historical; the path below is what the STM32 example runs.

Example wiring: [examples/stm32_h5/README.md](../examples/stm32_h5/README.md) · [GLUE.md](../examples/stm32_h5/GLUE.md).

Related demo: [examples/stm32_h5/DEMO_PT.md](../examples/stm32_h5/DEMO_PT.md).

## Layers

```text
Link set (flash)          Runtime slots (RAM)         Store + UCP
-----------------         -------------------         ------------
pt_parser_ops        -->  slot: PARSER + cfg     <--  store key SLOTS
pid_client_ops            slot: PID + cfg        <--  ucp_cfg APP 0x71
cluster_ops               slot: MODULE + cfg
dash_ops                  slot: MODULE + cfg     <--  pack ads (Tier B)
```

1. **Link set** — `comp_desc_t` + `comp_ops_t` (`start`/`stop`/`poll`/`on_can_rx`). `kind` is metadata only.
2. **Runtime** — portable `comp/` owns registry + apply/poll; glue materializes NVM blobs → runtime cfg (core/table/demux pointers).
3. **Store + UCP** — `store/` + flash port; `ucp_cfg` list/set/apply/save/load/ads. Host: `tools/ucp_cfg.py`.

## RX demux

`vcom_rx_demux` attaches one wide subscribe per bus and fans out to exact-id handlers so many parsers do not exhaust `VCOM_RX_SUBSCRIBERS`. Hot path: demux → component decode (`on_can_rx`). Soft path: `sig_table` + `poll`.

## Sig tiers

- **Tier A** — portable catalog in `sig/` (`SIG_CATALOG_REVISION`). See [sig/docs/CATALOG.md](../sig/docs/CATALOG.md).
- **Tier B** — pack manifests on host (`tools/packs/`), keyed by `pack_id+revision` from ads; IDs often `≥ 0xF000`.
- **Tier C** — private; default unpublished (`sig_table` publish bit / `ucp_tlm` skips unpublished).

## Product constraints

- **`diag_link` singleton** today — serialize schedules; multi-instance later.
- **No dynamic code load** — ops are linked; config selects which slots run.
- **Manifests stay on host** — MCU advertises pack ids/revisions only.
