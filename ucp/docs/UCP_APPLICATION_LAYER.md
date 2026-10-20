# UCP Application Layer

Application protocols run above transport/session and are routed by `{APP_ID, PKT_ID}`.

## Requirements

1. Allocate unique application `APP_ID` (outside transport-owned range).
2. Advertise `{app_id, version, role_flags}` through port descriptor hook.
3. Register app vtable hooks (`init`, `on_rx`, `on_tick`, `on_link_event`).
4. Validate app payload schemas inside app handlers (transport validates control schemas only).
5. Respect session gating: app traffic is valid only in `READY`.

## Versioning

Protocol and application versions are a `uint16_t`:

| Byte | Role |
| --- | --- |
| High | Major |
| Low | Minor |

Examples: `0x0100` is V1.0. `0x0203` would be V2.3.

- A **minor** bump keeps wire compatibility (additive fields, optional opcodes).
- A **major** bump is a breaking change.
- Session negotiation requires an **exact** `uint16` match for the protocol version and for each CAP-advertised app version.

This release ships V1.0 (`0x0100`) for `UCP_PROTOCOL_VERSION`, `ucp_tlm` (APP `0x20`), `ucp_cfg` (APP `0x71`), and `ucp_xfer` (APP `0x01`, route-only).

The signal catalog uses a separate revision counter (`SIG_CATALOG_REVISION`), not this major/minor encoding. Catalog ids live in `sig`.

Minimal REQ_RSP sketch (unbuilt): [examples/stm32_h5/snippets/minimal_ucp_app.c](../../examples/stm32_h5/snippets/minimal_ucp_app.c).

## Role Negotiation

- Telemetry-style channels use complementary `TX`/`RX` pairing.
- Request/response services require `REQ_RSP` role on both peers.

## Link Event Behavior

Applications must clear per-link transient state on timeout/discovery events and tolerate reconnect cycles.
