# -*- coding: utf-8 -*-
"""模板冒烟自检:关键文件 -> cmake configure -> build -> 产物 -> EN 残留中文。
用法:
  python template_selftest.py            # 全量(三模板 configure+build,约 10-20 分钟)
  python template_selftest.py --quick    # 快速(只查关键文件与 EN 残留,秒级)
  python template_selftest.py --only H743_Instrument_EN
改动任何模板后必跑;全部 PASS 才允许 commit。
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

sys.stdout.reconfigure(line_buffering=True)

TPLS = Path(r"F:\STM32\Templates")

COMMON_KEYS = [
    "CMakeLists.txt", "CMakePresets.json", "STM32H743XX_FLASH.ld",
    "startup_stm32h743xx.s", "cmake/stm32cubemx/CMakeLists.txt",
    "cmake/gcc-arm-none-eabi.cmake", "Core/Src/main.c", "Core/Src/freertos.c",
    "Core/Inc/lv_conf.h", "Core/Inc/FreeRTOSConfig.h",
]
PER_TPL_KEYS = {
    "H743_LVGL_Baseline": [
        "H743_LVGL_Baseline.ioc", "BSP/LTDC/app_display.c", "UI/pages/app_lvgl_ui.c",
        "APP/Debug/debug_io.c", "APP/RTOS/app_display_task.c",
    ],
    "H743_Instrument_CN": [
        "H743_Instrument_CN.ioc", "UI/Designer/CMakeLists.txt",
        "UI/Designer/signal_separation_ui_gen.c", "UI/Custom/app_instrument_screen.c",
        "SignalCore/CMakeLists.txt", "CalibrationCore/CMakeLists.txt",
        "Stm32SignalAdapters/CMakeLists.txt", "UI/Fonts/font_cn_16.c",
    ],
    "H743_Instrument_EN": [
        "H743_Instrument_EN.ioc", "UI/Designer/CMakeLists.txt",
        "translation_map.md", "UI/Fonts/font_cn_16.c",
    ],
}
# EN 残留检查范围(引号内不得有中文)
EN_SCAN_GLOBS = ["UI/Designer/**/*.xml", "UI/Designer/**/*.c",
                 "UI/Adapter/*.c", "UI/Custom/*.c", "UI/pages/*.c"]
HAN = re.compile(r"[\u4e00-\u9fff]")
QUOTED = re.compile(r'"([^"\n]*)"')

def check_files(root, name):
    missing = [k for k in COMMON_KEYS + PER_TPL_KEYS.get(name, [])
               if not (root / k).is_file()]
    for m in missing:
        print(f"  [MISS] {m}")
    return not missing

def check_en_residue(root):
    bad = []
    for g in EN_SCAN_GLOBS:
        for f in root.glob(g):
            if not f.is_file():
                continue
            for i, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                for m in QUOTED.finditer(line):
                    if HAN.search(m.group(1)):
                        bad.append(f"{f.relative_to(root)}:{i}: {m.group(0)[:60]}")
    for b in bad[:10]:
        print(f"  [残留] {b}")
    return not bad

def build(root):
    r = subprocess.run(["cmake", "--preset", "Debug"], cwd=root,
                       capture_output=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print("  [configure 失败]", (r.stdout + r.stderr).splitlines()[-3:])
        return False
    print("  configure OK,build 中(数分钟,静默属正常)...")
    r = subprocess.run(["cmake", "--build", "build/Debug", "-j8"], cwd=root,
                       capture_output=True, encoding="utf-8", errors="replace", timeout=1800)
    if r.returncode != 0:
        print("  [build 失败]")
        print("\n".join((r.stdout + r.stderr).splitlines()[-15:]))
        return False
    elf = root / "build" / "Debug" / f"{root.name}.elf"
    print(f"  build OK -> {elf.name}" if elf.is_file() else "  [产物缺失]")
    return elf.is_file()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true", help="跳过 configure/build")
    ap.add_argument("--only", help="只测指定模板名")
    a = ap.parse_args()

    names = [a.only] if a.only else list(PER_TPL_KEYS)
    results = {}
    for name in names:
        root = TPLS / name
        print(f"\n== {name} ==")
        if not root.is_dir():
            print("  [不存在]")
            results[name] = False
            continue
        ok = check_files(root, name)
        if name == "H743_Instrument_EN":
            ok = check_en_residue(root) and ok
        if ok and not a.quick:
            ok = build(root)
        results[name] = ok
        print(f"  => {'PASS' if ok else 'FAIL'}" + ("(quick)" if a.quick else ""))

    print("\n== 汇总 ==")
    for k, v in results.items():
        print(f"[{'PASS' if v else 'FAIL'}] {k}")
    sys.exit(0 if all(results.values()) else 1)

if __name__ == "__main__":
    main()
