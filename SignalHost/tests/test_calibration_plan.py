"""Formal ADC calibration plan tests (UTF-8)."""

from __future__ import annotations

import unittest

from calibration_host.calibration_plan import (
    AdcCalibrationPreset,
    build_adc_42_point_plan,
)


class CalibrationPlanTests(unittest.TestCase):
    def test_formal_plan_has_exact_42_38_order_and_endpoints(self) -> None:
        points = build_adc_42_point_plan()
        self.assertEqual(len(points), 42)
        self.assertEqual(sum(point.included_in_fit for point in points), 38)
        self.assertEqual(points[0].nominal_v, -4.6)
        self.assertEqual(points[20].nominal_v, 4.6)
        self.assertEqual(points[21].nominal_v, 4.6)
        self.assertEqual(points[-1].nominal_v, -4.6)
        self.assertEqual(
            [point.nominal_v for point in points[:21]],
            sorted(point.nominal_v for point in points[:21]),
        )
        self.assertEqual(
            [point.nominal_v for point in points[21:]],
            sorted((point.nominal_v for point in points[21:]), reverse=True),
        )

    def test_real_hardware_rejects_demo_or_unverified_config(self) -> None:
        with self.assertRaises(ValueError):
            AdcCalibrationPreset(
                source="FAKE_DEMO_CONFIG", verification="UNVERIFIED"
            ).validate(real_hardware=True)


if __name__ == "__main__":
    unittest.main()
