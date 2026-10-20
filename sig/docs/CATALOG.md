# Signal catalog tiers

## Tier A (this library)

Portable, append-only IDs in `sig_catalog.h`. PT / vehicle-common semantics for displays, relays, and control — engine-type agnostic (ICE/EV/hybrid consumers normalize into these).

- Revision: `SIG_CATALOG_REVISION` / `sig_catalog_revision()`
- Scaling: `eng = (raw * mult) / div + offset` via `sig_decode_eng`
- Hosts should track the same revision (see `tools/sig_catalog_host.py`)

## Tier B (packs)

OEM / body / vendor packs: IDs typically `≥ 0xF000` (or a reserved pack block). Meaning lives in **host-side manifests** keyed by `pack_id + revision` advertised by the board — not in this catalog.

## Tier C (private)

On-device-only coupling between components. May use extension IDs; default **not** published on UCP telemetry.

See also [docs/FUTURE_COMPONENTS_CONFIG.md](../../docs/FUTURE_COMPONENTS_CONFIG.md).
