# ucp_cfg overview

`ucp_cfg` is a UCP application, not a transport. APP id `0x71`, version `0x0100` (V1.0), role `REQ_RSP`. It edits the component slot table that [`comp`](../../comp/README.md) applies, and it persists that table through [`store`](../../store/README.md). Version encoding: [UCP_APPLICATION_LAYER.md](../../ucp/docs/UCP_APPLICATION_LAYER.md).

Wire opcodes:

| Opcode | Name | Effect |
| --- | --- | --- |
| `0x01` | `LIST_DESCS` | Registered `comp_desc_t` ids, kinds, names |
| `0x02` | `GET_SLOTS` | Working slot specs |
| `0x03` | `SET_SLOT` | Update one spec in RAM |
| `0x04` | `APPLY` | `comp_runtime_apply` on the working specs |
| `0x05` | `SAVE` | Write `STORE_KEY_SLOTS` and `STORE_KEY_PACK_ADS`, then `store_commit` |
| `0x06` | `LOAD` | Read those keys back into the working specs and ads |
| `0x07` | `GET_ADS` | Pack advertisements (`pack_id`, `revision`) |

Exact version match is required. The host client is `tools/ucp_cfg.py` (`tools/ucp_cfg_client.py`).

Pack ads are identifiers only. Tier B names and scales stay in host manifests (`tools/packs/`). See [signal catalog tiers](../../sig/docs/CATALOG.md) and [component model](../../docs/FUTURE_COMPONENTS_CONFIG.md).
