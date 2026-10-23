"""HIL artifact loading, replay extraction, and review reports (UTF-8)."""

from __future__ import annotations

import csv
from dataclasses import asdict
import json
import os
from pathlib import Path
import re
from typing import Iterable

from .calibration_plan import AdcCalibrationPreset
from .fit import AdcQualityReport, CalibrationPoint, evaluate_adc_quality
from .protocol import ProtocolEvent, parse_protocol_line


HOST_METADATA_RE = re.compile(r"^\[[^\]]+\]\s+(?P<body>.*)$")
REFERENCE_METADATA_RE = re.compile(
    r"\]\s+REF target=(?P<target>-?\d+(?:\.\d+)?) "
    r"dg=(?P<dg>-?\d+(?:\.\d+)?) measured=(?P<reference>-?\d+(?:\.\d+)?) "
    r"vpp=(?P<vpp>\d+(?:\.\d+)?) ds_scale=(?P<scale>\d+(?:\.\d+)?)"
)


class WorkflowJournal:
    """Append-only UTF-8 journal; each append is flushed before returning."""

    def __init__(self, path: Path) -> None:
        self.path = path

    def append(self, event: str, **fields: object) -> None:
        payload = {"event": event, **fields}
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("a", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps(payload, ensure_ascii=False) + "\n")
            stream.flush()
            os.fsync(stream.fileno())

    def load(self) -> list[dict]:
        if not self.path.exists():
            return []
        records: list[dict] = []
        for number, line in enumerate(
            self.path.read_text(encoding="utf-8").splitlines(), 1
        ):
            try:
                records.append(json.loads(line))
            except json.JSONDecodeError as exc:
                raise ValueError(f"invalid journal JSON at line {number}") from exc
        return records

    def recovery_summary(self) -> dict:
        records = self.load()
        points = [record for record in records if record.get("event") == "point_complete"]
        start = next(
            (record for record in records if record.get("event") == "workflow_start"),
            {},
        )
        return {
            "record_count": len(records),
            "completed_points": len(points),
            "last_plan_index": (
                points[-1].get("plan_index") if points else None
            ),
            "source": start.get("source"),
            "item": start.get("item"),
            "session_identity_available": False,
            "automatic_resume_allowed": False,
            "manual_confirmation_required": bool(points),
        }


def load_hil_points_csv(path: Path) -> list[CalibrationPoint]:
    points: list[CalibrationPoint] = []
    with path.open("r", encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            points.append(
                CalibrationPoint(
                    original_index=int(row["session_index"]) - 1,
                    reference_mv=float(row["reference_v"]) * 1000.0,
                    measured_code=float(row["measured_code"]),
                    crc=row["crc32"].upper(),
                    included=abs(float(row["nominal_v"])) <= 4.5,
                    exclusion_reason=(
                        "" if abs(float(row["nominal_v"])) <= 4.5
                        else "rail_audit_only"
                    ),
                    nominal_v=float(row["nominal_v"]),
                    direction=row["direction"],
                    direction_index=int(row["direction_index"]),
                    dg_offset_v=float(row["dg_offset_v"]),
                    reference_v=float(row["reference_v"]),
                    reference_vpp=float(row["reference_vpp"]),
                    minimum_code=int(row["minimum_code"]),
                    maximum_code=int(row["maximum_code"]),
                    flags=int(row["flags"], 0),
                    ds_scale_v_div=0.5,
                )
            )
    return points


def load_hil_points_log(path: Path) -> list[CalibrationPoint]:
    pending_reference: dict[str, float] | None = None
    points: list[CalibrationPoint] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        reference_match = REFERENCE_METADATA_RE.search(line)
        if reference_match:
            pending_reference = {
                name: float(value)
                for name, value in reference_match.groupdict().items()
            }
            continue
        if not line.startswith("[CAL] EXT "):
            continue
        event = parse_protocol_line(line)
        if event.kind != "cal_ext_point" or pending_reference is None:
            continue
        index = len(points)
        direction = "ascending" if index < 21 else "descending"
        direction_index = index if index < 21 else index - 21
        nominal_v = pending_reference["target"]
        points.append(
            CalibrationPoint(
                original_index=index,
                reference_mv=pending_reference["reference"] * 1000.0,
                measured_code=float(event.fields["mean"]),
                crc=str(event.fields["crc"]),
                raw_line=event.raw,
                included=abs(nominal_v) <= 4.5,
                exclusion_reason=(
                    "" if abs(nominal_v) <= 4.5 else "rail_audit_only"
                ),
                nominal_v=nominal_v,
                direction=direction,
                direction_index=direction_index,
                dg_offset_v=pending_reference["dg"],
                reference_v=pending_reference["reference"],
                reference_vpp=pending_reference["vpp"],
                minimum_code=int(event.fields["min"]),
                maximum_code=int(event.fields["max"]),
                flags=int(event.fields["flags"]),
                ds_scale_v_div=pending_reference["scale"],
            )
        )
        pending_reference = None
    if not points:
        raise ValueError("serial log contains no paired REF/[CAL] EXT points")
    return points


def open_review_artifact(path: Path) -> tuple[list[CalibrationPoint], AdcQualityReport]:
    suffix = path.suffix.lower()
    if suffix == ".csv":
        points = load_hil_points_csv(path)
    elif suffix in {".log", ".txt"}:
        points = load_hil_points_log(path)
    elif suffix == ".json":
        payload = json.loads(path.read_text(encoding="utf-8"))
        if isinstance(payload.get("points"), list):
            points = [CalibrationPoint(**item) for item in payload["points"]]
        elif payload.get("csv_path"):
            csv_path = Path(payload["csv_path"])
            if not csv_path.exists():
                raise FileNotFoundError(f"review CSV not found: {csv_path}")
            points = load_hil_points_csv(csv_path)
        else:
            raise ValueError("JSON does not contain points or csv_path")
    else:
        raise ValueError("review artifact must be CSV, JSON, LOG, or TXT")
    return points, evaluate_adc_quality(points)


def replay_device_events(lines: Iterable[str]) -> tuple[list[ProtocolEvent], list[str]]:
    """Extract device lines while preserving, but not parsing, Host metadata."""
    events: list[ProtocolEvent] = []
    metadata: list[str] = []
    for raw in lines:
        line = raw.rstrip("\r\n")
        if not line:
            continue
        if line.startswith("[CAL] ") or line.startswith("[CTRL] "):
            events.append(parse_protocol_line(line))
            continue
        if HOST_METADATA_RE.match(line) or line.startswith("FIT "):
            metadata.append(line)
            continue
        metadata.append(line)
    return events, metadata


def replay_device_log(path: Path) -> tuple[list[ProtocolEvent], list[str]]:
    return replay_device_events(path.read_text(encoding="utf-8").splitlines())


def review_hil_artifacts(
    csv_path: Path,
    serial_log_path: Path,
    *,
    preset: AdcCalibrationPreset | None = None,
) -> tuple[AdcQualityReport, dict]:
    points = load_hil_points_csv(csv_path)
    events, metadata = replay_device_log(serial_log_path)
    report = evaluate_adc_quality(points, preset=preset)
    counts = {
        "cal_ext_point": sum(event.kind == "cal_ext_point" for event in events),
        "cal_export_begin": sum(event.kind == "cal_export_begin" for event in events),
        "cal_export_point": sum(event.kind == "cal_export_point" for event in events),
        "cal_export_end": sum(event.kind == "cal_export_end" for event in events),
        "export_crc_errors": sum(
            event.kind == "cal_export_point_crc_error" for event in events
        ),
        "malformed_protocol": sum(
            event.kind in {
                "malformed_encoding",
                "malformed_export",
                "malformed_cal_status",
            }
            for event in events
        ),
        "host_metadata_lines": len(metadata),
    }
    return report, counts


def save_quality_report_json(path: Path, report: AdcQualityReport, **extra: object) -> None:
    payload = {
        "quality_pass": report.quality_pass,
        "point_count": report.point_count,
        "acquisition_point_count": report.acquisition_point_count,
        "gain_code_per_v": report.fit.gain_code_per_volt,
        "offset_code": report.fit.offset_code,
        "r2": report.fit.r_squared,
        "max_abs_residual_code": report.fit.maximum_absolute_residual_code,
        "max_hysteresis_code": report.maximum_corrected_hysteresis_code,
        "max_raw_pair_code_difference": report.maximum_raw_pair_code_difference,
        "max_pair_reference_delta_v": report.maximum_pair_reference_delta_v,
        "max_frame_pp_code": report.maximum_frame_pp_code,
        "max_reference_vpp_v": report.maximum_reference_vpp_v,
        "rail_safe": report.rail_safe,
        "gates": [asdict(gate) for gate in report.gates],
        "gain_mcpv": report.import_fields[0],
        "offset_mc": report.import_fields[1],
        "r2_ppm": report.import_fields[2],
        "residual_mc": report.import_fields[3],
        "crc32": report.crc32,
        "import_command": report.import_command,
        **extra,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
