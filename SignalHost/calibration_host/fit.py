"""Calibration point validation and dependency-free linear fitting (UTF-8)."""

from __future__ import annotations

from dataclasses import dataclass, replace
import math
from typing import Iterable

from .calibration_plan import AdcCalibrationPreset
from .crc import calibration_import_crc32, format_crc32


class FitError(ValueError):
    """Raised when a point set cannot produce a trustworthy linear model."""


@dataclass(frozen=True, slots=True)
class CalibrationPoint:
    original_index: int
    reference_mv: float
    measured_code: float
    timestamp: str = ""
    crc: str = ""
    raw_line: str = ""
    included: bool = True
    exclusion_reason: str = ""
    nominal_v: float | None = None
    direction: str = ""
    direction_index: int | None = None
    dg_offset_v: float | None = None
    reference_v: float | None = None
    reference_vpp: float | None = None
    minimum_code: int | None = None
    maximum_code: int | None = None
    flags: int = 0
    ds_scale_v_div: float | None = None
    closed_loop_iterations: int | None = None

    @property
    def voltage(self) -> float:
        return (
            self.reference_v
            if self.reference_v is not None
            else self.reference_mv / 1000.0
        )

    def excluded(self, reason: str) -> "CalibrationPoint":
        reason = reason.strip()
        if not reason:
            raise ValueError("exclusion reason is required")
        return replace(self, included=False, exclusion_reason=reason)


@dataclass(frozen=True, slots=True)
class Residual:
    original_index: int
    reference_mv: float
    measured_code: float
    predicted_code: float
    residual_code: float


@dataclass(frozen=True, slots=True)
class FitResult:
    version: int
    gain_code_per_volt: float
    offset_code: float
    r_squared: float
    maximum_absolute_residual_code: float
    residuals: tuple[Residual, ...]
    used_original_indices: tuple[int, ...]
    excluded_original_indices: tuple[int, ...]
    pass_state: str

    def voltage_to_code(self, voltage: float) -> float:
        if not math.isfinite(voltage):
            raise ValueError("voltage must be finite")
        return self.gain_code_per_volt * voltage + self.offset_code

    def code_to_voltage(self, code: float, minimum_gain: float = 1e-12) -> float:
        if not math.isfinite(code):
            raise ValueError("code must be finite")
        if abs(self.gain_code_per_volt) <= minimum_gain:
            raise ZeroDivisionError("gain is too small for inverse conversion")
        return (code - self.offset_code) / self.gain_code_per_volt


@dataclass(frozen=True, slots=True)
class FitPolicy:
    minimum_points: int | None = None
    minimum_reference_span_mv: float | None = None
    minimum_absolute_gain: float = 1e-12
    minimum_r_squared: float | None = None
    maximum_absolute_residual_code: float | None = None


def _sequential_sum(values: Iterable[float]) -> float:
    """Match the original PowerShell HIL fitter's ordered IEEE-754 additions."""
    total = 0.0
    for value in values:
        total += value
    return total


@dataclass(frozen=True, slots=True)
class QualityGate:
    name: str
    actual: float | int | bool
    limit: str
    passed: bool


@dataclass(frozen=True, slots=True)
class AdcQualityReport:
    fit: FitResult
    point_count: int
    acquisition_point_count: int
    maximum_corrected_hysteresis_code: float
    maximum_raw_pair_code_difference: float
    maximum_pair_reference_delta_v: float
    maximum_frame_pp_code: float
    maximum_reference_vpp_v: float
    rail_safe: bool
    gates: tuple[QualityGate, ...]
    quality_pass: bool
    import_fields: tuple[int, int, int, int, int]
    crc32: str
    import_command: str


def _validate_points(points: list[CalibrationPoint], policy: FitPolicy) -> None:
    if len(points) < 2:
        raise FitError("at least two included points are required")
    seen: set[float] = set()
    for point in points:
        if not math.isfinite(point.reference_mv) or not math.isfinite(point.measured_code):
            raise FitError("NaN/Inf is not allowed")
        if point.reference_mv in seen:
            raise FitError("duplicate reference value")
        seen.add(point.reference_mv)
    span_mv = max(seen) - min(seen)
    if span_mv <= 0.0:
        raise FitError("degenerate reference span")
    if policy.minimum_reference_span_mv is not None:
        if span_mv < policy.minimum_reference_span_mv:
            raise FitError("reference span is below configured minimum")


def fit_linear(
    source_points: Iterable[CalibrationPoint],
    *,
    policy: FitPolicy | None = None,
    version: int = 1,
    sort_copy: bool = False,
) -> FitResult:
    """Fit code = gain * voltage + offset without mutating source order."""
    effective_policy = policy or FitPolicy()
    original = list(source_points)
    points = [point for point in original if point.included]
    _validate_points(points, effective_policy)
    if sort_copy:
        points = sorted(points, key=lambda point: point.reference_mv)

    x_values = [point.voltage for point in points]
    y_values = [point.measured_code for point in points]
    x_mean = _sequential_sum(x_values) / len(x_values)
    y_mean = _sequential_sum(y_values) / len(y_values)
    denominator = _sequential_sum((value - x_mean) ** 2 for value in x_values)
    if denominator <= 0.0 or not math.isfinite(denominator):
        raise FitError("degenerate fit denominator")
    numerator = _sequential_sum(
        (x_value - x_mean) * (y_value - y_mean)
        for x_value, y_value in zip(x_values, y_values, strict=True)
    )
    gain = numerator / denominator
    offset = y_mean - gain * x_mean
    if not math.isfinite(gain) or not math.isfinite(offset):
        raise FitError("non-finite fit result")
    if abs(gain) <= effective_policy.minimum_absolute_gain:
        raise FitError("absolute gain is too small")

    residuals = tuple(
        Residual(
            original_index=point.original_index,
            reference_mv=point.reference_mv,
            measured_code=point.measured_code,
            predicted_code=gain * point.voltage + offset,
            residual_code=point.measured_code - (gain * point.voltage + offset),
        )
        for point in points
    )
    ss_res = _sequential_sum(item.residual_code**2 for item in residuals)
    ss_tot = _sequential_sum((value - y_mean) ** 2 for value in y_values)
    r_squared = 1.0 if ss_tot == 0.0 and ss_res == 0.0 else (
        0.0 if ss_tot == 0.0 else 1.0 - ss_res / ss_tot
    )
    maximum_residual = max(abs(item.residual_code) for item in residuals)

    pass_state = "NOT_CONFIGURED"
    configured = effective_policy.minimum_points is not None
    if configured:
        passed = len(points) >= effective_policy.minimum_points
        if effective_policy.minimum_r_squared is not None:
            passed = passed and r_squared >= effective_policy.minimum_r_squared
        if effective_policy.maximum_absolute_residual_code is not None:
            passed = passed and maximum_residual <= effective_policy.maximum_absolute_residual_code
        pass_state = "PASS" if passed else "FAIL"

    return FitResult(
        version=version,
        gain_code_per_volt=gain,
        offset_code=offset,
        r_squared=r_squared,
        maximum_absolute_residual_code=maximum_residual,
        residuals=residuals,
        used_original_indices=tuple(point.original_index for point in points),
        excluded_original_indices=tuple(
            point.original_index for point in original if not point.included
        ),
        pass_state=pass_state,
    )


def round_half_away_from_zero(value: float) -> int:
    if not math.isfinite(value):
        raise ValueError("value must be finite")
    return math.floor(value + 0.5) if value >= 0.0 else math.ceil(value - 0.5)


def fit_to_import_fields(result: FitResult) -> tuple[int, int, int, int, int]:
    return (
        round_half_away_from_zero(result.gain_code_per_volt * 1000.0),
        round_half_away_from_zero(result.offset_code * 1000.0),
        round_half_away_from_zero(result.r_squared * 1_000_000.0),
        math.ceil(result.maximum_absolute_residual_code * 1000.0),
        len(result.used_original_indices),
    )


def evaluate_adc_quality(
    source_points: Iterable[CalibrationPoint],
    *,
    preset: AdcCalibrationPreset | None = None,
    version: int = 1,
) -> AdcQualityReport:
    """Evaluate the formal 42/38-point ADC quality contract."""
    effective = preset or AdcCalibrationPreset()
    effective.validate()
    original = list(source_points)
    fit_points = [
        point
        for point in original
        if point.included
        and point.nominal_v is not None
        and abs(point.nominal_v) <= effective.fit_endpoint_v
    ]
    fit = fit_linear(
        fit_points,
        policy=FitPolicy(
            minimum_points=38,
            minimum_reference_span_mv=effective.fit_endpoint_v * 2000.0,
            minimum_r_squared=effective.minimum_r_squared,
            maximum_absolute_residual_code=effective.maximum_absolute_residual_code,
        ),
        version=version,
    )

    by_nominal: dict[float, dict[str, CalibrationPoint]] = {}
    for point in fit_points:
        by_nominal.setdefault(float(point.nominal_v), {})[point.direction] = point
    corrected: list[float] = []
    raw_differences: list[float] = []
    reference_differences: list[float] = []
    for nominal_v, pair in by_nominal.items():
        if set(pair) != {"ascending", "descending"}:
            raise FitError(f"missing direction pair for nominal_v={nominal_v:g}")
        ascending = pair["ascending"]
        descending = pair["descending"]
        reference_delta = descending.voltage - ascending.voltage
        code_delta = descending.measured_code - ascending.measured_code
        reference_differences.append(abs(reference_delta))
        raw_differences.append(abs(code_delta))
        corrected.append(
            abs(code_delta - fit.gain_code_per_volt * reference_delta)
        )

    if any(point.minimum_code is None or point.maximum_code is None for point in original):
        raise FitError("rail and frame pp require min/max for every acquisition point")
    maximum_frame_pp = max(
        float(point.maximum_code - point.minimum_code) for point in original
    )
    rail_safe = all(
        point.minimum_code > effective.minimum_code
        and point.maximum_code < effective.maximum_code
        for point in original
    )
    maximum_reference_vpp = max(
        float(point.reference_vpp or 0.0) for point in original
    )
    ds_scales = {
        point.ds_scale_v_div
        for point in original
        if point.ds_scale_v_div is not None
    }
    ds_scale_consistent = ds_scales == {effective.ds_sweep_scale_v_div}
    maximum_corrected = max(corrected)
    maximum_raw = max(raw_differences)
    maximum_reference_delta = max(reference_differences)
    flags_clear = all(point.flags == 0 for point in original)
    gates = (
        QualityGate("acquisition_points", len(original), "== 42", len(original) == 42),
        QualityGate("fit_points", len(fit_points), "== 38", len(fit_points) == 38),
        QualityGate(
            "r_squared",
            fit.r_squared,
            f">= {effective.minimum_r_squared}",
            fit.r_squared >= effective.minimum_r_squared,
        ),
        QualityGate(
            "maximum_absolute_residual_code",
            fit.maximum_absolute_residual_code,
            f"<= {effective.maximum_absolute_residual_code}",
            fit.maximum_absolute_residual_code
            <= effective.maximum_absolute_residual_code,
        ),
        QualityGate(
            "maximum_corrected_hysteresis_code",
            maximum_corrected,
            f"<= {effective.maximum_corrected_hysteresis_code}",
            maximum_corrected <= effective.maximum_corrected_hysteresis_code,
        ),
        QualityGate(
            "maximum_pair_reference_delta_v",
            maximum_reference_delta,
            f"<= {effective.maximum_pair_reference_delta_v}",
            maximum_reference_delta <= effective.maximum_pair_reference_delta_v,
        ),
        QualityGate(
            "maximum_frame_pp_code",
            maximum_frame_pp,
            f"<= {effective.maximum_frame_pp_code}",
            maximum_frame_pp <= effective.maximum_frame_pp_code,
        ),
        QualityGate(
            "maximum_reference_vpp_v",
            maximum_reference_vpp,
            f"<= {effective.maximum_reference_vpp_v}",
            maximum_reference_vpp <= effective.maximum_reference_vpp_v,
        ),
        QualityGate("rail_safe", rail_safe, "is True", rail_safe),
        QualityGate("flags_clear", flags_clear, "is True", flags_clear),
        QualityGate(
            "ds_sweep_scale_v_div",
            len(ds_scales),
            f"all == {effective.ds_sweep_scale_v_div}",
            ds_scale_consistent,
        ),
    )
    fields = fit_to_import_fields(fit)
    crc = format_crc32(calibration_import_crc32(*fields))
    return AdcQualityReport(
        fit=fit,
        point_count=len(fit_points),
        acquisition_point_count=len(original),
        maximum_corrected_hysteresis_code=maximum_corrected,
        maximum_raw_pair_code_difference=maximum_raw,
        maximum_pair_reference_delta_v=maximum_reference_delta,
        maximum_frame_pp_code=maximum_frame_pp,
        maximum_reference_vpp_v=maximum_reference_vpp,
        rail_safe=rail_safe,
        gates=gates,
        quality_pass=all(gate.passed for gate in gates),
        import_fields=fields,
        crc32=crc,
        import_command=(
            f"CAL IMPORT ADC {' '.join(str(value) for value in fields)} {crc}"
        ),
    )
