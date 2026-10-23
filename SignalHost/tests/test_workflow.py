"""42-point workflow, pause, journal, and profile gates (UTF-8)."""

from __future__ import annotations

from pathlib import Path
import tempfile
import unittest

from calibration_host.artifacts import WorkflowJournal
from calibration_host.session import CalibrationSession
from calibration_host.transport import SerialFake
from calibration_host.workflow import CalibrationWorkflow, WorkflowStage


class WorkflowTests(unittest.TestCase):
    def test_fake_runs_42_points_to_review_then_100_without_import(self) -> None:
        with tempfile.TemporaryDirectory(
            prefix="signalhost_workflow_", dir=Path("build")
        ) as folder:
            transport = SerialFake()
            session = CalibrationSession(transport)
            session.connect()
            journal = WorkflowJournal(Path(folder) / "journal_UTF-8.jsonl")
            workflow = CalibrationWorkflow(session, journal=journal)
            workflow.start()
            for _ in range(300):
                workflow.tick()
                if workflow.state.stage == WorkflowStage.HUMAN_REVIEW:
                    break
            self.assertEqual(workflow.state.stage, WorkflowStage.HUMAN_REVIEW)
            self.assertEqual(len(session.points), 42)
            self.assertTrue(workflow.quality_report.quality_pass)
            self.assertFalse(
                any(write.startswith("CAL IMPORT") for write in transport.writes)
            )
            workflow.approve_review()
            for _ in range(20):
                workflow.tick()
                if workflow.state.stage == WorkflowStage.COMPLETE:
                    break
            self.assertEqual(workflow.state.progress_percent, 100.0)
            self.assertEqual(workflow.state.status, "PASS")
            self.assertEqual(session.phase, "idle")
            self.assertEqual(transport.writes.count("CAL EXT EXIT"), 1)
            self.assertEqual(workflow.progress_history, sorted(workflow.progress_history))
            self.assertFalse(
                any(write.startswith("CAL IMPORT") for write in transport.writes)
            )
            summary = journal.recovery_summary()
            self.assertEqual(summary["completed_points"], 42)
            self.assertFalse(summary["automatic_resume_allowed"])

    def test_pause_waits_until_point_atomic_step_finishes(self) -> None:
        session = CalibrationSession(SerialFake())
        session.connect()
        workflow = CalibrationWorkflow(session)
        workflow.start()
        for _ in range(30):
            workflow.tick()
            if (
                workflow.state.stage == WorkflowStage.COLLECT
                and session.pending_operation is not None
            ):
                break
        self.assertEqual(workflow.state.stage, WorkflowStage.COLLECT)
        self.assertIsNotNone(session.pending_operation)
        workflow.request_pause()
        workflow.tick()
        self.assertTrue(workflow.state.paused)
        self.assertIsNone(session.pending_operation)
        completed = len(session.points)
        workflow.tick()
        self.assertEqual(len(session.points), completed)
        workflow.resume()
        workflow.tick()
        self.assertIsNotNone(session.pending_operation)

    def test_safe_stop_waits_for_atomic_point_then_exits_and_preserves_data(self) -> None:
        transport = SerialFake()
        session = CalibrationSession(transport)
        session.connect()
        workflow = CalibrationWorkflow(session)
        workflow.start()
        for _ in range(40):
            workflow.tick()
            if (
                workflow.state.stage == WorkflowStage.COLLECT
                and session.pending_operation is not None
            ):
                break
        self.assertEqual(session.pending_operation.name, "point")
        workflow.request_safe_stop()
        for _ in range(20):
            workflow.tick()
            if not workflow.active:
                break
        completed = len(session.points)
        self.assertGreaterEqual(completed, 1)
        self.assertEqual(session.phase, "idle")
        self.assertEqual(workflow.state.stage, WorkflowStage.SAFE_EXIT)
        self.assertEqual(workflow.state.status, "ABORTED")
        self.assertEqual(workflow.state.restore_status, "SIMULATED")
        self.assertEqual(transport.writes.count("CAL EXT EXIT"), 1)
        for _ in range(5):
            workflow.tick()
        self.assertEqual(len(session.points), completed)

    def test_safe_stop_from_idle_does_not_send_exit(self) -> None:
        transport = SerialFake()
        session = CalibrationSession(transport)
        session.connect()
        workflow = CalibrationWorkflow(session)
        workflow.start()
        workflow.request_safe_stop()
        workflow.tick()
        self.assertEqual(workflow.state.status, "ABORTED")
        self.assertEqual(session.phase, "idle")
        self.assertNotIn("CAL EXT EXIT", transport.writes)

    def test_safe_stop_reactivates_stopped_workflow_for_non_idle_session(self) -> None:
        transport = SerialFake()
        session = CalibrationSession(transport)
        session.connect()
        session.enter()
        session.poll()
        self.assertEqual(session.phase, "ready")
        workflow = CalibrationWorkflow(session)
        self.assertFalse(workflow.active)
        workflow.request_safe_stop()
        for _ in range(10):
            workflow.tick()
            if not workflow.active:
                break
        self.assertEqual(session.phase, "idle")
        self.assertEqual(workflow.state.status, "ABORTED")
        self.assertEqual(transport.writes.count("CAL EXT EXIT"), 1)

    def test_safe_stop_reconciles_unknown_state_before_exit(self) -> None:
        transport = SerialFake()
        session = CalibrationSession(transport)
        session.connect()
        session.enter()
        session.poll()
        session.requires_reconciliation = True
        session.reconciliation_cal_status_seen = False
        session.phase = "unknown"
        workflow = CalibrationWorkflow(session)
        workflow.request_safe_stop()
        for _ in range(20):
            workflow.tick()
            if not workflow.active:
                break
        self.assertIn("CAL STATUS", transport.writes)
        self.assertEqual(session.phase, "idle")
        self.assertFalse(session.requires_reconciliation)
        self.assertEqual(workflow.state.status, "ABORTED")

    def test_offline_review_blocks_mutating_commands(self) -> None:
        session = CalibrationSession(SerialFake(), mutating_enabled=False)
        session.connect()
        with self.assertRaises(PermissionError):
            session.enter()


if __name__ == "__main__":
    unittest.main()
