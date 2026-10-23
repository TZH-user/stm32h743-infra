"""Calibration session state machine and safety gates (UTF-8)."""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from enum import Enum
import math
import time
from typing import Callable

from .crc import calibration_import_crc32, format_crc32
from .fit import (
    AdcQualityReport,
    CalibrationPoint,
    FitPolicy,
    FitResult,
    fit_linear,
    fit_to_import_fields,
)
from .protocol import IncrementalLineParser, ProtocolEvent
from .transport import Transport, TransportDisconnected, TransportError


class StepState(str, Enum):
    NOT_STARTED = "未开始"
    QUEUED = "排队"
    WAITING = "等待业务结果"
    SUCCESS = "成功"
    FAILED = "失败"
    RETRYABLE = "可重试"
    FROZEN = "已冻结"
    UNKNOWN = "结果未知"


@dataclass(slots=True)
class CalibrationConfig:
    reference_min_mv: int | None = None
    reference_max_mv: int | None = None
    minimum_points: int | None = None
    minimum_reference_span_mv: float | None = None
    minimum_r_squared: float | None = None
    maximum_absolute_residual_code: float | None = None
    firmware_id: str = "NOT_CONFIGURED"
    reference_order: str | None = None

    @property
    def safety_configured(self) -> bool:
        configured = (
            self.reference_min_mv is not None
            and self.reference_max_mv is not None
            and self.minimum_points is not None
        )
        if not configured:
            return False
        try:
            self.validate()
        except ValueError:
            return False
        return True

    def validate(self) -> None:
        if (self.reference_min_mv is None) != (self.reference_max_mv is None):
            raise ValueError("reference minimum and maximum must be configured together")
        if self.reference_min_mv is not None and self.reference_max_mv is not None:
            if self.reference_min_mv >= self.reference_max_mv:
                raise ValueError("reference minimum must be below maximum")
            if self.reference_min_mv < -20000 or self.reference_max_mv > 20000:
                raise ValueError("configured reference range exceeds protocol bounds")
        if self.minimum_points is not None and self.minimum_points < 2:
            raise ValueError("minimum point count must be at least two")
        if self.reference_order not in {
            None, "ascending", "descending", "bidirectional"
        }:
            raise ValueError(
                "reference order must be ascending, descending, bidirectional, or blank"
            )

    def fit_policy(self) -> FitPolicy:
        return FitPolicy(
            minimum_points=self.minimum_points,
            minimum_reference_span_mv=self.minimum_reference_span_mv,
            minimum_r_squared=self.minimum_r_squared,
            maximum_absolute_residual_code=self.maximum_absolute_residual_code,
        )


@dataclass(slots=True)
class Profile:
    sequence: int | str
    source: str
    model: str
    quality: str
    crc32: str
    fit_version: int


@dataclass(slots=True)
class ExportPoint:
    sequence: int
    index: int
    reference: float
    measured: float
    frequency_hz: int
    phase_deg: float
    flags: int
    crc32: str
    raw_line: str


@dataclass(slots=True)
class ExportResult:
    sequence: int
    schema: int
    mode: int
    source: int
    item: int
    expected_points: int
    declared_points: int
    session_crc32: str
    finish_crc32: str
    points: list[ExportPoint]
    legacy: bool


@dataclass(slots=True)
class LogEntry:
    timestamp: str
    direction: str
    category: str
    text: str


Completion = Callable[[ProtocolEvent], bool]


@dataclass(slots=True)
class PendingOperation:
    name: str
    command: str
    completion: Completion
    started_at: float
    timeout_seconds: float
    context: dict = field(default_factory=dict)


class CalibrationSession:
    MUTATING_PREFIXES = (
        "CAL EXT ENTER",
        "CAL EXT START",
        "CAL EXT POINT",
        "CAL EXT FINISH",
        "CAL IMPORT ADC",
        "CAL CONFIRM",
        "CAL APPLY",
        "CAL UNDO",
        "CAL EXT EXIT",
    )

    def __init__(
        self,
        transport: Transport,
        *,
        config: CalibrationConfig | None = None,
        timeout_seconds: float = 2.0,
        clock: Callable[[], float] = time.monotonic,
        historical_protocol: bool = False,
        mutating_enabled: bool = True,
    ) -> None:
        self.transport = transport
        self.config = config or CalibrationConfig()
        self.timeout_seconds = timeout_seconds
        self.clock = clock
        self.parser = IncrementalLineParser(historical=historical_protocol)
        self.mutating_enabled = mutating_enabled
        self.connected = False
        self.phase = "idle"
        self.requires_reconciliation = False
        self.reconciliation_cal_status_seen = False
        self.pending_operation: PendingOperation | None = None
        self.points: list[CalibrationPoint] = []
        self.fit_result: FitResult | None = None
        self.quality_report: AdcQualityReport | None = None
        self.candidate_preview: Profile | None = None
        self.fit_version = 0
        self.candidate: Profile | None = None
        self.pending_profile: Profile | None = None
        self.active_profile: Profile | None = None
        self.previous_active_profile: Profile | None = None
        self.export_result: ExportResult | None = None
        self.export_error = ""
        self._export_context: dict | None = None
        self.logs: list[LogEntry] = []
        self.steps = {name: StepState.NOT_STARTED for name in (
            "enter", "start", "point", "finish", "fit", "import",
            "confirm", "apply", "undo", "export", "exit",
        )}
        self.last_error = ""
        self.last_status: dict = {}
        self.active_verified = False
        self.profile_verification = "NOT_STARTED"
        self._verification_active_sequence: int | None = None
        self.lifecycle_verification = "PARTIAL_HARDWARE_PENDING"

    def _log(self, direction: str, category: str, text: str) -> None:
        self.logs.append(
            LogEntry(datetime.now(timezone.utc).isoformat(), direction, category, text)
        )

    def connect(self) -> None:
        self.transport.open()
        self.connected = True
        self._log("EVENT", "connection", f"connected:{self.transport.name}")

    def disconnect(self, reason: str = "user") -> None:
        self.transport.close()
        self.connected = False
        if self.pending_operation is not None or self.phase != "idle":
            if self.pending_operation is not None:
                self.steps[self.pending_operation.name] = StepState.UNKNOWN
            self.requires_reconciliation = True
            self.reconciliation_cal_status_seen = False
            self.phase = "unknown"
            self.pending_operation = None
            self.last_status.clear()
        self._log("EVENT", "connection", f"disconnected:{reason}")

    def acknowledge_reconciled(self) -> None:
        if not self.reconciliation_cal_status_seen:
            raise RuntimeError("CAL STATUS is required before reconciliation")
        self.requires_reconciliation = False
        self.reconciliation_cal_status_seen = False
        self._log("EVENT", "state", "reconciliation acknowledged")

    def cancel_in_flight(self) -> None:
        if self.pending_operation is None:
            raise RuntimeError("no command is in-flight")
        operation = self.pending_operation
        is_mutating = operation.command.startswith(self.MUTATING_PREFIXES)
        self.steps[operation.name] = (
            StepState.UNKNOWN if is_mutating else StepState.RETRYABLE
        )
        if is_mutating:
            self.requires_reconciliation = True
            self.reconciliation_cal_status_seen = False
            self.phase = "unknown"
            self.last_status.clear()
        self.last_error = f"cancelled:{operation.command}"
        self._log("EVENT", "error", self.last_error)
        self.pending_operation = None

    def _issue(
        self,
        name: str,
        command: str,
        completion: Completion,
        *,
        context: dict | None = None,
        allow_during_reconciliation: bool = False,
    ) -> None:
        if not self.connected or not self.transport.is_open:
            raise TransportDisconnected("transport is not connected")
        if self.pending_operation is not None:
            raise RuntimeError("another command is in-flight")
        if (
            self.requires_reconciliation
            and command.startswith(self.MUTATING_PREFIXES)
            and not allow_during_reconciliation
        ):
            raise RuntimeError("state-changing commands are frozen until CAL STATUS reconciliation")
        if command.startswith(self.MUTATING_PREFIXES) and not self.mutating_enabled:
            raise PermissionError("offline REVIEW is read-only")
        self.steps[name] = StepState.QUEUED
        self._log("TX", "command", command)
        try:
            self.transport.write((command + "\r\n").encode("ascii"))
        except TransportDisconnected as exc:
            self.connected = False
            self.requires_reconciliation = True
            self.reconciliation_cal_status_seen = False
            self.phase = "unknown"
            self.last_status.clear()
            self.steps[name] = StepState.UNKNOWN
            self._log("EVENT", "error", str(exc))
            raise
        except TransportError as exc:
            self.steps[name] = StepState.RETRYABLE
            self.last_error = str(exc)
            self._log("EVENT", "error", str(exc))
            raise
        self.pending_operation = PendingOperation(
            name=name,
            command=command,
            completion=completion,
            started_at=self.clock(),
            timeout_seconds=self.timeout_seconds,
            context=context or {},
        )
        self.steps[name] = StepState.WAITING

    def query(self, command: str) -> None:
        expected_kind = {
            "PING": "pong",
            "STATUS": "control_status",
            "RTOS": "rtos",
            "CAL STATUS": "cal_status",
            "OUTPUT LOG": "control_ok",
        }.get(command)
        if expected_kind is None:
            raise ValueError("unsupported read-only command")
        self._issue(
            f"query_{command.lower().replace(' ', '_')}",
            command,
            lambda event: event.kind == expected_kind,
            allow_during_reconciliation=True,
        )

    def enter(self) -> None:
        if self.phase != "idle":
            raise RuntimeError("ENTER is only valid from idle")
        self._issue(
            "enter",
            "CAL EXT ENTER",
            lambda event: event.kind == "control_cal"
            and event.fields.get("state") == 3
            and event.fields.get("can_start") == 1,
        )

    def start(self, expected_points: int | None = None) -> None:
        if self.phase != "ready":
            raise RuntimeError("START is only valid after successful ENTER")
        count = self.config.minimum_points if expected_points is None else expected_points
        if count is None:
            raise ValueError("minimum point count is NOT_CONFIGURED")
        if not 0 <= count <= 100000:
            raise ValueError("expected point count outside protocol range")
        self._issue(
            "start",
            f"CAL EXT START {count}",
            lambda event: event.kind == "control_cal" and event.fields.get("state") == 5,
        )

    def validate_reference(self, reference_mv: float) -> list[str]:
        if not math.isfinite(reference_mv):
            raise ValueError("reference must be finite")
        if int(reference_mv) != reference_mv:
            raise ValueError("protocol reference must be an integer mV")
        if not -20000 <= reference_mv <= 20000:
            raise ValueError("reference outside protocol range -20000..20000 mV")
        if self.config.reference_min_mv is None or self.config.reference_max_mv is None:
            raise ValueError("real reference range is NOT_CONFIGURED")
        if not self.config.reference_min_mv <= reference_mv <= self.config.reference_max_mv:
            raise ValueError("reference outside configured range")
        if (
            self.config.reference_order != "bidirectional"
            and any(point.reference_mv == reference_mv for point in self.points)
        ):
            raise ValueError("duplicate reference value")
        warnings: list[str] = []
        if self.points:
            previous = self.points[-1].reference_mv
            if self.config.reference_order == "ascending" and reference_mv < previous:
                warnings.append("reference order violates configured ascending policy")
            elif self.config.reference_order == "descending" and reference_mv > previous:
                warnings.append("reference order violates configured descending policy")
            elif self.config.reference_order is None:
                warnings.append("reference order policy is NOT_CONFIGURED")
        return warnings

    def point(
        self,
        reference_mv: int,
        *,
        metadata: dict | None = None,
    ) -> list[str]:
        if self.phase != "session":
            raise RuntimeError("POINT is only valid in an active session")
        warnings = self.validate_reference(float(reference_mv))
        self._issue(
            "point",
            f"CAL EXT POINT {reference_mv}",
            lambda event: event.kind == "cal_ext_point"
            and event.fields.get("ref_mv") == reference_mv,
            context={"reference_mv": reference_mv, **(metadata or {})},
        )
        return warnings

    def finish(self) -> None:
        if self.phase != "session":
            raise RuntimeError("FINISH is only valid in an active session")
        self._issue(
            "finish",
            "CAL EXT FINISH",
            lambda event: event.kind == "control_cal" and event.fields.get("state") == 6,
        )

    def fit(self, *, sort_copy: bool = False) -> FitResult:
        if self.phase != "review":
            raise RuntimeError("fit is only valid after FINISH")
        self.fit_version += 1
        result = fit_linear(
            self.points,
            policy=self.config.fit_policy(),
            version=self.fit_version,
            sort_copy=sort_copy,
        )
        self.fit_result = result
        self.steps["fit"] = StepState.SUCCESS
        self._log("EVENT", "fit", f"fit version={result.version} state={result.pass_state}")
        return result

    def exclude_point(self, original_index: int, reason: str) -> None:
        for index, point in enumerate(self.points):
            if point.original_index == original_index:
                self.points[index] = point.excluded(reason)
                self._log("EVENT", "fit", f"exclude original_index={original_index} reason={reason}")
                return
        raise KeyError(original_index)

    def import_candidate(self) -> None:
        if self.phase != "review":
            raise RuntimeError("IMPORT is only valid after fit review")
        if self.fit_result is None:
            raise RuntimeError("fit is required before import")
        if self.quality_report is not None and not self.quality_report.quality_pass:
            raise RuntimeError("formal ADC quality gates must PASS before import")
        fields = fit_to_import_fields(self.fit_result)
        crc = calibration_import_crc32(*fields)
        command = f"CAL IMPORT ADC {' '.join(str(value) for value in fields)} {format_crc32(crc)}"
        self._log("EVENT", "audit", "IMPORT explicitly requested")
        self._issue(
            "import",
            command,
            lambda event: event.kind == "control_cal" and event.fields.get("cand") == 1,
            context={"fields": fields, "crc": format_crc32(crc)},
        )

    def cancel_candidate(self) -> None:
        if self.phase != "candidate":
            raise RuntimeError("no candidate to cancel")
        self.candidate = None
        self.phase = "review"
        self.steps["import"] = StepState.NOT_STARTED
        self._log("EVENT", "lifecycle", "candidate cancelled locally")

    def confirm(self) -> None:
        if self.phase != "candidate" or self.candidate is None:
            raise RuntimeError("candidate is required")
        self._log("EVENT", "audit", "CONFIRM explicitly requested")
        self._issue(
            "confirm",
            "CAL CONFIRM",
            lambda event: event.kind == "control_cal"
            and event.fields.get("cand") == 0
            and event.fields.get("pend") == 1,
        )

    def apply(self) -> None:
        if self.phase != "pending" or self.pending_profile is None:
            raise RuntimeError("pending profile is required")
        self._log("EVENT", "audit", "APPLY explicitly requested")
        self._issue(
            "apply",
            "CAL APPLY",
            lambda event: event.kind == "control_cal"
            and event.fields.get("pend") == 0
            and event.fields.get("applied") == 1,
        )

    def undo(self) -> None:
        self._log("EVENT", "audit", "UNDO explicitly requested")
        self._issue(
            "undo",
            "CAL UNDO",
            lambda event: event.kind == "control_cal"
            and event.fields.get("applied") == 0,
        )

    def export(self, sequence: int | None = None) -> None:
        if sequence is not None:
            if isinstance(sequence, bool) or not isinstance(sequence, int):
                raise ValueError("export sequence must be an integer")
            if not 1 <= sequence <= 0xFFFFFFFF:
                raise ValueError("export sequence must be in 1..4294967295")
        self.export_result = None
        self.export_error = ""
        self._export_context = None
        command = "CAL EXPORT LATEST" if sequence is None else f"CAL EXPORT {sequence}"
        self._issue(
            "export",
            command,
            lambda event: event.kind == "cal_export_end",
            context={"requested_sequence": sequence},
        )

    def exit(self) -> None:
        if self.phase == "idle":
            raise RuntimeError("already idle")
        self._issue(
            "exit",
            "CAL EXT EXIT",
            lambda event: event.kind == "control_cal" and event.fields.get("state") == 0,
        )

    def _profile_from_fit(self, crc: str) -> Profile:
        if self.fit_result is None:
            raise RuntimeError("fit missing")
        return Profile(
            sequence="NOT_PROVIDED",
            source="external_adc",
            model=(
                f"code={self.fit_result.gain_code_per_volt:.9g}*V"
                f"+{self.fit_result.offset_code:.9g}"
            ),
            quality=(
                f"R2={self.fit_result.r_squared:.9g}; "
                f"max_residual={self.fit_result.maximum_absolute_residual_code:.9g}; "
                f"points={len(self.fit_result.used_original_indices)}; "
                f"result={self.fit_result.pass_state}"
            ),
            crc32=crc,
            fit_version=self.fit_result.version,
        )

    def preview_import_candidate(self) -> Profile:
        if self.fit_result is None:
            raise RuntimeError("fit is required before candidate preview")
        if self.quality_report is not None:
            if not self.quality_report.quality_pass:
                raise RuntimeError("formal ADC quality gates did not pass")
            crc = self.quality_report.crc32
        else:
            fields = fit_to_import_fields(self.fit_result)
            crc = format_crc32(calibration_import_crc32(*fields))
        self.candidate_preview = self._profile_from_fit(crc)
        self._log("EVENT", "audit", "candidate preview generated; no command sent")
        return self.candidate_preview

    def _complete(self, event: ProtocolEvent) -> None:
        operation = self.pending_operation
        if operation is None:
            return
        if operation.name == "export" and self.export_result is None:
            self.steps[operation.name] = StepState.FAILED
            self.last_error = self.export_error or "invalid export transaction"
            self._log("EVENT", "error", self.last_error)
            self.pending_operation = None
            return
        if operation.name == "point" and event.kind == "cal_ext_point":
            context = operation.context
            self.points.append(
                CalibrationPoint(
                    original_index=len(self.points),
                    reference_mv=float(event.fields["ref_mv"]),
                    measured_code=float(event.fields["mean"]),
                    timestamp=event.timestamp,
                    crc=str(event.fields["crc"]),
                    raw_line=event.raw,
                    nominal_v=context.get("nominal_v"),
                    direction=str(context.get("direction", "")),
                    direction_index=context.get("direction_index"),
                    dg_offset_v=context.get("dg_offset_v"),
                    reference_v=context.get(
                        "reference_v", float(event.fields["ref_mv"]) / 1000.0
                    ),
                    reference_vpp=context.get("reference_vpp"),
                    minimum_code=int(event.fields["min"]),
                    maximum_code=int(event.fields["max"]),
                    flags=int(event.fields["flags"]),
                    ds_scale_v_div=context.get("ds_scale_v_div"),
                    closed_loop_iterations=context.get("closed_loop_iterations"),
                )
            )
        elif operation.name == "import":
            self.candidate = self._profile_from_fit(operation.context["crc"])
            self.phase = "candidate"
        elif operation.name == "confirm":
            self.pending_profile = self.candidate
            self.candidate = None
            self.phase = "pending"
        elif operation.name == "apply":
            self.previous_active_profile = self.active_profile
            self.active_profile = self.pending_profile
            self.pending_profile = None
            self.phase = "active"
            self.active_verified = False
            self.profile_verification = "AWAITING_STATUS"
            self._verification_active_sequence = None
        elif operation.name == "undo":
            self.active_profile = self.previous_active_profile
            self.previous_active_profile = None
        elif operation.name == "enter":
            self.phase = "ready"
        elif operation.name == "start":
            self.phase = "session"
        elif operation.name == "finish":
            self.phase = "review"
        elif operation.name == "exit":
            self.phase = "idle"
        self.steps[operation.name] = StepState.SUCCESS
        self.pending_operation = None

    def _export_requested_sequence(self) -> int | None:
        if self.pending_operation is None or self.pending_operation.name != "export":
            return None
        return self.pending_operation.context.get("requested_sequence")

    def _add_export_error(self, reason: str) -> None:
        if self._export_context is not None:
            errors = self._export_context["errors"]
            if reason not in errors:
                errors.append(reason)
        elif not self.export_error:
            self.export_error = reason

    def _handle_export_event(self, event: ProtocolEvent) -> None:
        if event.kind == "cal_export_begin":
            requested = self._export_requested_sequence()
            self.export_result = None
            self.export_error = ""
            self._export_context = {
                "begin": event.fields,
                "points": [],
                "next_index": 0,
                "errors": [],
            }
            if requested is not None and event.fields["sequence"] != requested:
                self._add_export_error(
                    f"export sequence mismatch: requested={requested} begin={event.fields['sequence']}"
                )
            return

        if event.kind in {"cal_export_point", "cal_export_point_crc_error"}:
            if self._export_context is None:
                self._add_export_error("export POINT received before BEGIN")
                return
            begin = self._export_context["begin"]
            index = event.fields["index"]
            expected_index = self._export_context["next_index"]
            if event.fields["sequence"] != begin["sequence"]:
                self._add_export_error(
                    f"export POINT sequence mismatch at index={index}"
                )
            if index != expected_index:
                self._add_export_error(
                    f"export POINT index mismatch: expected={expected_index} actual={index}"
                )
            else:
                self._export_context["next_index"] += 1
            if event.kind == "cal_export_point_crc_error":
                self._add_export_error(
                    f"export POINT CRC mismatch at index={index}: "
                    f"expected={event.fields['crc']} actual={event.fields['actual_crc']}"
                )
                return
            self._export_context["points"].append(
                ExportPoint(
                    sequence=event.fields["sequence"],
                    index=index,
                    reference=event.fields["reference"],
                    measured=event.fields["measured"],
                    frequency_hz=event.fields["frequency_hz"],
                    phase_deg=event.fields["phase_deg"],
                    flags=event.fields["flags"],
                    crc32=event.fields["crc"],
                    raw_line=event.raw,
                )
            )
            return

        if event.kind == "malformed_export":
            self._add_export_error(
                f"malformed export record: {event.fields.get('reason', 'unknown')}"
            )
            return

        if event.kind != "cal_export_end":
            return
        if self._export_context is None:
            self._add_export_error("export END received before BEGIN")
            return
        context = self._export_context
        begin = context["begin"]
        if event.fields["sequence"] != begin["sequence"]:
            self._add_export_error("export END sequence does not match BEGIN")
        if event.fields["points"] != begin["points"]:
            self._add_export_error("export END point count does not match BEGIN")
        if context["next_index"] != begin["points"]:
            self._add_export_error("export POINT index/count sequence is incomplete")
        if len(context["points"]) != begin["points"]:
            self._add_export_error("one or more export POINT records are invalid")
        errors = list(context["errors"])
        if errors:
            self.export_error = "; ".join(errors)
            self.export_result = None
        else:
            self.export_result = ExportResult(
                sequence=begin["sequence"],
                schema=begin["schema"],
                mode=begin["mode"],
                source=begin["source"],
                item=begin["item"],
                expected_points=begin["expected"],
                declared_points=begin["points"],
                session_crc32=begin["crc"],
                finish_crc32=event.fields["crc"],
                points=list(context["points"]),
                legacy=bool(begin["legacy"]),
            )
            self.export_error = ""
        self._export_context = None

    def _handle_event(self, event: ProtocolEvent) -> None:
        category = "business" if event.kind.startswith("cal_") or event.kind == "control_cal" else "protocol"
        self._log("RX", category, event.raw or event.kind)
        if event.kind.startswith("cal_export_") or event.kind == "malformed_export":
            self._handle_export_event(event)
        if event.kind in {"cal_status", "control_cal"}:
            self.last_status.update(event.fields)
        elif event.kind.startswith("cal_status_") or event.kind == "cal_last":
            section = str(event.fields.get("section", event.kind)).lower()
            self.last_status.setdefault("sections", {})[section] = dict(event.fields)
            if (
                self.requires_reconciliation
                and event.kind == "cal_status_profile"
                and event.fields.get("pending") not in {0, "0"}
            ):
                self.phase = "pending_unknown_profile"
            if (
                event.kind == "cal_status_profile"
                and self.profile_verification == "AWAITING_STATUS"
            ):
                active = event.fields.get("active")
                self._verification_active_sequence = (
                    active if isinstance(active, int) else None
                )
            if event.kind == "cal_status_profile2" and self.profile_verification == "AWAITING_STATUS":
                if (
                    self._verification_active_sequence is not None
                    and self._verification_active_sequence > 0
                    and event.fields.get("enabled") == 1
                    and event.fields.get("in_use") == 1
                ):
                    self.active_verified = True
                    self.profile_verification = "PASS"
                else:
                    self.profile_verification = "FAIL"
        if event.kind == "cal_status" and self.requires_reconciliation:
            self.reconciliation_cal_status_seen = True
            state = event.fields.get("state")
            if event.fields.get("pend") == 1:
                self.phase = "pending_unknown_profile"
            elif state == 5:
                self.phase = "session_unknown"
            elif state == 6:
                self.phase = "review"
            elif state == 3:
                self.phase = "ready"
            elif state == 0:
                self.phase = "idle"
        if event.kind in {"line_too_long", "malformed_encoding"}:
            self.last_error = event.kind
            if self.pending_operation:
                self.steps[self.pending_operation.name] = StepState.FROZEN
                self.pending_operation = None
            return
        if event.kind in {"control_error", "cal_ext_error"}:
            self.last_error = event.raw
            if self.pending_operation:
                self.steps[self.pending_operation.name] = StepState.RETRYABLE
                self.pending_operation = None
            return
        if self.pending_operation and self.pending_operation.completion(event):
            self._complete(event)

    def poll(self) -> list[ProtocolEvent]:
        events: list[ProtocolEvent] = []
        try:
            while True:
                chunk = self.transport.read_available()
                if not chunk:
                    break
                current = self.parser.feed(chunk)
                events.extend(current)
                for event in current:
                    self._handle_event(event)
        except TransportDisconnected as exc:
            self.connected = False
            self.requires_reconciliation = True
            self.reconciliation_cal_status_seen = False
            self.phase = "unknown"
            self.last_status.clear()
            if self.pending_operation:
                self.steps[self.pending_operation.name] = StepState.UNKNOWN
                self.pending_operation = None
            self._log("EVENT", "error", str(exc))
        if self.pending_operation:
            elapsed = self.clock() - self.pending_operation.started_at
            if elapsed >= self.pending_operation.timeout_seconds:
                operation = self.pending_operation
                is_mutating = operation.command.startswith(self.MUTATING_PREFIXES)
                self.steps[operation.name] = (
                    StepState.UNKNOWN if is_mutating else StepState.RETRYABLE
                )
                if is_mutating:
                    self.requires_reconciliation = True
                    self.reconciliation_cal_status_seen = False
                    self.phase = "unknown"
                    self.last_status.clear()
                self.last_error = f"timeout:{operation.command}"
                self._log("EVENT", "error", self.last_error)
                self.pending_operation = None
        return events

    def snapshot(self) -> dict:
        return {
            "connected": self.connected,
            "phase": self.phase,
            "transport": self.transport.name,
            "requires_reconciliation": self.requires_reconciliation,
            "config": asdict(self.config),
            "steps": {key: value.value for key, value in self.steps.items()},
            "points": [asdict(point) for point in self.points],
            "fit": asdict(self.fit_result) if self.fit_result else None,
            "quality_report": (
                asdict(self.quality_report) if self.quality_report else None
            ),
            "candidate_preview": (
                asdict(self.candidate_preview) if self.candidate_preview else None
            ),
            "candidate": asdict(self.candidate) if self.candidate else None,
            "pending": asdict(self.pending_profile) if self.pending_profile else None,
            "active": asdict(self.active_profile) if self.active_profile else None,
            "export": asdict(self.export_result) if self.export_result else None,
            "export_error": self.export_error,
            "last_status": self.last_status,
            "last_error": self.last_error,
            "active_verified": self.active_verified,
            "profile_verification": self.profile_verification,
            "verified_active_sequence": self._verification_active_sequence,
            "lifecycle_verification": self.lifecycle_verification,
        }
