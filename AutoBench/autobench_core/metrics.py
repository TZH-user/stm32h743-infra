"""幅度、THD、SNR 与频率精度的 PC 端计算。"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

from autobench_core.errors import CaptureDataError
from autobench_core.signals import Waveform


@dataclass(frozen=True, slots=True)
class WaveformMetrics:
    input_vpp: float
    output_vpp: float
    gain_db: float
    measured_frequency_hz: float
    frequency_error_ppm: float
    thd_percent: float
    snr_db: float


def peak_to_peak(values: np.ndarray) -> float:
    if values.size == 0:
        raise CaptureDataError("空波形无法计算 Vpp")
    return float(np.max(values) - np.min(values))


def _parabolic_peak(log_magnitude: np.ndarray, index: int) -> float:
    if index <= 0 or index >= log_magnitude.size - 1:
        return float(index)
    left, center, right = log_magnitude[index - 1 : index + 2]
    denominator = left - 2.0 * center + right
    if abs(denominator) < 1e-30:
        return float(index)
    return float(index + 0.5 * (left - right) / denominator)


def estimate_frequency_hz(
    time_s: np.ndarray,
    values: np.ndarray,
    expected_frequency_hz: float,
) -> float:
    centered = values - np.mean(values)
    if np.max(np.abs(centered)) <= 1e-15:
        raise CaptureDataError("波形近似常数，无法估计频率")
    count = centered.size
    dt = float(np.median(np.diff(time_s)))
    sample_rate_hz = 1.0 / dt
    window = np.hanning(count)
    spectrum = np.abs(np.fft.rfft(centered * window))
    frequencies = np.fft.rfftfreq(count, dt)
    spectrum[0] = 0.0
    resolution = sample_rate_hz / count
    half_width = max(expected_frequency_hz * 0.35, resolution * 8.0)
    mask = (frequencies >= max(resolution, expected_frequency_hz - half_width)) & (
        frequencies <= expected_frequency_hz + half_width
    )
    candidate_indices = np.flatnonzero(mask)
    if candidate_indices.size == 0:
        raise CaptureDataError("频率搜索窗口没有 FFT bin")
    peak_index = int(candidate_indices[np.argmax(spectrum[candidate_indices])])
    log_magnitude = np.log(np.maximum(spectrum, 1e-300))
    interpolated_bin = _parabolic_peak(log_magnitude, peak_index)
    return interpolated_bin * resolution


def _component_fit(time_s: np.ndarray, values: np.ndarray, frequency_hz: float) -> tuple[np.ndarray, float]:
    omega_t = 2.0 * np.pi * frequency_hz * time_s
    matrix = np.column_stack((np.sin(omega_t), np.cos(omega_t)))
    coefficients, _, _, _ = np.linalg.lstsq(matrix, values, rcond=None)
    fitted = matrix @ coefficients
    peak = float(np.hypot(coefficients[0], coefficients[1]))
    return fitted, peak / math.sqrt(2.0)


def analyze_waveform(
    waveform: Waveform,
    expected_frequency_hz: float,
    source_amplitude_vpp: float,
    max_harmonic: int = 5,
) -> WaveformMetrics:
    waveform.validate()
    output = waveform.output_v - np.mean(waveform.output_v)
    input_vpp = (
        peak_to_peak(waveform.input_v)
        if waveform.input_v is not None
        else float(source_amplitude_vpp)
    )
    output_vpp = peak_to_peak(waveform.output_v)
    if input_vpp <= 0 or output_vpp <= 0:
        raise CaptureDataError("Vpp 必须大于 0")
    gain_db = 20.0 * math.log10(output_vpp / input_vpp)

    measured_frequency_hz = estimate_frequency_hz(
        waveform.time_s, waveform.output_v, expected_frequency_hz
    )
    frequency_error_ppm = (
        (measured_frequency_hz - expected_frequency_hz) / expected_frequency_hz * 1e6
    )

    fundamental_fit, fundamental_rms = _component_fit(
        waveform.time_s, output, measured_frequency_hz
    )
    if fundamental_rms <= 1e-15:
        raise CaptureDataError("基波幅度过小，无法计算 THD/SNR")
    reconstruction = fundamental_fit.copy()
    harmonic_power = 0.0
    nyquist_hz = 0.5 / float(np.median(np.diff(waveform.time_s)))
    for order in range(2, max_harmonic + 1):
        harmonic_frequency_hz = measured_frequency_hz * order
        if harmonic_frequency_hz >= nyquist_hz:
            break
        fitted, harmonic_rms = _component_fit(
            waveform.time_s, output, harmonic_frequency_hz
        )
        reconstruction += fitted
        harmonic_power += harmonic_rms * harmonic_rms
    thd_percent = math.sqrt(harmonic_power) / fundamental_rms * 100.0

    residual = output - reconstruction
    noise_rms = float(np.sqrt(np.mean(residual * residual)))
    snr_db = math.inf if noise_rms <= 1e-15 else 20.0 * math.log10(fundamental_rms / noise_rms)
    return WaveformMetrics(
        input_vpp=input_vpp,
        output_vpp=output_vpp,
        gain_db=gain_db,
        measured_frequency_hz=measured_frequency_hz,
        frequency_error_ppm=frequency_error_ppm,
        thd_percent=thd_percent,
        snr_db=snr_db,
    )

