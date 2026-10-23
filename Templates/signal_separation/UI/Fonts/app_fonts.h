#ifndef APP_FONTS_H
#define APP_FONTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/*
 * 中文字库：Noto Sans SC Medium，16px，4bpp 抗锯齿，存内部 Flash。
 *
 * 覆盖范围：
 *   - ASCII 0x20-0x7E（与汉字同字号混排用；纯数字读数 label 可继续用 montserrat）
 *   - GB2312 一级汉字 3755 字（常用字全集）
 *   - 全角标点：、。，：；！？“”‘’（）《》【】—…·～￥％ 等
 *   - 工程符号：°±×÷µ ΔΣΦΩ αβγδθλμπστφω ℃℉ ≤≥≠≈∞√‰ ←↑→↓
 *   - 缺字兜底：fallback 已指向 lv_font_montserrat_14
 *
 * 用法：lv_obj_set_style_text_font(label, &font_cn_16, 0);
 * 重新生成 / 补字：UI/Fonts/tools/gen_font_cn.ps1（详见脚本头注释）
 */
LV_FONT_DECLARE(font_cn_16)

#ifdef __cplusplus
}
#endif

#endif /* APP_FONTS_H */
