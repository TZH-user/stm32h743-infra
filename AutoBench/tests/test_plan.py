from __future__ import annotations

import copy
import unittest
from pathlib import Path

from autobench_core.errors import PlanValidationError, SafetyLimitError
from autobench_core.plan import load_plan, validate_plan


ROOT = Path(__file__).resolve().parents[1]


class PlanTests(unittest.TestCase):
    def test_bode_plan_expands_twenty_points(self) -> None:
        plan = load_plan(ROOT / "plans" / "bode_100_1M.json")
        self.assertEqual(len(plan.frequencies_hz), 20)
        self.assertAlmostEqual(plan.frequencies_hz[0], 100.0)
        self.assertAlmostEqual(plan.frequencies_hz[-1], 1_000_000.0)

    def test_six_vpp_is_rejected_by_hard_limit(self) -> None:
        plan = load_plan(ROOT / "plans" / "thd_1k.json")
        raw = copy.deepcopy(plan.raw)
        raw["source"]["amplitude_vpp"] = 6.0
        with self.assertRaises(SafetyLimitError):
            validate_plan(raw, ROOT / "plans" / "invalid_6vpp.json")

    def test_plan_cannot_raise_hard_limit(self) -> None:
        plan = load_plan(ROOT / "plans" / "thd_1k.json")
        raw = copy.deepcopy(plan.raw)
        raw["safety"]["amplitude_vpp_max"] = 10.0
        with self.assertRaises(SafetyLimitError):
            validate_plan(raw, ROOT / "plans" / "invalid_limit.json")

    def test_unknown_root_field_is_rejected(self) -> None:
        plan = load_plan(ROOT / "plans" / "thd_1k.json")
        raw = copy.deepcopy(plan.raw)
        raw["unexpected"] = True
        with self.assertRaises(PlanValidationError):
            validate_plan(raw, ROOT / "plans" / "invalid_unknown.json")


if __name__ == "__main__":
    unittest.main()
