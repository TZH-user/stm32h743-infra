# -*- coding: utf-8 -*-
"""校验 UI 会显示的字符(C 字符串字面量 / XML 属性值)是否都在字库字符集内。

用法: python check_charset_coverage.py [charset 文件]
charset 文件缺省时自动重新生成到临时目录(等价 gen_charset.py 输出)。
发现缺字时退出码 1,并列出字符与所在文件——把缺字放进一个 UTF-8 文本文件,
执行 gen_font_cn.ps1 -ExtraChars <该文件> 重新生成字库即可补齐。

注意:注释中的字符不参与校验(不上屏);仅扫描下列会进显示路径的位置:
  - .c 文件的 "..." 字符串字面量
  - .xml 文件的属性值
"""
import glob
import os
import re
import subprocess
import sys
import tempfile

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(TOOLS, "..", "..", ".."))

C_STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')
XML_ATTR = re.compile(r'="([^"]*)"')


def charset_from(path):
    with open(path, encoding="utf-8") as f:
        chars = set(f.read())
    chars |= {chr(c) for c in range(0x20, 0x7F)}
    return chars


def main():
    if len(sys.argv) >= 2:
        charset_file = sys.argv[1]
    else:
        charset_file = os.path.join(tempfile.gettempdir(), "charset_cn_check.txt")
        subprocess.run(
            [sys.executable, os.path.join(TOOLS, "gen_charset.py"), charset_file],
            check=True, capture_output=True,
        )
    charset = charset_from(charset_file)

    targets = (
        glob.glob(os.path.join(ROOT, "UI", "Designer", "screens", "*.xml"))
        + glob.glob(os.path.join(ROOT, "UI", "Designer", "screens", "*_gen.c"))
        + glob.glob(os.path.join(ROOT, "UI", "Designer", "components", "*.xml"))
        + glob.glob(os.path.join(ROOT, "UI", "Designer", "components", "*_gen.c"))
        + [
            os.path.join(ROOT, "UI", "Designer", "globals.xml"),
            os.path.join(ROOT, "UI", "Designer", "signal_separation_ui_gen.c"),
            os.path.join(ROOT, "UI", "Adapter", "app_ui_adapter.c"),
            os.path.join(ROOT, "UI", "Adapter", "app_calibration_ui_adapter.c"),
            os.path.join(ROOT, "UI", "Adapter", "app_signal_result_view.c"),
            os.path.join(ROOT, "UI", "Custom", "app_instrument_screen.c"),
        ]
    )

    missing = {}
    for path in targets:
        with open(path, encoding="utf-8") as f:
            text = f.read()
        pattern = XML_ATTR if path.endswith(".xml") else C_STRING
        for m in pattern.finditer(text):
            for ch in m.group(1):
                if ord(ch) <= 0x7E:
                    continue
                if ch not in charset:
                    missing.setdefault(ch, set()).add(os.path.basename(path))

    if not missing:
        print("OK: 显示路径中的全部字符均在字库覆盖范围内")
        return 0
    print("以下显示字符不在字库中(用 gen_font_cn.ps1 -ExtraChars 补齐):")
    for ch, files in sorted(missing.items()):
        print(f"  U+{ord(ch):04X} {ch}  <- {', '.join(sorted(files))}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
