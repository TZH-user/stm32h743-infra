"""Tk GUI layout, disabled-state, and confirmation tests (UTF-8)."""

from __future__ import annotations

import tkinter as tk
import unittest
from unittest.mock import patch

from calibration_host.gui import CalibrationWorkbench
from calibration_host.instrument_adapter import PreparedInstrumentChange
from calibration_host.session import CalibrationSession
from calibration_host.transport import SerialFake, SerialTransport
from calibration_host.workflow import CalibrationWorkflow, WorkflowStage


def descendants(widget: tk.Misc):
    for child in widget.winfo_children():
        yield child
        yield from descendants(child)


class GuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.root = tk.Tk()
        cls.root.attributes("-alpha", 0.0)
        cls.root.geometry("1280x780")
        cls.workbench = CalibrationWorkbench(cls.root)
        cls.root.update()

    @classmethod
    def tearDownClass(cls) -> None:
        cls.root.destroy()

    def test_default_is_fake_and_real_serial_open_is_gated(self) -> None:
        self.assertEqual(self.workbench.transport_var.get(), "Serial Fake")
        with self.assertRaises(PermissionError):
            SerialTransport("COM3").open()

    def test_remote_undo_visibility_tracks_applied_state(self) -> None:
        matching = [
            widget for widget in descendants(self.workbench)
            if isinstance(widget, ttk_button_type())
            and str(widget.cget("text")) == "UNDO"
        ]
        self.assertEqual(len(matching), 1)
        if self.workbench.session is None or not self.workbench.session.connected:
            with patch("calibration_host.gui.messagebox.showerror"):
                self.workbench._connect()
            self.workbench.session.poll()
        self.workbench.session.last_status["applied"] = 0
        self.workbench._refresh()
        self.assertIn("disabled", matching[0].state())
        self.workbench.session.last_status["applied"] = 1
        self.workbench._refresh()
        self.assertNotIn("disabled", matching[0].state())

    def test_resize_keeps_notebook_inside_window(self) -> None:
        for geometry in ("960x640", "1440x900"):
            with self.subTest(geometry=geometry):
                self.root.geometry(geometry)
                self.root.update()
                self.assertGreater(self.workbench.notebook.winfo_width(), 500)
                self.assertGreater(self.workbench.notebook.winfo_height(), 400)
                self.assertLessEqual(
                    self.workbench.notebook.winfo_rootx() + self.workbench.notebook.winfo_width(),
                    self.root.winfo_rootx() + self.root.winfo_width(),
                )

    def test_fake_connect_and_apply_requires_two_confirmations(self) -> None:
        with patch("calibration_host.gui.messagebox.showerror"):
            self.workbench._connect()
        session = self.workbench.session
        self.assertIsNotNone(session)
        session.poll()
        self.assertTrue(session.connected)

        from calibration_host.session import Profile
        session.phase = "pending"
        session.pending_profile = Profile("NOT_PROVIDED", "test", "code=V", "PASS", "12345678", 1)
        with patch("calibration_host.gui.messagebox.askyesno", side_effect=[True, True]) as confirm:
            self.workbench._apply()
        self.assertEqual(confirm.call_count, 2)
        self.assertEqual(session.pending_operation.command, "CAL APPLY")
        session.poll()

    def test_export_latest_populates_decoded_point_table(self) -> None:
        if self.workbench.session is None or not self.workbench.session.connected:
            with patch("calibration_host.gui.messagebox.showerror"):
                self.workbench._connect()
        session = self.workbench.session
        session.poll()
        self.workbench.export_sequence_var.set("LATEST")
        self.workbench._request_export()
        session.poll()
        self.workbench._refresh()
        self.assertEqual(session.steps["export"].value, "成功")
        self.assertEqual(len(self.workbench.export_tree.get_children()), 3)
        first = self.workbench.export_tree.item(
            self.workbench.export_tree.get_children()[0], "values"
        )
        self.assertEqual(first[1], "-1")
        self.assertEqual(first[2], "-0.75")

    def test_undo_requires_two_confirmations(self) -> None:
        if self.workbench.session is None or not self.workbench.session.connected:
            with patch("calibration_host.gui.messagebox.showerror"):
                self.workbench._connect()
        session = self.workbench.session
        session.poll()
        session.transport.applied = 1
        session.transport.previous_sequence = 1
        session.last_status["applied"] = 1
        self.workbench._refresh()
        with patch("calibration_host.gui.messagebox.askyesno", side_effect=[True, True]) as confirm:
            self.workbench._undo()
        self.assertEqual(confirm.call_count, 2)
        self.assertEqual(session.pending_operation.command, "CAL UNDO")
        session.poll()

    def test_fake_workflow_binding_reaches_100_without_import(self) -> None:
        root = tk.Toplevel(self.root)
        root.attributes("-alpha", 0.0)
        root.geometry("1280x780")
        workbench = CalibrationWorkbench(root)
        root.update()
        try:
            with patch("calibration_host.gui.messagebox.showerror"):
                workbench._connect()
            workbench.session.poll()
            workbench._start_workflow()
            for _ in range(300):
                workbench.workflow.tick()
                if workbench.workflow.state.stage.value == "人工审计":
                    break
            self.assertEqual(workbench.workflow.state.completed_points, 42)
            with patch(
                "calibration_host.gui.messagebox.askyesno", return_value=True
            ):
                workbench._approve_review()
            for _ in range(20):
                workbench.workflow.tick()
                if workbench.workflow.state.progress_percent == 100.0:
                    break
            workbench._refresh()
            self.assertEqual(float(workbench.workflow_progress["value"]), 100.0)
            self.assertEqual(workbench.session.phase, "idle")
            self.assertFalse(
                any(
                    command.startswith("CAL IMPORT")
                    for command in workbench.session.transport.writes
                )
            )
        finally:
            root.destroy()

    def test_duplicate_start_is_blocked_without_replacing_workflow(self) -> None:
        root = tk.Toplevel(self.root)
        root.attributes("-alpha", 0.0)
        workbench = CalibrationWorkbench(root)
        root.update()
        try:
            with patch("calibration_host.gui.messagebox.showerror"):
                workbench._connect()
            workbench.session.poll()
            workbench._start_workflow()
            original = workbench.workflow
            workbench._refresh()
            self.assertIn("disabled", workbench.workflow_start_button.state())
            self.assertIn("session=idle", workbench.workflow_stage.cget("text"))
            with patch("calibration_host.gui.messagebox.showerror") as error:
                workbench._start_workflow()
            self.assertIs(workbench.workflow, original)
            self.assertIn("已在运行", error.call_args.args[1])
        finally:
            root.destroy()

    def test_safe_stop_button_exits_and_keeps_completed_points(self) -> None:
        root = tk.Toplevel(self.root)
        root.attributes("-alpha", 0.0)
        workbench = CalibrationWorkbench(root)
        root.update()
        try:
            with patch("calibration_host.gui.messagebox.showerror"):
                workbench._connect()
            workbench.session.poll()
            workbench._start_workflow()
            for _ in range(40):
                workbench.workflow.tick()
                if (
                    workbench.workflow.state.stage == WorkflowStage.COLLECT
                    and workbench.session.pending_operation is not None
                ):
                    break
            workbench._refresh()
            self.assertNotIn(
                "disabled", workbench.workflow_safe_stop_button.state()
            )
            with patch(
                "calibration_host.gui.messagebox.askyesno", return_value=True
            ):
                workbench._safe_stop_workflow()
            for _ in range(20):
                workbench.workflow.tick()
                if not workbench.workflow.active:
                    break
            completed = len(workbench.session.points)
            workbench._refresh()
            self.assertGreaterEqual(completed, 1)
            self.assertEqual(workbench.session.phase, "idle")
            self.assertEqual(workbench.workflow.state.status, "ABORTED")
            self.assertIn("session=idle", workbench.workflow_stage.cget("text"))
        finally:
            root.destroy()

    def test_real_mode_safe_stop_confirms_tracked_instrument_restore(self) -> None:
        class RealFake(SerialFake):
            name = "real-fake"

        root = tk.Toplevel(self.root)
        root.attributes("-alpha", 0.0)
        workbench = CalibrationWorkbench(root, allow_instruments=True)
        session = CalibrationSession(RealFake())
        session.connect()
        workflow = CalibrationWorkflow(session)
        workflow.start()
        workbench.session = session
        workbench.workflow = workflow
        original = PreparedInstrumentChange(
            device="dg4062",
            channel=1,
            prepare_action="dg4062-prepare",
            apply_action="dg4062-apply",
            arguments=(),
            confirmation_required="ORIGINAL",
            current_state={"offset_v": 0.02},
            proposed_state={"offset_v": 1.0},
            changes_real_output=True,
        )
        restore = PreparedInstrumentChange(
            device="dg4062",
            channel=1,
            prepare_action="dg4062-prepare",
            apply_action="dg4062-apply",
            arguments=(),
            confirmation_required="RESTORE_EXACT",
            current_state={"offset_v": 1.0},
            proposed_state={"offset_v": 0.02},
            changes_real_output=True,
        )
        workbench.instrument_restore_sources["dg4062"] = original
        workflow.request_safe_stop()
        workflow.tick()
        self.assertTrue(workflow.awaiting_restore)
        try:
            with (
                patch.object(
                    workbench.instrument_adapter,
                    "can_restore",
                    return_value=True,
                ),
                patch.object(
                    workbench.instrument_adapter,
                    "prepare_restore",
                    return_value=restore,
                ),
                patch.object(
                    workbench.instrument_adapter,
                    "apply_restore",
                    return_value=({"applied": True}, {"ok": True}),
                ) as apply_restore,
                patch(
                    "calibration_host.gui.messagebox.askyesno",
                    return_value=True,
                ),
            ):
                workbench._complete_instrument_restore()
            apply_restore.assert_called_once_with(restore, "RESTORE_EXACT")
            self.assertEqual(workflow.state.status, "ABORTED")
            self.assertEqual(workflow.state.restore_status, "RESTORED")
        finally:
            root.destroy()

    def test_instrument_gate_is_disabled_by_default(self) -> None:
        with (
            patch.object(
                self.workbench.instrument_adapter, "status"
            ) as status,
            patch("calibration_host.gui.messagebox.showerror") as error,
        ):
            self.workbench._instrument_read("dg4062")
        status.assert_not_called()
        error.assert_called_once()


def ttk_button_type():
    from tkinter import ttk

    return ttk.Button


if __name__ == "__main__":
    unittest.main()
