"""CSV/JSON round-trip and replay tests (UTF-8)."""

from __future__ import annotations

import json
from pathlib import Path
import unittest

from calibration_host.fit import CalibrationPoint
from calibration_host.persistence import load_points_csv, load_points_json, save_points_csv, save_session_json
from calibration_host.protocol import export_point_crc32
from calibration_host.session import CalibrationConfig, CalibrationSession
from calibration_host.transport import ReplayTransport, SerialFake


ROOT = Path(__file__).resolve().parents[1]
ARTIFACTS = ROOT / "build" / "calibration_test_artifacts"
REPLAYS = ROOT / "replays"


class PersistenceReplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        ARTIFACTS.mkdir(parents=True, exist_ok=True)

    def test_csv_json_preserve_units_order_crc_and_exclusions(self) -> None:
        config = CalibrationConfig(-5000, 5000, 3, firmware_id="TEST")
        session = CalibrationSession(SerialFake(), config=config)
        session.points = [
            CalibrationPoint(2, 1000, 900, "t2", "AABBCCDD", "raw2"),
            CalibrationPoint(0, -1000, -700, "t0", "11223344", "raw0").excluded("review"),
        ]
        csv_path = ARTIFACTS / "roundtrip_points_UTF-8.csv"
        json_path = ARTIFACTS / "roundtrip_session_UTF-8.json"
        save_points_csv(csv_path, session.points)
        save_session_json(json_path, session)
        self.assertEqual(load_points_csv(csv_path), session.points)
        loaded = load_points_json(json_path)
        self.assertEqual(loaded, session.points)
        payload = json.loads(json_path.read_text(encoding="utf-8"))
        self.assertEqual(payload["units"]["reference_mv"], "mV")
        self.assertEqual(payload["config"]["firmware_id"], "TEST")

    def test_three_replay_sets_are_repeatable(self) -> None:
        expected_kinds = {
            "normal_flow_UTF-8.log": ["control_ok", "control_cal", "cal_ext_point", "cal_status"],
            "confirm_apply_failure_UTF-8.log": ["control_error", "control_cal", "cal_status"],
            "disconnect_unknown_UTF-8.log": ["control_ok"],
        }
        for filename, required in expected_kinds.items():
            with self.subTest(filename=filename):
                runs: list[list[str]] = []
                for _ in range(2):
                    transport = ReplayTransport.from_file(str(REPLAYS / filename))
                    session = CalibrationSession(transport)
                    session.connect()
                    kinds: list[str] = []
                    while transport.is_open and transport.records:
                        kinds.extend(event.kind for event in session.poll())
                    runs.append(kinds)
                self.assertEqual(runs[0], runs[1])
                for kind in required:
                    self.assertIn(kind, runs[0])

    def test_legacy_export_replay_requires_historical_mode(self) -> None:
        crc = export_point_crc32(0x3F800000, 0x40000000, 1000, 0, 0)
        records = [
            (
                "RX",
                "[CAL] EXPORT BEGIN seq=9 schema=1 mode=1 source=2 item=1 "
                "expected=1 points=1 crc=11111111",
            ),
            (
                "RX",
                "[CAL] EXPORT POINT seq=9 index=0 ref_f32=3F800000 "
                f"measured_f32=40000000 freq_hz=1000 phase_f32=00000000 "
                f"flags=00000000 crc={crc:08X}",
            ),
            ("RX", "[CAL] EXPORT END seq=9 points=1 crc=22222222"),
        ]
        live = CalibrationSession(ReplayTransport(list(records)))
        live.connect()
        live.poll()
        self.assertIsNone(live.export_result)

        historical = CalibrationSession(
            ReplayTransport(list(records)), historical_protocol=True
        )
        historical.connect()
        historical.poll()
        self.assertIsNotNone(historical.export_result)
        self.assertTrue(historical.export_result.legacy)
        self.assertEqual(historical.export_result.points[0].reference, 1.0)
        self.assertEqual(historical.export_result.points[0].measured, 2.0)


if __name__ == "__main__":
    unittest.main()
