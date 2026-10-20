# UCP Time Source and Timeout Handling

Portable session timeouts assume `ucp_port_get_time_ms()` is a **monotonic** millisecond clock. A clock that jumps backward will immediately fire READY/HANDSHAKING timeouts and stall rediscovery.

## Platform contract (`ucp_port_get_time_ms`)

1. Return milliseconds as `uint32_t`.
2. Must not overflow a 32-bit multiply when converting from RTOS ticks.
3. Natural wrap of the 32-bit result (e.g. ~49.7 days at 1 kHz) is acceptable.
4. Session code compares time with unsigned modular elapsed helpers in `ucp_port.h`:
   - `ucp_elapsed_ms` / `ucp_elapsed_ge_ms` / `ucp_elapsed_gt_ms` for “how long since T”
   - `ucp_time_before_deadline_ms` for “have we reached a future deadline”

### Forbidden conversion (soak failure at ~71 minutes)

```c
/* WRONG at TX_TIMER_TICKS_PER_SECOND == 1000 */
return (tx_time_get() * 1000u) / TX_TIMER_TICKS_PER_SECOND;
```

`UINT32_MAX / 1000 = 4,294,967` ms ≈ **71.6 minutes**. After that `now_ms` jumps toward 0, `(now_ms - last_rx_ms)` underflows, and the link takes a false `TIMEOUT`.

### Required conversion

When ticks are not already 1 ms:

```c
return (uint32_t)(((uint64_t)ticks * 1000ull) / (uint64_t)TICKS_PER_SECOND);
```

When the RTOS tick **is** 1 ms (as in the STM32 example with `TX_TIMER_TICKS_PER_SECOND == 1000`), returning `(uint32_t)tx_time_get()` is correct — do not multiply by 1000.

### Overflow lifetime after a correct conversion

| Clock | Wrap | Wall time at 1000 Hz |
|-------|------|----------------------|
| Intermediate `ticks * 1000` in **uint32** | `2^32 / 1000` ms | **71.6 minutes** (bug) |
| Intermediate in **uint64**, result **uint32** | `2^32` ticks | **49.71 days** |
| True uint64 millisecond counter | `2^64` ms | ~584.9 million years |

Session timestamps remain `uint32_t`. After ~49.71 days `now_ms` wraps. Unsigned elapsed math (`now - then`) remains correct across that wrap as long as the interval being measured is less than ~49 days (all UCP timeouts are seconds).

## Session timeout rules (portable)

- `READY`: timeout if `ucp_elapsed_gt_ms(now, last_rx_ms, UCP_LINK_TIMEOUT_MS)` (3 s).
- `HANDSHAKING`: timeout if no progress for `UCP_HANDSHAKE_PROGRESS_TIMEOUT_MS`.
- `TIMEOUT` → `DISCOVERING` with a **quiet window** of `UCP_LINK_TIMEOUT_MS` before TX `HELLO_REQ`, so the peer can also expire. Incoming `HELLO_*` is still processed during quiet.
- Control `0xF0`/`0xF1`/`0xF2` must **not** refresh `last_rx_ms` and must **not** generate a reply (prevents keepalive-by-status-storm).
- Application packets while not `READY` are dropped (no `WARNING` flood).
