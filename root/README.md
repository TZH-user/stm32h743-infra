# F:\STM32 —— STM32H743 模板体系与模块库(人工接管入口)

2026-07-26/27 由 AI 协作建成。本文件是**人类工程师的第一站**;`CLAUDE.md` 是 AI 会话规约,内容对人同样有效,两份都读约需十分钟。

## 这里有什么

```
F:\STM32\
├── README.md / CLAUDE.md / .gitignore     ← 入口、规约、git 白名单(只跟踪下列资产)
├── Templates\                              ← 模板区(只作克隆源,严禁直接开发)
│   ├── 模板索引.md                          ← 三模板怎么选、版本溯源、上游同步纪律
│   ├── H743_LVGL_Baseline\                 ← 最小点亮底座
│   ├── H743_Instrument_CN\ / _EN\          ← 全功能仪器(中/英界面),各自带 README
│   ├── _tools\projclone.py                 ← 工程克隆(排除缓存+三处改名+校验)
│   ├── _tools\template_selftest.py         ← 模板冒烟自检(改模板后必跑)
│   ├── _tools\translate_ui.py              ← EN 翻译重刷/校验(配 EN\translation_map.md)
│   ├── 工程克隆工具ProjClone_策划案.md      ← 已实施,含实施记录
│   └── ProjNew工具_策划案.md               ← 待实施(派生+模块选装+裁剪一条龙)
└── stm32project\Function\                  ← 模块库:Hardware 13 硬件驱动 + Algorithm 9 算法组
```

上游工程:`stm32project\VSCodeCMAKEProject\signal_separation`(仍在演进;Instrument 模板与 Function\Algorithm 均为其快照,同步纪律见模板索引)。

## 新项目三步(最常用操作)

```powershell
# 1. 从模板派生(先自动 dry-run 过目;目标短路径、无空格)
python F:\STM32\Templates\_tools\projclone.py --src F:\STM32\Templates\H743_Instrument_CN `
    --dst F:\STM32\stm32project\MyExp --new-name MyExp --profile template-derive
# 2. 编译烧录基线
cd F:\STM32\stm32project\MyExp; cmake --preset Debug; cmake --build build/Debug -j8
# 3. git init 提交基线,再开始改;需要硬件驱动先翻 Function\Hardware(接入方法见工程 CMakeLists 注释组)
```

## 环境要求(接管前自检)

| 依赖 | 验证命令 | 备注 |
|---|---|---|
| Python 3.10+(PATH) | `python --version` | 所有 _tools 脚本 |
| CMake + Ninja | `cmake --version; ninja --version` | |
| Arm GNU 工具链 | 见各工程 cmake\gcc-arm-none-eabi.cmake | 当前 F:\STM32\stm32toll\Arm\...15.2 |
| STM32_Programmer_CLI | 烧录 | C:\Users/<user>\AppData\Local\stm32cube\bundles\programmer\2.22.0+st.1\bin\ |
| LVGL Editor | 改仪器 UI 必需 | 打开 UI\Designer\project.xml,Generate 后直接重编译 |
| STM32CubeMX | 改外设配置 | 打开工程根 .ioc;用户代码区之外会被重生成 |

## 维护三条铁律

1. 改模板 → 跑 `template_selftest.py` → 更新模板索引版本列 → git commit;CN/EN 成对同步(EN 用 translate_ui.py 重刷)。
2. 新项目只从模板派生,驱动优先取 Function,结项把改进 diff 回流。
3. build\ 与备份目录永不入库;留档进 `_archive\`,版本管理交给 git。
