"""Protocol parser tests (UTF-8)."""

from __future__ import annotations

import math
import unittest

from calibration_host.protocol import (
    IncrementalLineParser,
    StatusV3Collector,
    binary32_from_hex,
    export_point_crc32,
    parse_protocol_line,
)


POINT_LINE = (
    "[CAL] EXT n=2 ref_mv=-1000 mean=123.456 min=120 max=127 "
    "flags=0x2 crc=89ABCDEF"
)


class ProtocolParserTests(unittest.TestCase):
    def test_split_packet(self) -> None:
        parser = IncrementalLineParser()
        self.assertEqual(parser.feed(b"[CTRL] PO"), [])
        events = parser.feed(b"NG\r\n")
        self.assertEqual([event.kind for event in events], ["pong"])

    def test_sticky_packets_and_line_endings(self) -> None:
        parser = IncrementalLineParser()
        events = parser.feed(b"[CTRL] PONG\r[CTRL] PONG\n[CTRL] PONG\r\n")
        self.assertEqual([event.kind for event in events], ["pong", "pong", "pong"])

    def test_malformed_encoding(self) -> None:
        event = IncrementalLineParser().feed(b"[CTRL] P\xffNG\n")[0]
        self.assertEqual(event.kind, "malformed_encoding")

    def test_oversized_line(self) -> None:
        parser = IncrementalLineParser(maximum_line_bytes=16)
        events = parser.feed(b"01234567890123456789\r\n[CTRL] PONG\n")
        self.assertEqual([event.kind for event in events], ["line_too_long", "pong"])

    def test_unknown_and_duplicate_lines_are_preserved(self) -> None:
        parser = IncrementalLineParser()
        events = parser.feed(b"vendor trace\nvendor trace\n")
        self.assertEqual([event.kind for event in events], ["unknown", "unknown"])
        self.assertEqual(events[0].raw, events[1].raw)

    def test_calibration_point_fields(self) -> None:
        event = parse_protocol_line(POINT_LINE)
        self.assertEqual(event.kind, "cal_ext_point")
        self.assertEqual(event.fields["n"], 2)
        self.assertEqual(event.fields["ref_mv"], -1000)
        self.assertAlmostEqual(event.fields["mean"], 123.456)
        self.assertEqual(event.fields["crc"], "89ABCDEF")

    def test_status_fields(self) -> None:
        event = parse_protocol_line(
            "[CAL] STATUS v=3 state=5 error=0 exclusive=1 can_start=0 future=9"
        )
        self.assertEqual(event.kind, "cal_status")
        self.assertEqual(event.fields["state"], 5)
        self.assertEqual(event.fields["section"], "CORE")
        self.assertEqual(event.fields["future"], 9)

        session = parse_protocol_line(
            "[CAL] SESSION worker_active=0 reason=none points=2/3"
        )
        self.assertEqual(session.kind, "cal_status_session")
        self.assertEqual(session.fields["completed_points"], 2)
        self.assertEqual(session.fields["expected_points"], 3)

    def test_status_v3_sections_missing_duplicate_and_extension(self) -> None:
        sections = (
            ("[CAL] SELECT mode=1 source=2 item=1 candidate=0", "cal_status_select"),
            ("[CAL] PROFILE configured=1 active=7 pending=0", "cal_status_profile"),
            (
                "[CAL] PROFILE2 previous=6 undo_available=1 enabled=1 "
                "in_use=1 applied=1",
                "cal_status_profile2",
            ),
            ("[CAL] ADC valid=1 seq=7 dc_uv=1 rms_uv=2 peak_uv=3", "cal_status_adc"),
            ("[CAL] LAST output_mv=0 mean_mc=1 min=2 max=3", "cal_last"),
            ("[CAL] IO low=0 high=0 dir=-1 samples=4096", "cal_status_io"),
            ("[CAL] HW relay=0x0 dac_code=299 adc_run=0", "cal_status_hw"),
        )
        self.assertEqual(
            [parse_protocol_line(line).kind for line, _ in sections],
            [kind for _, kind in sections],
        )
        self.assertEqual(
            parse_protocol_line(
                "[CAL] PROFILE configured=1 active=7 active=8 pending=0"
            ).kind,
            "malformed_cal_status",
        )
        self.assertEqual(
            parse_protocol_line("[CAL] ADC valid=1 seq=7 dc_uv=1 rms_uv=2").kind,
            "malformed_cal_status",
        )
        self.assertEqual(
            parse_protocol_line("[CAL] FUTURE key=value").kind,
            "cal_extension",
        )

        collector = StatusV3Collector()
        collector.feed(
            parse_protocol_line(
                "[CAL] STATUS v=3 state=5 error=0 exclusive=1 can_start=0"
            )
        )
        collector.feed(
            parse_protocol_line(
                "[CAL] SESSION worker_active=0 reason=none points=2/3"
            )
        )
        for line, _kind in sections[:-1]:
            collector.feed(parse_protocol_line(line))
        collector.feed(parse_protocol_line(sections[1][0]))
        collector.feed(parse_protocol_line("[CAL] FUTURE key=value"))
        snapshot = collector.snapshot()
        self.assertFalse(snapshot.complete)
        self.assertEqual(snapshot.missing_sections, ("HW",))
        self.assertEqual(snapshot.duplicate_sections, ("PROFILE",))
        self.assertEqual(len(snapshot.extensions), 1)

    def test_binary32_is_reinterpreted_from_exactly_eight_hex_digits(self) -> None:
        self.assertEqual(binary32_from_hex("3FC00000"), 1.5)
        self.assertEqual(binary32_from_hex("C0100000"), -2.25)
        self.assertTrue(math.isinf(binary32_from_hex("7F800000")))
        for invalid in ("3F80000", "03F800000", "0x3F800000", "GGGGGGGG"):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                binary32_from_hex(invalid)

    def test_short_export_begin_point_end_and_point_crc(self) -> None:
        begin = parse_protocol_line(
            "[CAL] EXPORT B s=7 v=1 mo=2 so=3 it=4 ep=2 np=1 c=1234ABCD"
        )
        self.assertEqual(begin.kind, "cal_export_begin")
        self.assertEqual(begin.fields["sequence"], 7)
        self.assertEqual(begin.fields["expected"], 2)
        self.assertEqual(begin.fields["points"], 1)

        crc = export_point_crc32(0x3FC00000, 0xC0100000, 1000, 0x3F000000, 1)
        point = parse_protocol_line(
            "[CAL] EXPORT P s=7 i=0 r=3FC00000 m=C0100000 f=1000 "
            f"p=3F000000 g=00000001 c={crc:08X}"
        )
        self.assertEqual(point.kind, "cal_export_point")
        self.assertEqual(point.fields["reference"], 1.5)
        self.assertEqual(point.fields["measured"], -2.25)
        self.assertEqual(point.fields["phase_deg"], 0.5)
        self.assertTrue(point.fields["crc_valid"])

        end = parse_protocol_line("[CAL] EXPORT E s=7 n=1 c=89ABCDEF")
        self.assertEqual(end.kind, "cal_export_end")
        self.assertEqual(end.fields["points"], 1)

    def test_export_point_crc_mismatch_and_truncation_are_not_valid(self) -> None:
        mismatch = parse_protocol_line(
            "[CAL] EXPORT P s=7 i=0 r=3FC00000 m=C0100000 f=1000 "
            "p=3F000000 g=00000001 c=00000000"
        )
        self.assertEqual(mismatch.kind, "cal_export_point_crc_error")
        self.assertFalse(mismatch.fields["crc_valid"])

        truncated = parse_protocol_line(
            "[CAL] EXPORT P s=7 i=0 r=3FC00000 m=C0100000 f=1000 "
            "p=3F000000 g=00000001 c=1234"
        )
        self.assertEqual(truncated.kind, "malformed_export")

    def test_legacy_export_is_historical_only_and_truncated_crc_is_malformed(self) -> None:
        legacy = (
            "[CAL] EXPORT BEGIN seq=7 schema=1 mode=2 source=3 item=4 "
            "expected=2 points=1 crc=1234ABCD"
        )
        self.assertEqual(parse_protocol_line(legacy).kind, "unknown")
        historical = parse_protocol_line(legacy, historical=True)
        self.assertEqual(historical.kind, "cal_export_begin")
        self.assertTrue(historical.fields["legacy"])
        self.assertEqual(
            parse_protocol_line(legacy[:-8] + "1234", historical=True).kind,
            "malformed_export",
        )


if __name__ == "__main__":
    unittest.main()
