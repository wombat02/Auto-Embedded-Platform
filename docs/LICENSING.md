# Licensing

The sources in this repository are MIT. CubeMX packs used by the STM32 example are obtained by generating code from `examples/stm32_h5/stm32_h5.ioc` and are not published here.

## MIT (this repository's [LICENSE](../LICENSE))

| Tree | Notes |
| --- | --- |
| `sig/`, `ucp/`, `ucp_tlm/`, `ucp_cfg/`, `comp/`, `store/`, `vcom/` | Portable libraries |
| `vcom_diag/` except `third_party/` | `diag_link` / `diag_server` wrappers |
| `tools/` | Host scripts |
| `examples/stm32_h5/` | `aep_*` glue, `log.*`, `.ioc`, and docs |

`vcom_diag/third_party/iso14229` is also MIT, under its own [LICENSE](../vcom_diag/third_party/iso14229/LICENSE) (upstream tag `0.11.0`, including embedded `isotp-c`). See [vcom_diag/third_party/README.md](../vcom_diag/third_party/README.md).

## Cube packs (not in this repository)

Generate ST HAL, CMSIS, ThreadX, and USBX from the example `.ioc` (STM32Cube FW_H5 V1.6.0). Those trees keep their own licenses after generate:

| Pack | Typical license in the generate |
| --- | --- |
| STM32H5xx HAL | BSD-3-Clause |
| CMSIS | Apache-2.0 |
| ThreadX / USBX (Azure RTOS) | Microsoft Azure RTOS (`LICENSE.txt`, `LICENSED-HARDWARE.txt`) |

STM32H5 is on the Azure RTOS licensed-hardware list. That grants production and distribution rights for binaries on that MCU under Microsoft's terms, which are narrower than MIT.

Bring-up after generate: [examples/stm32_h5/BRINGUP.md](../examples/stm32_h5/BRINGUP.md).

Host-only Python packages (`udsoncan`, `python-can`, `can-isotp`, `pyserial`, `textual`) are not vendored. Their licenses apply only on the PC that installs them.
