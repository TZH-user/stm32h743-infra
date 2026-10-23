#!/usr/bin/env python3
"""SignalHost 单用例验收器（UTF-8）。"""

from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(description="验证 SignalHost 用例输出")
    parser.add_argument("--case", required=True, type=Path)
    parser.add_argument("--truth", required=True, type=Path)
    parser.add_argument("--result-csv", required=True, type=Path)
    parser.add_argument("--result-json", required=True, type=Path)
    parser.add_argument("--metrics", required=True, type=Path)
    args = parser.parse_args()

    config = load_json(args.case)
    truth = load_json(args.truth)
    result = load_json(args.result_json)
    metrics: dict[str, object] = {"case_name": config["name"], "passed": True}

    if config["analysis_mode"] == "dual":
        expected = sorted(truth["components"], key=lambda item: item["frequency_hz"])
        actual = sorted(result["components"], key=lambda item: item["frequency_hz"])
        errors = []
        types_ok = True
        for reference, measured in zip(expected, actual, strict=True):
            error_percent = abs(float(measured["frequency_hz"]) - float(reference["frequency_hz"])) \
                            / float(reference["frequency_hz"]) * 100.0
            errors.append(error_percent)
            types_ok = types_ok and measured["type"].lower() == reference["type"].lower()
        passed = bool(result["vote"]["valid"]) and max(errors) < 1.0 and types_ok
        metrics.update(
            {
                "passed": passed,
                "frequency_error_percent": errors,
                "maximum_frequency_error_percent": max(errors),
                "types_ok": types_ok,
                "vote_confidence": float(result["vote"]["total_confidence"]),
                "valid_frames": int(result["vote"]["valid_frames"]),
                "observed_snr_db": float(result["peak_track"]["mean_selected_snr_db"]),
            }
        )
    elif config["analysis_mode"] == "peak":
        with args.result_csv.open("r", encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream))
        actual = [float(row["peak_frequency_hz"]) for row in rows]
        expected = [float(value) for value in truth["track_frequency_hz"]]
        errors = [measured - reference for measured, reference in zip(actual, expected, strict=True)]
        rmse = math.sqrt(sum(error * error for error in errors) / len(errors))
        maximum = max(abs(error) for error in errors)
        resolution = float(config["sample_rate_hz"]) / float(truth["frame_samples"])
        passed = len(actual) == len(expected) and rmse <= 1.5 * resolution
        metrics.update(
            {
                "passed": passed,
                "frame_count": len(actual),
                "frequency_resolution_hz": resolution,
                "track_rmse_hz": rmse,
                "track_maximum_absolute_error_hz": maximum,
            }
        )

    args.metrics.parent.mkdir(parents=True, exist_ok=True)
    args.metrics.write_text(
        json.dumps(metrics, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    status = "PASS" if metrics["passed"] else "FAIL"
    print(f"verify_results: {status} {json.dumps(metrics, ensure_ascii=False)}")
    return 0 if metrics["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

