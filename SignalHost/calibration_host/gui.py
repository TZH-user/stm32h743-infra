"""Tk calibration workbench consuming CalibrationSession state only (UTF-8)."""

from __future__ import annotations

from datetime import datetime
import json
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk

from .artifacts import WorkflowJournal, open_review_artifact
from .calibration_plan import AdcCalibrationPreset, build_adc_42_point_plan
from .fit import FitError
from .instrument_adapter import (
    PreparedInstrumentChange,
    StructuredInstrumentAdapter,
)
from .persistence import error_context, save_points_csv, save_replay_log, save_session_json
from .session import CalibrationConfig, CalibrationSession, StepState
from .transport import ReplayTransport, SerialFake, SerialTransport
from .workflow import CalibrationWorkflow, WorkflowStage


FAKE_CONFIG = CalibrationConfig(
    reference_min_mv=-5000,
    reference_max_mv=5000,
    minimum_points=3,
    minimum_reference_span_mv=500.0,
    minimum_r_squared=0.99,
    maximum_absolute_residual_code=10.0,
    firmware_id="SERIAL_FAKE_V1",
    reference_order="ascending",
)


class CalibrationWorkbench(ttk.Frame):
    def __init__(
        self,
        master: tk.Misc,
        *,
        allow_real_serial: bool = False,
        allow_instruments: bool = False,
    ) -> None:
        super().__init__(master, padding=8)
        self.allow_real_serial = allow_real_serial
        self.allow_instruments = allow_instruments
        self.instrument_adapter = StructuredInstrumentAdapter()
        self.prepared_instruments: dict[str, PreparedInstrumentChange] = {}
        self.instrument_restore_sources: dict[str, PreparedInstrumentChange] = {}
        self.session: CalibrationSession | None = None
        self.workflow: CalibrationWorkflow | None = None
        self._restore_dialog_scheduled = False
        self.formal_preset = AdcCalibrationPreset()
        self.step_labels: dict[str, ttk.Label] = {}
        self.profile_labels: dict[str, ttk.Label] = {}
        self._last_log_count = 0
        self._build_style()
        self._build_ui()
        self.pack(fill="both", expand=True)
        self._poll_after_id = self.after(60, self._poll)

    def _build_style(self) -> None:
        style = ttk.Style(self)
        if "vista" in style.theme_names():
            style.theme_use("vista")
        style.configure("Title.TLabel", font=("Segoe UI", 15, "bold"))
        style.configure("Metric.TLabel", font=("Consolas", 10))
        style.configure("Danger.TButton", foreground="#8B1E1E")
        style.configure("Pass.TLabel", foreground="#1F6B3A", font=("Segoe UI", 10, "bold"))
        style.configure("Warn.TLabel", foreground="#9A5A00", font=("Segoe UI", 10, "bold"))

    def _build_ui(self) -> None:
        self.columnconfigure(0, weight=1)
        self.rowconfigure(2, weight=1)
        ttk.Label(self, text="SignalHost 校准工作台", style="Title.TLabel").grid(
            row=0, column=0, sticky="w", pady=(0, 6)
        )
        self._build_connection_bar().grid(row=1, column=0, sticky="ew", pady=(0, 6))
        self.notebook = ttk.Notebook(self)
        self.notebook.grid(row=2, column=0, sticky="nsew")
        self._build_workbench_tab()
        self._build_log_tab()

    def _build_connection_bar(self) -> ttk.Frame:
        frame = ttk.Frame(self)
        frame.columnconfigure(8, weight=1)
        ttk.Label(frame, text="运行模式").grid(row=0, column=0, padx=(0, 4))
        self.transport_var = tk.StringVar(value="Serial Fake")
        self.transport_box = ttk.Combobox(
            frame,
            textvariable=self.transport_var,
            values=("Serial Fake", "Replay", "Real Hardware"),
            state="readonly",
            width=22,
        )
        self.transport_box.grid(row=0, column=1, padx=(0, 8))
        ttk.Label(frame, text="Port").grid(row=0, column=2, padx=(0, 4))
        self.port_var = tk.StringVar(value="")
        ttk.Entry(frame, textvariable=self.port_var, width=12).grid(row=0, column=3, padx=(0, 8))
        ttk.Label(frame, text="Baud").grid(row=0, column=4, padx=(0, 4))
        self.baud_var = tk.StringVar(value="2000000")
        ttk.Entry(frame, textvariable=self.baud_var, width=10).grid(row=0, column=5, padx=(0, 8))
        self.connect_button = ttk.Button(frame, text="连接", command=self._connect)
        self.connect_button.grid(row=0, column=6, padx=2)
        ttk.Button(frame, text="断开", command=self._disconnect).grid(row=0, column=7, padx=2)
        for column, (label, command) in enumerate(
            (("PING", "PING"), ("STATUS", "STATUS"), ("RTOS", "RTOS"), ("CAL STATUS", "CAL STATUS")),
        ):
            ttk.Button(frame, text=label, command=lambda value=command: self._query(value)).grid(
                row=1, column=column, padx=2, pady=(4, 0)
            )
        self.connection_status = ttk.Label(frame, text="未连接", anchor="e")
        self.connection_status.grid(row=0, column=8, sticky="e", padx=(8, 0))
        self.instrument_status = ttk.Label(
            frame, text="DG4062: 未连接  DS4014E: 未连接"
        )
        self.instrument_status.grid(
            row=1, column=4, columnspan=5, sticky="e", pady=(4, 0)
        )
        return frame

    def _build_workbench_tab(self) -> None:
        tab = ttk.Frame(self.notebook, padding=6)
        tab.columnconfigure(1, weight=1)
        tab.rowconfigure(0, weight=1)
        self.notebook.add(tab, text="校准流程")

        left = ttk.Frame(tab, padding=(0, 0, 8, 0), width=250)
        left.grid(row=0, column=0, sticky="nsew")
        left.columnconfigure(0, weight=1)
        left.rowconfigure(0, weight=1)
        left_pages = ttk.Notebook(left, width=240)
        left_pages.grid(row=0, column=0, sticky="nsew")
        config_page = ttk.Frame(left_pages, padding=5)
        manual_page = ttk.Frame(left_pages, padding=5)
        left_pages.add(config_page, text="配置")
        left_pages.add(manual_page, text="手动流程")
        self._build_config(config_page)
        self._build_steps(manual_page)

        right = ttk.Notebook(tab)
        right.grid(row=0, column=1, sticky="nsew")
        self._build_workflow_tab(right)
        self._build_points_tab(right)
        self._build_fit_tab(right)
        self._build_lifecycle_tab(right)
        self._build_instrument_tab(right)
        self._build_export_tab(right)

    def _build_config(self, parent: ttk.Frame) -> None:
        box = ttk.LabelFrame(parent, text="安全配置", padding=6)
        box.pack(fill="x", pady=(0, 8))
        fields = (
            ("参考最小 mV", "reference_min", ""),
            ("参考最大 mV", "reference_max", ""),
            ("最小点数", "minimum_points", ""),
            ("最小 R²", "minimum_r2", ""),
            ("最大残差", "maximum_residual", ""),
            ("点序策略", "reference_order", ""),
        )
        self.config_vars: dict[str, tk.StringVar] = {}
        for row, (label, key, value) in enumerate(fields):
            ttk.Label(box, text=label).grid(row=row, column=0, sticky="w", pady=2)
            variable = tk.StringVar(value=value)
            self.config_vars[key] = variable
            ttk.Entry(box, textvariable=variable, width=15).grid(row=row, column=1, sticky="ew", pady=2)
        ttk.Button(box, text="应用配置", command=self._apply_config).grid(
            row=len(fields), column=0, columnspan=2, sticky="ew", pady=(5, 0)
        )
        self.config_status = ttk.Label(box, text="NOT_CONFIGURED", wraplength=210)
        self.config_status.grid(row=len(fields) + 1, column=0, columnspan=2, sticky="w", pady=(4, 0))
        ttk.Separator(box).grid(
            row=len(fields) + 2, column=0, columnspan=2, sticky="ew", pady=6
        )
        ttk.Label(
            box,
            text=(
                "正式预置 VERIFIED\n"
                "采集 +/-4.6 V / 拟合 +/-4.5 V\n"
                "42 点采集 / 38 点拟合\n"
                "DS 0.5 V/div -> 0.2 V/div\n"
                "reference 60 mV / 100 mVpp\n"
                "rail 15..4080 / R² 0.999900\n"
                "残差 8 / 滞回 4 / 帧峰峰 16 code\n"
                "来源: 2026-07-28 最终受控 HIL"
            ),
            justify="left",
            wraplength=220,
        ).grid(row=len(fields) + 3, column=0, columnspan=2, sticky="w")
        ttk.Button(box, text="取消当前等待", command=self._cancel_in_flight).grid(
            row=len(fields) + 4, column=0, columnspan=2, sticky="ew", pady=(5, 0)
        )

    def _build_steps(self, parent: ttk.Frame) -> None:
        box = ttk.LabelFrame(parent, text="引导式流程", padding=6)
        box.pack(fill="both", expand=True)
        actions = (
            ("enter", "进入独占", self._enter),
            ("start", "开始会话", self._start),
            ("point", "逐点采集", self._focus_point),
            ("finish", "结束采点", self._finish),
            ("fit", "拟合", self._fit),
            ("import", "导入候选", self._import),
            ("confirm", "确认 pending", self._confirm),
            ("apply", "应用 active", self._apply),
            ("exit", "退出独占", self._exit),
        )
        for row, (name, text, callback) in enumerate(actions):
            ttk.Button(box, text=text, command=callback, width=16).grid(
                row=row, column=0, sticky="ew", pady=2
            )
            label = ttk.Label(box, text=StepState.NOT_STARTED.value, width=14)
            label.grid(row=row, column=1, sticky="w", padx=(6, 0))
            self.step_labels[name] = label

    def _build_points_tab(self, notebook: ttk.Notebook) -> None:
        frame = ttk.Frame(notebook, padding=6)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(1, weight=1)
        notebook.add(frame, text="点表")
        controls = ttk.Frame(frame)
        controls.grid(row=0, column=0, sticky="ew", pady=(0, 5))
        ttk.Label(controls, text="参考值 (mV)").pack(side="left")
        self.reference_var = tk.StringVar()
        self.reference_entry = ttk.Entry(controls, textvariable=self.reference_var, width=14)
        self.reference_entry.pack(side="left", padx=5)
        self.point_button = ttk.Button(controls, text="发送并等待业务结果", command=self._point)
        self.point_button.pack(side="left")
        ttk.Button(controls, text="显式排除", command=self._exclude_selected).pack(side="left", padx=5)
        self.point_notice = ttk.Label(controls, text="", wraplength=420)
        self.point_notice.pack(side="left", padx=8)

        columns = (
            "index", "direction", "nominal", "reference", "measured",
            "minimum", "maximum", "flags", "crc", "included",
        )
        self.point_tree = ttk.Treeview(frame, columns=columns, show="headings", height=14)
        headings = (
            "序号", "方向", "名义 V", "DS reference V", "mean code",
            "min", "max", "flags", "CRC", "拟合",
        )
        widths = (52, 78, 72, 110, 92, 58, 58, 65, 92, 74)
        for column, heading, width in zip(columns, headings, widths, strict=True):
            self.point_tree.heading(column, text=heading)
            self.point_tree.column(column, width=width, minwidth=55, stretch=column == "time")
        self.point_tree.grid(row=1, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(frame, orient="vertical", command=self.point_tree.yview)
        scroll.grid(row=1, column=1, sticky="ns")
        self.point_tree.configure(yscrollcommand=scroll.set)

    def _build_workflow_tab(self, notebook: ttk.Notebook) -> None:
        frame = ttk.Frame(notebook, padding=8)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(4, weight=1)
        notebook.add(frame, text="42 点工作流")

        header = ttk.Frame(frame)
        header.grid(row=0, column=0, sticky="ew")
        header.columnconfigure(1, weight=1)
        self.workflow_stage = ttk.Label(
            header, text="未启动 / session=--", style="Warn.TLabel"
        )
        self.workflow_stage.grid(row=0, column=0, sticky="w")
        self.workflow_progress_text = ttk.Label(
            header, text="0.0%", style="Metric.TLabel"
        )
        self.workflow_progress_text.grid(row=0, column=2, sticky="e")
        self.workflow_progress = ttk.Progressbar(
            frame, mode="determinate", maximum=100.0
        )
        self.workflow_progress.grid(row=1, column=0, sticky="ew", pady=(5, 7))

        self.workflow_metrics = ttk.Label(
            frame,
            text=(
                "点 0/42  方向 --  target -- V  reference -- V  DG -- V  "
                "DS -- V/div  Vpp -- V  迭代 --"
            ),
            style="Metric.TLabel",
            wraplength=980,
        )
        self.workflow_metrics.grid(row=2, column=0, sticky="ew")
        self.workflow_timing = ttk.Label(
            frame,
            text="已用 0.0 s  最近 0.0 s  剩余 --  恢复 NOT_STARTED",
            wraplength=980,
        )
        self.workflow_timing.grid(row=3, column=0, sticky="ew", pady=(3, 7))

        plan_frame = ttk.Frame(frame)
        plan_frame.grid(row=4, column=0, sticky="nsew")
        plan_frame.columnconfigure(0, weight=1)
        plan_frame.rowconfigure(0, weight=1)
        columns = ("n", "direction", "target", "fit")
        self.plan_tree = ttk.Treeview(
            plan_frame, columns=columns, show="headings", height=12
        )
        for column, heading, width in (
            ("n", "n", 50),
            ("direction", "方向", 110),
            ("target", "名义目标 V", 110),
            ("fit", "纳入拟合", 90),
        ):
            self.plan_tree.heading(column, text=heading)
            self.plan_tree.column(column, width=width, stretch=column == "direction")
        self.plan_tree.grid(row=0, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(
            plan_frame, orient="vertical", command=self.plan_tree.yview
        )
        scroll.grid(row=0, column=1, sticky="ns")
        self.plan_tree.configure(yscrollcommand=scroll.set)
        for point in build_adc_42_point_plan(self.formal_preset):
            self.plan_tree.insert(
                "",
                "end",
                iid=f"plan-{point.plan_index}",
                values=(
                    point.plan_index + 1,
                    point.direction,
                    f"{point.nominal_v:g}",
                    "是" if point.included_in_fit else "rail 审计",
                ),
            )

        controls = ttk.Frame(frame)
        controls.grid(row=5, column=0, sticky="ew", pady=(8, 0))
        self.workflow_start_button = ttk.Button(
            controls, text="启动 42 点流程", command=self._start_workflow
        )
        self.workflow_start_button.pack(side="left")
        self.workflow_pause_button = ttk.Button(
            controls, text="暂停", command=self._pause_workflow
        )
        self.workflow_pause_button.pack(
            side="left", padx=4
        )
        self.workflow_resume_button = ttk.Button(
            controls, text="继续", command=self._resume_workflow
        )
        self.workflow_resume_button.pack(side="left")
        self.workflow_safe_stop_button = ttk.Button(
            controls,
            text="安全停止并退出",
            command=self._safe_stop_workflow,
            style="Danger.TButton",
        )
        self.workflow_safe_stop_button.pack(side="left", padx=4)
        self.workflow_start_button.state(["disabled"])
        self.workflow_pause_button.state(["disabled"])
        self.workflow_resume_button.state(["disabled"])
        self.workflow_safe_stop_button.state(["disabled"])
        self.review_button = ttk.Button(
            controls, text="人工审计通过", command=self._approve_review
        )
        self.review_button.pack(side="left", padx=4)
        ttk.Button(
            controls, text="打开 REVIEW", command=self._open_review
        ).pack(side="right")
        self.workflow_block = ttk.Label(
            frame, text="", style="Warn.TLabel", wraplength=980
        )
        self.workflow_block.grid(row=6, column=0, sticky="ew", pady=(5, 0))
        frame.bind("<Configure>", self._resize_workflow_labels)

    def _resize_workflow_labels(self, event: tk.Event) -> None:
        wrap = max(360, int(event.width) - 28)
        self.workflow_metrics.configure(wraplength=wrap)
        self.workflow_timing.configure(wraplength=wrap)
        self.workflow_block.configure(wraplength=wrap)

    def _build_fit_tab(self, notebook: ttk.Notebook) -> None:
        frame = ttk.Frame(notebook, padding=6)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(1, weight=1)
        notebook.add(frame, text="拟合与残差")
        self.fit_metrics = ttk.Label(
            frame,
            text="gain=--  offset=--  R²=--  max|residual|=--  points=--  result=NOT_CONFIGURED",
            style="Metric.TLabel",
            wraplength=900,
        )
        self.fit_metrics.grid(row=0, column=0, sticky="ew", pady=(0, 5))
        self.plot_canvas = tk.Canvas(frame, background="#FAFAFA", highlightthickness=1, highlightbackground="#B8B8B8")
        self.plot_canvas.grid(row=1, column=0, sticky="nsew")
        self.plot_canvas.bind("<Configure>", lambda _event: self._draw_plot())

    def _build_lifecycle_tab(self, notebook: ttk.Notebook) -> None:
        frame = ttk.Frame(notebook, padding=6)
        for column in range(3):
            frame.columnconfigure(column, weight=1, uniform="profile")
        notebook.add(frame, text="Profile 生命周期")
        for column, key in enumerate(("candidate", "pending", "active")):
            section = ttk.LabelFrame(frame, text=key.upper(), padding=8)
            section.grid(row=0, column=column, sticky="nsew", padx=4)
            label = ttk.Label(section, text="无", justify="left", wraplength=250)
            label.pack(fill="both", expand=True)
            self.profile_labels[key] = label
        controls = ttk.Frame(frame)
        controls.grid(row=1, column=0, columnspan=3, sticky="ew", pady=(10, 0))
        ttk.Button(controls, text="取消本地 candidate", command=self._cancel_candidate).pack(side="left")
        ttk.Button(controls, text="CONFIRM", command=self._confirm).pack(side="left", padx=5)
        ttk.Button(controls, text="APPLY", command=self._apply, style="Danger.TButton").pack(side="left")
        self.undo_button = ttk.Button(
            controls,
            text="UNDO",
            command=self._undo,
            style="Danger.TButton",
        )
        self.undo_button.pack(side="right")
        dac = ttk.Treeview(
            frame,
            columns=("channel", "gain", "offset", "clamp", "step", "monotonic", "plateau", "response"),
            show="headings",
            height=2,
        )
        dac.grid(row=2, column=0, columnspan=3, sticky="ew", pady=(12, 0))
        for column, heading in (
            ("channel", "DAC"), ("gain", "gain"), ("offset", "offset"),
            ("clamp", "限幅"), ("step", "最大步进"), ("monotonic", "单调性"),
            ("plateau", "平台"), ("response", "无响应"),
        ):
            dac.heading(column, text=heading)
            dac.column(column, width=100, minwidth=70, stretch=True)
        for channel in ("DAC1", "DAC2"):
            dac.insert("", "end", values=(channel,) + ("NOT_PROVIDED",) * 7)

    def _build_instrument_tab(self, notebook: ttk.Notebook) -> None:
        frame = ttk.Frame(notebook, padding=8)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(2, weight=1)
        notebook.add(frame, text="仪器联动")

        controls = ttk.Frame(frame)
        controls.grid(row=0, column=0, sticky="ew")
        self.dg_offset_var = tk.StringVar(value="0.020")
        self.ds_scale_var = tk.StringVar(value="0.5")
        ttk.Label(controls, text="DG CH1 offset V").pack(side="left")
        ttk.Entry(
            controls, textvariable=self.dg_offset_var, width=10
        ).pack(side="left", padx=(4, 8))
        ttk.Button(
            controls, text="DG 状态", command=lambda: self._instrument_read("dg4062")
        ).pack(side="left")
        ttk.Button(
            controls, text="DG prepare", command=self._prepare_dg
        ).pack(side="left", padx=4)
        ttk.Button(
            controls, text="DG apply", command=lambda: self._apply_instrument("dg4062")
        ).pack(side="left")
        ttk.Separator(controls, orient="vertical").pack(
            side="left", fill="y", padx=10
        )
        ttk.Label(controls, text="DS CH1 V/div").pack(side="left")
        ttk.Entry(
            controls, textvariable=self.ds_scale_var, width=8
        ).pack(side="left", padx=(4, 8))
        ttk.Button(
            controls, text="DS 状态", command=lambda: self._instrument_read("ds4014e")
        ).pack(side="left")
        ttk.Button(
            controls, text="DS prepare", command=self._prepare_ds
        ).pack(side="left", padx=4)
        ttk.Button(
            controls, text="DS apply", command=lambda: self._apply_instrument("ds4014e")
        ).pack(side="left")

        self.instrument_gate = ttk.Label(
            frame,
            text=(
                "进程门禁: "
                + ("ENABLED" if self.allow_instruments else "DISABLED")
            ),
            style="Warn.TLabel",
        )
        self.instrument_gate.grid(row=1, column=0, sticky="ew", pady=(7, 5))
        self.instrument_text = tk.Text(
            frame, wrap="word", font=("Consolas", 9), height=18
        )
        self.instrument_text.grid(row=2, column=0, sticky="nsew")
        self.instrument_text.insert(
            "1.0",
            "DG4062 CH1 / DS4014E CH1\nstatus -> prepare -> confirm -> apply -> readback\n",
        )
        self.instrument_text.configure(state="disabled")

    def _build_export_tab(self, notebook: ttk.Notebook) -> None:
        frame = ttk.Frame(notebook, padding=6)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(2, weight=1)
        notebook.add(frame, text="会话导出")

        controls = ttk.Frame(frame)
        controls.grid(row=0, column=0, sticky="ew", pady=(0, 5))
        ttk.Label(controls, text="Sequence").pack(side="left")
        self.export_sequence_var = tk.StringVar(value="LATEST")
        ttk.Entry(controls, textvariable=self.export_sequence_var, width=16).pack(
            side="left", padx=5
        )
        self.export_button = ttk.Button(
            controls,
            text="读取导出",
            command=self._request_export,
        )
        self.export_button.pack(side="left")
        self.export_status = ttk.Label(frame, text="尚未导出", wraplength=900)
        self.export_status.grid(row=1, column=0, sticky="ew", pady=(0, 5))

        columns = ("index", "reference", "measured", "frequency", "phase", "flags", "crc")
        self.export_tree = ttk.Treeview(frame, columns=columns, show="headings", height=14)
        headings = ("索引", "reference", "measured", "频率 Hz", "phase deg", "flags", "POINT CRC")
        widths = (55, 110, 110, 95, 95, 90, 100)
        for column, heading, width in zip(columns, headings, widths, strict=True):
            self.export_tree.heading(column, text=heading)
            self.export_tree.column(column, width=width, minwidth=50, stretch=True)
        self.export_tree.grid(row=2, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(frame, orient="vertical", command=self.export_tree.yview)
        scroll.grid(row=2, column=1, sticky="ns")
        self.export_tree.configure(yscrollcommand=scroll.set)

    def _build_log_tab(self) -> None:
        frame = ttk.Frame(self.notebook, padding=6)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(1, weight=1)
        self.notebook.add(frame, text="原始日志")
        controls = ttk.Frame(frame)
        controls.grid(row=0, column=0, sticky="ew", pady=(0, 5))
        self.log_filter = tk.StringVar(value="ALL")
        ttk.Combobox(
            controls,
            textvariable=self.log_filter,
            values=("ALL", "TX", "RX", "EVENT", "业务事件"),
            state="readonly",
            width=12,
        ).pack(side="left")
        ttk.Button(controls, text="刷新", command=self._refresh_logs).pack(side="left", padx=4)
        ttk.Button(controls, text="保存日志", command=self._save_logs).pack(side="left", padx=4)
        ttk.Button(controls, text="加载回放", command=self._load_replay).pack(side="left", padx=4)
        ttk.Button(controls, text="复制错误上下文", command=self._copy_error_context).pack(side="left", padx=4)
        ttk.Button(controls, text="导出 CSV/JSON", command=self._export).pack(side="right")
        self.log_text = tk.Text(frame, wrap="none", font=("Consolas", 9), undo=False)
        self.log_text.grid(row=1, column=0, sticky="nsew")
        vertical = ttk.Scrollbar(frame, orient="vertical", command=self.log_text.yview)
        vertical.grid(row=1, column=1, sticky="ns")
        horizontal = ttk.Scrollbar(frame, orient="horizontal", command=self.log_text.xview)
        horizontal.grid(row=2, column=0, sticky="ew")
        self.log_text.configure(yscrollcommand=vertical.set, xscrollcommand=horizontal.set)

    def _require_session(self) -> CalibrationSession:
        if self.session is None or not self.session.connected:
            raise RuntimeError("请先连接 Serial Fake 或已授权的真实串口")
        return self.session

    def _connect(self) -> None:
        try:
            if self.transport_var.get() == "Serial Fake":
                session = CalibrationSession(SerialFake(chunk_size=11), config=FAKE_CONFIG)
                self._load_config_fields(FAKE_CONFIG)
            elif self.transport_var.get() == "Replay":
                self._open_review()
                return
            else:
                port = self.port_var.get().strip()
                config = self._config_from_fields()
                config.validate()
                session = CalibrationSession(
                    SerialTransport(
                        port,
                        baudrate=int(self.baud_var.get()),
                        allow_real_serial=self.allow_real_serial,
                    ),
                    config=config,
                )
            session.connect()
            self.session = session
            self.workflow = None
            self.connection_status.configure(text=f"已连接: {session.transport.name} / 默认只读")
            self._last_log_count = 0
            self._query("OUTPUT LOG")
        except Exception as exc:
            messagebox.showerror("连接失败", str(exc), parent=self)

    def _disconnect(self) -> None:
        if self.session is not None:
            self.session.disconnect()
        self.connection_status.configure(text="未连接")
        self.workflow = None
        self._refresh()

    def _start_workflow(self) -> None:
        try:
            session = self._require_session()
            if self.workflow is not None and self.workflow.active:
                raise RuntimeError(
                    "42 点流程已在运行；请使用“暂停”或“安全停止并退出”"
                )
            if session.phase != "idle":
                raise RuntimeError(
                    f"当前会话 phase={session.phase}，必须先安全退出到 idle"
                )
            if session.pending_operation is not None:
                raise RuntimeError(
                    f"仍在等待命令：{session.pending_operation.command}"
                )
            if session.points:
                raise RuntimeError(
                    "当前会话保留着已采点数据；请先导出，再断开并重新连接后启动新流程"
                )
            stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            journal = WorkflowJournal(
                Path(__file__).resolve().parents[1]
                / "build"
                / f"calibration_workflow_{stamp}_UTF-8.jsonl"
            )
            workflow = CalibrationWorkflow(
                session, preset=self.formal_preset, journal=journal
            )
            workflow.start()
            self.workflow = workflow
            self._restore_dialog_scheduled = False
            self._refresh_workflow()
        except Exception as exc:
            messagebox.showerror("流程未启动", str(exc), parent=self)

    def _require_instrument_gate(self) -> None:
        if not self.allow_instruments:
            raise PermissionError(
                "instrument control is disabled; start with --allow-instruments"
            )

    def _show_instrument_result(self, title: str, payload: object) -> None:
        self.instrument_text.configure(state="normal")
        self.instrument_text.delete("1.0", "end")
        self.instrument_text.insert(
            "1.0",
            f"{title}\n{json.dumps(payload, ensure_ascii=False, indent=2)}",
        )
        self.instrument_text.configure(state="disabled")

    def _instrument_read(self, device: str) -> None:
        def action() -> None:
            self._require_instrument_gate()
            result = self.instrument_adapter.status(device)
            self._show_instrument_result(f"{device} status", result)
            self.instrument_status.configure(text=f"{device}: 状态正常")

        self._call(action)

    def _prepare_dg(self) -> None:
        def action() -> None:
            self._require_instrument_gate()
            self.instrument_adapter.status("dg4062")
            prepared = self.instrument_adapter.prepare_dg4062_offset(
                float(self.dg_offset_var.get())
            )
            self.prepared_instruments["dg4062"] = prepared
            self._show_instrument_result(
                "DG4062 CH1 prepared",
                {
                    "current_state": prepared.current_state,
                    "proposed_state": prepared.proposed_state,
                    "changes_real_output": prepared.changes_real_output,
                    "confirmation_required": prepared.confirmation_required,
                },
            )

        self._call(action)

    def _prepare_ds(self) -> None:
        def action() -> None:
            self._require_instrument_gate()
            self.instrument_adapter.status("ds4014e")
            prepared = self.instrument_adapter.prepare_ds4014e_scale(
                float(self.ds_scale_var.get())
            )
            self.prepared_instruments["ds4014e"] = prepared
            self._show_instrument_result(
                "DS4014E CH1 prepared",
                {
                    "current_state": prepared.current_state,
                    "proposed_state": prepared.proposed_state,
                    "changes_real_output": prepared.changes_real_output,
                    "confirmation_required": prepared.confirmation_required,
                },
            )

        self._call(action)

    def _apply_instrument(self, device: str) -> None:
        try:
            self._require_instrument_gate()
            prepared = self.prepared_instruments.get(device)
            if prepared is None:
                raise RuntimeError(f"{device} has no current prepare result")
            if not messagebox.askyesno(
                "仪器写入确认",
                (
                    f"{device.upper()} CH1 将立即改变。\n"
                    f"{prepared.confirmation_required}\n"
                    "确认执行并回读？"
                ),
                parent=self,
            ):
                return
            applied, readback = self.instrument_adapter.apply(
                prepared, prepared.confirmation_required
            )
            self.instrument_restore_sources.setdefault(device, prepared)
            self.prepared_instruments.pop(device, None)
            self._show_instrument_result(
                f"{device} apply/readback",
                {"apply": applied, "readback": readback},
            )
            self.instrument_status.configure(text=f"{device}: apply 已回读")
        except (ValueError, RuntimeError, PermissionError) as exc:
            messagebox.showerror("仪器操作未执行", str(exc), parent=self)

    def _pause_workflow(self) -> None:
        self._call(lambda: self._require_workflow().request_pause())

    def _resume_workflow(self) -> None:
        self._call(lambda: self._require_workflow().resume())

    def _safe_stop_workflow(self) -> None:
        try:
            workflow = self._require_workflow()
            session = self._require_session()
            if workflow.safe_stop_in_progress:
                raise RuntimeError("安全停止已在进行")
            pending = (
                "无"
                if session.pending_operation is None
                else session.pending_operation.command
            )
            if not messagebox.askyesno(
                "安全停止并退出",
                (
                    f"当前 phase={session.phase}，已完成 {len(session.points)}/42 点。\n"
                    f"在途命令：{pending}\n\n"
                    "确认后将等待当前原子命令完成，停止继续发点，发送 "
                    "CAL EXT EXIT，并保留已采数据和日志。继续吗？"
                ),
                parent=self,
            ):
                return
            workflow.request_safe_stop("gui_user")
            self._refresh()
        except (ValueError, RuntimeError, PermissionError) as exc:
            messagebox.showerror("安全停止未执行", str(exc), parent=self)

    def _complete_instrument_restore(self) -> None:
        self._restore_dialog_scheduled = False
        workflow = self.workflow
        if workflow is None or not workflow.awaiting_restore:
            return
        try:
            if not self.instrument_restore_sources:
                workflow.complete_restore("NOT_REQUIRED")
                self._refresh()
                return
            restored: dict[str, object] = {}
            for device in ("dg4062", "ds4014e"):
                original = self.instrument_restore_sources.get(device)
                if original is None:
                    continue
                if not self.instrument_adapter.can_restore(device, 1):
                    raise RuntimeError(
                        f"{device} CH1 通信不健康，禁止自动重试恢复"
                    )
                prepared = self.instrument_adapter.prepare_restore(original)
                if not messagebox.askyesno(
                    "仪器恢复确认",
                    (
                        f"{device.upper()} CH1 将恢复到本程序首次写入前状态。\n"
                        f"目标变更：{json.dumps(prepared.proposed_state, ensure_ascii=False)}\n"
                        f"{prepared.confirmation_required}\n\n"
                        "确认执行并回读吗？"
                    ),
                    parent=self,
                ):
                    workflow.complete_restore("USER_DECLINED")
                    self._refresh()
                    return
                applied, readback = self.instrument_adapter.apply_restore(
                    prepared,
                    prepared.confirmation_required,
                )
                restored[device] = {"apply": applied, "readback": readback}
                self.instrument_restore_sources.pop(device, None)
            self._show_instrument_result("安全退出：仪器恢复回读", restored)
            workflow.complete_restore("RESTORED")
        except (ValueError, RuntimeError, PermissionError) as exc:
            if workflow.awaiting_restore:
                workflow.complete_restore("FAILED")
            messagebox.showerror(
                "仪器恢复未完成",
                f"{exc}\n板端已退出独占；请保持现场并人工核对仪器。",
                parent=self,
            )
        self._refresh()

    def _approve_review(self) -> None:
        if not messagebox.askyesno(
            "人工审计",
            "确认质量报告与原始点表已审阅？本操作只生成 candidate 预览，不发送 IMPORT。",
            parent=self,
        ):
            return
        self._call(lambda: self._require_workflow().approve_review())

    def _require_workflow(self) -> CalibrationWorkflow:
        if self.workflow is None:
            raise RuntimeError("42 点工作流尚未启动")
        return self.workflow

    def _open_review(self) -> None:
        name = filedialog.askopenfilename(
            parent=self,
            title="打开只读 REVIEW",
            filetypes=(
                ("校准 artifact", "*.csv *.json *.log *.txt"),
                ("所有文件", "*.*"),
            ),
        )
        if not name:
            return
        try:
            points, report = open_review_artifact(Path(name))
            session = CalibrationSession(
                ReplayTransport([]),
                config=CalibrationConfig(
                    reference_min_mv=-5000,
                    reference_max_mv=5000,
                    minimum_points=38,
                    minimum_reference_span_mv=9000.0,
                    minimum_r_squared=self.formal_preset.minimum_r_squared,
                    maximum_absolute_residual_code=(
                        self.formal_preset.maximum_absolute_residual_code
                    ),
                    firmware_id="OFFLINE_REVIEW",
                    reference_order="bidirectional",
                ),
                mutating_enabled=False,
            )
            session.connect()
            session.phase = "review"
            session.points = points
            session.fit_result = report.fit
            session.quality_report = report
            session.steps["fit"] = StepState.SUCCESS
            self.session = session
            self.workflow = None
            self.connection_status.configure(
                text=f"只读 REVIEW: {Path(name).name}"
            )
            self._refresh()
        except Exception as exc:
            messagebox.showerror("REVIEW 打开失败", str(exc), parent=self)

    def _query(self, command: str) -> None:
        self._call(lambda: self._require_session().query(command))

    def _config_from_fields(self) -> CalibrationConfig:
        def optional_int(key: str) -> int | None:
            text = self.config_vars[key].get().strip()
            return int(text) if text else None

        def optional_float(key: str) -> float | None:
            text = self.config_vars[key].get().strip()
            return float(text) if text else None

        return CalibrationConfig(
            reference_min_mv=optional_int("reference_min"),
            reference_max_mv=optional_int("reference_max"),
            minimum_points=optional_int("minimum_points"),
            minimum_r_squared=optional_float("minimum_r2"),
            maximum_absolute_residual_code=optional_float("maximum_residual"),
            reference_order=self.config_vars["reference_order"].get().strip() or None,
        )

    def _load_config_fields(self, config: CalibrationConfig) -> None:
        mapping = {
            "reference_min": config.reference_min_mv,
            "reference_max": config.reference_max_mv,
            "minimum_points": config.minimum_points,
            "minimum_r2": config.minimum_r_squared,
            "maximum_residual": config.maximum_absolute_residual_code,
            "reference_order": config.reference_order,
        }
        for key, value in mapping.items():
            self.config_vars[key].set("" if value is None else str(value))
        self.config_status.configure(
            text="FAKE_DEMO_CONFIG" if config.firmware_id.startswith("SERIAL_FAKE") else "NOT_CONFIGURED"
        )

    def _apply_config(self) -> None:
        try:
            config = self._config_from_fields()
            config.validate()
            if self.session:
                self.session.config = config
            self.config_status.configure(text="CONFIGURED" if config.safety_configured else "NOT_CONFIGURED")
        except Exception as exc:
            messagebox.showerror("配置错误", str(exc), parent=self)

    def _cancel_in_flight(self) -> None:
        self._call(lambda: self._require_session().cancel_in_flight())

    def _enter(self) -> None:
        self._call(lambda: self._require_session().enter())

    def _start(self) -> None:
        self._call(lambda: self._require_session().start())

    def _focus_point(self) -> None:
        self.reference_entry.focus_set()

    def _point(self) -> None:
        def action() -> None:
            reference = int(self.reference_var.get())
            warnings = self._require_session().validate_reference(float(reference))
            if warnings and not messagebox.askyesno(
                "采集顺序提示", "\n".join(warnings) + "\n仍按原始顺序发送吗？", parent=self
            ):
                return
            self._require_session().point(reference)
            self.point_notice.configure(text="等待板端最终 [CAL] EXT 业务行")

        self._call(action)

    def _finish(self) -> None:
        self._call(lambda: self._require_session().finish())

    def _fit(self) -> None:
        self._call(lambda: self._require_session().fit(sort_copy=True))

    def _import(self) -> None:
        session = self._require_session()
        if session.fit_result is None or session.fit_result.pass_state != "PASS":
            state = "无拟合结果" if session.fit_result is None else session.fit_result.pass_state
            messagebox.showerror("导入被阻止", f"拟合状态为 {state}，仅 PASS 可导入", parent=self)
            return
        self._call(session.import_candidate)

    def _cancel_candidate(self) -> None:
        self._call(lambda: self._require_session().cancel_candidate())

    def _confirm(self) -> None:
        if not messagebox.askyesno(
            "确认 pending", "仅将 candidate 持久写为 pending，不改变 active。继续吗？", parent=self
        ):
            return
        self._call(lambda: self._require_session().confirm())

    def _apply(self) -> None:
        session = self._require_session()
        if not session.config.safety_configured:
            messagebox.showerror("APPLY 被阻止", "安全配置为 NOT_CONFIGURED", parent=self)
            return
        if not messagebox.askyesno(
            "第一次确认", "APPLY 将请求板端提交 pending。继续吗？", parent=self
        ):
            return
        if not messagebox.askyesno(
            "第二次确认", "成功后该 profile 将跨重启生效。确认发送 CAL APPLY？", parent=self
        ):
            return
        self._call(session.apply)

    def _undo(self) -> None:
        try:
            session = self._require_session()
        except RuntimeError as exc:
            messagebox.showerror("操作未执行", str(exc), parent=self)
            return
        if not messagebox.askyesno(
            "第一次确认",
            "UNDO 将请求板端把 previous profile 持久回滚为 active。继续吗？",
            parent=self,
        ):
            return
        if not messagebox.askyesno(
            "第二次确认",
            "该操作会改变跨重启生效的 active profile。确认发送 CAL UNDO？",
            parent=self,
        ):
            return
        self._call(session.undo)

    def _request_export(self) -> None:
        def request() -> None:
            text = self.export_sequence_var.get().strip()
            sequence = None if not text or text.upper() == "LATEST" else int(text, 10)
            self._require_session().export(sequence)

        self._call(request)

    def _exit(self) -> None:
        self._call(lambda: self._require_session().exit())

    def _exclude_selected(self) -> None:
        selected = self.point_tree.selection()
        if not selected:
            return
        reason = simpledialog.askstring("排除原因", "请输入排除原因（原始数据仍保留）：", parent=self)
        if not reason:
            return
        original_index = int(self.point_tree.item(selected[0], "values")[0])
        self._call(lambda: self._require_session().exclude_point(original_index, reason))

    def _call(self, action) -> None:
        try:
            action()
            self._refresh()
        except (ValueError, RuntimeError, FitError, PermissionError) as exc:
            messagebox.showerror("操作未执行", str(exc), parent=self)

    def _poll(self) -> None:
        if self.workflow is not None and self.workflow.active:
            self.workflow.tick()
            self._refresh()
            if (
                self.workflow.awaiting_restore
                and not self._restore_dialog_scheduled
            ):
                self._restore_dialog_scheduled = True
                self.after_idle(self._complete_instrument_restore)
        elif self.session is not None and self.session.connected:
            self.session.poll()
            self._refresh()
        self._poll_after_id = self.after(60, self._poll)

    def destroy(self) -> None:
        after_id = getattr(self, "_poll_after_id", None)
        if after_id is not None:
            try:
                self.after_cancel(after_id)
            except tk.TclError:
                pass
            self._poll_after_id = None
        super().destroy()

    def _refresh(self) -> None:
        session = self.session
        if session is None:
            return
        for name, label in self.step_labels.items():
            label.configure(text=session.steps.get(name, StepState.NOT_STARTED).value)
        if session.requires_reconciliation:
            self.connection_status.configure(text="结果未知：仅允许只读复核")
        undo_ready = (
            session.connected
            and session.pending_operation is None
            and session.last_status.get("applied") == 1
            and not session.requires_reconciliation
        )
        self.undo_button.state(["!disabled"] if undo_ready else ["disabled"])
        workflow_active = self.workflow is not None and self.workflow.active
        start_ready = (
            session.connected
            and session.phase == "idle"
            and session.pending_operation is None
            and not session.requires_reconciliation
            and not workflow_active
            and not session.points
        )
        safe_stop_ready = (
            session.connected
            and self.workflow is not None
            and not self.workflow.safe_stop_in_progress
            and (workflow_active or session.phase != "idle")
        )
        pause_ready = (
            workflow_active
            and self.workflow is not None
            and self.workflow.state.stage == WorkflowStage.COLLECT
            and not self.workflow.state.paused
            and not self.workflow.safe_stop_in_progress
        )
        resume_ready = (
            workflow_active
            and self.workflow is not None
            and self.workflow.state.paused
            and not self.workflow.safe_stop_in_progress
        )
        self.workflow_start_button.state(
            ["!disabled"] if start_ready else ["disabled"]
        )
        self.workflow_safe_stop_button.state(
            ["!disabled"] if safe_stop_ready else ["disabled"]
        )
        self.workflow_pause_button.state(
            ["!disabled"] if pause_ready else ["disabled"]
        )
        self.workflow_resume_button.state(
            ["!disabled"] if resume_ready else ["disabled"]
        )
        self._refresh_workflow()
        self._refresh_points()
        self._refresh_profiles()
        self._refresh_export()
        self._refresh_logs()
        self._draw_plot()

    def _refresh_points(self) -> None:
        if self.session is None:
            return
        self.point_tree.delete(*self.point_tree.get_children())
        for point in self.session.points:
            self.point_tree.insert(
                "",
                "end",
                values=(
                    point.original_index,
                    point.direction or "--",
                    "--" if point.nominal_v is None else f"{point.nominal_v:g}",
                    f"{point.voltage:.9g}",
                    f"{point.measured_code:g}",
                    "--" if point.minimum_code is None else point.minimum_code,
                    "--" if point.maximum_code is None else point.maximum_code,
                    f"0x{point.flags:X}",
                    point.crc,
                    "是" if point.included else f"否: {point.exclusion_reason}",
                ),
            )
        if self.session.pending_operation and self.session.pending_operation.name == "point":
            self.point_button.state(["disabled"])
        else:
            self.point_button.state(["!disabled"])

    def _refresh_profiles(self) -> None:
        if self.session is None:
            return
        for key, profile in (
            ("candidate", self.session.candidate or self.session.candidate_preview),
            ("pending", self.session.pending_profile),
            ("active", self.session.active_profile),
        ):
            text = "无" if profile is None else (
                f"sequence: {profile.sequence}\nsource: {profile.source}\n"
                f"model: {profile.model}\nquality: {profile.quality}\nCRC: {profile.crc32}"
            )
            self.profile_labels[key].configure(text=text)

    def _refresh_workflow(self) -> None:
        workflow = self.workflow
        phase = "--" if self.session is None else self.session.phase
        if workflow is None:
            if self.session is not None and self.session.quality_report is not None:
                report = self.session.quality_report
                self.workflow_stage.configure(
                    text=(
                        "离线 REVIEW / "
                        + ("PASS" if report.quality_pass else "FAIL")
                        + f" / session={phase}"
                    ),
                    style=(
                        "Pass.TLabel" if report.quality_pass else "Warn.TLabel"
                    ),
                )
                self.workflow_block.configure(
                    text=(
                        f"只读；CRC {report.crc32}；"
                        f"修正滞回 {report.maximum_corrected_hysteresis_code:.6g} code"
                    )
                )
            else:
                self.workflow_stage.configure(
                    text=f"未启动 / session={phase}",
                    style="Warn.TLabel",
                )
            return
        state = workflow.state
        self.workflow_stage.configure(
            text=f"{state.stage.value} / {state.status} / session={phase}",
            style="Pass.TLabel" if state.status == "PASS" else "Warn.TLabel",
        )
        self.workflow_progress.configure(value=state.progress_percent)
        self.workflow_progress_text.configure(
            text=f"{state.progress_percent:.1f}%"
        )
        self.workflow_metrics.configure(
            text=(
                f"点 {state.completed_points}/{state.total_points}  "
                f"方向 {state.current_direction or '--'}  "
                f"target {state.current_nominal_v if state.current_nominal_v is not None else '--'} V  "
                f"reference {state.current_reference_v if state.current_reference_v is not None else '--'} V  "
                f"DG {state.current_dg_offset_v if state.current_dg_offset_v is not None else '--'} V  "
                f"DS {state.current_ds_scale_v_div if state.current_ds_scale_v_div is not None else '--'} V/div  "
                f"Vpp {state.current_reference_vpp if state.current_reference_vpp is not None else '--'} V  "
                f"迭代 {state.closed_loop_iterations if state.closed_loop_iterations is not None else '--'}"
            )
        )
        eta = (
            "--"
            if state.estimated_remaining_seconds is None
            else f"{state.estimated_remaining_seconds:.1f} s"
        )
        self.workflow_timing.configure(
            text=(
                f"已用 {state.elapsed_seconds:.1f} s  "
                f"最近 {state.last_step_seconds:.1f} s  "
                f"剩余 {eta}  恢复 {state.restore_status}"
            )
        )
        self.workflow_block.configure(text=state.block_reason)
        for index in range(len(workflow.plan)):
            iid = f"plan-{index}"
            if self.plan_tree.exists(iid):
                self.plan_tree.item(
                    iid,
                    tags=("done",) if index < state.completed_points else (),
                )
        self.plan_tree.tag_configure("done", foreground="#1F6B3A")

    def _refresh_export(self) -> None:
        if self.session is None:
            return
        self.export_tree.delete(*self.export_tree.get_children())
        result = self.session.export_result
        if result is None:
            status = self.session.export_error or "尚未导出"
            self.export_status.configure(text=status)
            return
        self.export_status.configure(
            text=(
                f"sequence={result.sequence}  schema={result.schema}  mode={result.mode}  "
                f"source={result.source}  item={result.item}  points={result.declared_points}  "
                f"session CRC={result.session_crc32}  finish CRC={result.finish_crc32}"
            )
        )
        for point in result.points:
            self.export_tree.insert(
                "",
                "end",
                values=(
                    point.index,
                    f"{point.reference:.9g}",
                    f"{point.measured:.9g}",
                    point.frequency_hz,
                    f"{point.phase_deg:.9g}",
                    f"{point.flags:08X}",
                    point.crc32,
                ),
            )

    def _refresh_logs(self) -> None:
        if self.session is None:
            return
        selected_filter = self.log_filter.get()
        entries = self.session.logs
        self.log_text.configure(state="normal")
        self.log_text.delete("1.0", "end")
        for entry in entries:
            if selected_filter not in {"ALL", entry.direction}:
                if not (selected_filter == "业务事件" and entry.category in {"business", "lifecycle", "fit"}):
                    continue
            self.log_text.insert(
                "end", f"{entry.timestamp} {entry.direction:<5} {entry.category:<10} {entry.text}\n"
            )
        self.log_text.configure(state="disabled")
        self.log_text.see("end")
        self._last_log_count = len(entries)

    def _draw_plot(self) -> None:
        canvas = self.plot_canvas
        canvas.delete("all")
        width = max(canvas.winfo_width(), 320)
        height = max(canvas.winfo_height(), 220)
        margin = 45
        first_split = height // 3
        second_split = 2 * height // 3
        canvas.create_line(margin, first_split, width - margin, first_split, fill="#888")
        canvas.create_line(margin, second_split, width - margin, second_split, fill="#888")
        canvas.create_line(margin, margin, margin, height - margin, fill="#555")
        canvas.create_text(margin + 5, 14, text="拟合散点 / 直线", anchor="w", fill="#222")
        canvas.create_text(margin + 5, first_split + 14, text="残差", anchor="w", fill="#222")
        canvas.create_text(margin + 5, second_split + 14, text="上下行修正滞回", anchor="w", fill="#222")
        if self.session is None or self.session.fit_result is None:
            canvas.create_text(width // 2, height // 3, text="尚无拟合结果", fill="#666")
            return
        result = self.session.fit_result
        residuals = result.residuals
        x_values = [item.reference_mv for item in residuals]
        y_values = [item.measured_code for item in residuals]
        if not x_values:
            return
        x_min, x_max = min(x_values), max(x_values)
        y_min, y_max = min(y_values), max(y_values)
        x_span = max(x_max - x_min, 1.0)
        y_span = max(y_max - y_min, 1.0)
        top_bottom = first_split - 12
        for item in residuals:
            x = margin + (item.reference_mv - x_min) / x_span * (width - 2 * margin)
            y = top_bottom - (item.measured_code - y_min) / y_span * (top_bottom - margin)
            canvas.create_oval(x - 3, y - 3, x + 3, y + 3, fill="#176B87", outline="")
        predicted_first = result.gain_code_per_volt * (x_min / 1000.0) + result.offset_code
        predicted_last = result.gain_code_per_volt * (x_max / 1000.0) + result.offset_code
        y1 = top_bottom - (predicted_first - y_min) / y_span * (top_bottom - margin)
        y2 = top_bottom - (predicted_last - y_min) / y_span * (top_bottom - margin)
        canvas.create_line(margin, y1, width - margin, y2, fill="#B33A3A", width=2)
        maximum = max(result.maximum_absolute_residual_code, 1.0)
        residual_center = (first_split + second_split) / 2
        for item in residuals:
            x = margin + (item.reference_mv - x_min) / x_span * (width - 2 * margin)
            y = residual_center - item.residual_code / maximum * (
                (second_split - first_split) / 2 - 20
            )
            canvas.create_rectangle(x - 2, y - 2, x + 2, y + 2, fill="#C47F00", outline="")
        pairs: dict[float, dict[str, object]] = {}
        if self.session is not None:
            for point in self.session.points:
                if point.nominal_v is None or not point.included:
                    continue
                pairs.setdefault(point.nominal_v, {})[point.direction] = point
        hysteresis: list[tuple[float, float]] = []
        for nominal_v, pair in pairs.items():
            if set(pair) != {"ascending", "descending"}:
                continue
            ascending = pair["ascending"]
            descending = pair["descending"]
            code_delta = descending.measured_code - ascending.measured_code
            reference_delta = descending.voltage - ascending.voltage
            corrected = abs(
                code_delta - result.gain_code_per_volt * reference_delta
            )
            hysteresis.append((nominal_v, corrected))
        if hysteresis:
            h_max = max(max(value for _, value in hysteresis), 1.0)
            h_center = (second_split + height - margin) / 2
            for nominal_v, value in hysteresis:
                x = margin + (
                    (nominal_v * 1000.0) - x_min
                ) / x_span * (width - 2 * margin)
                y = h_center - value / h_max * (
                    (height - margin - second_split) / 2 - 12
                )
                canvas.create_oval(
                    x - 2, y - 2, x + 2, y + 2,
                    fill="#6B4FA1", outline="",
                )
        quality = self.session.quality_report if self.session else None
        quality_text = ""
        if quality is not None:
            quality_text = (
                f"  hysteresis={quality.maximum_corrected_hysteresis_code:.9g} code"
                f"  frame_pp={quality.maximum_frame_pp_code:.9g} code"
                f"  rail={'PASS' if quality.rail_safe else 'FAIL'}"
            )
        self.fit_metrics.configure(
            text=(
                f"gain={result.gain_code_per_volt:.9g} code/V  "
                f"offset={result.offset_code:.9g} code  R²={result.r_squared:.9g}  "
                f"max|residual|={result.maximum_absolute_residual_code:.9g} code  "
                f"points={len(result.used_original_indices)}  result={result.pass_state}"
                f"{quality_text}"
            )
        )

    def _save_logs(self) -> None:
        if self.session is None:
            return
        name = filedialog.asksaveasfilename(
            parent=self,
            title="保存 UTF-8 回放日志",
            defaultextension=".log",
            initialfile="calibration_replay_UTF-8.log",
        )
        if name:
            save_replay_log(Path(name), self.session.logs)

    def _load_replay(self) -> None:
        name = filedialog.askopenfilename(parent=self, title="加载 UTF-8 回放日志")
        if not name:
            return
        try:
            session = CalibrationSession(
                ReplayTransport.from_file(name),
                config=FAKE_CONFIG,
                historical_protocol=True,
            )
            session.connect()
            self.session = session
            self.connection_status.configure(text=f"回放: {Path(name).name}")
            while session.transport.is_open and session.transport.records:
                session.poll()
            self._refresh()
        except Exception as exc:
            messagebox.showerror("回放失败", str(exc), parent=self)

    def _copy_error_context(self) -> None:
        if self.session is None:
            return
        text = error_context(self.session.logs)
        self.clipboard_clear()
        self.clipboard_append(text or "无错误上下文")

    def _export(self) -> None:
        if self.session is None:
            return
        folder = filedialog.askdirectory(parent=self, title="选择导出目录")
        if not folder:
            return
        root = Path(folder)
        save_session_json(root / "calibration_session_UTF-8.json", self.session)
        save_points_csv(root / "calibration_points_UTF-8.csv", self.session.points)


def create_application(
    *,
    allow_real_serial: bool = False,
    allow_instruments: bool = False,
) -> tuple[tk.Tk, CalibrationWorkbench]:
    root = tk.Tk()
    root.title("SignalHost 校准工作台")
    root.geometry("1280x780")
    root.minsize(960, 640)
    workbench = CalibrationWorkbench(
        root,
        allow_real_serial=allow_real_serial,
        allow_instruments=allow_instruments,
    )
    return root, workbench
