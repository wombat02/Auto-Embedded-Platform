# UCP documentation

Portable UCP provides a transport/session core for any MCU/RTOS. Platform UART/USB/time/log/application hooks go through `ucp_port.*`.

## Source layout

- `ucp_protocol.*` — frame format, packet IDs, checksum helpers
- `ucp_parser.*` — byte-stream parser
- `ucp_session.*` — discovery/handshake/ready state machine
- `ucp_port.*` — weak defaults + platform integration contract
- `ucp_xfer.*` — transport pull/push transfer channel (APP `0x01`)
- `ucp_log.h` — compile-time logging controls
- `ucp_types.h` — shared payload and device-ID types

## Recommended default build settings

- `UCP_LOG_ENABLE=1`
- `UCP_LOG_LEVEL=UCP_LOG_LEVEL_INFO`
- `UCP_DEBUG_VERBOSE=0`

## Index

- [OVERVIEW.md](OVERVIEW.md)
- [PORTING.md](PORTING.md) · [UCP_PORTING_GUIDE.md](UCP_PORTING_GUIDE.md)
- [TESTING.md](TESTING.md)
- [UCP_V1_FRAME_FORMAT.md](UCP_V1_FRAME_FORMAT.md)
- [UCP_DISCOVERY_HANDSHAKE.md](UCP_DISCOVERY_HANDSHAKE.md)
- [UCP_SESSION_STATE_MACHINE.md](UCP_SESSION_STATE_MACHINE.md)
- [UCP_TRANSPORT_SESSION_ARCHITECTURE.md](UCP_TRANSPORT_SESSION_ARCHITECTURE.md)
- [UCP_APPLICATION_LAYER.md](UCP_APPLICATION_LAYER.md) (version encoding: major/minor)
- [UCP_SERVICE_IMPLEMENTATION_GUIDELINES.md](UCP_SERVICE_IMPLEMENTATION_GUIDELINES.md)
- [UCP_XFER.md](UCP_XFER.md)
- [UCP_TIME_AND_TIMEOUTS.md](UCP_TIME_AND_TIMEOUTS.md)
