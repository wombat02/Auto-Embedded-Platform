# UCP V1 Frame Format

This document defines the transport wire format and control packet ownership for UCP V1.

## Wire Layout

`[SOF][APP_ID][PKT_ID][LEN][PAYLOAD...][CHECKSUM]`

- `SOF`: fixed start marker (`0xA5`)
- `APP_ID`: protocol namespace selector
- `PKT_ID`: packet identifier inside app namespace
- `LEN`: payload length in bytes
- `CHECKSUM`: XOR over `APP_ID`, `PKT_ID`, `LEN`, and payload bytes

Maximum payload: `UCP_PKT_MAX_PAYLOAD_LEN_BYTES = 64`.

## APP_ID Ownership

- `0x00`: UCP transport/control
- `0x01`: transport transfer channel (`ucp_xfer` pull/push; see `UCP_XFER.md`)
- `0x02..0x1F`: reserved for future transport-owned channels
- `0x20..0x3F`: telemetry/application channels
- `0x60..0x6F`: request/response service channels
- `0x40..0x4F`, `0xF0..0xFF`: reserved ranges

Applications must not consume transport-owned control IDs.

## Reserved Control PKT_IDs (`APP_ID=0x00`)

- `0x00` `DEVICE_HELLO_REQ`
- `0x01` `DEVICE_HELLO_RSP`
- `0x02` `CAPABILITY_DISCOVERY_REQ`
- `0x03` `CAPABILITY_DISCOVERY_RSP`
- `0x06` `KEEPALIVE`
- `0x07` `LINK_READY_REQ`
- `0x08` `LINK_READY_RSP`
- `0xF0` `NOT_SUPPORTED`
- `0xF1` `WARNING`
- `0xF2` `ERROR`

All multibyte fields in payload schemas are little-endian.
