# Documentation

## Start here

| Document | Use |
| --- | --- |
| [QUICKSTART.md](QUICKSTART.md) | Host tests, then board after Cube generate |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Stack overview and why each layer exists |
| [FUTURE_COMPONENTS_CONFIG.md](FUTURE_COMPONENTS_CONFIG.md) | Component slots, sig tiers, config/NVM model |
| [LICENSING.md](LICENSING.md) | MIT portable sources vs Cube packs |
| [../examples/stm32_h5/README.md](../examples/stm32_h5/README.md) | Example map: glue file → portable module |
| [../examples/stm32_h5/BRINGUP.md](../examples/stm32_h5/BRINGUP.md) | CubeMX generate + USER CODE paste list |
| [../examples/stm32_h5/GLUE.md](../examples/stm32_h5/GLUE.md) | How example glue wires to portable libs |
| [../examples/stm32_h5/INTEGRATION.md](../examples/stm32_h5/INTEGRATION.md) | Board pins and include paths |
| [../examples/stm32_h5/DEMO_PT.md](../examples/stm32_h5/DEMO_PT.md) | Dual-bus PT / PID / cluster / dash demo |

## Portable libraries

Each library README links to its overview, porting notes, and the STM32 example that implements the port.

| Library | Entry |
| --- | --- |
| `aep_log` | [../aep_log/README.md](../aep_log/README.md) |
| `sig` | [../sig/README.md](../sig/README.md) |
| `ucp` | [../ucp/README.md](../ucp/README.md) · [../ucp/docs/README.md](../ucp/docs/README.md) |
| `ucp_tlm` | [../ucp_tlm/README.md](../ucp_tlm/README.md) |
| `ucp_cfg` | [../ucp_cfg/README.md](../ucp_cfg/README.md) |
| `comp` | [../comp/README.md](../comp/README.md) |
| `store` | [../store/README.md](../store/README.md) |
| `vcom` | [../vcom/README.md](../vcom/README.md) |
| `vcom_diag` | [../vcom_diag/README.md](../vcom_diag/README.md) |
