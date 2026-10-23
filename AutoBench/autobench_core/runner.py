"""AutoBench 离线执行、判定与 CLI。"""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any

from autobench_core.errors import (
    AutoBenchError,
    CaptureDataError,
    OfflineHardwareBlockedError,
    PlanValidationError,
    SafetyLimitError,
)
from autobench_core.metrics import analyze_waveform
from autobench_core.plan import (
    GLOBAL_AMPLITUDE_VPP_MAX,
    MeasurementPlan,
    load_plan,
    plan_summary,
)
from autobench_core.reports import write_all_reports
from autobench_core.signals import load_board_vcp_csv, synthesize_waveform


PROJECT_ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True, slots=True)
class RunResult:
    passed: bool
    payload: dict[str, Any]
    report_paths: dict[str, Path]


def _safe_name(name: str) -> str:
    cleaned = re.sub(r"[^0-9A-Za-z_.-]+", "_", name).strip("_.")
    return cleaned or "autobench"


def _capture_mode(
    plan: MeasurementPlan,
    simulate: bool,
    board_vcp_csv: str | None,
) -> tuple[str, Path | None]:
    if simulate and board_vcp_csv:
        raise PlanValidationError("--simulate 与 --board-vcp-csv 不能同时使用")
    if simulate:
        return "simulate", None
    if board_vcp_csv:
        return "board_vcp", Path(board_vcp_csv).expanduser().resolve()
    if plan.capture_source == "board_vcp":
        path = Path(str(plan.capture["path"]))
        if not path.is_absolute():
            path = plan.path.parent / path
        return "board_vcp", path.resolve()
    return plan.capture_source, None


def _evaluate_checks(
    plan: MeasurementPlan,
    rows: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    checks: list[dict[str, Any]] = []
    rules = plan.pass_rules
    if "flatness_db_max" in rules:
        gains = [float(row["gain_db"]) for row in rows]
        actual = max(gains) - min(gains)
        limit = rules["flatness_db_max"]
        checks.append(
            {
                "name": "flatness_db_max",
                "actual": actual,
                "limit": limit,
                "passed": actual <= limit,
                "description": f"幅频平坦度 {actual:.4f} dB <= {limit:.4f} dB",
            }
        )
    if "thd_percent_max" in rules:
        actual = max(float(row["thd_percent"]) for row in rows)
        limit = rules["thd_percent_max"]
        checks.append(
            {
                "name": "thd_percent_max",
                "actual": actual,
                "limit": limit,
                "passed": actual <= limit,
                "description": f"最大 THD {actual:.5f}% <= {limit:.5f}%",
            }
        )
    if "snr_db_min" in rules:
        actual = min(float(row["snr_db"]) for row in rows)
        limit = rules["snr_db_min"]
        checks.append(
            {
                "name": "snr_db_min",
                "actual": actual,
                "limit": limit,
                "passed": actual >= limit,
                "description": f"最小 SNR {actual:.3f} dB >= {limit:.3f} dB",
            }
        )
    if "frequency_error_ppm_max" in rules:
        actual = max(abs(float(row["frequency_error_ppm"])) for row in rows)
        limit = rules["frequency_error_ppm_max"]
        checks.append(
            {
                "name": "frequency_error_ppm_max",
                "actual": actual,
                "limit": limit,
                "passed": actual <= limit,
                "description": f"最大频率绝对误差 {actual:.3f} ppm <= {limit:.3f} ppm",
            }
        )
    return checks


def execute_offline(
    plan: MeasurementPlan,
    *,
    simulate: bool = False,
    board_vcp_csv: str | None = None,
    output_dir: str | Path | None = None,
    progress: bool = True,
) -> RunResult:
    capture_mode, board_path = _capture_mode(plan, simulate, board_vcp_csv)
    if capture_mode == "scope":
        # 此调用发生在导入实际 DS4014E Toolkit 之前，保证零 socket/SCPI。
        from instruments.ds4014e import block_scope_access

        block_scope_access()
    if capture_mode not in ("simulate", "board_vcp"):
        raise OfflineHardwareBlockedError(f"离线阶段不支持 capture.source={capture_mode}")

    rows: list[dict[str, Any]] = []
    simulation = plan.raw.get("simulation", {})
    for index, frequency_hz in enumerate(plan.frequencies_hz, start=1):
        if progress:
            print(
                f"[{index}/{len(plan.frequencies_hz)}] {frequency_hz:.9g} Hz "
                f"<- {capture_mode}",
                flush=True,
            )
        if capture_mode == "simulate":
            waveform = synthesize_waveform(
                frequency_hz,
                plan.amplitude_vpp,
                plan.capture,
                simulation,
                index - 1,
            )
        else:
            assert board_path is not None
            waveform = load_board_vcp_csv(
                board_path,
                frequency_hz,
                len(plan.frequencies_hz),
            )
        metrics = analyze_waveform(waveform, frequency_hz, plan.amplitude_vpp)
        rows.append(
            {
                "index": index,
                "expected_frequency_hz": frequency_hz,
                "measured_frequency_hz": metrics.measured_frequency_hz,
                "frequency_error_ppm": metrics.frequency_error_ppm,
                "input_vpp": metrics.input_vpp,
                "output_vpp": metrics.output_vpp,
                "gain_db": metrics.gain_db,
                "thd_percent": metrics.thd_percent,
                "snr_db": metrics.snr_db,
                "capture_source": capture_mode,
                "passed": True,
            }
        )

    checks = _evaluate_checks(plan, rows)
    passed = all(check["passed"] for check in checks)
    if not passed:
        for row in rows:
            row["passed"] = False
    generated_at = datetime.now().astimezone().isoformat(timespec="seconds")
    payload: dict[str, Any] = {
        "plan_name": plan.name,
        "plan_path": str(plan.path),
        "dut": plan.raw["dut"],
        "metrics": list(plan.metrics),
        "source_amplitude_vpp": plan.amplitude_vpp,
        "global_amplitude_limit_vpp": GLOBAL_AMPLITUDE_VPP_MAX,
        "capture_source": capture_mode,
        "verification_scope": "pure_offline_software",
        "hardware_access_attempted": False,
        "instrument_output_changed": False,
        "generated_at": generated_at,
        "passed": passed,
        "checks": checks,
        "rows": rows,
    }
    if output_dir is None:
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        output_path = PROJECT_ROOT / "reports" / f"{_safe_name(plan.name)}_{timestamp}"
    else:
        output_path = Path(output_dir).expanduser().resolve()
    report_paths = write_all_reports(output_path, payload)
    return RunResult(passed=passed, payload=payload, report_paths=report_paths)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="AutoBench 离线测量台：dry-run、模拟/board_vcp、PC 指标与报告。"
    )
    parser.add_argument("--plan", required=True, help="JSON 计划绝对或相对路径")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="只解析/校验并打印逐点计划；零仪器、零串口通信",
    )
    parser.add_argument(
        "--simulate",
        action="store_true",
        help="强制使用可重复模拟波形，不访问仪器或串口",
    )
    parser.add_argument(
        "--board-vcp-csv",
        default=None,
        help="用指定 board_vcp CSV 覆盖 capture.source/path",
    )
    parser.add_argument("--output-dir", default=None, help="报告输出目录")
    return parser


def run_cli(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        plan = load_plan(args.plan)
        override = "simulate" if args.simulate else ("board_vcp" if args.board_vcp_csv else None)
        if args.dry_run:
            for line in plan_summary(plan, override):
                print(line)
            return 0
        result = execute_offline(
            plan,
            simulate=args.simulate,
            board_vcp_csv=args.board_vcp_csv,
            output_dir=args.output_dir,
        )
        print(json.dumps({
            "passed": result.passed,
            "hardware_access_attempted": False,
            "instrument_output_changed": False,
            "reports": {key: str(path) for key, path in result.report_paths.items()},
        }, ensure_ascii=False, indent=2))
        return 0 if result.passed else 1
    except SafetyLimitError as exc:
        print(f"[安全拒绝] {exc}", file=sys.stderr)
        return 3
    except PlanValidationError as exc:
        print(f"[计划错误] {exc}", file=sys.stderr)
        return 2
    except OfflineHardwareBlockedError as exc:
        print(f"[离线硬门禁] {exc}", file=sys.stderr)
        return 4
    except (CaptureDataError, AutoBenchError, OSError, ValueError) as exc:
        print(f"[执行失败] {exc}", file=sys.stderr)
        return 5

