# store overview

`store` is a portable keyed blob store. The RAM image is a small key→value map. `store_commit` writes that map as a CRC'd TLV into alternating halves of the region the port exposes. Layout math stays here; the media (flash, file, RAM) stays in glue.

| API | Role |
| --- | --- |
| `store_init` | Read the newest valid half, or start empty |
| `store_get` / `store_set` / `store_delete` | RAM map |
| `store_commit` | Persist the map through `store_port_erase` + `store_port_write` |

Well-known keys:

| Key | Macro | Contents |
| --- | --- | --- |
| `0x0001` | `STORE_KEY_SLOTS` | Component slot specs (`ucp_cfg` save/load) |
| `0x0002` | `STORE_KEY_PACK_ADS` | Pack advertisements |

Caps in `store.c`: 8 keys, 64 bytes of payload per key, 512-byte commit image, 16-byte header. A larger product record needs a code change here, not a glue-only change.

Weak `store_port_*` stubs return errors so host tests can supply a RAM port. The STM32 example overrides them.

Port contract: [PORTING.md](PORTING.md).
