# UCP Service Implementation Guidelines

Use this guidance for request/response services implemented as application-layer UCP channels.

## Service Envelope

- Request packet: `corr_id(u16), opcode(u8), flags(u8), body...`
- Response packet: `corr_id(u16), opcode(u8), status(u8), body...`
- Multibyte fields are little-endian.

## Required Behaviors

1. One service protocol per `APP_ID`.
2. Exactly one terminal response per accepted request.
3. Deterministic status mapping (`OK`, `BAD_PACKET`, `INVALID_ARG`, `UNSUPPORTED_OPCODE`, `BUSY`, `TIMEOUT`, `VALIDATE_FAILED`, `BACKEND_ERROR`).
4. Correlation IDs unique per in-flight request per app.
5. Clear pending/in-flight state on link restart events.

## Runtime Ownership

If backend state is owned by a dedicated worker context, service ingress should queue requests and not mutate backend state directly from transport callback context.

## Size Budget

UCP payload max is 64 bytes. Service envelope consumes 4 bytes; service body budget is 60 bytes.

For larger bodies use the transport transfer channel (`ucp_xfer`, APP `0x01` — see `UCP_XFER.md`):

1. `ucp_xfer_offer(&token, body, len)`
2. RSP body: `flag=UCP_XFER_SVC_FLAG_OFFERED (0x01)`, `token u16 LE`, `total_len u16 LE`
3. Client pulls via INFO/CHUNK on APP `0x01`

Bulk uploads (firmware windows) should use push mode on the same channel, not the service envelope.

## Client Rules

- Check negotiated app/version/roles before issuing requests.
- Use retry only for idempotent operations.
- Treat mutating operations as non-idempotent unless explicit transaction semantics exist.
