# vcom porting

## PHY vtable (implemented in glue)

```c
typedef struct {
    void *hw;
    bool (*set_standby)(void *hw, bool standby);
    bool (*set_bitrate)(void *hw, uint32_t bit_s);
    bool (*start)(void *hw);
    void (*stop)(void *hw);
    bool (*send)(void *hw, const vcom_can_frame_t *fr);
    bool (*rx_pop)(void *hw, vcom_can_frame_t *fr);
} vcom_can_phy_t;
```

K-Line is a separate PHY (`vcom_kline_phy_t`). Echo cancellation stays in glue.

## Glue responsibilities

- Implement `vcom_can_phy_t` (and optional K-Line PHY) on your controller
- Call `vcom_core_drain_rx` and `vcom_core_tick` from a comms thread
- Map config to which buses start; never put pin names in `vcom/`

## Example

[examples/stm32_h5/README.md](../../examples/stm32_h5/README.md) · [GLUE.md](../../examples/stm32_h5/GLUE.md)

`examples/stm32_h5/Core/Src/aep_fdcan_phy.c`, `aep_kline_phy.c`, and the com thread in `aep_app.c`.
