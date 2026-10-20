# ucp_tlm porting

## Glue responsibilities

1. Include `ucp_tlm_sig.h` and put `g_ucp_tlm_sig_app_vtable` in your `ucp_port` app registry (example: `aep_ucp_port.c`).
2. After `sig_table_snapshot`, call `ucp_tlm_sig_submit`.
3. Ensure `ucp_port_send_frame` / session poll run so TX can leave the device.

Producer role flags: `TX | REQ_RSP`. Consumer host tools use `RX | REQ_RSP`.

## Example

[examples/stm32_h5/README.md](../../examples/stm32_h5/README.md) · [GLUE.md](../../examples/stm32_h5/GLUE.md)

Parsers and modules update signals; `aep_ucp_port.c` registers `g_ucp_tlm_sig_app_vtable` next to xfer and `ucp_cfg`.
