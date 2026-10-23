#!/usr/bin/env python3
"""Capture completed fake-workflow GUI screenshots at acceptance sizes (UTF-8)."""

from __future__ import annotations

from pathlib import Path
import sys

from PIL import ImageGrab

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from calibration_host.gui import create_application
from calibration_host.workflow import WorkflowStage


def capture(width: int, height: int) -> Path:
    root, workbench = create_application()
    root.geometry(f"{width}x{height}+40+40")
    root.update()
    workbench._connect()
    workbench.session.poll()
    workbench._start_workflow()
    for _ in range(300):
        workbench.workflow.tick()
        if workbench.workflow.state.stage == WorkflowStage.HUMAN_REVIEW:
            break
    workbench.workflow.approve_review()
    for _ in range(20):
        workbench.workflow.tick()
        if workbench.workflow.state.stage == WorkflowStage.COMPLETE:
            break
    workbench._refresh()
    root.lift()
    root.attributes("-topmost", True)
    root.focus_force()
    root.update()
    x = root.winfo_rootx()
    y = root.winfo_rooty()
    actual_width = root.winfo_width()
    actual_height = root.winfo_height()
    output = (
        ROOT / "build" / f"gui_fake_complete_{width}x{height}_UTF-8.png"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    ImageGrab.grab(
        bbox=(x, y, x + actual_width, y + actual_height),
        all_screens=True,
    ).save(output)
    root.attributes("-topmost", False)
    root.destroy()
    return output


def main() -> int:
    for width, height in ((960, 640), (1280, 780), (1440, 900)):
        print(capture(width, height))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
