# SPDX-License-Identifier: MIT
"""Shared mock PT-CAN / UDS DID / dash constants (mirror aep_demo_pt.h)."""

PT_PROFILE_A = 0
PT_PROFILE_B = 1

PT_FRAME_A_POWERTRAIN = 0x201
PT_FRAME_A_TORQUE = 0x202
PT_FRAME_B_POWERTRAIN = 0x211
PT_FRAME_B_VEHICLE = 0x212

# Legacy aliases
PT_FRAME_POWERTRAIN = PT_FRAME_A_POWERTRAIN
PT_FRAME_TORQUE = PT_FRAME_A_TORQUE

CLUSTER_FRAME_ID = 0x301
DASH_FRAME_STEER = 0x401
DASH_FRAME_STALKS = 0x402

PACK_DEMO_DASH = 0xD001
PACK_DEMO_DASH_REV = 0x0001
SIG_DASH_STEER_DEG = 0xF010
SIG_DASH_TURN_L = 0xF011
SIG_DASH_TURN_R = 0xF012
SIG_DASH_WIPER = 0xF013
SIG_DASH_HIGH_BEAM = 0xF014

UDS_REQ_ID = 0x7E0
UDS_RESP_ID = 0x7E8
UDS_FUNC_ID = 0x7DF

SIG_EXT_BOOST = 0xF001
SIG_EXT_FUEL_PRESS = 0xF002

DID_BOOST = 0xF201
DID_FUEL_PRESS = 0xF202

COMP_ID_PT_PARSER = 0x0001
COMP_ID_PID_CLIENT = 0x0002
COMP_ID_CLUSTER = 0x0003
COMP_ID_DASH = 0x0004

SIG_ENGINE_RPM = 0x0004
SIG_COOLANT_TEMP = 0x0005
SIG_ENGINE_TORQUE = 0x0006


def encode_pt_powertrain(rpm: float, coolant_c: float) -> bytes:
    """Profile A 0x201: RPM u16 BE (raw=rpm*4), coolant u8 (raw=C+40)."""
    rpm_raw = max(0, min(0xFFFF, int(round(rpm * 4.0))))
    cool_raw = max(0, min(0xFF, int(round(coolant_c + 40.0))))
    return bytes([(rpm_raw >> 8) & 0xFF, rpm_raw & 0xFF, cool_raw, 0, 0, 0, 0, 0])


def encode_pt_torque(nm: float) -> bytes:
    """Profile A 0x202: torque i16 BE (raw=Nm*10)."""
    tq_raw = max(-32768, min(32767, int(round(nm * 10.0))))
    if tq_raw < 0:
        tq_raw = (tq_raw + 0x10000) & 0xFFFF
    return bytes([(tq_raw >> 8) & 0xFF, tq_raw & 0xFF, 0, 0, 0, 0, 0, 0])


def encode_pt_b_powertrain(rpm: float, load_pct: float, coolant_c: float) -> bytes:
    """Profile B 0x211: rpm u16 LE, load u16 LE (raw=%*10), coolant u8."""
    rpm_i = max(0, min(0xFFFF, int(round(rpm))))
    load_raw = max(0, min(0xFFFF, int(round(load_pct * 10.0))))
    cool_raw = max(0, min(0xFF, int(round(coolant_c + 40.0))))
    return bytes(
        [
            rpm_i & 0xFF,
            (rpm_i >> 8) & 0xFF,
            load_raw & 0xFF,
            (load_raw >> 8) & 0xFF,
            cool_raw,
            0,
            0,
            0,
        ]
    )


def encode_pt_b_vehicle(vss_kmh: float, gear: int, throttle_pct: float) -> bytes:
    """Profile B 0x212: vss u16 LE (raw=kmh*100), gear i8, throttle u16 LE."""
    vss = max(0, min(0xFFFF, int(round(vss_kmh * 100.0))))
    thr = max(0, min(0xFFFF, int(round(throttle_pct * 10.0))))
    g = max(-128, min(127, int(gear))) & 0xFF
    return bytes([vss & 0xFF, (vss >> 8) & 0xFF, g, thr & 0xFF, (thr >> 8) & 0xFF, 0, 0, 0])


def encode_dash_steer(deg: float) -> bytes:
    raw = max(-32768, min(32767, int(round(deg))))
    if raw < 0:
        raw = (raw + 0x10000) & 0xFFFF
    return bytes([(raw >> 8) & 0xFF, raw & 0xFF, 0, 0, 0, 0, 0, 0])


def encode_dash_stalks(*, turn_l: bool, turn_r: bool, wiper: bool, high_beam: bool) -> bytes:
    f = (1 if turn_l else 0) | (2 if turn_r else 0) | (4 if wiper else 0) | (8 if high_beam else 0)
    return bytes([f, 0, 0, 0, 0, 0, 0, 0])


def decode_cluster(data: bytes) -> tuple[int, int, int]:
    """0x301: RPM u16 BE, coolant i8 C, torque i16 BE Nm."""
    if len(data) < 5:
        raise ValueError("cluster frame too short")
    rpm = (data[0] << 8) | data[1]
    cool = int.from_bytes(bytes([data[2]]), "big", signed=True)
    tq = int.from_bytes(data[3:5], "big", signed=True)
    return rpm, cool, tq
