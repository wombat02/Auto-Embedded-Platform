# ucp_cfg porting

## Glue responsibilities

1. Register `g_ucp_cfg_app_vtable` in the `ucp_port` app registry (with `ucp_tlm` and any other apps).
2. Fill `ucp_cfg_bind_t`: component runtime, working `comp_slot_spec_t` array, ctx buffers, optional `before_apply` / `on_applied`.
3. Call `ucp_cfg_bind` after `store_init` and before the session services traffic.
4. Call `ucp_cfg_set_ads` for Tier B packs the device should advertise.
5. Provide a working [`store`](../../store/docs/PORTING.md) port. `SAVE` commits through it.

`ucp_cfg` does not know about HAL, threads, or flash layout.

## Example

[examples/stm32_h5/README.md](../../examples/stm32_h5/README.md) · [GLUE.md](../../examples/stm32_h5/GLUE.md)

- Registry: `examples/stm32_h5/Core/Src/aep_ucp_port.c` (vtable index 3)
- Bind and factory/NVM specs: `examples/stm32_h5/Core/Src/aep_app.c`
- Host: `python tools/ucp_cfg.py --port COMx list`
