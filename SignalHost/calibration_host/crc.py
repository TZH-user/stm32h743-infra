"""Calibration CRC helpers matching CalibrationCore_Crc32 (UTF-8)."""

from __future__ import annotations

import struct
import zlib


INT32_MIN = -(2**31)
INT32_MAX = 2**31 - 1


def _require_int32(value: int, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{field} must be int")
    if not INT32_MIN <= value <= INT32_MAX:
        raise ValueError(f"{field} is outside int32")
    return value


def calibration_import_payload(
    gain_mcpv: int,
    offset_mc: int,
    r2_ppm: int,
    residual_mc: int,
    points: int,
) -> bytes:
    """Return little-endian int32[5] used by CAL IMPORT ADC."""
    values = (
        _require_int32(gain_mcpv, "gain_mcpv"),
        _require_int32(offset_mc, "offset_mc"),
        _require_int32(r2_ppm, "r2_ppm"),
        _require_int32(residual_mc, "residual_mc"),
        _require_int32(points, "points"),
    )
    return struct.pack("<5i", *values)


def calibration_import_crc32(
    gain_mcpv: int,
    offset_mc: int,
    r2_ppm: int,
    residual_mc: int,
    points: int,
) -> int:
    """Compute reflected CRC-32/ISO-HDLC used by CalibrationCore."""
    payload = calibration_import_payload(
        gain_mcpv,
        offset_mc,
        r2_ppm,
        residual_mc,
        points,
    )
    return zlib.crc32(payload) & 0xFFFFFFFF


def format_crc32(value: int) -> str:
    return f"{value & 0xFFFFFFFF:08X}"
