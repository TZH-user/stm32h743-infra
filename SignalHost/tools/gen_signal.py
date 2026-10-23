#!/usr/bin/env python3
"""SignalHost 激励生成器（UTF-8）。"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import numpy as np


FRAME_SAMPLES = 4096
ADC_MIN = 0
ADC_MAX = 4095


def load_case(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        config = json.load(stream)
    required = {"name", "kind", "sample_rate_hz", "analysis_mode"}
    missing = sorted(required - set(config))
    if missing:
        raise ValueError(f"case 缺少字段: {', '.join(missing)}")
    return config


def sine_wave(config: dict, sample_count: int) -> tuple[np.ndarray, dict]:
    rate = float(config["sample_rate_hz"])
    index = np.arange(sample_count, dtype=np.float64)
    phase = 2.0 * math.pi * float(config["frequency_hz"]) * index / rate
    signal = float(config["amplitude_code"]) * np.sin(phase)
    truth = {"components": [{"frequency_hz": float(config["frequency_hz"]), "type": "sine"}]}
    return signal, truth


def dual_wave(config: dict, sample_count: int) -> tuple[np.ndarray, dict]:
    rate = float(config["sample_rate_hz"])
    index = np.arange(sample_count, dtype=np.float64)
    signal = np.zeros(sample_count, dtype=np.float64)
    truth_components: list[dict] = []
    for tone in config["tones"]:
        frequency = float(tone["frequency_hz"])
        amplitude = float(tone["amplitude_code"])
        phase_offset = float(tone.get("phase_rad", 0.0))
        signal += amplitude * np.sin(2.0 * math.pi * frequency * index / rate + phase_offset)
        truth_components.append({"frequency_hz": frequency, "type": "sine", "amplitude_code": amplitude})
    truth_components.sort(key=lambda item: item["frequency_hz"])
    return signal, {"components": truth_components}


def sweep_wave(config: dict) -> tuple[np.ndarray, dict]:
    frame_count = int(config["frames"])
    rate = float(config["sample_rate_hz"])
    frequencies = np.linspace(float(config["start_hz"]), float(config["stop_hz"]), frame_count)
    frames: list[np.ndarray] = []
    local_index = np.arange(FRAME_SAMPLES, dtype=np.float64)
    amplitude = float(config["amplitude_code"])
    for frequency in frequencies:
        frames.append(amplitude * np.sin(2.0 * math.pi * frequency * local_index / rate))
    return np.concatenate(frames), {"track_frequency_hz": frequencies.tolist(), "components": []}


def am_wave(config: dict, sample_count: int) -> tuple[np.ndarray, dict]:
    rate = float(config["sample_rate_hz"])
    index = np.arange(sample_count, dtype=np.float64)
    carrier = float(config["carrier_hz"])
    modulation = float(config["modulation_hz"])
    depth = float(config["depth"])
    envelope = 1.0 + depth * np.sin(2.0 * math.pi * modulation * index / rate)
    signal = float(config["amplitude_code"]) * envelope * np.sin(
        2.0 * math.pi * carrier * index / rate
    )
    return signal, {"components": [{"frequency_hz": carrier, "type": "am_carrier"}]}


def fm_wave(config: dict, sample_count: int) -> tuple[np.ndarray, dict]:
    rate = float(config["sample_rate_hz"])
    index = np.arange(sample_count, dtype=np.float64)
    carrier = float(config["carrier_hz"])
    modulation = float(config["modulation_hz"])
    deviation = float(config["deviation_hz"])
    beta = deviation / modulation
    phase = (
        2.0 * math.pi * carrier * index / rate
        + beta * np.sin(2.0 * math.pi * modulation * index / rate)
    )
    signal = float(config["amplitude_code"]) * np.sin(phase)
    return signal, {"components": [{"frequency_hz": carrier, "type": "fm_carrier"}]}


def build_signal(config: dict) -> tuple[np.ndarray, dict]:
    kind = str(config["kind"]).lower()
    frames = int(config.get("frames", 8))
    sample_count = frames * FRAME_SAMPLES
    if kind == "sine":
        return sine_wave(config, sample_count)
    if kind == "dual":
        return dual_wave(config, sample_count)
    if kind == "sweep":
        return sweep_wave(config)
    if kind == "am":
        return am_wave(config, sample_count)
    if kind == "fm":
        return fm_wave(config, sample_count)
    raise ValueError(f"不支持的 kind: {kind}")


def add_noise(signal: np.ndarray, snr_db: float | None, seed: int) -> tuple[np.ndarray, float]:
    if snr_db is None:
        return signal.copy(), 0.0
    signal_rms = float(np.sqrt(np.mean(np.square(signal))))
    noise_rms = signal_rms / (10.0 ** (float(snr_db) / 20.0))
    random = np.random.default_rng(seed)
    noise = random.normal(0.0, noise_rms, signal.shape)
    return signal + noise, noise_rms


def write_csv(path: Path, config: dict, codes: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(f"# sample_rate_hz={int(config['sample_rate_hz'])}\n")
        stream.write("# sample_format=unsigned12\n")
        stream.write(f"# analysis_mode={config['analysis_mode']}\n")
        stream.write(f"# case_name={config['name']}\n")
        for code in codes:
            stream.write(f"{int(code)}\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="生成 SignalHost CSV 激励")
    parser.add_argument("--case", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--truth", required=True, type=Path)
    args = parser.parse_args()

    config = load_case(args.case)
    clean, truth = build_signal(config)
    noisy, noise_rms = add_noise(clean, config.get("snr_db"), int(config.get("seed", 20260727)))
    dc_code = float(config.get("dc_code", 2048.0))
    unclipped = dc_code + noisy
    codes = np.rint(np.clip(unclipped, ADC_MIN, ADC_MAX)).astype(np.uint16)
    clip_count = int(np.count_nonzero((unclipped < ADC_MIN) | (unclipped > ADC_MAX)))

    write_csv(args.output, config, codes)
    truth.update(
        {
            "case_name": config["name"],
            "kind": config["kind"],
            "analysis_mode": config["analysis_mode"],
            "sample_rate_hz": int(config["sample_rate_hz"]),
            "frame_samples": FRAME_SAMPLES,
            "frame_count": int(codes.size // FRAME_SAMPLES),
            "requested_snr_db": config.get("snr_db"),
            "noise_rms_code": noise_rms,
            "clip_count": clip_count,
        }
    )
    args.truth.parent.mkdir(parents=True, exist_ok=True)
    with args.truth.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(truth, stream, ensure_ascii=False, indent=2)
        stream.write("\n")

    print(
        f"gen_signal: case={config['name']} samples={codes.size} "
        f"frames={codes.size // FRAME_SAMPLES} snr={config.get('snr_db')} "
        f"clip_count={clip_count}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
