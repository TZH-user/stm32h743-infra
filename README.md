# STM32H743 Infra — Templates · AutoBench · SignalHost

<p align="center">
  <em>Development infrastructure for STM32H743: versioned firmware templates with a clone-derive toolchain, an automated instrument test bench, and a PC-side algorithm simulation bench.</em>
</p>

<p align="center">
  <img src="docs/img/lvgl-pro-editor.png" alt="LVGL Pro Editor" width="360"/>
  <img src="docs/img/scope-waveform.png" alt="scope waveform" width="360"/>
</p>

## Overview

This repository is the **engineering backbone** of my STM32H743 work — the infrastructure that makes every board project reproducible instead of a one-off:

- **`Templates/`** — a versioned set of H743 firmware templates (CubeMX + HAL + FreeRTOS/CMSIS-RTOS2 + LVGL 9.5), each self-contained and verified by a smoke-test script. New projects are **derived by cloning**, never developed in place.
- **`AutoBench/`** — a Python automated measurement bench that executes JSON test plans against real instruments (Rigol DG4062 / DS4014E) with a strict zero-instrument `--dry-run` mode and a hard 5 Vpp amplitude safety cap.
- **`SignalHost/`** — a PC algorithm simulation bench: synthetic stimulus → external algorithm source (referenced, not copied) → CSV/JSON results → auto-generated plots and acceptance checks.
- **`root/`** — the original workspace entry docs (human/AI conventions, measurement records).

> 🇨🇳 中文版说明见 [README_CN.md](README_CN.md)

## Highlights

- 🧬 **Clone-to-derive toolchain** — `projclone.py` (dry-run first, cache exclusion, three-point rename, checksum validation) + `template_selftest.py` (key files + configure + build + CN/EN residue check) + `translate_ui.py` (161-entry translation map refresh between CN/EN templates)
- 📦 **Template matrix** — `H743_LVGL_Baseline` (minimal bring-up), `H743_Instrument_CN` / `_EN` (full six-screen signal-separation instrument UI, bilingual), `STM32H743_ZYNQ_DualSPI_Link_Template`, plus a deliberately kept `_deprecated_` template as a lessons-learned archive
- 🔬 **AutoBench safety-first design** — JSON v1 plans, dual-layer amplitude limits (plan cap + 5 Vpp hard ceiling), `--dry-run` guarantees zero socket/serial/SCPI, PC-side metrics for Vpp ratio / flatness / THD / SNR / frequency accuracy, reports as CSV + Markdown + JSON + PNG
- 🖥️ **SignalHost clean-room simulation** — algorithm sources stay external via `SIGNALHOST_CORE_DIR`; ships a minimal `arm_math.h` compatibility layer implementing only the RFFT/SinCos APIs actually used; auto acceptance with archived verification records
- 📜 **Written conventions** — upstream-sync discipline (signal_separation → templates), template↔module-library backflow rules, CN/EN paired maintenance

## Tech Stack

<div>
  <img src="https://img.shields.io/badge/STM32-H743XIH6-0072CE"/>
  <img src="https://img.shields.io/badge/HAL-STM32H7-034EA2"/>
  <img src="https://img.shields.io/badge/FreeRTOS-CMSIS_RTOS2-9C27B0"/>
  <img src="https://img.shields.io/badge/LVGL-9.5-47B955"/>
  <img src="https://img.shields.io/badge/Arm%20GNU-15.2-263238"/>
  <img src="https://img.shields.io/badge/CMake%20%2B%20Ninja-build-064F8C"/>
  <img src="https://img.shields.io/badge/Python-3.12-3776AB"/>
  <img src="https://img.shields.io/badge/Rigol-DG4062%20%2F%20DS4014E-C8102E"/>
</div>

## Repository Layout

```
Templates/                        # template zone (clone-source only, never develop here)
├── H743_LVGL_Baseline/           # minimal bring-up: CubeMX + FreeRTOS + LVGL 9.5 + display stack
├── H743_Instrument_CN/ _EN/      # full instrument: six-screen UI, calibration, serial remote
├── LTDC(touch+fmc+LVGL)…Rtos/    # full-featured display/VCP/SWIO/RTOS reference
├── STM32H743_ZYNQ_DualSPI_Link_Template/
├── signal_separation/            # active upstream project (template snapshots derive from it)
├── _deprecated_H743_UI_Instrument_20260726/   # kept as lessons-learned archive
├── _tools/                       # projclone.py / template_selftest.py / translate_ui.py
├── 模板索引.md                    # template selection table, versions, sync discipline
└── 工程克隆工具ProjClone_策划案.md
AutoBench/                        # instrument automation bench (Python)
├── autobench.py autobench_core/  # plan loading, expansion, safety limits
├── instruments/                  # DG4062 / DS4014E adapters
├── plans/ schema/ examples/ tests/
└── reports/                      # acceptance outputs (CSV/MD/JSON/PNG)
SignalHost/                       # PC algorithm simulation bench (C + CMake + Python tools)
├── src/ compat/                  # host main + CMSIS API minimal compat layer
├── calibration_host/ cases/ replays/ tools/
root/                             # workspace entry docs + measurement records
├── README.md / CLAUDE.md         # engineering conventions (CN)
└── mso4104b_*.{csv,png,json}     # oscilloscope capture records
```

## Getting Started

```bash
# Derive a new project from a template (dry-run first!)
python Templates/_tools/projclone.py --src Templates/H743_Instrument_CN \
    --dst ../MyExp --new-name MyExp --profile template-derive
cd ../MyExp && cmake --preset Debug && cmake --build build/Debug -j8

# AutoBench, zero instruments:
python -B AutoBench/autobench.py --plan AutoBench/plans/bode_100_1M.json --dry-run
# AutoBench, simulated 20-point bode:
python -B AutoBench/autobench.py --plan AutoBench/plans/bode_100_1M.json --simulate \
    --output-dir reports/acceptance_bode
```

## Documentation

- Workspace conventions (human entry): [root/README.md](root/README.md) · AI rules: [root/CLAUDE.md](root/CLAUDE.md)
- Template index & sync discipline: [Templates/模板索引.md](Templates/模板索引.md)
- AutoBench manual: [AutoBench/README_使用说明_UTF-8.md](AutoBench/README_使用说明_UTF-8.md)
- SignalHost manual: [SignalHost/README_使用说明_UTF-8.md](SignalHost/README_使用说明_UTF-8.md)

## License

Released under the **MIT License** © 2026 TZH-user — see [LICENSE](LICENSE). STM32 HAL/CMSIS/FreeRTOS/LVGL remain under their respective upstream licenses.
