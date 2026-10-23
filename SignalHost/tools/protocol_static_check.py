#!/usr/bin/env python3
"""Static contract check against signal_separation firmware sources (UTF-8)."""

from __future__ import annotations

import argparse
from pathlib import Path


DEFAULT_FIRMWARE_ROOT = Path(
    r"F:\STM32\stm32project\VSCodeCMAKEProject\signal_separation"
)


def require(text: str, needle: str, source: Path) -> None:
    if needle not in text:
        raise AssertionError(f"missing in {source}: {needle}")


def main() -> int:
    parser = argparse.ArgumentParser(description="核对 SignalHost 与固件静态协议契约")
    parser.add_argument("--firmware-root", type=Path, default=DEFAULT_FIRMWARE_ROOT)
    args = parser.parse_args()
    root = args.firmware_root.resolve()
    serial_path = root / "APP" / "RTOS" / "app_serial_control_task.c"
    board_path = root / "APP" / "Calibration" / "app_calibration_board_port.c"
    service_path = root / "APP" / "Calibration" / "app_calibration_service.c"
    serial_protocol_path = (
        root / "APP" / "Calibration" / "app_calibration_serial_protocol.h"
    )
    crc_path = root / "CalibrationCore" / "src" / "calibration_core.c"
    core_header_path = root / "CalibrationCore" / "include" / "calibration_core.h"
    usart_path = root / "Core" / "Src" / "usart.c"
    protocol_doc_path = root / "Docs" / "串口控制命令说明_20260726_UTF-8.md"

    serial = serial_path.read_text(encoding="utf-8")
    board = board_path.read_text(encoding="utf-8")
    service = service_path.read_text(encoding="utf-8")
    serial_protocol = serial_protocol_path.read_text(encoding="utf-8")
    crc = crc_path.read_text(encoding="utf-8")
    core_header = core_header_path.read_text(encoding="utf-8")
    usart = usart_path.read_text(encoding="utf-8")
    protocol_doc = protocol_doc_path.read_text(encoding="utf-8")

    for command_token in (
        "PING", "STATUS", "RTOS", "CAL", "EXT", "IMPORT", "EXPORT",
        "CONFIRM", "APPLY", "UNDO",
    ):
        require(serial, f'strcmp(command, "{command_token}")' if command_token in {"PING", "STATUS", "RTOS", "CAL"} else f'strcmp(operation, "{command_token}")', serial_path)
    for external_token in ("ENTER", "START", "POINT", "FINISH", "EXIT"):
        require(serial, f'strcmp(sub, "{external_token}")', serial_path)
    for action in (
        "APP_UI_REMOTE_ACTION_CAL_EXT_ENTER",
        "APP_UI_REMOTE_ACTION_CAL_EXT_FINISH",
        "APP_UI_REMOTE_ACTION_CAL_EXT_EXIT",
        "APP_UI_REMOTE_ACTION_CAL_CONFIRM",
        "APP_UI_REMOTE_ACTION_CAL_APPLY",
        "APP_UI_REMOTE_ACTION_CAL_UNDO",
    ):
        require(serial, action, serial_path)
    require(serial, "AppUiRemoteControl_PostExtStart", serial_path)
    require(serial, "AppUiRemoteControl_PostImportAdc", serial_path)
    require(serial, "int32_t fields[APP_CALIBRATION_ADC_IMPORT_FIELD_COUNT]", serial_path)
    require(serial, "CalibrationCore_Crc32(fields, sizeof(fields))", serial_path)
    require(serial_protocol, "APP_CALIBRATION_EXT_REFERENCE_MIN_MV       (-5000)", serial_protocol_path)
    require(serial_protocol, "APP_CALIBRATION_EXT_REFERENCE_MAX_MV         5000", serial_protocol_path)
    require(serial, '"[CTRL] OK CAL EXT POINT queued\\r\\n"', serial_path)
    require(
        serial_protocol,
        "#define APP_CALIBRATION_STATUS_PROTOCOL_VERSION             3U",
        serial_protocol_path,
    )
    status_contract = (
        ("APP_CALIBRATION_STATUS_CORE_FORMAT", "[CAL] STATUS v=3 state=%u"),
        ("APP_CALIBRATION_STATUS_SESSION_FORMAT", "[CAL] SESSION worker_active=%u"),
        ("APP_CALIBRATION_STATUS_SELECTION_FORMAT", "[CAL] SELECT mode=%u"),
        ("APP_CALIBRATION_STATUS_PROFILE_FORMAT", "[CAL] PROFILE configured=%u"),
        ("APP_CALIBRATION_STATUS_PROFILE2_FORMAT", "[CAL] PROFILE2 previous=%lu"),
        ("APP_CALIBRATION_STATUS_ADC_FORMAT", "[CAL] ADC valid=%u"),
        ("APP_CALIBRATION_STATUS_LAST_FORMAT", "[CAL] LAST output_mv=%ld"),
        ("APP_CALIBRATION_STATUS_IO_FORMAT", "[CAL] IO low=%lu"),
        ("APP_CALIBRATION_STATUS_HW_FORMAT", "[CAL] HW relay=0x%x"),
    )
    for macro, format_prefix in status_contract:
        require(serial_protocol, macro, serial_protocol_path)
        require(serial_protocol, format_prefix, serial_protocol_path)
        require(serial, macro, serial_path)
    require(board, '"[CAL] EXT n=%lu ref_mv=%ld mean=%lu.%03lu min=%u max=%u "', board_path)
    require(board, '"flags=0x%lx crc=%08lx\\r\\n"', board_path)
    require(board, "int32_t crc_payload[6]", board_path)
    require(
        serial_protocol,
        '"[CAL] EXPORT B s=%lu v=%u mo=%lu so=%lu it=%lu ep=%lu np=%lu c=%08lx\\r\\n"',
        serial_protocol_path,
    )
    require(
        serial_protocol,
        '"[CAL] EXPORT P s=%lu i=%lu r=%08lx m=%08lx f=%lu p=%08lx g=%08lx c=%08lx\\r\\n"',
        serial_protocol_path,
    )
    require(
        serial_protocol,
        '"[CAL] EXPORT E s=%lu n=%lu c=%08lx\\r\\n"',
        serial_protocol_path,
    )
    for format_macro in (
        "APP_CALIBRATION_EXPORT_BEGIN_FORMAT",
        "APP_CALIBRATION_EXPORT_POINT_FORMAT",
        "APP_CALIBRATION_EXPORT_END_FORMAT",
    ):
        require(board, format_macro, board_path)
    if "[CAL] EXPORT POINT" in serial_protocol:
        raise AssertionError(f"legacy long EXPORT remains in {serial_protocol_path}")
    for field in (
        "float reference_value;",
        "float measured_value;",
        "uint32_t frequency_hz;",
        "float phase_deg;",
        "uint32_t flags;",
    ):
        require(core_header, field, core_header_path)
    require(service, "AppCalibrationService_RequestUndoApply", service_path)
    require(service, "app_calibration_execute_undo", service_path)
    require(service, "s_snapshot.runtime_applied = false;", service_path)
    require(protocol_doc, "[CAL] EXPORT B s=<seq>", protocol_doc_path)
    require(protocol_doc, "[CAL] EXPORT P s=<seq>", protocol_doc_path)
    require(protocol_doc, "[CAL] EXPORT E s=<seq>", protocol_doc_path)

    require(crc, "0xEDB88320UL", crc_path)
    require(crc, "uint32_t crc = 0xFFFFFFFFUL", crc_path)
    require(crc, "return ~crc;", crc_path)
    require(usart, "huart1.Init.BaudRate = 2000000;", usart_path)

    print("PROTOCOL_STATIC_PASS")
    print(f"firmware_root={root}")
    print(
        "contract=2Mbps/8N1 ASCII, CAL IMPORT ADC int32[5], "
        "CAL STATUS v3 multi-line, CAL EXPORT short B/P/E with POINT CRC, CAL UNDO"
    )
    host_crc_path = Path(__file__).resolve().parents[1] / "calibration_host" / "crc.py"
    host_crc = host_crc_path.read_text(encoding="utf-8")
    require(host_crc, 'struct.pack("<5i"', host_crc_path)
    require(host_crc, "r2_ppm", host_crc_path)
    require(host_crc, "residual_mc", host_crc_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
