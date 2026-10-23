"""SignalHost calibration GUI entry point (UTF-8)."""

from __future__ import annotations

import argparse

from .gui import create_application


def main() -> int:
    parser = argparse.ArgumentParser(description="SignalHost 图形化校准工作台")
    parser.add_argument(
        "--allow-real-serial",
        action="store_true",
        help="显式允许真实串口连接；仍不会自动连接任何端口",
    )
    parser.add_argument(
        "--allow-instruments",
        action="store_true",
        help="显式启用 DG4062/DS4014E 结构化 CLI；仍不会自动连接或写入",
    )
    args = parser.parse_args()
    root, _workbench = create_application(
        allow_real_serial=args.allow_real_serial,
        allow_instruments=args.allow_instruments,
    )
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
