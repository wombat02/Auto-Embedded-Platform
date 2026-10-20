# store porting

`store` is a portable keyed blob API. **Layout and media are 100% glue** via:

| Hook | Role |
| --- | --- |
| `store_port_region_size` | Usable byte length of the NVS region |
| `store_port_read` | Read bytes at offset within the region |
| `store_port_write` | Program bytes (flash may require erase first) |
| `store_port_erase` | Erase a sub-range (page/sector aligned as needed) |

## Media options

- **Internal flash** — reserve a sector. `store.c` already uses alternating halves and a CRC.
- **External SPI/QSPI flash** — same hooks; offsets map into a dedicated partition.
- **SD / file** — `read`/`write` a file; `erase` can truncate or rewrite.
- **RAM** — fine for bring-up and demos; lost on reboot.

Weak stubs return errors so host unit tests can supply a RAM port.

## Example (stm32_h5)

[examples/stm32_h5/README.md](../../examples/stm32_h5/README.md) · [GLUE.md](../../examples/stm32_h5/GLUE.md)

`examples/stm32_h5/Core/Src/aep_store_port.c` implements a RAM region for demonstration only. A product port should use real NVS. `ucp_cfg save` still works until reboot.
