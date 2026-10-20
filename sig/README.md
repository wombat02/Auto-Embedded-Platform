# sig

Tier A signal catalog, TLV codec, and slot table. No UCP, RTOS, or HAL.

| Doc | Contents |
| --- | --- |
| [docs/OVERVIEW.md](docs/OVERVIEW.md) | Role in the stack |
| [docs/CATALOG.md](docs/CATALOG.md) | Tier A / B / C |
| [docs/PORTING.md](docs/PORTING.md) | How glue uses the table |
| [docs/TESTING.md](docs/TESTING.md) | Host unit tests |

Catalog revision: `SIG_CATALOG_REVISION` (`0x0001`). That value is a revision counter, not a UCP major/minor version.

Example: [examples/stm32_h5/README.md](../examples/stm32_h5/README.md).
