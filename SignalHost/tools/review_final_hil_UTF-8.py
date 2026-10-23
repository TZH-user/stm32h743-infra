#!/usr/bin/env python3
"""Replay the bundled final HIL fixture and save deterministic reports (UTF-8)."""

from __future__ import annotations

import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from calibration_host.artifacts import review_hil_artifacts, save_quality_report_json


def main() -> int:
    csv_path = ROOT / "replays" / "final_hil_points_203309_UTF-8.csv"
    log_path = ROOT / "replays" / "final_hil_serial_203309_UTF-8.log"
    report, counts = review_hil_artifacts(csv_path, log_path)
    output = ROOT / "build" / "final_hil_quality_report_20260728_UTF-8.json"
    replay_output = ROOT / "build" / "final_hil_replay_report_20260728_UTF-8.json"
    save_quality_report_json(
        output,
        report,
        csv_path=str(csv_path),
        serial_log_path=str(log_path),
    )
    replay_output.write_text(
        json.dumps(
            {
                **counts,
                "quality_pass": report.quality_pass,
                "crc32": report.crc32,
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(output)
    print(replay_output)
    return 0 if report.quality_pass and counts["malformed_protocol"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
