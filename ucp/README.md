# ucp

Frame, byte parser, session, and port contract.

| Doc | Contents |
| --- | --- |
| [docs/README.md](docs/README.md) | Protocol doc index |
| [docs/OVERVIEW.md](docs/OVERVIEW.md) | Role in the stack |
| [docs/PORTING.md](docs/PORTING.md) | Glue hooks (summary + link to full guide) |
| [docs/TESTING.md](docs/TESTING.md) | Host unit tests |

Host/tests: `ucp_port_set_ops()` installs send/time/descriptor hooks. Firmware can leave ops NULL and override the weak `ucp_port_send_frame` / time / descriptors (see example `aep_ucp_port.c`).

Version encoding (major/minor): [docs/UCP_APPLICATION_LAYER.md](docs/UCP_APPLICATION_LAYER.md).

Example: [examples/stm32_h5/README.md](../examples/stm32_h5/README.md) · [GLUE.md](../examples/stm32_h5/GLUE.md) · [BRINGUP.md](../examples/stm32_h5/BRINGUP.md).

Role negotiation:

- Both peers `REQ_RSP` ⇒ negotiate `REQ_RSP` **and** complementary TX/RX
- Same-direction overlap warns only when that is the only overlap
