# vcom overview

Portable CAN/K-Line scheduling and PHY seams. No HAL types, no controller handles.

## Types

```c
typedef struct {
    uint32_t id;
    uint8_t  ide;   /* 0 standard, 1 extended */
    uint8_t  dlc;   /* 0..8 */
    uint8_t  data[8];
} vcom_can_frame_t;

enum { VCOM_BUS_CAN0 = 0, VCOM_BUS_CAN1 = 1, VCOM_BUS_COUNT = 2 };
```

## Scheduler

- High band: one-shot queue via `vcom_tx_post` / `vcom_tx_post_local` (per-bus depth 8)
- Low band: periodic slots via `vcom_tx_periodic` (24 slots)
- Tick order in `vcom_core_tick`: drain once-queue, then due periodics
- Typical glue tick: on the order of **1–10 ms**; ISO-TP uses a separate **1 ms** poll in `vcom_diag`

## RX

```c
bool vcom_rx_subscribe(vcom_core_t *c, uint8_t bus,
                       uint32_t id, uint32_t id_mask,
                       vcom_rx_fn fn, void *user);
void vcom_core_drain_rx(vcom_core_t *c, uint8_t bus);
```

Stack fit: [docs/ARCHITECTURE.md](../../docs/ARCHITECTURE.md). UDS/ISO-TP: [vcom_diag](../../vcom_diag/README.md).
