"""模拟波形生成与 board_vcp CSV 读取。"""

from __future__ import annotations

import csv
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np

from autobench_core.errors import CaptureDataError


@dataclass(frozen=True, slots=True)
class Waveform:
    time_s: np.ndarray
    output_v: np.ndarray
    input_v: np.ndarray | None = None
    source: str = "unknown"

    def validate(self) -> "Waveform":
        if self.time_s.ndim != 1 or self.output_v.ndim != 1:
            raise CaptureDataError("波形必须是一维数组")
        if self.time_s.size != self.output_v.size or self.time_s.size < 64:
            raise CaptureDataError("波形 time/output 点数必须一致且不少于 64")
        if self.input_v is not None and self.input_v.size != self.time_s.size:
            raise CaptureDataError("input_v 点数必须与 time_s 一致")
        if not np.all(np.isfinite(self.time_s)) or not np.all(np.isfinite(self.output_v)):
            raise CaptureDataError("波形包含 NaN 或 Inf")
        dt = np.diff(self.time_s)
        if np.any(dt <= 0):
            raise CaptureDataError("time_s 必须严格递增")
        median_dt = float(np.median(dt))
        if np.max(np.abs(dt - median_dt)) > max(1e-12, median_dt * 0.02):
            raise CaptureDataError("当前离线算法要求近似等间隔采样（抖动不超过 2%）")
        return self


def _simulation_gain_linear(frequency_hz: float, simulation: dict[str, Any]) -> float:
    gain = 10.0 ** (float(simulation.get("gain_db", 0.0)) / 20.0)
    cutoff_hz = simulation.get("cutoff_hz")
    if cutoff_hz is not None:
        gain /= math.sqrt(1.0 + (frequency_hz / float(cutoff_hz)) ** 2)
    return gain


def synthesize_waveform(
    frequency_hz: float,
    amplitude_vpp: float,
    capture: dict[str, Any],
    simulation: dict[str, Any],
    point_index: int,
) -> Waveform:
    """生成可重复的输入/输出波形，不访问任何外设。"""

    samples = int(capture.get("samples", 8192))
    requested_rate = capture.get("sample_rate_hz")
    sample_rate_hz = (
        float(requested_rate)
        if requested_rate is not None
        else min(frequency_hz * 64.0, 20_000_000.0)
    )
    if sample_rate_hz <= frequency_hz * 2.2:
        raise CaptureDataError(
            f"模拟采样率 {sample_rate_hz:g} Sa/s 对 {frequency_hz:g} Hz 不满足奈奎斯特裕量"
        )
    time_s = np.arange(samples, dtype=float) / sample_rate_hz
    nominal_peak = amplitude_vpp / 2.0
    input_v = nominal_peak * np.sin(2.0 * np.pi * frequency_hz * time_s)

    ppm = float(simulation.get("frequency_error_ppm", 0.0))
    actual_frequency_hz = frequency_hz * (1.0 + ppm * 1e-6)
    output_peak = nominal_peak * _simulation_gain_linear(frequency_hz, simulation)
    output_v = output_peak * np.sin(2.0 * np.pi * actual_frequency_hz * time_s)

    harmonics = simulation.get("harmonics_dbc", {2: -60.0, 3: -70.0})
    for order_text, dbc in harmonics.items():
        order = int(order_text)
        harmonic_peak = output_peak * 10.0 ** (float(dbc) / 20.0)
        output_v += harmonic_peak * np.sin(
            2.0 * np.pi * actual_frequency_hz * order * time_s + order * 0.17
        )

    snr_db = float(simulation.get("noise_snr_db", 70.0))
    signal_rms = abs(output_peak) / math.sqrt(2.0)
    noise_rms = signal_rms / (10.0 ** (snr_db / 20.0))
    seed = int(simulation.get("seed", 20260727)) + point_index
    rng = np.random.default_rng(seed)
    output_v += rng.normal(0.0, noise_rms, samples)
    return Waveform(time_s, output_v, input_v, source="simulate").validate()


def _resolve_column(fieldnames: list[str], candidates: tuple[str, ...]) -> str | None:
    normalized = {name.strip().lower(): name for name in fieldnames}
    for candidate in candidates:
        if candidate in normalized:
            return normalized[candidate]
    return None


def load_board_vcp_csv(
    path: str | Path,
    expected_frequency_hz: float,
    frequency_count: int,
) -> Waveform:
    """读取 board_vcp 长表 CSV。

    必需列：time_s 与 volt_v/output_v；可选 input_v、frequency_hz。
    多频点计划必须提供 frequency_hz 列用于分组。
    """

    resolved = Path(path).expanduser().resolve()
    try:
        handle = resolved.open("r", newline="", encoding="utf-8-sig")
    except FileNotFoundError as exc:
        raise CaptureDataError(f"board_vcp CSV 不存在: {resolved}") from exc
    with handle:
        reader = csv.DictReader(row for row in handle if not row.lstrip().startswith("#"))
        if not reader.fieldnames:
            raise CaptureDataError(f"board_vcp CSV 缺少表头: {resolved}")
        time_col = _resolve_column(reader.fieldnames, ("time_s", "time"))
        output_col = _resolve_column(reader.fieldnames, ("output_v", "volt_v", "voltage_v"))
        input_col = _resolve_column(reader.fieldnames, ("input_v", "source_v"))
        frequency_col = _resolve_column(reader.fieldnames, ("frequency_hz", "freq_hz"))
        if time_col is None or output_col is None:
            raise CaptureDataError("board_vcp CSV 必须含 time_s 和 volt_v/output_v 列")
        if frequency_count > 1 and frequency_col is None:
            raise CaptureDataError("多频点 board_vcp CSV 必须含 frequency_hz 列")

        rows: list[dict[str, str]] = []
        for row in reader:
            if frequency_col is not None:
                try:
                    row_frequency = float(row[frequency_col])
                except (TypeError, ValueError) as exc:
                    raise CaptureDataError("frequency_hz 列包含非数值") from exc
                tolerance = max(1e-9, abs(expected_frequency_hz) * 1e-6)
                if abs(row_frequency - expected_frequency_hz) > tolerance:
                    continue
            rows.append(row)
    if not rows:
        raise CaptureDataError(f"CSV 中没有 {expected_frequency_hz:g} Hz 对应数据")

    try:
        time_s = np.asarray([float(row[time_col]) for row in rows], dtype=float)
        output_v = np.asarray([float(row[output_col]) for row in rows], dtype=float)
        input_v = (
            np.asarray([float(row[input_col]) for row in rows], dtype=float)
            if input_col is not None
            else None
        )
    except (TypeError, ValueError) as exc:
        raise CaptureDataError("board_vcp CSV 波形列包含非数值") from exc
    return Waveform(time_s, output_v, input_v, source="board_vcp").validate()
