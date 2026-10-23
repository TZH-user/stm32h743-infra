from __future__ import annotations

import io
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

from autobench_core.plan import load_plan
from autobench_core.runner import execute_offline, run_cli


ROOT = Path(__file__).resolve().parents[1]


class OfflineSafetyTests(unittest.TestCase):
    @patch("socket.create_connection", side_effect=AssertionError("禁止 socket"))
    def test_dry_run_never_creates_socket(self, _mock_socket) -> None:
        stdout = io.StringIO()
        with redirect_stdout(stdout):
            code = run_cli(
                ["--plan", str(ROOT / "plans" / "bode_100_1M.json"), "--dry-run"]
            )
        self.assertEqual(code, 0)
        self.assertIn("不建立 socket", stdout.getvalue())

    @patch("socket.create_connection", side_effect=AssertionError("禁止 socket"))
    def test_scope_branch_blocks_before_socket(self, _mock_socket) -> None:
        stderr = io.StringIO()
        with redirect_stderr(stderr):
            code = run_cli(["--plan", str(ROOT / "plans" / "bode_100_1M.json")])
        self.assertEqual(code, 4)
        self.assertIn("离线硬门禁", stderr.getvalue())

    @patch("instruments.dg4062.plan_output_command", side_effect=AssertionError("禁止输出控制"))
    @patch("socket.create_connection", side_effect=AssertionError("禁止 socket"))
    def test_simulation_exception_path_has_no_output_control(
        self, _mock_socket, _mock_output
    ) -> None:
        plan = load_plan(ROOT / "plans" / "thd_1k.json")
        result = execute_offline(
            plan,
            simulate=True,
            output_dir=ROOT / "reports" / "test_simulation_safety",
            progress=False,
        )
        self.assertFalse(result.payload["hardware_access_attempted"])
        self.assertFalse(result.payload["instrument_output_changed"])


if __name__ == "__main__":
    unittest.main()

