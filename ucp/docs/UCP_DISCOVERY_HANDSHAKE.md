# UCP Discovery and Handshake

This document defines link bring-up and capability negotiation behavior.

## Link States

`DOWN -> DISCOVERING -> HANDSHAKING -> READY -> TIMEOUT -> DISCOVERING`

## Timing Constants

- `UCP_LINK_TIMEOUT_MS = 3000`
- `UCP_KEEPALIVE_PERIOD_MS = 2000`
- `UCP_HELLO_RETRY_PERIOD_MS = 500`
- `UCP_HANDSHAKE_RETRY_PERIOD_MS = 500`
- `UCP_CAP_REQ_RETRY_MS = 600`
- `UCP_CAP_RSP_WINDOW_MS = 800`
- `UCP_CAP_PEER_LISTEN_MS = 300`

## Handshake Rules

1. Start in `DISCOVERING`, periodically transmit `HELLO_REQ`.
2. On valid peer hello, enter `HANDSHAKING`.
3. In handshake, perform capability exchange before completing `LINK_READY`.
4. Do not enter `READY` until peer capability is successfully stored.
5. If no progress occurs for timeout window, transition to `TIMEOUT` then rediscover.
6. After `TIMEOUT`, suppress TX `HELLO_REQ` for `UCP_LINK_TIMEOUT_MS` so the peer can also expire. Incoming HELLO is still accepted.
7. `HELLO_REQ` received in `READY` always starts renegotiation (do not require nonce change).
8. Increment `local_session_nonce` on `DOWN` and on `TIMEOUT`.
9. Control `0xF0`/`0xF1`/`0xF2` are sink-only: no reply, do not refresh `last_rx_ms`.
10. Ignore HELLO whose `HW_ID` equals the local hardware ID (UART echo).

## Negotiation Rules

- App intersection requires exact `app_id` + exact `version`.
- Telemetry apps negotiate on complementary `TX`/`RX` role pairing.
- Service apps require `REQ_RSP` role on both peers.

## Driver Guardrails

1. **DMA lifetime:** DMA TX source buffer must remain valid until TX-complete callback.
2. **Loop ordering:** drain RX and dispatch packets before `ucp_session_tick(now_ms)`.
3. **Fresh timestamp:** capture `now_ms` after RX processing.
4. **Restart hygiene:** clear CAP hold/quiet windows and deferred state on timeout/discovering.
5. **CAP checksum recovery:** on CAP parse/checksum failure, reset parser and clear CAP in-flight retry gate.
6. **Time:** `now_ms` must come from a monotonic clock (64-bit tick conversion). See `UCP_TIME_AND_TIMEOUTS.md`.
