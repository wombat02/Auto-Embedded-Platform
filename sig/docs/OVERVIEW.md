# sig overview

`sig` owns typed signal identity and encoding for telemetry:

- **Catalog** — Tier A id, type, unit, scale, and `stale_ms` (`sig_catalog.h`) — see [CATALOG.md](CATALOG.md)
- **TLV codec** — pack/unpack values for on-wire use (`sig_tlv`): `id | type | qflags | raw`
- **Slot table** — bind ids, write samples, age by catalog `stale_ms`, snapshot for a UCP app (`sig_table`)
- **Rates** — `LIVE` / `HF` / `MF` / `LF` / `CHANGE` (`sig_rate_t`); TLM schedules bands by rate. `CHANGE` sets `dirty` on value/age; clear with `sig_table_clear_dirty` after send

It does not open links, schedule CAN, or call an RTOS. [`ucp_tlm`](../../ucp_tlm/README.md) consumes snapshots; example glue writes bound ids from parsers/modules.

## Quality and aging

- Writers call `sig_table_write(..., valid=1, now_ms)` on producer RX.
- Glue calls `sig_table_age(table, now_ms)` before snapshot: if `now - stamp >= stale_ms`, clear `valid` and set `aged` (raw kept).
- Live TLV `qflags`: `SIG_TLV_Q_VALID` (0x01) or `SIG_TLV_Q_AGED` (0x02). Aged published slots are still encoded so hosts can show SIG stale while the UCP link is up.

## Catalog revision

`SIG_CATALOG_REVISION` identifies the Tier A dictionary. Hosts and firmware should advertise/match this when interpreting eng units. Live UCP frames carry **raw + quality flags**.
