#!/usr/bin/env python3
"""汇总三档 SNR 的 SignalCore 投票退化证据（UTF-8）。"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main() -> int:
    parser = argparse.ArgumentParser(description="汇总 SignalHost SNR 退化结果")
    parser.add_argument("--results", nargs=3, required=True, type=Path)
    parser.add_argument("--snr", nargs=3, required=True, type=float)
    parser.add_argument("--output-csv", required=True, type=Path)
    parser.add_argument("--output-json", required=True, type=Path)
    parser.add_argument("--output-plot", required=True, type=Path)
    args = parser.parse_args()

    rows = []
    for snr, path in zip(args.snr, args.results, strict=True):
        result = json.loads(path.read_text(encoding="utf-8"))
        vote_confidence = float(result["vote"]["total_confidence"])
        degradation_confidence = vote_confidence if result["vote"]["valid"] else 0.0
        rows.append(
            {
                "requested_snr_db": snr,
                "observed_selected_snr_db": float(result["peak_track"]["mean_selected_snr_db"]),
                "vote_valid": bool(result["vote"]["valid"]),
                "valid_frames": int(result["vote"]["valid_frames"]),
                "invalid_frames": int(result["vote"]["invalid_frames"]),
                "vote_confidence": vote_confidence,
                "degradation_confidence": degradation_confidence,
            }
        )

    rows.sort(key=lambda row: row["requested_snr_db"], reverse=True)
    observed_degrades = all(
        rows[index]["observed_selected_snr_db"] > rows[index + 1]["observed_selected_snr_db"]
        for index in range(len(rows) - 1)
    )
    confidence_degrades = all(
        rows[index]["degradation_confidence"] + 0.02 >= rows[index + 1]["degradation_confidence"]
        for index in range(len(rows) - 1)
    ) and rows[0]["degradation_confidence"] > rows[-1]["degradation_confidence"]
    passed = observed_degrades and confidence_degrades

    args.output_csv.parent.mkdir(parents=True, exist_ok=True)
    with args.output_csv.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    summary = {
        "passed": passed,
        "observed_snr_strictly_decreases": observed_degrades,
        "confidence_degrades": confidence_degrades,
        "rows": rows,
    }
    args.output_json.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    figure, axis = plt.subplots(figsize=(8, 5), constrained_layout=True)
    x = [row["requested_snr_db"] for row in rows]
    y = [row["degradation_confidence"] for row in rows]
    axis.plot(x, y, "o-", label="Vote degradation confidence")
    axis.set_xlabel("Requested SNR (dB)")
    axis.set_ylabel("Confidence")
    axis.set_title("SignalCore confidence degradation")
    axis.grid(True, alpha=0.25)
    axis.invert_xaxis()
    axis.legend()
    figure.savefig(args.output_plot, dpi=150)
    plt.close(figure)

    print(f"summarize_snr: {'PASS' if passed else 'FAIL'} {json.dumps(summary, ensure_ascii=False)}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())

