# ucp_tlm overview

`ucp_tlm` is a UCP **application** (not a transport):

- Live TLV bands (APP `0x20`):
  - `0x10` LIVE (default 20 ms → 50 Hz)
  - `0x11` HF (default 40 ms → 25 Hz)
  - `0x12` MF (default 100 ms → 10 Hz)
  - `0x13` LF (default 500 ms → 2 Hz)
  - `0x14` CHANGE (when dirty; cleared after send)
- Scheduler advances each band’s due time only after a successful send; caps frames per tick (`UCP_TLM_MAX_FRAMES_PER_TICK`)
- Multipart frame: `generation | part_index | part_count | TLVs…`
- Each TLV: `id[2 LE] | type | qflags | raw[W]` (`SIG_TLV_Q_VALID` / `SIG_TLV_Q_AGED`)
- Published slots that are **valid or aged** are encoded (never-written slots omitted)
- Service `GET_DIRECTORY` (`opcode 0x01`) on `0xFA` / `0xFB` returns an **xfer offer** (`ucp_xfer` APP `0x01`); host pulls the directory blob

It reads `sig` snapshots submitted by glue and emits frames when the UCP session is ready. Session/framing stay in `ucp`; catalog identity and aging stay in `sig`.

Exact version match (`0x0100` / V1.0) is required with the peer. See [UCP_APPLICATION_LAYER.md](../../ucp/docs/UCP_APPLICATION_LAYER.md).
