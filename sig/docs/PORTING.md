# sig porting

## Glue responsibilities

1. Call `sig_table_init` and `sig_table_bind` for each id you publish.
2. On a periodic or event path, `sig_table_write` then `sig_table_snapshot`.
3. Hand the snapshot to your telemetry app (example: `ucp_tlm_sig_submit`).

## Do not put in this library

- HAL handles, pin names, ThreadX types
- UCP framing or session state
- Vehicle-specific decode maps (those stay in product firmware)

## Example

[examples/stm32_h5/README.md](../../examples/stm32_h5/README.md) · [GLUE.md](../../examples/stm32_h5/GLUE.md)

See `examples/stm32_h5/Core/Src/aep_app.c` (`aep_app_on_blink`). Parsers and modules in that project also `sig_table_write` Tier A and extension ids.
