"""Front-panel calibration workflow state machine (UTF-8)."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
import time
from typing import Callable

from .artifacts import WorkflowJournal
from .calibration_plan import (
    AdcCalibrationPreset,
    CalibrationPlanPoint,
    build_adc_42_point_plan,
)
from .fit import AdcQualityReport, evaluate_adc_quality
from .session import CalibrationConfig, CalibrationSession, StepState


class WorkflowStage(str, Enum):
    SITE_CHECK = "现场检查"
    CONNECT = "连接设备"
    SNAPSHOT = "只读快照"
    MINIMUM_STEP = "最小阶跃"
    ENTER = "进入独占"
    START = "开始会话"
    PRE_RAMP = "预爬坡"
    COLLECT = "42 点采集"
    FINISH = "FINISH/REVIEW"
    EXPORT = "EXPORT"
    QUALITY = "拟合与质量门"
    HUMAN_REVIEW = "人工审计"
    OPTIONAL_IMPORT = "可选 IMPORT"
    OPTIONAL_CONFIRM = "可选 CONFIRM"
    OPTIONAL_APPLY = "可选 APPLY"
    EXIT = "退出独占"
    RESTORE = "恢复仪器"
    SAFE_EXIT = "安全停止"
    COMPLETE = "完成"


STAGES = tuple(WorkflowStage)
STAGE_PROGRESS = {
    WorkflowStage.SITE_CHECK: 0.0,
    WorkflowStage.CONNECT: 4.0,
    WorkflowStage.SNAPSHOT: 8.0,
    WorkflowStage.MINIMUM_STEP: 12.0,
    WorkflowStage.ENTER: 16.0,
    WorkflowStage.START: 20.0,
    WorkflowStage.PRE_RAMP: 24.0,
    WorkflowStage.COLLECT: 28.0,
    WorkflowStage.FINISH: 74.0,
    WorkflowStage.EXPORT: 78.0,
    WorkflowStage.QUALITY: 83.0,
    WorkflowStage.HUMAN_REVIEW: 88.0,
    WorkflowStage.OPTIONAL_IMPORT: 92.0,
    WorkflowStage.OPTIONAL_CONFIRM: 94.0,
    WorkflowStage.OPTIONAL_APPLY: 96.0,
    WorkflowStage.EXIT: 97.0,
    WorkflowStage.RESTORE: 98.0,
    WorkflowStage.SAFE_EXIT: 0.0,
    WorkflowStage.COMPLETE: 100.0,
}


@dataclass(slots=True)
class WorkflowState:
    stage: WorkflowStage = WorkflowStage.SITE_CHECK
    status: str = "IDLE"
    completed_points: int = 0
    total_points: int = 42
    progress_percent: float = 0.0
    current_direction: str = ""
    current_nominal_v: float | None = None
    current_reference_v: float | None = None
    current_dg_offset_v: float | None = None
    current_ds_scale_v_div: float | None = None
    current_reference_vpp: float | None = None
    closed_loop_iterations: int | None = None
    elapsed_seconds: float = 0.0
    last_step_seconds: float = 0.0
    estimated_remaining_seconds: float | None = None
    pause_requested: bool = False
    paused: bool = False
    block_reason: str = ""
    restore_status: str = "NOT_STARTED"


class CalibrationWorkflow:
    def __init__(
        self,
        session: CalibrationSession,
        *,
        preset: AdcCalibrationPreset | None = None,
        journal: WorkflowJournal | None = None,
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        self.session = session
        self.preset = preset or AdcCalibrationPreset()
        self.preset.validate(real_hardware=session.transport.name != "serial-fake")
        self.plan = build_adc_42_point_plan(self.preset)
        self.journal = journal
        self.clock = clock
        self.state = WorkflowState(total_points=len(self.plan))
        self.quality_report: AdcQualityReport | None = None
        self.active = False
        self._stage_started = 0.0
        self._workflow_started = 0.0
        self._issued = False
        self._journaled_points = 0
        self._safe_stop_requested = False
        self._safe_stop_reason = ""
        self._restore_after_safe_stop = False
        self.progress_history = [0.0]

    def start(self) -> None:
        if not self.session.connected:
            raise RuntimeError("workflow requires a connected session")
        if self.session.phase != "idle":
            raise RuntimeError(
                f"workflow must start from idle; current phase={self.session.phase}"
            )
        if self.session.pending_operation is not None:
            raise RuntimeError("workflow cannot start while a command is pending")
        self.session.config = CalibrationConfig(
            reference_min_mv=-5000,
            reference_max_mv=5000,
            minimum_points=38,
            minimum_reference_span_mv=9000.0,
            minimum_r_squared=self.preset.minimum_r_squared,
            maximum_absolute_residual_code=(
                self.preset.maximum_absolute_residual_code
            ),
            firmware_id=(
                "SERIAL_FAKE_FORMAL_42"
                if self.session.transport.name == "serial-fake"
                else "REAL_HARDWARE"
            ),
            reference_order="bidirectional",
        )
        now = self.clock()
        self._workflow_started = now
        self._stage_started = now
        self.active = True
        self.state.status = "RUNNING"
        if self.journal:
            self.journal.append(
                "workflow_start",
                source=2,
                item=1,
                expected_points=len(self.plan),
                preset=self.preset.name,
            )

    def request_safe_stop(self, reason: str = "user") -> None:
        if self._safe_stop_requested:
            raise RuntimeError("safe stop is already in progress")
        if not self.active and self.session.phase == "idle":
            raise RuntimeError("workflow is not active and session is already idle")
        self.active = True
        self._safe_stop_requested = True
        self._safe_stop_reason = reason
        self.state.pause_requested = True
        self.state.status = "STOP_REQUESTED"
        self.state.block_reason = "等待当前原子命令完成后退出独占"
        if self.journal:
            self.journal.append(
                "safe_stop_requested",
                reason=reason,
                stage=self.state.stage.value,
                completed_points=len(self.session.points),
            )

    @property
    def awaiting_restore(self) -> bool:
        return (
            self.active
            and self.state.stage == WorkflowStage.RESTORE
            and self.state.restore_status == "PENDING"
        )

    @property
    def safe_stop_in_progress(self) -> bool:
        return self._safe_stop_requested and self.active

    def complete_restore(self, status: str) -> None:
        if not self.awaiting_restore:
            raise RuntimeError("instrument restore is not awaiting completion")
        normalized = status.strip().upper()
        if not normalized:
            raise ValueError("restore status must not be empty")
        self.state.restore_status = normalized
        restore_ok = normalized in {"RESTORED", "NOT_REQUIRED", "SIMULATED"}
        if self.journal:
            self.journal.append(
                "instrument_restore",
                status=normalized,
                safe_stop=self._restore_after_safe_stop,
            )
        if self._restore_after_safe_stop:
            self._finish_safe_stop(restore_ok)
        elif restore_ok:
            self._finish_complete()
        else:
            self.state.status = "RESTORE_FAILED"
            self.state.block_reason = (
                "板端已退出独占，但仪器恢复未完成；请保持现场并人工处理"
            )
            self.active = False

    def request_pause(self) -> None:
        if self.state.stage != WorkflowStage.COLLECT:
            raise RuntimeError("pause is only available during point collection")
        self.state.pause_requested = True

    def resume(self) -> None:
        if not self.state.paused:
            raise RuntimeError("workflow is not paused")
        self.state.paused = False
        self.state.pause_requested = False
        self.state.status = "RUNNING"

    def approve_review(self) -> None:
        if self.state.stage != WorkflowStage.HUMAN_REVIEW:
            raise RuntimeError("human review is not awaiting approval")
        if self.quality_report is None or not self.quality_report.quality_pass:
            raise RuntimeError("quality gates did not pass")
        self.session.preview_import_candidate()
        if self.journal:
            self.journal.append(
                "human_review_approved",
                crc32=self.quality_report.crc32,
                import_sent=False,
            )
        self._advance(WorkflowStage.OPTIONAL_IMPORT)

    def _set_progress(self, value: float) -> None:
        bounded = max(self.state.progress_percent, min(100.0, value))
        self.state.progress_percent = bounded
        if bounded != self.progress_history[-1]:
            self.progress_history.append(bounded)

    def _advance(self, stage: WorkflowStage) -> None:
        now = self.clock()
        self.state.last_step_seconds = max(0.0, now - self._stage_started)
        self._stage_started = now
        self.state.stage = stage
        self.state.status = "RUNNING"
        self.state.block_reason = ""
        self._issued = False
        self._set_progress(STAGE_PROGRESS[stage])
        if self.journal:
            self.journal.append("stage", stage=stage.value)

    def _update_timing(self) -> None:
        if not self._workflow_started:
            return
        elapsed = max(0.0, self.clock() - self._workflow_started)
        self.state.elapsed_seconds = elapsed
        progress = self.state.progress_percent
        self.state.estimated_remaining_seconds = (
            None if progress <= 0.0
            else max(0.0, elapsed * (100.0 - progress) / progress)
        )

    def _journal_new_points(self) -> None:
        while self._journaled_points < len(self.session.points):
            point = self.session.points[self._journaled_points]
            if self.journal:
                self.journal.append(
                    "point_complete",
                    plan_index=point.original_index,
                    command=f"CAL EXT POINT {round(point.reference_mv)}",
                    direction=point.direction,
                    nominal_v=point.nominal_v,
                    reference_v=point.voltage,
                    measured_code=point.measured_code,
                    minimum_code=point.minimum_code,
                    maximum_code=point.maximum_code,
                    flags=point.flags,
                    crc32=point.crc,
                )
            self._journaled_points += 1

    def _enter_restore(self, *, safe_stop: bool) -> None:
        self._restore_after_safe_stop = safe_stop
        self.state.stage = WorkflowStage.RESTORE
        self.state.status = "RESTORING"
        self.state.block_reason = ""
        self._issued = False
        if not safe_stop:
            self._set_progress(STAGE_PROGRESS[WorkflowStage.RESTORE])
        if self.session.transport.name == "serial-fake":
            self.state.restore_status = "PENDING"
            self.complete_restore("SIMULATED")
        else:
            self.state.restore_status = "PENDING"
            self.state.status = "WAITING_RESTORE"
            self.state.block_reason = "板端已退出独占，等待恢复本程序改动过的仪器 CH1"
        if self.journal:
            self.journal.append(
                "stage",
                stage=WorkflowStage.RESTORE.value,
                safe_stop=safe_stop,
            )

    def _finish_safe_stop(self, restore_ok: bool) -> None:
        self._journal_new_points()
        self.state.completed_points = len(self.session.points)
        self.state.stage = WorkflowStage.SAFE_EXIT
        self.state.status = "ABORTED" if restore_ok else "ABORTED_RESTORE_FAILED"
        self.state.block_reason = (
            "已安全退出独占；已采数据与日志保留"
            if restore_ok
            else "板端已退出独占，但仪器恢复未完成；已采数据与日志保留"
        )
        self.active = False
        if self.journal:
            self.journal.append(
                "workflow_aborted",
                reason=self._safe_stop_reason,
                completed_points=self.state.completed_points,
                board_phase=self.session.phase,
                restore_status=self.state.restore_status,
            )

    def _finish_complete(self) -> None:
        self._advance(WorkflowStage.COMPLETE)
        self.state.status = "PASS"
        self.active = False
        if self.journal:
            self.journal.append(
                "workflow_complete",
                progress=100.0,
                import_sent=False,
                apply_sent=False,
                board_phase=self.session.phase,
                restore_status=self.state.restore_status,
            )

    def _safe_stop_tick(self) -> None:
        self._journal_new_points()
        self.state.completed_points = len(self.session.points)
        if self.session.pending_operation is not None:
            self.state.status = "WAITING_ATOMIC"
            self.state.block_reason = (
                f"等待命令完成：{self.session.pending_operation.command}"
            )
            return
        if self.session.requires_reconciliation:
            if not self.session.reconciliation_cal_status_seen:
                self.session.query("CAL STATUS")
                self.state.status = "RECONCILING"
                self.state.block_reason = "结果未知，正在只读查询 CAL STATUS"
                return
            self.session.acknowledge_reconciled()
        if self.session.phase == "idle":
            self._enter_restore(safe_stop=True)
            return
        if not self._issued:
            self.session.exit()
            self._issued = True
            self.state.status = "EXITING"
            self.state.block_reason = "正在发送 CAL EXT EXIT 并等待 state=0"
            return
        if self.session.pending_operation is None:
            if self.session.phase != "idle":
                self.state.status = "EXIT_FAILED"
                self.state.block_reason = (
                    f"退出命令结束但 phase={self.session.phase}，禁止继续"
                )
                self.active = False
                return
            self._enter_restore(safe_stop=True)

    def _collect_tick(self) -> None:
        self._journal_new_points()
        completed = len(self.session.points)
        self.state.completed_points = completed
        self._set_progress(28.0 + (completed / len(self.plan)) * 46.0)
        if self.state.pause_requested and self.session.pending_operation is None:
            self.state.paused = True
            self.state.status = "PAUSED"
            return
        if self.state.paused or self.session.pending_operation is not None:
            return
        if completed >= len(self.plan):
            self._advance(WorkflowStage.FINISH)
            return
        point: CalibrationPlanPoint = self.plan[completed]
        reference_mv = round(point.nominal_v * 1000.0)
        if point.direction == "descending":
            reference_mv += 1
        reference_v = reference_mv / 1000.0
        self.state.current_direction = point.direction
        self.state.current_nominal_v = point.nominal_v
        self.state.current_reference_v = reference_v
        self.state.current_dg_offset_v = point.nominal_v * 2.0
        self.state.current_ds_scale_v_div = self.preset.ds_sweep_scale_v_div
        self.state.current_reference_vpp = 0.015625
        self.state.closed_loop_iterations = 1
        self.session.point(
            reference_mv,
            metadata={
                "nominal_v": point.nominal_v,
                "direction": point.direction,
                "direction_index": point.direction_index,
                "dg_offset_v": self.state.current_dg_offset_v,
                "reference_v": reference_v,
                "reference_vpp": self.state.current_reference_vpp,
                "ds_scale_v_div": self.state.current_ds_scale_v_div,
                "closed_loop_iterations": 1,
            },
        )

    def tick(self) -> WorkflowState:
        if not self.active:
            return self.state
        self.session.poll()
        self._update_timing()
        if self._safe_stop_requested:
            self._safe_stop_tick()
            return self.state
        if self.session.requires_reconciliation:
            self.state.status = "UNKNOWN"
            self.state.block_reason = "结果未知，仅允许 STATUS 复核"
            return self.state
        stage = self.state.stage
        if stage == WorkflowStage.SITE_CHECK:
            self._advance(WorkflowStage.CONNECT)
        elif stage == WorkflowStage.CONNECT:
            self._advance(WorkflowStage.SNAPSHOT)
        elif stage == WorkflowStage.SNAPSHOT:
            if not self._issued:
                self.session.query("CAL STATUS")
                self._issued = True
            elif self.session.pending_operation is None:
                if self.journal:
                    self.journal.append(
                        "board_snapshot",
                        status=self.session.last_status,
                    )
                self._advance(WorkflowStage.MINIMUM_STEP)
        elif stage == WorkflowStage.MINIMUM_STEP:
            if self.session.transport.name != "serial-fake":
                self.state.status = "WAITING_CONFIRMATION"
                self.state.block_reason = "等待 DG/DS prepare 结果和人工确认"
            else:
                self._advance(WorkflowStage.ENTER)
        elif stage == WorkflowStage.ENTER:
            if not self._issued:
                self.session.enter()
                self._issued = True
            elif self.session.pending_operation is None:
                self._advance(WorkflowStage.START)
        elif stage == WorkflowStage.START:
            if not self._issued:
                self.session.start(len(self.plan))
                self._issued = True
            elif self.session.pending_operation is None:
                self._advance(WorkflowStage.PRE_RAMP)
        elif stage == WorkflowStage.PRE_RAMP:
            self._advance(WorkflowStage.COLLECT)
        elif stage == WorkflowStage.COLLECT:
            self._collect_tick()
        elif stage == WorkflowStage.FINISH:
            if not self._issued:
                self.session.finish()
                self._issued = True
            elif self.session.pending_operation is None:
                self._advance(WorkflowStage.EXPORT)
        elif stage == WorkflowStage.EXPORT:
            if not self._issued:
                self.session.export()
                self._issued = True
            elif self.session.pending_operation is None:
                if self.session.steps["export"] != StepState.SUCCESS:
                    self.state.status = "FAILED"
                    self.state.block_reason = self.session.export_error
                    self.active = False
                else:
                    self._advance(WorkflowStage.QUALITY)
        elif stage == WorkflowStage.QUALITY:
            self.quality_report = evaluate_adc_quality(
                self.session.points, preset=self.preset
            )
            self.session.quality_report = self.quality_report
            self.session.fit_result = self.quality_report.fit
            self.session.steps["fit"] = StepState.SUCCESS
            if self.journal:
                self.journal.append(
                    "quality",
                    passed=self.quality_report.quality_pass,
                    crc32=self.quality_report.crc32,
                )
            self._advance(WorkflowStage.HUMAN_REVIEW)
            self.state.status = "WAITING_CONFIRMATION"
            self.state.block_reason = "等待人工审计"
        elif stage in {
            WorkflowStage.OPTIONAL_IMPORT,
            WorkflowStage.OPTIONAL_CONFIRM,
            WorkflowStage.OPTIONAL_APPLY,
        }:
            next_stage = {
                WorkflowStage.OPTIONAL_IMPORT: WorkflowStage.OPTIONAL_CONFIRM,
                WorkflowStage.OPTIONAL_CONFIRM: WorkflowStage.OPTIONAL_APPLY,
                WorkflowStage.OPTIONAL_APPLY: WorkflowStage.EXIT,
            }[stage]
            self._advance(next_stage)
        elif stage == WorkflowStage.EXIT:
            if self.session.phase == "idle":
                self._enter_restore(safe_stop=False)
            elif not self._issued:
                self.session.exit()
                self._issued = True
            elif self.session.pending_operation is None:
                if self.session.phase != "idle":
                    self.state.status = "EXIT_FAILED"
                    self.state.block_reason = (
                        f"CAL EXT EXIT 未回到 idle：phase={self.session.phase}"
                    )
                    self.active = False
                else:
                    self._enter_restore(safe_stop=False)
        elif stage == WorkflowStage.RESTORE:
            if self.state.restore_status == "NOT_STARTED":
                self._enter_restore(safe_stop=False)
        return self.state
