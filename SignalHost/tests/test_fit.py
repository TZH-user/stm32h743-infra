"""Linear model validation and fitting tests (UTF-8)."""

from __future__ import annotations

from dataclasses import replace
import math
import unittest

from calibration_host.fit import (
    CalibrationPoint,
    FitError,
    FitPolicy,
    FitResult,
    fit_linear,
    fit_to_import_fields,
)


def points(values: list[tuple[float, float]]) -> list[CalibrationPoint]:
    return [CalibrationPoint(index, reference, measured) for index, (reference, measured) in enumerate(values)]


class FitTests(unittest.TestCase):
    def test_positive_gain_and_offset(self) -> None:
        result = fit_linear(points([(-1000, -700), (0, 100), (1000, 900)]))
        self.assertAlmostEqual(result.gain_code_per_volt, 800.0)
        self.assertAlmostEqual(result.offset_code, 100.0)
        self.assertAlmostEqual(result.r_squared, 1.0)
        self.assertAlmostEqual(result.code_to_voltage(900), 1.0)

    def test_negative_gain(self) -> None:
        result = fit_linear(points([(-1000, 900), (0, 100), (1000, -700)]))
        self.assertAlmostEqual(result.gain_code_per_volt, -800.0)

    def test_noise_and_residual_statistics(self) -> None:
        result = fit_linear(points([(-1000, -701), (0, 102), (1000, 899)]))
        self.assertGreater(result.r_squared, 0.99)
        self.assertGreater(result.maximum_absolute_residual_code, 0.0)
        self.assertEqual(len(result.residuals), 3)

    def test_outlier_is_not_automatically_deleted(self) -> None:
        source = points([(-1000, -700), (0, 100), (500, 2500), (1000, 900)])
        result = fit_linear(source)
        self.assertIn(2, result.used_original_indices)
        source[2] = source[2].excluded("manual outlier review")
        revised = fit_linear(source, version=2)
        self.assertNotIn(2, revised.used_original_indices)
        self.assertIn(2, revised.excluded_original_indices)

    def test_original_order_is_preserved_when_sorting_fit_copy(self) -> None:
        source = points([(1000, 900), (-1000, -700), (0, 100)])
        result = fit_linear(source, sort_copy=True)
        self.assertEqual([item.original_index for item in source], [0, 1, 2])
        self.assertEqual(result.used_original_indices, (1, 2, 0))

    def test_configured_pass_and_not_configured(self) -> None:
        source = points([(-1000, -700), (0, 100), (1000, 900)])
        self.assertEqual(fit_linear(source).pass_state, "NOT_CONFIGURED")
        policy = FitPolicy(minimum_points=3, minimum_r_squared=0.99, maximum_absolute_residual_code=1)
        self.assertEqual(fit_linear(source, policy=policy).pass_state, "PASS")

    def test_invalid_point_sets(self) -> None:
        cases = (
            points([(0, 1), (0, 2)]),
            points([(0, 1), (math.nan, 2)]),
            points([(0, 1), (1000, math.inf)]),
            points([(0, 4), (1000, 4)]),
        )
        for source in cases:
            with self.subTest(source=source), self.assertRaises(FitError):
                fit_linear(source)

    def test_import_fields_use_ppm_and_millicode_residual(self) -> None:
        result = FitResult(
            version=1,
            gain_code_per_volt=-414.002,
            offset_code=2056.898,
            r_squared=0.999981,
            maximum_absolute_residual_code=7.439,
            residuals=(),
            used_original_indices=tuple(range(38)),
            excluded_original_indices=(),
            pass_state="PASS",
        )
        self.assertEqual(
            fit_to_import_fields(result),
            (-414002, 2056898, 999981, 7439, 38),
        )
        conservative = replace(
            result, maximum_absolute_residual_code=2.206229315303972
        )
        self.assertEqual(fit_to_import_fields(conservative)[3], 2207)


if __name__ == "__main__":
    unittest.main()
