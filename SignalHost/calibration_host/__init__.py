"""SignalHost AD/DA calibration workbench (UTF-8)."""

from .crc import calibration_import_crc32
from .calibration_plan import AdcCalibrationPreset, build_adc_42_point_plan
from .fit import (
    AdcQualityReport,
    CalibrationPoint,
    FitResult,
    evaluate_adc_quality,
    fit_linear,
)
from .protocol import IncrementalLineParser, ProtocolEvent

__all__ = [
    "CalibrationPoint",
    "AdcCalibrationPreset",
    "AdcQualityReport",
    "FitResult",
    "IncrementalLineParser",
    "ProtocolEvent",
    "calibration_import_crc32",
    "fit_linear",
    "evaluate_adc_quality",
    "build_adc_42_point_plan",
]
