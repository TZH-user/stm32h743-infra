"""ADC external calibration plan and validated defaults (UTF-8)."""

from __future__ import annotations

from dataclasses import asdict, dataclass
import math


ADC_42_TARGETS_V = (
    -4.6, -4.5, -4.0, -3.5, -3.0, -2.5, -2.0, -1.5, -1.0, -0.5,
    0.0,
    0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 4.6,
)


@dataclass(frozen=True, slots=True)
class CalibrationPlanPoint:
    plan_index: int
    direction: str
    direction_index: int
    nominal_v: float
    included_in_fit: bool


@dataclass(frozen=True, slots=True)
class AdcCalibrationPreset:
    name: str = "ADC 外部 42 点正式预置"
    acquisition_endpoint_v: float = 4.6
    fit_endpoint_v: float = 4.5
    ds_sweep_scale_v_div: float = 0.5
    ds_restore_scale_v_div: float = 0.2
    reference_convergence_v: float = 0.060
    maximum_pair_reference_delta_v: float = 0.060
    maximum_reference_vpp_v: float = 0.100
    minimum_code: int = 15
    maximum_code: int = 4080
    minimum_r_squared: float = 0.999900
    maximum_absolute_residual_code: float = 8.0
    maximum_corrected_hysteresis_code: float = 4.0
    maximum_frame_pp_code: float = 16.0
    source: str = "2026-07-28 最终受控 HIL"
    verification: str = "VERIFIED"

    def validate(self, *, real_hardware: bool = False) -> None:
        numeric = asdict(self)
        for name, value in numeric.items():
            if isinstance(value, (int, float)) and not math.isfinite(float(value)):
                raise ValueError(f"{name} must be finite")
        if self.acquisition_endpoint_v <= self.fit_endpoint_v:
            raise ValueError("acquisition endpoint must exceed fit endpoint")
        if self.fit_endpoint_v <= 0.0:
            raise ValueError("fit endpoint must be positive")
        if self.minimum_code < 0 or self.maximum_code > 4095:
            raise ValueError("rail limits must remain inside 12-bit ADC range")
        if self.minimum_code >= self.maximum_code:
            raise ValueError("minimum_code must be below maximum_code")
        if real_hardware and (
            self.verification != "VERIFIED" or self.source == "FAKE_DEMO_CONFIG"
        ):
            raise ValueError("real hardware requires a verified non-demo preset")


def build_adc_42_point_plan(
    preset: AdcCalibrationPreset | None = None,
) -> tuple[CalibrationPlanPoint, ...]:
    effective = preset or AdcCalibrationPreset()
    effective.validate()
    points: list[CalibrationPlanPoint] = []
    for direction, targets in (
        ("ascending", ADC_42_TARGETS_V),
        ("descending", tuple(reversed(ADC_42_TARGETS_V))),
    ):
        for direction_index, nominal_v in enumerate(targets):
            points.append(
                CalibrationPlanPoint(
                    plan_index=len(points),
                    direction=direction,
                    direction_index=direction_index,
                    nominal_v=nominal_v,
                    included_in_fit=abs(nominal_v) <= effective.fit_endpoint_v,
                )
            )
    return tuple(points)
