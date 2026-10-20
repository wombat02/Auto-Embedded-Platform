# SPDX-License-Identifier: MIT
"""Host-side Tier A catalog mirror (keep in sync with sig/include/sig_catalog.h)."""

from __future__ import annotations

import struct
from typing import Dict, Optional, Tuple

SIG_CATALOG_REVISION = 0x0001

# id -> (type, mult, div, offset, stale_ms, name)  eng = raw*mult/div + offset
# type: 0x04 u16, 0x05 i16, 0x02 u8, 0x03 i8, 0x10 flag32
Entry = Tuple[int, int, int, int, int, str]

CATALOG: Dict[int, Entry] = {
    0x0004: (0x04, 1, 4, 0, 200, "engine_rpm"),
    0x0005: (0x05, 1, 1, -40, 1000, "coolant_temp"),
    0x0006: (0x05, 1, 10, 0, 500, "engine_torque"),
    0x0007: (0x04, 1, 10, 0, 500, "engine_load"),
    0x0008: (0x04, 1, 10, 0, 200, "throttle_pos"),
    0x0009: (0x04, 1, 10, 0, 200, "map_kpa"),
    0x000A: (0x04, 1, 100, 0, 200, "maf_gps"),
    0x000B: (0x05, 1, 1, -40, 1000, "iat"),
    0x000C: (0x05, 1, 10, 0, 200, "boost_kpa"),
    0x000D: (0x04, 1, 1, 0, 500, "fuel_press_kpa"),
    0x000E: (0x04, 1, 10, 0, 500, "oil_press_kpa"),
    0x000F: (0x05, 1, 1, -40, 1000, "oil_temp"),
    0x0010: (0x04, 1, 1000, 0, 200, "lambda"),
    0x0011: (0x04, 1, 100, 0, 200, "afr"),
    0x0012: (0x05, 1, 10, 0, 200, "ign_adv_deg"),
    0x0013: (0x04, 1, 1, 0, 200, "inj_pw_us"),
    0x0014: (0x04, 1, 10, 0, 500, "fuel_duty"),
    0x0015: (0x04, 1, 10, 0, 500, "wastegate_duty"),
    0x0016: (0x04, 1, 10, 0, 2000, "ethanol_pct"),
    0x0020: (0x04, 1, 100, 0, 200, "vehicle_speed"),
    0x0021: (0x03, 1, 1, 0, 500, "gear"),
    0x0022: (0x02, 1, 1, 0, 200, "clutch"),
    0x0023: (0x02, 1, 1, 0, 200, "brake"),
    0x0024: (0x04, 1, 10, 0, 200, "accel_pedal"),
    0x0025: (0x04, 1, 100, 0, 200, "wheel_fl_kmh"),
    0x0026: (0x04, 1, 100, 0, 200, "wheel_fr_kmh"),
    0x0027: (0x04, 1, 100, 0, 200, "wheel_rl_kmh"),
    0x0028: (0x04, 1, 100, 0, 200, "wheel_rr_kmh"),
    0x0029: (0x05, 1, 100, 0, 200, "yaw_rate"),
    0x002A: (0x05, 1, 100, 0, 200, "lat_accel"),
    0x002B: (0x05, 1, 100, 0, 200, "long_accel"),
    0x0030: (0x04, 1, 100, 0, 1000, "batt_volt"),
    0x0031: (0x02, 1, 1, 0, 1000, "alt_status"),
    0x0032: (0x02, 1, 1, 0, 1000, "mil"),
    0x0033: (0x02, 1, 1, 0, 500, "engine_run"),
    0x0034: (0x02, 1, 1, 0, 1000, "drive_mode"),
    0x0035: (0x10, 1, 1, 0, 500, "pt_flags"),
}

# Tier B / extension examples (not in Tier A catalog)
EXT: Dict[int, Entry] = {
    0xF001: (0x04, 1, 100, 0, 500, "ext_boost_bar"),  # host demo: raw/100 bar
    0xF002: (0x04, 1, 1, 0, 500, "ext_fuel_kpa"),
    0xF010: (0x05, 1, 1, 0, 500, "dash_steer_deg"),  # i16 degrees (raw=eng)
    0xF011: (0x02, 1, 1, 0, 500, "dash_turn_l"),
    0xF012: (0x02, 1, 1, 0, 500, "dash_turn_r"),
    0xF013: (0x02, 1, 1, 0, 500, "dash_wiper"),
    0xF014: (0x02, 1, 1, 0, 500, "dash_high_beam"),
}


def _as_signed(typ: int, raw: int) -> int:
    if typ == 0x03:
        return struct.unpack("<b", struct.pack("<B", raw & 0xFF))[0]
    if typ == 0x05:
        return struct.unpack("<h", struct.pack("<H", raw & 0xFFFF))[0]
    if typ == 0x07:
        return struct.unpack("<i", struct.pack("<I", raw & 0xFFFFFFFF))[0]
    return raw


def decode_eng(sid: int, raw: int, typ: Optional[int] = None) -> Optional[float]:
    e = CATALOG.get(sid) or EXT.get(sid)
    if e is None:
        return None
    etyp, mult, div, offset, _stale_ms, _name = e
    if typ is not None and typ != etyp:
        etyp = typ
    r = _as_signed(etyp, raw)
    return (r * mult) / float(div) + offset


def scale_display(sid: int, typ: int, raw: int) -> str:
    e = CATALOG.get(sid) or EXT.get(sid)
    eng = decode_eng(sid, raw, typ)
    if e is None or eng is None:
        return f"id=0x{sid:04X} type=0x{typ:02X} raw={raw}"
    _t, _m, _d, _o, _stale, name = e
    if typ == 0x10:
        return f"{name}=0x{raw:08X}"
    if float(eng).is_integer():
        return f"{name}={int(eng)}"
    return f"{name}={eng:.3f}"
