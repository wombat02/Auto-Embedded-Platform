# UCP Session State Machine

Portable state machine implementation lives in `ucp_session.c`.

## States

- `DOWN`: uninitialized
- `DISCOVERING`: HELLO exchange
- `HANDSHAKING`: capability + link-ready exchange
- `READY`: application traffic allowed
- `TIMEOUT`: transient recovery state

## Handshake Substeps

- `CAP_REQ`: capability negotiation in progress
- `LINK_READY_REQ`: capability complete, link-ready exchange in progress

## Critical Invariants

1. `READY` is blocked until `peer_capability_received=true`.
2. Duplicate `LINK_READY_*` must not continuously reset handshake progress.
3. CAP parse/checksum failures must release CAP in-flight retry gate.
4. `ucp_session_tick(now_ms)` must run with fresh monotonic timestamp from current loop iteration.
5. Restart paths must clear deferred link-ready/cap timing fields to prevent stale carry-over.
6. `now_ms` comparisons use `ucp_elapsed_*` / `ucp_time_before_deadline_ms` (unsigned modular). A non-monotonic clock (uint32 tick×1000 overflow) is a port bug, not a session bug.
7. `HELLO_REQ` in `READY` is a peer restart: clear cap state and enter `HANDSHAKING`.
8. After `TIMEOUT`, `discover_quiet_until_ms` delays TX HELLO so the peer can time out too.

## Tick Behavior (Summary)

- `DISCOVERING`: optional quiet window, then HELLO retries
- `HANDSHAKING`: CAP/LINK_READY retries and progress timeout supervision
- `READY`: keepalive transmit + receive timeout supervision (`last_rx` not updated by 0xF0/0xF1/0xF2)
- `TIMEOUT`: immediate reset back to `DISCOVERING` (nonce advanced, quiet window armed)
