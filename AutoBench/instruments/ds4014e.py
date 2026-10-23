"""DS4014E Toolkit 复用边界说明与离线硬门禁。

示波器驱动唯一复用源：
F:\\STM32\\stm32project\\VSCodeCMAKEProject\\DS4014E_Toolkit\\ds4014e.py

为避免复制后产生第二份驱动，本离线阶段不在 AutoBench 内重写 SCPI 客户端。
后续上电阶段应在显式授权后通过适配器加载该 Toolkit，并逐项完成验证矩阵。
"""

from __future__ import annotations

from pathlib import Path

from autobench_core.errors import OfflineHardwareBlockedError


TOOLKIT_PATH = Path(
    r"F:\STM32\stm32project\VSCodeCMAKEProject\DS4014E_Toolkit\ds4014e.py"
)

PENDING_POWER_ON_VALIDATION = (
    "RAW 波形读取",
    "屏幕截图",
    "capture 单次采集",
    "run/stop/single/tforce 状态改变",
    "wave 读出配置写入与恢复",
    "自动测量语法与超时恢复",
)


def block_scope_access() -> None:
    """在导入 socket 驱动前阻止 scope 分支。"""

    raise OfflineHardwareBlockedError(
        "当前为仪器下电的离线软件阶段，scope 分支已在加载 DS4014E 驱动前硬阻止；"
        "不得执行 socket 探测、SCPI、capture 或采集状态改变。"
    )

