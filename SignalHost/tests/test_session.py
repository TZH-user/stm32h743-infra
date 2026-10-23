"""Command state machine, fake, and lifecycle tests (UTF-8)."""

from __future__ import annotations

import unittest

from calibration_host.fit import FitResult
from calibration_host.session import CalibrationConfig, CalibrationSession, StepState
from calibration_host.transport import FakeFaults, SerialFake, TransportDisconnected


CONFIG = CalibrationConfig(
    reference_min_mv=-5000,
    reference_max_mv=5000,
    minimum_points=3,
    minimum_reference_span_mv=500,
    minimum_r_squared=0.99,
    maximum_absolute_residual_code=5,
    firmware_id="TEST_FAKE",
    reference_order="ascending",
)


def complete(session: CalibrationSession, action) -> None:
    action()
    session.poll()
    if session.pending_operation is not None:
        raise AssertionError(f"operation did not complete: {session.pending_operation.command}")


def reach_review(session: CalibrationSession) -> None:
    complete(session, session.enter)
    complete(session, session.start)
    for reference in (-1000, 0, 1000):
        complete(session, lambda value=reference: session.point(value))
    complete(session, session.finish)


class SessionTests(unittest.TestCase):
    def make_session(self, fake: SerialFake | None = None, *, clock=None) -> CalibrationSession:
        session = CalibrationSession(fake or SerialFake(chunk_size=7), config=CONFIG, clock=clock or __import__("time").monotonic)
        session.connect()
        return session

    def test_normal_flow_and_profile_lifecycle(self) -> None:
        session = self.make_session()
        reach_review(session)
        fit = session.fit(sort_copy=True)
        self.assertEqual(fit.pass_state, "PASS")
        complete(session, session.import_candidate)
        self.assertIsNotNone(session.candidate)
        complete(session, session.confirm)
        self.assertIsNone(session.candidate)
        self.assertIsNotNone(session.pending_profile)
        complete(session, session.apply)
        self.assertIsNone(session.pending_profile)
        self.assertIsNotNone(session.active_profile)
        complete(session, session.exit)
        self.assertEqual(session.phase, "idle")

    def test_export_latest_and_explicit_sequence_validate_transaction(self) -> None:
        session = self.make_session()
        complete(session, session.export)
        result = session.export_result
        self.assertIsNotNone(result)
        self.assertEqual(result.sequence, 1)
        self.assertEqual(result.declared_points, 3)
        self.assertEqual([point.index for point in result.points], [0, 1, 2])
        self.assertEqual(result.points[0].reference, -1.0)
        self.assertEqual(result.points[0].measured, -0.75)
        self.assertEqual(result.points[2].frequency_hz, 10000)
        self.assertEqual(session.transport.writes[-1], "CAL EXPORT LATEST")

        complete(session, lambda: session.export(1))
        self.assertEqual(session.export_result.sequence, 1)
        self.assertEqual(session.transport.writes[-1], "CAL EXPORT 1")

    def test_export_rejects_bad_sequence_and_crc_bad_point(self) -> None:
        session = self.make_session()
        for invalid in (0, -1, 0x1_0000_0000, 1.5, True):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                session.export(invalid)

        bad_crc = self.make_session(
            SerialFake(faults=FakeFaults(export_crc_error_at=1))
        )
        bad_crc.export()
        events = bad_crc.poll()
        self.assertIn("cal_export_point_crc_error", [event.kind for event in events])
        self.assertEqual(bad_crc.steps["export"], StepState.FAILED)
        self.assertIsNone(bad_crc.export_result)
        self.assertIn("CRC mismatch at index=1", bad_crc.export_error)

    def test_undo_is_mutating_and_completes_only_after_applied_clears(self) -> None:
        session = self.make_session()
        reach_review(session)
        session.fit()
        complete(session, session.import_candidate)
        complete(session, session.confirm)
        complete(session, session.apply)
        self.assertEqual(session.last_status["applied"], 1)

        complete(session, session.undo)
        self.assertEqual(session.transport.writes[-1], "CAL UNDO")
        self.assertEqual(session.steps["undo"], StepState.SUCCESS)
        self.assertEqual(session.last_status["applied"], 0)

    def test_apply_requires_followup_status_for_runtime_verification(self) -> None:
        session = self.make_session()
        complete(session, session.enter)
        complete(session, lambda: session.start(3))
        for reference in (-1000, 0, 1000):
            complete(session, lambda value=reference: session.point(value))
        complete(session, session.finish)
        session.fit()
        complete(session, session.import_candidate)
        complete(session, session.confirm)
        complete(session, session.apply)
        self.assertFalse(session.active_verified)
        self.assertEqual(session.profile_verification, "AWAITING_STATUS")
        complete(session, lambda: session.query("CAL STATUS"))
        self.assertTrue(session.active_verified)
        self.assertEqual(session.profile_verification, "PASS")

    def test_candidate_cancel_does_not_create_pending(self) -> None:
        session = self.make_session()
        reach_review(session)
        session.fit()
        complete(session, session.import_candidate)
        session.cancel_candidate()
        self.assertIsNone(session.candidate)
        self.assertIsNone(session.pending_profile)
        self.assertEqual(session.phase, "review")

    def test_out_of_order_and_duplicate_clicks_are_blocked(self) -> None:
        session = self.make_session()
        with self.assertRaises(RuntimeError):
            session.finish()
        session.enter()
        with self.assertRaises(RuntimeError):
            session.enter()
        with self.assertRaises(RuntimeError):
            session.start()

    def test_only_one_point_can_be_in_flight(self) -> None:
        session = self.make_session()
        complete(session, session.enter)
        complete(session, session.start)
        session.point(0)
        with self.assertRaises(RuntimeError):
            session.point(1000)
        session.poll()
        self.assertEqual(len(session.points), 1)

    def test_duplicate_final_response_does_not_duplicate_point(self) -> None:
        fake = SerialFake()
        session = self.make_session(fake)
        complete(session, session.enter)
        complete(session, session.start)
        complete(session, lambda: session.point(0))
        point_line = next(log.text for log in session.logs if log.text.startswith("[CAL] EXT n="))
        fake._queue(point_line)
        session.poll()
        self.assertEqual(len(session.points), 1)

    def test_duplicate_range_nonfinite_and_disorder_validation(self) -> None:
        session = self.make_session()
        complete(session, session.enter)
        complete(session, session.start)
        complete(session, lambda: session.point(1000))
        self.assertTrue(session.validate_reference(0))
        with self.assertRaises(ValueError):
            session.validate_reference(1000)
        with self.assertRaises(ValueError):
            session.validate_reference(6000)
        with self.assertRaises(ValueError):
            session.validate_reference(float("nan"))

    def test_queue_full_and_rejection_are_retryable(self) -> None:
        for faults in (
            FakeFaults(queue_full_for={"CAL EXT ENTER"}),
            FakeFaults(reject_for={"CAL EXT ENTER"}),
        ):
            with self.subTest(faults=faults):
                session = self.make_session(SerialFake(faults=faults))
                session.enter()
                session.poll()
                self.assertEqual(session.steps["enter"], StepState.RETRYABLE)

    def test_mutating_timeout_freezes_until_status_reconciliation(self) -> None:
        now = [10.0]
        fake = SerialFake(faults=FakeFaults(drop_response_for={"CAL EXT ENTER"}))
        session = self.make_session(fake, clock=lambda: now[0])
        session.enter()
        now[0] += 3.0
        session.poll()
        self.assertEqual(session.steps["enter"], StepState.UNKNOWN)
        self.assertTrue(session.requires_reconciliation)
        with self.assertRaises(RuntimeError):
            session.enter()
        complete(session, lambda: session.query("CAL STATUS"))
        session.acknowledge_reconciled()
        self.assertFalse(session.requires_reconciliation)

    def test_cancel_mutating_command_requires_reconciliation(self) -> None:
        fake = SerialFake(faults=FakeFaults(drop_response_for={"CAL EXT ENTER"}))
        session = self.make_session(fake)
        session.enter()
        session.cancel_in_flight()
        self.assertEqual(session.steps["enter"], StepState.UNKNOWN)
        self.assertTrue(session.requires_reconciliation)
        self.assertEqual(session.phase, "unknown")

    def test_disconnect_during_command_marks_result_unknown(self) -> None:
        fake = SerialFake(faults=FakeFaults(disconnect_for={"CAL EXT ENTER"}))
        session = self.make_session(fake)
        with self.assertRaises(TransportDisconnected):
            session.enter()
        self.assertFalse(session.connected)
        self.assertTrue(session.requires_reconciliation)
        self.assertEqual(session.steps["enter"], StepState.UNKNOWN)

    def test_wrong_crc_is_rejected_by_fake(self) -> None:
        fake = SerialFake()
        fake.open()
        fake.write(b"CAL IMPORT ADC 800000 100000 1000000 0 3 DEADBEEF\r\n")
        response = fake.read_available().decode("ascii")
        self.assertIn("[CTRL] ERR CAL IMPORT CRC", response)

    def test_import_command_uses_five_fields_and_known_crc(self) -> None:
        session = self.make_session()
        session.phase = "review"
        session.fit_result = FitResult(
            version=1,
            gain_code_per_volt=-414.002,
            offset_code=2056.898,
            r_squared=0.999981,
            maximum_absolute_residual_code=7.439,
            residuals=(),
            used_original_indices=tuple(range(38)),
            excluded_original_indices=(),
            pass_state="PASS",
        )
        session.import_candidate()
        self.assertEqual(
            session.transport.writes[-1],
            "CAL IMPORT ADC -414002 2056898 999981 7439 38 6B0AA818",
        )
        session.poll()
        self.assertEqual(session.steps["import"], StepState.SUCCESS)

    def test_confirm_and_apply_failures_do_not_advance_lifecycle(self) -> None:
        fake = SerialFake(faults=FakeFaults(reject_for={"CAL CONFIRM"}))
        session = self.make_session(fake)
        reach_review(session)
        session.fit()
        complete(session, session.import_candidate)
        session.confirm()
        session.poll()
        self.assertEqual(session.steps["confirm"], StepState.RETRYABLE)
        self.assertEqual(session.phase, "candidate")
        self.assertIsNotNone(session.candidate)

        fake.faults.reject_for = {"CAL APPLY"}
        fake.pending = 1
        session.pending_profile = session.candidate
        session.candidate = None
        session.phase = "pending"
        session.apply()
        session.poll()
        self.assertEqual(session.steps["apply"], StepState.RETRYABLE)
        self.assertIsNotNone(session.pending_profile)
        self.assertIsNone(session.active_profile)

    def test_reconnect_status_restores_pending_presence_without_inventing_profile(self) -> None:
        fake = SerialFake()
        fake.pending = 1
        fake.state = 6
        session = self.make_session(fake)
        session.requires_reconciliation = True
        session.phase = "unknown"
        complete(session, lambda: session.query("CAL STATUS"))
        self.assertEqual(session.phase, "pending_unknown_profile")
        self.assertIsNone(session.pending_profile)
        session.acknowledge_reconciled()

    def test_reconciliation_requires_fresh_cal_status_not_general_status(self) -> None:
        session = self.make_session()
        complete(session, session.enter)
        session.disconnect("link lost")
        session.connect()
        complete(session, lambda: session.query("STATUS"))
        with self.assertRaises(RuntimeError):
            session.acknowledge_reconciled()
        complete(session, lambda: session.query("CAL STATUS"))
        session.acknowledge_reconciled()


if __name__ == "__main__":
    unittest.main()
