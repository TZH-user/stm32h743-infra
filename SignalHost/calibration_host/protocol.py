"""Incremental ASCII line parser for the calibration control protocol (UTF-8)."""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
import re
import struct
from typing import Any
import zlib


KEY_VALUE_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)=([^\s]+)")
EXT_POINT_RE = re.compile(
    r"^\[CAL\] EXT n=(?P<n>\d+) ref_mv=(?P<ref_mv>-?\d+) "
    r"mean=(?P<mean>-?\d+(?:\.\d+)?) min=(?P<minimum>\d+) "
    r"max=(?P<maximum>\d+) flags=0x(?P<flags>[0-9A-Fa-f]+) "
    r"crc=(?P<crc>[0-9A-Fa-f]{8})$"
)
HEX32_RE = re.compile(r"^[0-9A-Fa-f]{8}$")
SHORT_EXPORT_RE = re.compile(
    r"^\[CAL\] EXPORT (?P<record>[BPE])(?: (?P<body>.*))?$"
)
LEGACY_EXPORT_RE = re.compile(
    r"^\[CAL\] EXPORT (?P<record>BEGIN|POINT|END)(?: (?P<body>.*))?$"
)
STATUS_V3_SPECS = {
    "[CAL] STATUS": (
        "cal_status",
        "CORE",
        ("v", "state", "error", "exclusive", "can_start"),
    ),
    "[CAL] SESSION": (
        "cal_status_session",
        "SESSION",
        ("worker_active", "reason", "points"),
    ),
    "[CAL] SELECT": (
        "cal_status_select",
        "SELECT",
        ("mode", "source", "item", "candidate"),
    ),
    "[CAL] PROFILE": (
        "cal_status_profile",
        "PROFILE",
        ("configured", "active", "pending"),
    ),
    "[CAL] PROFILE2": (
        "cal_status_profile2",
        "PROFILE2",
        ("previous", "undo_available", "enabled", "in_use", "applied"),
    ),
    "[CAL] ADC": (
        "cal_status_adc",
        "ADC",
        ("valid", "seq", "dc_uv", "rms_uv", "peak_uv"),
    ),
    "[CAL] LAST": (
        "cal_last",
        "LAST",
        ("output_mv", "mean_mc", "min", "max"),
    ),
    "[CAL] IO": (
        "cal_status_io",
        "IO",
        ("low", "high", "dir", "samples"),
    ),
    "[CAL] HW": (
        "cal_status_hw",
        "HW",
        ("relay", "dac_code", "adc_run"),
    ),
}
STATUS_V3_SECTIONS = tuple(spec[1] for spec in STATUS_V3_SPECS.values())


@dataclass(frozen=True, slots=True)
class ProtocolEvent:
    kind: str
    raw: str
    fields: dict[str, Any] = field(default_factory=dict)
    timestamp: str = field(
        default_factory=lambda: datetime.now(timezone.utc).isoformat()
    )


@dataclass(frozen=True, slots=True)
class StatusV3Snapshot:
    sections: dict[str, dict[str, Any]]
    missing_sections: tuple[str, ...]
    duplicate_sections: tuple[str, ...]
    extensions: tuple[str, ...]

    @property
    def complete(self) -> bool:
        return not self.missing_sections and not self.duplicate_sections


class StatusV3Collector:
    """Collect one multi-line STATUS v3 response without rejecting extensions."""

    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self.sections: dict[str, dict[str, Any]] = {}
        self.duplicates: list[str] = []
        self.extensions: list[str] = []

    def feed(self, event: ProtocolEvent) -> None:
        section = event.fields.get("section")
        if (
            event.kind == "cal_status"
            and section == "CORE"
            and "CORE" in self.sections
        ):
            self.reset()
        if section in STATUS_V3_SECTIONS:
            if section in self.sections and section not in self.duplicates:
                self.duplicates.append(str(section))
            else:
                self.sections[str(section)] = dict(event.fields)
        elif event.kind == "cal_extension":
            self.extensions.append(event.raw)

    def snapshot(self) -> StatusV3Snapshot:
        missing = tuple(
            section
            for section in STATUS_V3_SECTIONS
            if section not in self.sections
        )
        return StatusV3Snapshot(
            sections=dict(self.sections),
            missing_sections=missing,
            duplicate_sections=tuple(self.duplicates),
            extensions=tuple(self.extensions),
        )


def _coerce(value: str) -> Any:
    lowered = value.lower()
    if lowered.startswith("0x"):
        try:
            return int(lowered, 16)
        except ValueError:
            return value
    try:
        return int(value)
    except ValueError:
        try:
            return float(value)
        except ValueError:
            return value


def _key_values(line: str) -> dict[str, Any]:
    return {match.group(1): _coerce(match.group(2)) for match in KEY_VALUE_RE.finditer(line)}


def _parse_status_v3_line(line: str) -> ProtocolEvent | None:
    # PROFILE2 must be tested before PROFILE because the prefixes overlap.
    for prefix in sorted(STATUS_V3_SPECS, key=len, reverse=True):
        if line != prefix and not line.startswith(prefix + " "):
            continue
        kind, section, required = STATUS_V3_SPECS[prefix]
        body = line[len(prefix):].strip()
        tokens, error = _token_fields(body)
        if error:
            return ProtocolEvent(
                "malformed_cal_status",
                line,
                {"section": section, "reason": error},
            )
        if section == "CORE" and "v" not in tokens:
            legacy_required = ("state", "error", "exclusive", "can_start")
            missing = tuple(name for name in legacy_required if name not in tokens)
            if missing:
                return ProtocolEvent(
                    "malformed_cal_status",
                    line,
                    {"section": "LEGACY", "reason": f"missing:{','.join(missing)}"},
                )
            fields = {name: _coerce(value) for name, value in tokens.items()}
            fields.update({"section": "LEGACY", "v": 1})
            return ProtocolEvent("cal_status", line, fields)
        missing = tuple(name for name in required if name not in tokens)
        if missing:
            return ProtocolEvent(
                "malformed_cal_status",
                line,
                {"section": section, "reason": f"missing:{','.join(missing)}"},
            )
        fields = {name: _coerce(value) for name, value in tokens.items()}
        fields["section"] = section
        if section == "CORE" and fields["v"] != 3:
            return ProtocolEvent(
                "unsupported_cal_status",
                line,
                {"section": section, "version": fields["v"]},
            )
        if section == "SESSION":
            match = re.fullmatch(r"(\d+)/(\d+)", tokens["points"])
            if match is None:
                return ProtocolEvent(
                    "malformed_cal_status",
                    line,
                    {"section": section, "reason": "invalid:points"},
                )
            fields["completed_points"] = int(match.group(1))
            fields["expected_points"] = int(match.group(2))
        return ProtocolEvent(kind, line, fields)
    return None


def binary32_from_hex(text: str) -> float:
    """Reinterpret exactly eight hexadecimal digits as IEEE-754 binary32."""
    if not HEX32_RE.fullmatch(text):
        raise ValueError("binary32 field must contain exactly eight hexadecimal digits")
    bits = int(text, 16)
    return struct.unpack("<f", struct.pack("<I", bits))[0]


def export_point_crc32(
    reference_bits: int,
    measured_bits: int,
    frequency_hz: int,
    phase_bits: int,
    flags: int,
) -> int:
    """CRC32 of the little-endian 20-byte CalibrationMeasurementPoint payload."""
    payload = struct.pack(
        "<IIIII",
        reference_bits,
        measured_bits,
        frequency_hz,
        phase_bits,
        flags,
    )
    return zlib.crc32(payload) & 0xFFFFFFFF


def _token_fields(body: str) -> tuple[dict[str, str], str | None]:
    fields: dict[str, str] = {}
    for token in body.split():
        if "=" not in token:
            return {}, f"token_without_equals:{token}"
        key, value = token.split("=", 1)
        if not key or not value or key in fields:
            return {}, f"invalid_or_duplicate_field:{key}"
        fields[key] = value
    return fields, None


def _required_field(fields: dict[str, str], *names: str) -> str:
    for name in names:
        if name in fields:
            return fields[name]
    raise ValueError(f"missing field: {'/'.join(names)}")


def _decimal_field(fields: dict[str, str], *names: str) -> int:
    value = _required_field(fields, *names)
    if not re.fullmatch(r"\d+", value):
        raise ValueError(f"decimal field {'/'.join(names)} is invalid")
    return int(value, 10)


def _hex32_field(fields: dict[str, str], *names: str) -> tuple[int, str]:
    value = _required_field(fields, *names)
    if not HEX32_RE.fullmatch(value):
        raise ValueError(f"hex field {'/'.join(names)} must be exactly eight digits")
    return int(value, 16), value.upper()


def _parse_export_record(
    line: str,
    record: str,
    body: str,
    *,
    legacy: bool,
) -> ProtocolEvent:
    fields, error = _token_fields(body)
    if error:
        return ProtocolEvent("malformed_export", line, {"reason": error, "legacy": legacy})
    try:
        if record in {"B", "BEGIN"}:
            _crc_value, crc_text = _hex32_field(fields, "c", "crc")
            normalized: dict[str, Any] = {
                "sequence": _decimal_field(fields, "s", "seq"),
                "schema": _decimal_field(fields, "v", "schema"),
                "mode": _decimal_field(fields, "mo", "mode"),
                "source": _decimal_field(fields, "so", "source"),
                "item": _decimal_field(fields, "it", "item"),
                "expected": _decimal_field(fields, "ep", "expected"),
                "points": _decimal_field(fields, "np", "points"),
                "crc": crc_text,
                "legacy": legacy,
                "raw_fields": fields,
            }
            return ProtocolEvent("cal_export_begin", line, normalized)

        if record in {"P", "POINT"}:
            reference_bits, reference_hex = _hex32_field(fields, "r", "ref_f32")
            measured_bits, measured_hex = _hex32_field(fields, "m", "measured_f32")
            phase_bits, phase_hex = _hex32_field(fields, "p", "phase_f32")
            crc_value, crc_text = _hex32_field(fields, "c", "crc")
            frequency_hz = _decimal_field(fields, "f", "freq_hz")
            flags, flags_hex = _hex32_field(fields, "g", "flags")
            actual_crc = export_point_crc32(
                reference_bits,
                measured_bits,
                frequency_hz,
                phase_bits,
                flags,
            )
            normalized = {
                "sequence": _decimal_field(fields, "s", "seq"),
                "index": _decimal_field(fields, "i", "index"),
                "reference_bits": reference_bits,
                "reference_hex": reference_hex,
                "reference": binary32_from_hex(reference_hex),
                "measured_bits": measured_bits,
                "measured_hex": measured_hex,
                "measured": binary32_from_hex(measured_hex),
                "frequency_hz": frequency_hz,
                "phase_bits": phase_bits,
                "phase_hex": phase_hex,
                "phase_deg": binary32_from_hex(phase_hex),
                "flags": flags,
                "flags_hex": flags_hex,
                "crc": crc_text,
                "actual_crc": f"{actual_crc:08X}",
                "crc_valid": actual_crc == crc_value,
                "legacy": legacy,
                "raw_fields": fields,
            }
            kind = "cal_export_point" if actual_crc == crc_value else "cal_export_point_crc_error"
            return ProtocolEvent(kind, line, normalized)

        _crc_value, crc_text = _hex32_field(fields, "c", "crc")
        return ProtocolEvent(
            "cal_export_end",
            line,
            {
                "sequence": _decimal_field(fields, "s", "seq"),
                "points": _decimal_field(fields, "n", "points"),
                "crc": crc_text,
                "legacy": legacy,
                "raw_fields": fields,
            },
        )
    except (ValueError, struct.error) as exc:
        return ProtocolEvent(
            "malformed_export",
            line,
            {"reason": str(exc), "record": record, "legacy": legacy},
        )


def parse_protocol_line(line: str, *, historical: bool = False) -> ProtocolEvent:
    if "\ufffd" in line:
        return ProtocolEvent("malformed_encoding", line, {"reason": "replacement_character"})
    short_export = SHORT_EXPORT_RE.match(line)
    if short_export:
        return _parse_export_record(
            line,
            short_export.group("record"),
            short_export.group("body") or "",
            legacy=False,
        )
    if historical:
        legacy_export = LEGACY_EXPORT_RE.match(line)
        if legacy_export:
            return _parse_export_record(
                line,
                legacy_export.group("record"),
                legacy_export.group("body") or "",
                legacy=True,
            )
        if line.startswith("[CAL] EXPORT "):
            return ProtocolEvent(
                "malformed_export",
                line,
                {"reason": "invalid legacy export record", "legacy": True},
            )
    elif line.startswith("[CAL] EXPORT "):
        return ProtocolEvent("unknown", line)
    point = EXT_POINT_RE.match(line)
    if point:
        groups = point.groupdict()
        return ProtocolEvent(
            "cal_ext_point",
            line,
            {
                "n": int(groups["n"]),
                "ref_mv": int(groups["ref_mv"]),
                "mean": float(groups["mean"]),
                "min": int(groups["minimum"]),
                "max": int(groups["maximum"]),
                "flags": int(groups["flags"], 16),
                "crc": groups["crc"].upper(),
            },
        )
    if line.startswith("[CAL] EXT ERR"):
        return ProtocolEvent("cal_ext_error", line, _key_values(line))
    status = _parse_status_v3_line(line)
    if status is not None:
        return status
    if line.startswith("[CAL] "):
        return ProtocolEvent("cal_extension", line, {"extension": line[6:].split()[0]})
    if line == "[CTRL] PONG":
        return ProtocolEvent("pong", line)
    if line.startswith("[CTRL] OK"):
        return ProtocolEvent("control_ok", line, _key_values(line))
    if line.startswith("[CTRL] ERR"):
        return ProtocolEvent("control_error", line, _key_values(line))
    if line.startswith("[CTRL] CAL"):
        return ProtocolEvent("control_cal", line, _key_values(line))
    if line.startswith("[CTRL] STATUS"):
        return ProtocolEvent("control_status", line, _key_values(line))
    if line.startswith("[CTRL] RTOS") or line.startswith("[CTRL] STACK"):
        return ProtocolEvent("rtos", line, _key_values(line))
    return ProtocolEvent("unknown", line)


class IncrementalLineParser:
    """Split arbitrary byte chunks while accepting CR, LF, and CRLF."""

    def __init__(self, maximum_line_bytes: int = 512, *, historical: bool = False) -> None:
        if maximum_line_bytes < 1:
            raise ValueError("maximum_line_bytes must be positive")
        self.maximum_line_bytes = maximum_line_bytes
        self.historical = historical
        self._buffer = bytearray()
        self._discarding = False
        self._last_was_cr = False

    def reset(self) -> None:
        self._buffer.clear()
        self._discarding = False
        self._last_was_cr = False

    def feed(self, data: bytes) -> list[ProtocolEvent]:
        if not isinstance(data, bytes):
            raise TypeError("data must be bytes")
        events: list[ProtocolEvent] = []
        for byte in data:
            if byte == 0x0A and self._last_was_cr:
                self._last_was_cr = False
                continue
            self._last_was_cr = byte == 0x0D
            if byte in (0x0D, 0x0A):
                if self._discarding:
                    events.append(
                        ProtocolEvent(
                            "line_too_long",
                            "",
                            {"maximum_line_bytes": self.maximum_line_bytes},
                        )
                    )
                elif self._buffer:
                    line = self._buffer.decode("ascii", errors="replace")
                    events.append(parse_protocol_line(line, historical=self.historical))
                self._buffer.clear()
                self._discarding = False
                continue
            if self._discarding:
                continue
            if len(self._buffer) >= self.maximum_line_bytes:
                self._buffer.clear()
                self._discarding = True
                continue
            self._buffer.append(byte)
        return events

    def finish(self) -> list[ProtocolEvent]:
        if self._discarding:
            self.reset()
            return [
                ProtocolEvent(
                    "line_too_long", "", {"maximum_line_bytes": self.maximum_line_bytes}
                )
            ]
        if not self._buffer:
            return []
        line = self._buffer.decode("ascii", errors="replace")
        self._buffer.clear()
        self._last_was_cr = False
        return [parse_protocol_line(line, historical=self.historical)]
