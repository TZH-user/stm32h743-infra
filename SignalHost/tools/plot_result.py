#!/usr/bin/env python3
"""SignalHost 三联图绘制工具（UTF-8）。"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def read_input(path: Path) -> tuple[int, np.ndarray]:
    sample_rate = 0
    values: list[int] = []
    with path.open("r", encoding="utf-8") as stream:
        for line in stream:
            text = line.strip()
            if text.startswith("# sample_rate_hz="):
                sample_rate = int(text.split("=", 1)[1])
            elif text and not text.startswith("#"):
                values.append(int(text))
    if sample_rate <= 0 or not values:
        raise ValueError("输入 CSV 缺少有效采样率或样本")
    return sample_rate, np.asarray(values, dtype=np.float64)


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def main() -> int:
    parser = argparse.ArgumentParser(description="绘制 SignalHost 三联图")
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--result-csv", required=True, type=Path)
    parser.add_argument("--result-json", required=True, type=Path)
    parser.add_argument("--truth", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    rate, samples = read_input(args.input)
    rows = read_rows(args.result_csv)
    result = json.loads(args.result_json.read_text(encoding="utf-8"))
    truth = json.loads(args.truth.read_text(encoding="utf-8"))
    frame_samples = int(result["frame_samples"])
    first = samples[:frame_samples] - np.mean(samples[:frame_samples])

    fig, axes = plt.subplots(3, 1, figsize=(12, 10), constrained_layout=True)
    time_ms = np.arange(first.size) * 1000.0 / rate
    axes[0].plot(time_ms, first, linewidth=0.8)
    axes[0].set_title(f"{result['case_name']} - time domain (first frame)")
    axes[0].set_xlabel("Time (ms)")
    axes[0].set_ylabel("Centered ADC code")
    axes[0].grid(True, alpha=0.25)

    window = np.hanning(first.size)
    spectrum = np.abs(np.fft.rfft(first * window)) * 2.0 / np.sum(window)
    frequencies = np.fft.rfftfreq(first.size, 1.0 / rate)
    axes[1].plot(frequencies, spectrum, linewidth=0.8)
    axes[1].set_xlim(0.0, min(55000.0, rate / 2.0))
    axes[1].set_title("Spectrum (first frame)")
    axes[1].set_xlabel("Frequency (Hz)")
    axes[1].set_ylabel("Amplitude (code)")
    axes[1].grid(True, alpha=0.25)

    frame_index = np.asarray([int(row["frame_index"]) for row in rows])
    peak = np.asarray([float(row["peak_frequency_hz"]) for row in rows])
    axes[2].plot(frame_index, peak, "o-", markersize=3, label="SignalCore FFT peak")
    expected_track = truth.get("track_frequency_hz")
    if expected_track:
        axes[2].plot(frame_index, expected_track, "--", label="Expected track")
    else:
        for component in truth.get("components", []):
            axes[2].axhline(float(component["frequency_hz"]), linestyle="--", alpha=0.7)
        detected = result.get("components", [])
        label = ", ".join(
            f"{item['frequency_hz']:.2f} Hz/{item['type']}" for item in detected
        )
        axes[2].text(
            0.01,
            0.96,
            f"Detected: {label}\nVote confidence: {result['vote']['total_confidence']:.4f}",
            transform=axes[2].transAxes,
            va="top",
            bbox={"facecolor": "white", "alpha": 0.8, "edgecolor": "0.7"},
        )
    axes[2].set_title("Algorithm result / peak trajectory")
    axes[2].set_xlabel("Frame")
    axes[2].set_ylabel("Frequency (Hz)")
    axes[2].grid(True, alpha=0.25)
    axes[2].legend(loc="best")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, dpi=150)
    plt.close(fig)
    print(f"plot_result: saved {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

