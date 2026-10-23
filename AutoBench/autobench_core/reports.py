"""CSV、Markdown、JSON 与 PNG 报告输出。"""

from __future__ import annotations

import csv
import json
import os
from pathlib import Path
from typing import Any


REPORT_FIELDS = (
    "index",
    "expected_frequency_hz",
    "measured_frequency_hz",
    "frequency_error_ppm",
    "input_vpp",
    "output_vpp",
    "gain_db",
    "thd_percent",
    "snr_db",
    "capture_source",
    "passed",
)


def _json_safe(value: Any) -> Any:
    if isinstance(value, float) and (value != value or value in (float("inf"), float("-inf"))):
        return None
    if isinstance(value, dict):
        return {key: _json_safe(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_safe(item) for item in value]
    return value


def write_csv(output_dir: Path, rows: list[dict[str, Any]]) -> Path:
    path = output_dir / "data_UTF-8.csv"
    with path.open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.DictWriter(handle, fieldnames=REPORT_FIELDS, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    return path


def write_json(output_dir: Path, payload: dict[str, Any]) -> Path:
    path = output_dir / "结果_UTF-8.json"
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(_json_safe(payload), handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    return path


def write_markdown(output_dir: Path, payload: dict[str, Any]) -> Path:
    path = output_dir / "报告_UTF-8.md"
    verdict = "PASS" if payload["passed"] else "FAIL"
    lines = [
        f"# AutoBench 报告：{payload['plan_name']}",
        "",
        f"> 结果：**{verdict}**  ",
        f"> 验证范围：{payload['verification_scope']}  ",
        f"> 生成时间：{payload['generated_at']}",
        "",
        "## 结论与依据",
        "",
        "- **有依据**：本报告中的数值由本地 numpy 对离线波形或 board_vcp CSV 计算得到。",
        "- **有依据**：本次运行没有建立仪器 socket、没有发送 SCPI、没有访问串口、没有改变仪器输出或采集状态。",
        "- **待上电验证**：DG4062/DS4014E 真实直通幅频、RAW、截图、capture 与状态改变类操作。",
        "",
        "## 判定规则",
        "",
    ]
    if payload["checks"]:
        for check in payload["checks"]:
            marker = "PASS" if check["passed"] else "FAIL"
            lines.append(f"- `{marker}` {check['description']}")
    else:
        lines.append("- 计划未配置判定阈值，仅输出计算结果。")
    lines.extend(
        [
            "",
            "## 逐点结果",
            "",
            "| # | 设定频率/Hz | 测得频率/Hz | 误差/ppm | 增益/dB | THD/% | SNR/dB | 结果 |",
            "|---:|---:|---:|---:|---:|---:|---:|:---:|",
        ]
    )
    for row in payload["rows"]:
        lines.append(
            f"| {row['index']} | {row['expected_frequency_hz']:.9g} | "
            f"{row['measured_frequency_hz']:.9g} | {row['frequency_error_ppm']:.3f} | "
            f"{row['gain_db']:.4f} | {row['thd_percent']:.5f} | "
            f"{row['snr_db']:.3f} | {'PASS' if row['passed'] else 'FAIL'} |"
        )
    lines.extend(
        [
            "",
            "## 安全边界",
            "",
            f"- 激励幅度：`{payload['source_amplitude_vpp']:.6g} Vpp`。",
            f"- AutoBench 硬上限：`{payload['global_amplitude_limit_vpp']:.6g} Vpp`。",
            "- scope 分支在加载 DS4014E Toolkit 前直接拒绝；异常路径中不存在 DG4062 输出开启/关闭调用。",
            "- 仪器状态改变与真实电气结果不得由本报告推断为已验证。",
            "",
        ]
    )
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))
    return path


def write_png(output_dir: Path, payload: dict[str, Any]) -> Path:
    cache_dir = output_dir.parent / ".matplotlib_cache"
    cache_dir.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault("MPLCONFIGDIR", str(cache_dir))
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rows = payload["rows"]
    frequencies = [row["expected_frequency_hz"] for row in rows]
    path = output_dir / "曲线.png"
    metrics = set(payload["metrics"])
    if "vpp_ratio_db" in metrics:
        figure, axis = plt.subplots(figsize=(8.5, 5.0), dpi=140)
        axis.plot(frequencies, [row["gain_db"] for row in rows], "o-", linewidth=1.5)
        if len(frequencies) > 1 and min(frequencies) > 0:
            axis.set_xscale("log")
        axis.set_xlabel("Frequency (Hz)")
        axis.set_ylabel("Vpp ratio (dB)")
        axis.grid(True, which="both", alpha=0.3)
    else:
        figure, axes = plt.subplots(3, 1, figsize=(8.5, 8.0), dpi=140, sharex=True)
        axes[0].plot(frequencies, [row["thd_percent"] for row in rows], "o-")
        axes[0].set_ylabel("THD (%)")
        axes[1].plot(frequencies, [row["snr_db"] for row in rows], "o-")
        axes[1].set_ylabel("SNR (dB)")
        axes[2].plot(frequencies, [row["frequency_error_ppm"] for row in rows], "o-")
        axes[2].set_ylabel("Error (ppm)")
        axes[2].set_xlabel("Frequency (Hz)")
        for axis in axes:
            axis.grid(True, alpha=0.3)
            if len(frequencies) > 1 and min(frequencies) > 0:
                axis.set_xscale("log")
    figure.suptitle(f"AutoBench offline: {payload['plan_name']} ({'PASS' if payload['passed'] else 'FAIL'})")
    figure.tight_layout()
    figure.savefig(path)
    plt.close(figure)
    return path


def write_all_reports(output_dir: Path, payload: dict[str, Any]) -> dict[str, Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    return {
        "csv": write_csv(output_dir, payload["rows"]),
        "markdown": write_markdown(output_dir, payload),
        "json": write_json(output_dir, payload),
        "png": write_png(output_dir, payload),
    }

