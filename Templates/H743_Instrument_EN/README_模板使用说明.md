# H743_Instrument_EN —— 全功能仪器模板(英文界面)

## 版本与来源

- 由 H743_Instrument_CN 二次克隆 + UI 字符串翻译而来(2026-07-26/27);CN 克隆自 signal_separation @ 2026-07-26
- 修订记录:
  - 2026-07-27 UI 翻译收尾(补 &#10; 实体与 roller options 漏网串,共 161 条映射)
  - 2026-07-27 test 页排版短化:DC/AC full-scale→DC/AC Full、Output→Out、Points→Pts(槽宽适配,详见 translation_map.md 附录)
- 翻译映射存档:`translation_map.md`(中英对照 + 修订记录);要改译文,改映射后可重刷

## 与 CN 版的差异(仅此三点)

1. **UI 屏显字符串为英文**:范围=Designer XML+生成代码、instrument 屏、适配层;**代码注释与串口日志仍为中文**(设计决策:界面语言≠开发文档语言)。
2. **字体沿用 font_cn_16**(内置 ASCII,英文显示正常)。纯英文项目想省 ~几百 KB flash:lv_conf.h 把 `LV_FONT_DEFAULT` 改回 `&lv_font_montserrat_14` 并开启该字体宏,同时从 CMake 剔除 UI/Fonts/font_cn_16.c。
3. **英文文本按原版布局槽宽做了缩写适配**(仪器面板惯例:INT/EXT/FS/Pts)。新增英文文案时注意:XML 为绝对坐标布局,标签宽度按 ~8px/字符 估算,别超相邻控件间距。

## 改界面语言/文案

- 单条改文案:直接在 LVGL Editor 里改 XML → Generate(gen 代码同步);
- 批量重刷翻译(例如上游中文版更新后):参考 translation_map.md 的映射,用脚本对引号内文本做长串优先替换(本模板即由此流程生成)。
- 若要把 Designer 库名 signal_separation_ui 改掉:在 LVGL Editor 中改项目名重新 Generate,再同步 UI\pages\app_lvgl_ui.c 与 UI\Custom\app_instrument_screen.c 的 include 与两处调用;不要手工改 gen 文件。

## 其余一切(结构/开工流程/裁剪指引/UI 迭代规则/历史文档说明)

与 H743_Instrument_CN 完全一致,见该模板 README_模板使用说明.md——两模板同源同结构,维护时**成对操作**:CN 改了功能,EN 重新克隆+重刷翻译,或手工同步后跑 template_selftest.py 验证。
