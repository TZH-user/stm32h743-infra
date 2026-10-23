"""DG4062 命令规划器（纯字符串生成，不包含 socket/串口访问）。

协议知识来源（只读提炼）：
F:\\MCP\\LabInstrumentsMCP\\source\\lab_instruments\\dg4062.py

本模块只用于计划审查和离线单元测试。AutoBench 离线执行器不会发送这些命令，
也不会调用 :OUTP<n> ON/OFF。真实仪器控制统一列为待上电验证。
"""

from __future__ import annotations

import math


SUPPORTED_WAVEFORMS = {"SIN", "SQU", "RAMP", "PULS", "NOIS", "ARB", "USER"}


def _channel(channel: int) -> int:
    if channel not in (1, 2):
        raise ValueError("DG4062 通道只能是 1 或 2")
    return channel


def _finite_positive(label: str, value: float) -> float:
    converted = float(value)
    if not math.isfinite(converted) or converted <= 0:
        raise ValueError(f"{label} 必须是有限正数")
    return converted


def normalize_waveform(waveform: str) -> str:
    normalized = waveform.strip().upper()
    aliases = {"SINE": "SIN", "SQUARE": "SQU", "PULSE": "PULS", "NOISE": "NOIS"}
    normalized = aliases.get(normalized, normalized)
    if normalized not in SUPPORTED_WAVEFORMS:
        raise ValueError(f"不支持的 DG4062 波形: {waveform}")
    return normalized


def plan_waveform_commands(
    channel: int,
    waveform: str,
    frequency_hz: float,
    amplitude_vpp: float,
    offset_v: float = 0.0,
) -> tuple[str, ...]:
    """按已验证语义生成命令文本，但绝不发送。"""

    channel = _channel(channel)
    normalized = normalize_waveform(waveform)
    amplitude_vpp = _finite_positive("amplitude_vpp", amplitude_vpp)
    offset_v = float(offset_v)
    if not math.isfinite(offset_v):
        raise ValueError("offset_v 必须是有限数值")
    if normalized == "NOIS":
        return (
            f":SOUR{channel}:FUNC NOIS",
            f":SOUR{channel}:VOLT {amplitude_vpp:.12g}",
            f":SOUR{channel}:VOLT:OFFS {offset_v:.12g}",
        )
    frequency_hz = _finite_positive("frequency_hz", frequency_hz)
    return (
        f":SOUR{channel}:APPL:{normalized} "
        f"{frequency_hz:.12g},{amplitude_vpp:.12g},{offset_v:.12g}",
    )


def plan_output_command(channel: int, enabled: bool) -> str:
    """返回已验证的输出命令文本；离线执行器禁止调用。"""

    channel = _channel(channel)
    return f":OUTP{channel} {'ON' if enabled else 'OFF'}"

