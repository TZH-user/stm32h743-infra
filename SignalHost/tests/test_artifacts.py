"""Final HIL artifact and formal quality-gate regression tests (UTF-8)."""

from __future__ import annotations

from dataclasses import replace
from pathlib import Path
import unittest

from calibration_host.artifacts import (
    load_hil_points_csv,
    replay_device_events,
    review_hil_artifacts,
)
from calibration_host.fit import evaluate_adc_quality


ROOT = Path(__file__).resolve().parents[1]
CSV_PATH = ROOT / "replays" / "final_hil_points_203309_UTF-8.csv"
LOG_PATH = ROOT / "replays" / "final_hil_serial_203309_UTF-8.log"


class ArtifactTests(unittest.TestCase):
    def test_final_hil_replay_and_review_exact_values(self) -> None:
        report, counts = review_hil_artifacts(CSV_PATH, LOG_PATH)
        self.assertEqual(counts["cal_ext_point"], 42)
        self.assertEqual(counts["cal_export_begin"], 1)
        self.assertEqual(counts["cal_export_point"], 42)
        self.assertEqual(counts["cal_export_end"], 1)
        self.assertEqual(counts["export_crc_errors"], 0)
        self.assertEqual(counts["malformed_protocol"], 0)
        self.assertGreater(counts["host_metadata_lines"], 0)
        self.assertTrue(report.quality_pass)
        self.assertEqual(report.point_count, 38)
        self.assertEqual(report.fit.gain_code_per_volt, -412.40217767274504)
        self.assertEqual(report.fit.offset_code, 2119.451107815013)
        self.assertEqual(report.fit.r_squared, 0.9999990331848746)
        self.assertEqual(
            report.fit.maximum_absolute_residual_code, 2.206229315303972
        )
        self.assertEqual(
            report.maximum_corrected_hysteresis_code, 2.5113488759407474
        )
        self.assertEqual(
            report.maximum_raw_pair_code_difference, 12.539999999999964
        )
        self.assertEqual(
            report.maximum_pair_reference_delta_v, 0.026763392857142798
        )
        self.assertEqual(report.maximum_frame_pp_code, 11.0)
        self.assertEqual(report.crc32, "3D4E58A9")

    def test_host_fit_metadata_is_not_a_protocol_error(self) -> None:
        events, metadata = replay_device_events(
            [
                '[2026-07-28T20:52:23+08:00] FIT {"payload":"' + ("x" * 741) + '"}',
                "[CAL] STATUS v=3 state=6 error=0 exclusive=1 can_start=1",
            ]
        )
        self.assertEqual([event.kind for event in events], ["cal_status"])
        self.assertEqual(len(metadata), 1)

    def test_rail_fault_and_mixed_ds_scale_fail_closed(self) -> None:
        points = load_hil_points_csv(CSV_PATH)
        rail_fault = list(points)
        rail_fault[0] = replace(
            rail_fault[0], nominal_v=-4.8, minimum_code=10
        )
        rail_fault[-1] = replace(
            rail_fault[-1], nominal_v=-4.8, maximum_code=4090
        )
        rail_report = evaluate_adc_quality(rail_fault)
        self.assertFalse(rail_report.quality_pass)
        self.assertFalse(
            next(gate for gate in rail_report.gates if gate.name == "rail_safe").passed
        )

        mixed_scale = list(points)
        mixed_scale[10] = replace(mixed_scale[10], ds_scale_v_div=0.2)
        scale_report = evaluate_adc_quality(mixed_scale)
        self.assertFalse(scale_report.quality_pass)
        self.assertFalse(
            next(
                gate
                for gate in scale_report.gates
                if gate.name == "ds_sweep_scale_v_div"
            ).passed
        )


if __name__ == "__main__":
    unittest.main()
