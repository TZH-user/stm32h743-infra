"""AutoBench 可分类异常。"""


class AutoBenchError(RuntimeError):
    """AutoBench 基础异常。"""


class PlanValidationError(AutoBenchError):
    """JSON 计划结构或参数无效。"""


class SafetyLimitError(PlanValidationError):
    """计划违反硬安全上限。"""


class OfflineHardwareBlockedError(AutoBenchError):
    """离线阶段主动阻止任何仪器或串口访问。"""


class CaptureDataError(AutoBenchError):
    """模拟数据或 board_vcp CSV 不可用。"""

