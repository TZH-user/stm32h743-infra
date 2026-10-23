"""Structured instrument adapter safety tests (UTF-8)."""

from __future__ import annotations

import unittest

from calibration_host.instrument_adapter import (
    InstrumentCliError,
    StructuredInstrumentAdapter,
)


class FakeRunner:
    def __init__(self) -> None:
        self.calls: list[tuple[str, tuple[str, ...]]] = []
        self.fail_ds = False
        self.nonzero_error = False

    def __call__(self, action: str, arguments: tuple[str, ...]) -> dict:
        self.calls.append((action, arguments))
        if self.fail_ds and action.startswith("ds4014e"):
            raise OSError("WinError 10054")
        if action.endswith("-status"):
            return {
                "device": action.split("-")[0],
                "system_error": (
                    '1,"bad"' if self.nonzero_error else '0,"No error"'
                ),
            }
        if action.endswith("-prepare"):
            if action == "dg4062-prepare":
                current_state = {
                    "channel": 1,
                    "function": "SIN",
                    "frequency_hz": 1_000.0,
                    "amplitude_unit": "VPP",
                    "amplitude": 1.0,
                    "offset_v": 0.25,
                    "phase_deg": 30.0,
                }
            else:
                current_state = {
                    "channel": 1,
                    "scale_v_per_div": 0.2,
                    "offset_v": 0.0,
                }
            return {
                "confirmation_required": f"CONFIRM_{len(self.calls)}",
                "current_state": current_state,
                "proposed_state": {"changed": True},
                "changes_real_output": True,
                "system_error": '0,"No error"',
            }
        return {
            "applied": True,
            "readback": {"ok": True},
            "system_error": '0,"No error"',
        }


class InstrumentAdapterTests(unittest.TestCase):
    def test_status_prepare_exact_confirm_apply_readback(self) -> None:
        runner = FakeRunner()
        adapter = StructuredInstrumentAdapter(runner=runner)
        with self.assertRaises(InstrumentCliError):
            adapter.prepare_dg4062_offset(1.0)
        adapter.status("dg4062")
        prepared = adapter.prepare_dg4062_offset(1.0)
        with self.assertRaises(PermissionError):
            adapter.apply(prepared, "STALE_CONFIRMATION")
        applied, readback = adapter.apply(
            prepared, prepared.confirmation_required
        )
        self.assertTrue(applied["applied"])
        self.assertEqual(readback["device"], "dg4062")
        self.assertTrue(adapter.can_restore("dg4062", 1))
        self.assertEqual(
            [action for action, _ in runner.calls],
            [
                "dg4062-status",
                "dg4062-prepare",
                "dg4062-apply",
                "dg4062-status",
            ],
        )

    def test_restore_replays_first_write_state_and_clears_tracking(self) -> None:
        runner = FakeRunner()
        adapter = StructuredInstrumentAdapter(runner=runner)
        adapter.status("dg4062")
        original = adapter.prepare_dg4062_offset(4.0)
        adapter.apply(original, original.confirmation_required)
        restore = adapter.prepare_restore(original)
        self.assertIn("--frequency-hz", restore.arguments)
        self.assertIn("1000", restore.arguments)
        self.assertIn("--offset-v", restore.arguments)
        self.assertIn("0.25", restore.arguments)
        adapter.apply_restore(restore, restore.confirmation_required)
        self.assertFalse(adapter.can_restore("dg4062", 1))
        self.assertEqual(
            [action for action, _ in runner.calls],
            [
                "dg4062-status",
                "dg4062-prepare",
                "dg4062-apply",
                "dg4062-status",
                "dg4062-status",
                "dg4062-prepare",
                "dg4062-apply",
                "dg4062-status",
            ],
        )

    def test_other_channels_and_devices_are_blocked(self) -> None:
        adapter = StructuredInstrumentAdapter(runner=FakeRunner())
        with self.assertRaises(PermissionError):
            adapter.status("e8267d")

    def test_ds_first_10054_freezes_without_retry(self) -> None:
        runner = FakeRunner()
        runner.fail_ds = True
        adapter = StructuredInstrumentAdapter(runner=runner)
        with self.assertRaises(InstrumentCliError):
            adapter.status("ds4014e")
        self.assertTrue(adapter.ds4014e_frozen)
        with self.assertRaises(InstrumentCliError):
            adapter.status("ds4014e")
        self.assertEqual(len(runner.calls), 1)

    def test_nonzero_error_queue_stops(self) -> None:
        runner = FakeRunner()
        runner.nonzero_error = True
        adapter = StructuredInstrumentAdapter(runner=runner)
        with self.assertRaises(InstrumentCliError):
            adapter.status("dg4062")


if __name__ == "__main__":
    unittest.main()
