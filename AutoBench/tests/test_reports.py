from __future__ import annotations

import unittest
from pathlib import Path
from unittest.mock import patch

from autobench_core.plan import load_plan
from autobench_core.runner import execute_offline


ROOT = Path(__file__).resolve().parents[1]


class ReportTests(unittest.TestCase):
    @patch("socket.create_connection", side_effect=AssertionError("禁止 socket"))
    def test_simulation_creates_all_reports(self, _mock_socket) -> None:
        plan = load_plan(ROOT / "plans" / "thd_1k.json")
        result = execute_offline(
            plan,
            simulate=True,
            output_dir=ROOT / "reports" / "test_thd_1k",
            progress=False,
        )
        self.assertTrue(result.passed)
        self.assertEqual(set(result.report_paths), {"csv", "markdown", "json", "png"})
        for path in result.report_paths.values():
            self.assertTrue(path.is_file(), path)

    @patch("socket.create_connection", side_effect=AssertionError("禁止 socket"))
    def test_board_vcp_branch_creates_reports(self, _mock_socket) -> None:
        plan = load_plan(ROOT / "plans" / "board_vcp_demo.json")
        result = execute_offline(
            plan,
            output_dir=ROOT / "reports" / "test_board_vcp",
            progress=False,
        )
        self.assertTrue(result.passed)
        self.assertEqual(result.payload["capture_source"], "board_vcp")


if __name__ == "__main__":
    unittest.main()

