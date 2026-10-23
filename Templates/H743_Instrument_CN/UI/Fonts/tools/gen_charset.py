# -*- coding: utf-8 -*-
"""生成中文字库字符集文件(供 lv_font_conv --symbols 使用)。

输出内容 = GB2312 一级汉字(3755 字,按区位码算法生成,零遗漏)
         + 中文全角标点
         + 工程/测量常用符号(℃ Ω μ ± 等)
ASCII 0x20-0x7E 不写入本文件,由 lv_font_conv --range 0x20-0x7E 单独覆盖。

用法: python gen_charset.py <输出文件> [额外字符文件]
额外字符文件(可选): 每行任意 UTF-8 文本,出现过的非 ASCII 字符都会并入字库,
用于日后补充字库外的生僻字,重跑 gen_font_cn.ps1 即可。
"""
import sys


def gb2312_level1():
    """GB2312 一级汉字: 区 16~55、位 1~94(55 区仅到 89 位),共 3755 字。"""
    chars = []
    for qu in range(16, 56):
        max_wei = 89 if qu == 55 else 94
        for wei in range(1, max_wei + 1):
            raw = bytes([qu + 0xA0, wei + 0xA0])
            chars.append(raw.decode("gb2312"))
    return chars


# 全角标点(含全角空格 U+3000、全角波浪线 U+FF5E、全角人民币 U+FFE5)
PUNCT = (
    "、。，：；！？"      # 、。,:;!?
    "“”‘’"                        # “”‘’
    "（）《》〈〉"            # ()《》〈〉
    "【】〔〕「」『』"  # 【】〔〕「」『』
    "—…·～　￥％"      # —…·~ (全角空格)¥%
)

# 工程/测量符号: 度、正负、乘除、micro(两个码点)、希腊字母、温度、比较、箭头
SYMBOLS = (
    "°±×÷µ"                  # °±×÷µ(micro sign)
    "αβγδθλμπστφω"  # αβγδθλμπστφω
    "ΔΣΦΩ"                        # ΔΣΦΩ
    "℃℉"                                    # ℃℉
    "≤≥≠≈∞√‰"      # ≤≥≠≈∞√‰
    "←↑→↓"                        # ←↑→↓
)


def main():
    if len(sys.argv) < 2:
        print("usage: gen_charset.py <out_file> [extra_text_file]", file=sys.stderr)
        sys.exit(1)

    pool = list(PUNCT) + list(SYMBOLS) + gb2312_level1()

    if len(sys.argv) >= 3:
        with open(sys.argv[2], "r", encoding="utf-8") as f:
            for ch in f.read():
                if ord(ch) > 0x7E and not ch.isspace() or ch == "　":
                    pool.append(ch)

    seen = set()
    charset = []
    for ch in pool:
        if ch not in seen:
            seen.add(ch)
            charset.append(ch)

    with open(sys.argv[1], "w", encoding="utf-8", newline="") as f:
        f.write("".join(charset))
    print(f"charset: {len(charset)} chars -> {sys.argv[1]}")


if __name__ == "__main__":
    main()
