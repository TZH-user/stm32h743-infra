"""JSON 计划加载、轻量 schema 校验与执行点展开。"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np

from autobench_core.errors import PlanValidationError, SafetyLimitError
from instruments.dg4062 import normalize_waveform


GLOBAL_AMPLITUDE_VPP_MAX = 5.0
SUPPORTED_CAPTURE_SOURCES = {"simulate", "board_vcp", "scope"}
SUPPORTED_METRICS = {"vpp_ratio_db", "thd", "snr", "frequency_accuracy"}
SUPPORTED_PASS_RULES = {
    "flatness_db_max",
    "thd_percent_max",
    "snr_db_min",
    "frequency_error_ppm_max",
}
ALLOWED_ROOT_KEYS = {
    "version", "name", "dut", "source", "capture", "simulation", "metrics", "pass", "safety"
}
ALLOWED_SOURCE_KEYS = {
    "instrument", "channel", "waveform", "amplitude_vpp", "offset_v", "frequency_hz", "sweep"
}
ALLOWED_SWEEP_KEYS = {"start_hz", "stop_hz", "points", "spacing"}
ALLOWED_CAPTURE_KEYS = {
    "source", "channel", "settle_ms", "sample_rate_hz", "samples", "path"
}
ALLOWED_SIMULATION_KEYS = {
    "gain_db", "cutoff_hz", "noise_snr_db", "frequency_error_ppm", "harmonics_dbc", "seed"
}
ALLOWED_SAFETY_KEYS = {"amplitude_vpp_max"}


@dataclass(frozen=True, slots=True)
class MeasurementPlan:
    path: Path
    raw: dict[str, Any]
    frequencies_hz: tuple[float, ...]

    @property
    def name(self) -> str:
        return str(self.raw["name"])

    @property
    def source(self) -> dict[str, Any]:
        return self.raw["source"]

    @property
    def capture(self) -> dict[str, Any]:
        return self.raw["capture"]

    @property
    def metrics(self) -> tuple[str, ...]:
        return tuple(self.raw["metrics"])

    @property
    def pass_rules(self) -> dict[str, float]:
        return {key: float(value) for key, value in self.raw["pass"].items()}

    @property
    def amplitude_vpp(self) -> float:
        return float(self.source["amplitude_vpp"])

    @property
    def capture_source(self) -> str:
        return str(self.capture["source"])


def _require_dict(parent: dict[str, Any], key: str) -> dict[str, Any]:
    value = parent.get(key)
    if not isinstance(value, dict):
        raise PlanValidationError(f"{key} 必须是 JSON object")
    return value


def _reject_unknown(value: dict[str, Any], allowed: set[str], label: str) -> None:
    unknown = set(value) - allowed
    if unknown:
        raise PlanValidationError(f"{label} 含未知字段: {', '.join(sorted(unknown))}")


def _finite_number(value: Any, label: str, *, positive: bool = False) -> float:
    if isinstance(value, bool):
        raise PlanValidationError(f"{label} 必须是数值")
    try:
        converted = float(value)
    except (TypeError, ValueError) as exc:
        raise PlanValidationError(f"{label} 必须是数值") from exc
    if not math.isfinite(converted):
        raise PlanValidationError(f"{label} 必须是有限数值")
    if positive and converted <= 0:
        raise PlanValidationError(f"{label} 必须大于 0")
    return converted


def _validate_source(source: dict[str, Any]) -> tuple[float, ...]:
    _reject_unknown(source, ALLOWED_SOURCE_KEYS, "source")
    if source.get("instrument") != "dg4062":
        raise PlanValidationError("source.instrument 当前只支持 dg4062")
    if source.get("channel") not in (1, 2):
        raise PlanValidationError("source.channel 只能是 1 或 2")
    try:
        normalize_waveform(str(source.get("waveform", "")))
    except ValueError as exc:
        raise PlanValidationError(str(exc)) from exc

    _finite_number(source.get("amplitude_vpp"), "source.amplitude_vpp", positive=True)
    _finite_number(source.get("offset_v", 0.0), "source.offset_v")

    has_frequency = "frequency_hz" in source
    has_sweep = "sweep" in source
    if has_frequency == has_sweep:
        raise PlanValidationError("source 必须且只能提供 frequency_hz 或 sweep")
    if has_frequency:
        return (_finite_number(source["frequency_hz"], "source.frequency_hz", positive=True),)

    sweep = _require_dict(source, "sweep")
    _reject_unknown(sweep, ALLOWED_SWEEP_KEYS, "source.sweep")
    start_hz = _finite_number(sweep.get("start_hz"), "source.sweep.start_hz", positive=True)
    stop_hz = _finite_number(sweep.get("stop_hz"), "source.sweep.stop_hz", positive=True)
    if stop_hz < start_hz:
        raise PlanValidationError("source.sweep.stop_hz 不能小于 start_hz")
    points = sweep.get("points")
    if not isinstance(points, int) or isinstance(points, bool) or not 1 <= points <= 10000:
        raise PlanValidationError("source.sweep.points 必须是 1..10000 的整数")
    spacing = sweep.get("spacing")
    if spacing not in ("linear", "log"):
        raise PlanValidationError("source.sweep.spacing 只能是 linear 或 log")
    if points == 1:
        return (start_hz,)
    values = (
        np.geomspace(start_hz, stop_hz, points)
        if spacing == "log"
        else np.linspace(start_hz, stop_hz, points)
    )
    return tuple(float(value) for value in values)


def _validate_capture(capture: dict[str, Any], frequency_count: int) -> None:
    _reject_unknown(capture, ALLOWED_CAPTURE_KEYS, "capture")
    source = capture.get("source")
    if source not in SUPPORTED_CAPTURE_SOURCES:
        raise PlanValidationError(
            f"capture.source 只能是 {', '.join(sorted(SUPPORTED_CAPTURE_SOURCES))}"
        )
    if "channel" in capture and capture["channel"] not in (1, 2, 3, 4):
        raise PlanValidationError("capture.channel 只能是 1..4")
    if "settle_ms" in capture:
        value = capture["settle_ms"]
        if not isinstance(value, int) or isinstance(value, bool) or value < 0:
            raise PlanValidationError("capture.settle_ms 必须是非负整数")
    if "sample_rate_hz" in capture:
        _finite_number(capture["sample_rate_hz"], "capture.sample_rate_hz", positive=True)
    if "samples" in capture:
        value = capture["samples"]
        if not isinstance(value, int) or isinstance(value, bool) or not 64 <= value <= 10_000_000:
            raise PlanValidationError("capture.samples 必须是 64..10000000 的整数")
    if source == "board_vcp" and not str(capture.get("path", "")).strip():
        raise PlanValidationError("capture.source=board_vcp 时必须提供 capture.path")
    if source == "board_vcp" and frequency_count > 1:
        # 多频点数据需要 CSV 中的 frequency_hz 列；实际列检查在加载时完成。
        pass


def _validate_simulation(simulation: dict[str, Any]) -> None:
    _reject_unknown(simulation, ALLOWED_SIMULATION_KEYS, "simulation")
    for key in ("gain_db", "noise_snr_db", "frequency_error_ppm"):
        if key in simulation:
            _finite_number(simulation[key], f"simulation.{key}")
    if "cutoff_hz" in simulation:
        _finite_number(simulation["cutoff_hz"], "simulation.cutoff_hz", positive=True)
    if "seed" in simulation:
        value = simulation["seed"]
        if not isinstance(value, int) or isinstance(value, bool):
            raise PlanValidationError("simulation.seed 必须是整数")
    harmonics = simulation.get("harmonics_dbc", {})
    if not isinstance(harmonics, dict):
        raise PlanValidationError("simulation.harmonics_dbc 必须是 object")
    for key, value in harmonics.items():
        try:
            order = int(key)
        except ValueError as exc:
            raise PlanValidationError("simulation.harmonics_dbc 的键必须是谐波次数") from exc
        if order < 2 or order > 20:
            raise PlanValidationError("模拟谐波次数必须在 2..20")
        _finite_number(value, f"simulation.harmonics_dbc.{key}")


def _validate_safety(raw: dict[str, Any], source: dict[str, Any]) -> None:
    safety = raw.get("safety", {})
    if not isinstance(safety, dict):
        raise PlanValidationError("safety 必须是 JSON object")
    _reject_unknown(safety, ALLOWED_SAFETY_KEYS, "safety")
    plan_limit = _finite_number(
        safety.get("amplitude_vpp_max", GLOBAL_AMPLITUDE_VPP_MAX),
        "safety.amplitude_vpp_max",
        positive=True,
    )
    amplitude = float(source["amplitude_vpp"])
    if plan_limit > GLOBAL_AMPLITUDE_VPP_MAX:
        raise SafetyLimitError(
            f"计划安全上限 {plan_limit:g} Vpp 超过 AutoBench 硬上限 "
            f"{GLOBAL_AMPLITUDE_VPP_MAX:g} Vpp"
        )
    if amplitude > plan_limit:
        raise SafetyLimitError(
            f"激励幅度 {amplitude:g} Vpp 超过计划上限 {plan_limit:g} Vpp，拒绝执行"
        )


def validate_plan(raw: dict[str, Any], path: Path) -> MeasurementPlan:
    if not isinstance(raw, dict):
        raise PlanValidationError("计划根节点必须是 JSON object")
    _reject_unknown(raw, ALLOWED_ROOT_KEYS, "计划根节点")
    if raw.get("version", 1) != 1:
        raise PlanValidationError("当前只支持 plan version 1")
    for key in ("name", "dut"):
        if not isinstance(raw.get(key), str) or not raw[key].strip():
            raise PlanValidationError(f"{key} 必须是非空字符串")

    source = _require_dict(raw, "source")
    capture = _require_dict(raw, "capture")
    frequencies = _validate_source(source)
    _validate_capture(capture, len(frequencies))

    metrics = raw.get("metrics")
    if not isinstance(metrics, list) or not metrics:
        raise PlanValidationError("metrics 必须是非空数组")
    if len(set(metrics)) != len(metrics):
        raise PlanValidationError("metrics 不允许重复")
    unsupported = set(metrics) - SUPPORTED_METRICS
    if unsupported:
        raise PlanValidationError(f"不支持的 metrics: {', '.join(sorted(unsupported))}")

    pass_rules = _require_dict(raw, "pass")
    unsupported_rules = set(pass_rules) - SUPPORTED_PASS_RULES
    if unsupported_rules:
        raise PlanValidationError(f"不支持的 pass 规则: {', '.join(sorted(unsupported_rules))}")
    for key, value in pass_rules.items():
        _finite_number(value, f"pass.{key}", positive=True)

    simulation = raw.get("simulation", {})
    if not isinstance(simulation, dict):
        raise PlanValidationError("simulation 必须是 JSON object")
    _validate_simulation(simulation)
    _validate_safety(raw, source)
    return MeasurementPlan(path=path, raw=raw, frequencies_hz=frequencies)


def load_plan(path: str | Path) -> MeasurementPlan:
    resolved = Path(path).expanduser().resolve()
    try:
        with resolved.open("r", encoding="utf-8-sig") as handle:
            raw = json.load(handle)
    except FileNotFoundError as exc:
        raise PlanValidationError(f"计划文件不存在: {resolved}") from exc
    except json.JSONDecodeError as exc:
        raise PlanValidationError(
            f"JSON 解析失败: {resolved}:{exc.lineno}:{exc.colno} {exc.msg}"
        ) from exc
    return validate_plan(raw, resolved)


def plan_summary(plan: MeasurementPlan, capture_override: str | None = None) -> list[str]:
    capture_source = capture_override or plan.capture_source
    source = plan.source
    safety_limit = plan.raw.get("safety", {}).get(
        "amplitude_vpp_max", GLOBAL_AMPLITUDE_VPP_MAX
    )
    lines = [
        f"计划: {plan.name}",
        f"DUT: {plan.raw['dut']}",
        f"执行模式: {capture_source}（离线阶段）",
        f"激励语义: DG4062 CH{source['channel']} {source['waveform']} "
        f"{plan.amplitude_vpp:g} Vpp, offset={float(source.get('offset_v', 0.0)):g} V",
        f"硬安全上限: {GLOBAL_AMPLITUDE_VPP_MAX:g} Vpp；计划上限: {float(safety_limit):g} Vpp",
        f"频点数: {len(plan.frequencies_hz)}",
        f"指标: {', '.join(plan.metrics)}",
        "硬件边界: 不建立 socket、不访问串口、不发送 SCPI、不改变输出或采集状态",
    ]
    for index, frequency_hz in enumerate(plan.frequencies_hz, start=1):
        lines.append(
            f"  {index:03d}. {frequency_hz:.9g} Hz -> "
            f"生成/读取离线波形 -> PC 计算 {', '.join(plan.metrics)}"
        )
    return lines
