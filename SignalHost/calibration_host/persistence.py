"""CSV/JSON/log export and deterministic replay helpers (UTF-8)."""

from __future__ import annotations

import csv
from dataclasses import asdict
from datetime import datetime, timezone
import json
from pathlib import Path

from .fit import CalibrationPoint
from .session import CalibrationSession, LogEntry


POINT_FIELDS = (
    "original_index",
    "reference_mv",
    "measured_code",
    "timestamp",
    "crc",
    "raw_line",
    "included",
    "exclusion_reason",
    "nominal_v",
    "direction",
    "direction_index",
    "dg_offset_v",
    "reference_v",
    "reference_vpp",
    "minimum_code",
    "maximum_code",
    "flags",
    "ds_scale_v_div",
    "closed_loop_iterations",
)


def save_session_json(path: Path, session: CalibrationSession) -> None:
    payload = session.snapshot()
    payload["schema"] = "signalhost-calibration-session-v1"
    payload["generated_at"] = datetime.now(timezone.utc).isoformat()
    payload["units"] = {
        "reference_mv": "mV",
        "measured_code": "ADC code",
        "gain": "code/V",
        "offset": "code",
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def load_points_json(path: Path) -> list[CalibrationPoint]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    return [CalibrationPoint(**item) for item in payload["points"]]


def save_points_csv(path: Path, points: list[CalibrationPoint]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=POINT_FIELDS)
        writer.writeheader()
        for point in points:
            writer.writerow(asdict(point))


def load_points_csv(path: Path) -> list[CalibrationPoint]:
    def optional_float(row: dict[str, str], name: str) -> float | None:
        value = row.get(name, "")
        return None if value in {"", None} else float(value)

    def optional_int(row: dict[str, str], name: str) -> int | None:
        value = row.get(name, "")
        return None if value in {"", None} else int(value)

    points: list[CalibrationPoint] = []
    with path.open("r", encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            points.append(
                CalibrationPoint(
                    original_index=int(row["original_index"]),
                    reference_mv=float(row["reference_mv"]),
                    measured_code=float(row["measured_code"]),
                    timestamp=row["timestamp"],
                    crc=row["crc"],
                    raw_line=row["raw_line"],
                    included=row["included"].lower() == "true",
                    exclusion_reason=row["exclusion_reason"],
                    nominal_v=optional_float(row, "nominal_v"),
                    direction=row.get("direction", ""),
                    direction_index=optional_int(row, "direction_index"),
                    dg_offset_v=optional_float(row, "dg_offset_v"),
                    reference_v=optional_float(row, "reference_v"),
                    reference_vpp=optional_float(row, "reference_vpp"),
                    minimum_code=optional_int(row, "minimum_code"),
                    maximum_code=optional_int(row, "maximum_code"),
                    flags=int(row.get("flags") or 0),
                    ds_scale_v_div=optional_float(row, "ds_scale_v_div"),
                    closed_loop_iterations=optional_int(
                        row, "closed_loop_iterations"
                    ),
                )
            )
    return points


def save_replay_log(path: Path, logs: list[LogEntry]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write("# SignalHost calibration replay UTF-8 v1\n")
        for entry in logs:
            if entry.direction == "RX":
                stream.write(f"RX|{entry.text}\n")
            elif entry.direction == "EVENT" and entry.category == "connection" and entry.text.startswith("disconnected"):
                stream.write(f"DISCONNECT|{entry.text}\n")


def error_context(logs: list[LogEntry], radius: int = 3) -> str:
    error_indices = [
        index for index, entry in enumerate(logs)
        if entry.category == "error" or " ERR " in entry.text
    ]
    if not error_indices:
        return ""
    index = error_indices[-1]
    selected = logs[max(0, index - radius) : index + radius + 1]
    return "\n".join(
        f"{entry.timestamp} {entry.direction}/{entry.category} {entry.text}"
        for entry in selected
    )
