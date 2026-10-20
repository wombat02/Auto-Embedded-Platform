# store

Portable keyed blob store. API: `store_init` / `get` / `set` / `delete` / `commit`. Media is entirely `store_port_*`.

| Doc | Contents |
| --- | --- |
| [docs/OVERVIEW.md](docs/OVERVIEW.md) | Keys, caps, commit image |
| [docs/PORTING.md](docs/PORTING.md) | Port hooks and media options |

Example: [examples/stm32_h5/README.md](../examples/stm32_h5/README.md) — `Core/Src/aep_store_port.c` is a **RAM** demonstration. A product implements the hooks on real NVS.
