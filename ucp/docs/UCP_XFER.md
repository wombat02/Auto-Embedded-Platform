# UCP Transfer Channel (`APP_ID=0x01`)

Shared pull/push blob transport above the fixed 64-byte V1 frame. Available whenever the link is `READY`. **Not** advertised in CAPABILITY_DISCOVERY.

## Modes

| Mode | Use | Flow |
|------|-----|------|
| **Pull** | Directories, CFG blobs, large service bodies | Producer `ucp_xfer_offer`; consumer `INFO_REQ` then `CHUNK_REQ` loop |
| **Push** | Bulk windows (firmware, logs) | Producer `ucp_xfer_push_begin`; segments with windowed `PUSH_ACK` |

Max SDU: **1024** bytes. One pull offer slot and one push RX reassembly buffer.

## Pull packets

| PKT | ID | Payload |
|-----|-----|---------|
| INFO_REQ | `0x01` | `token u16` |
| INFO_RSP | `0x02` | `token u16`, `total_len u16`, `chunk_cap u8`, `chunk_count u8`, `flags u8` |
| CHUNK_REQ | `0x03` | `token u16`, `index u8` |
| CHUNK_RSP | `0x04` | `token u16`, `index u8`, `chunk_count u8`, `data…` |
| ABORT | `0x05` | `token u16` |

## Push packets

| PKT | ID | Payload |
|-----|-----|---------|
| PUSH_DATA | `0x10` | `token u16`, `seq u16`, `flags u8`, `data…` |
| PUSH_ACK | `0x11` | `token u16`, `seq_acked u16`, `status u8` |

Flags: `FIRST=0x01`, `CONT=0x02`, `LAST=0x04`.

Window: sender may emit up to `UCP_XFER_PUSH_WINDOW` (4) segments, then waits for ACK. Always ACK on `LAST`. ACK timeout 500 ms.

Status: `OK=0`, `BUSY=1`, `OVERFLOW=2`, `BAD_SEQ=3`, `ABORTED=4`.

## Service helper pattern

If a REQ/RSP body fits in one UCP payload, send it inline. Otherwise:

1. `ucp_xfer_offer(&token, body, len)`
2. Short RSP body: `{ UCP_XFER_SVC_FLAG_OFFERED, token u16 LE, total_len u16 LE }`

## Future OTA

An update app owns BEGIN/offset/CRC/COMMIT and repeatedly push-sends ≤1024 B windows on this channel.
