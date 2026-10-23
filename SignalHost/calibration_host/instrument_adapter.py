"""Structured lab-instrument CLI adapter; raw SCPI is intentionally absent (UTF-8)."""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path
import shutil
import subprocess
from typing import Callable


DEFAULT_SKILL_SCRIPT = Path(
    r"C:\Users/<user>\.codex\skills\lab-instrument-control"
    r"\scripts\instrument_control_UTF-8.ps1"
)


class InstrumentCliError(RuntimeError):
    pass


@dataclass(frozen=True, slots=True)
class PreparedInstrumentChange:
    device: str
    channel: int
    prepare_action: str
    apply_action: str
    arguments: tuple[str, ...]
    confirmation_required: str
    current_state: dict
    proposed_state: dict
    changes_real_output: bool


Runner = Callable[[str, tuple[str, ...]], dict]


def _system_error_is_zero(value: object) -> bool:
    if value is None:
        return True
    if isinstance(value, dict):
        code = value.get("code", value.get("error_code", 0))
        return code in {0, "0"}
    if isinstance(value, (list, tuple)):
        return all(_system_error_is_zero(item) for item in value)
    text = str(value).strip().lower()
    return text.startswith("0") or "no error" in text


class StructuredInstrumentAdapter:
    """Enforce status -> prepare -> exact confirm -> apply -> readback."""

    _DG_WAVEFORM_NAMES = {
        "SIN": "sine",
        "SINE": "sine",
        "SQU": "square",
        "SQUARE": "square",
        "RAMP": "ramp",
        "PULS": "pulse",
        "PULSE": "pulse",
        "ARB": "arb",
        "NOIS": "noise",
        "NOISE": "noise",
    }

    def __init__(
        self,
        *,
        script_path: Path = DEFAULT_SKILL_SCRIPT,
        runner: Runner | None = None,
    ) -> None:
        self.script_path = script_path
        self._runner = runner or self._run_cli
        self._status_seen: set[str] = set()
        self._healthy = {"dg4062": True, "ds4014e": True}
        self.changed_channels: set[tuple[str, int]] = set()

    def _run_cli(self, action: str, arguments: tuple[str, ...]) -> dict:
        shell = shutil.which("pwsh") or shutil.which("powershell")
        if shell is None:
            raise InstrumentCliError("PowerShell executable not found")
        if not self.script_path.is_file():
            raise InstrumentCliError(
                f"instrument Skill CLI not found: {self.script_path}"
            )
        process = subprocess.run(
            [
                shell,
                "-NoProfile",
                "-File",
                str(self.script_path),
                action,
                *arguments,
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if process.returncode != 0:
            detail = (process.stderr or process.stdout).strip()
            raise InstrumentCliError(
                f"{action} failed with exit code {process.returncode}: {detail}"
            )
        try:
            return json.loads(process.stdout)
        except json.JSONDecodeError as exc:
            raise InstrumentCliError(f"{action} returned non-JSON output") from exc

    def _call(self, device: str, action: str, arguments: tuple[str, ...]) -> dict:
        if device not in {"dg4062", "ds4014e"}:
            raise PermissionError("only DG4062 and DS4014E are supported")
        if not self._healthy[device]:
            raise InstrumentCliError(
                f"{device} is frozen after a communication failure"
            )
        try:
            result = self._runner(action, arguments)
        except Exception as exc:
            if device == "ds4014e" and (
                "10054" in str(exc) or "timed out" in str(exc).lower()
            ):
                self._healthy[device] = False
            if isinstance(exc, InstrumentCliError):
                raise
            raise InstrumentCliError(f"{action} failed: {exc}") from exc
        if not _system_error_is_zero(result.get("system_error")):
            raise InstrumentCliError(
                f"{action} reported non-zero system_error: "
                f"{result.get('system_error')}"
            )
        return result

    def status(self, device: str) -> dict:
        action = f"{device}-status"
        result = self._call(device, action, ())
        self._status_seen.add(device)
        return result

    def _prepare(
        self,
        device: str,
        channel: int,
        action: str,
        arguments: tuple[str, ...],
    ) -> PreparedInstrumentChange:
        if channel != 1:
            raise PermissionError("calibration workflow is restricted to CH1")
        if device not in self._status_seen:
            raise InstrumentCliError(f"{device}-status is required before prepare")
        result = self._call(device, action, arguments)
        confirmation = result.get("confirmation_required")
        if not isinstance(confirmation, str) or not confirmation:
            raise InstrumentCliError(
                f"{action} did not return confirmation_required"
            )
        return PreparedInstrumentChange(
            device=device,
            channel=channel,
            prepare_action=action,
            apply_action=action.removesuffix("-prepare") + "-apply",
            arguments=arguments,
            confirmation_required=confirmation,
            current_state=dict(result.get("current_state") or {}),
            proposed_state=dict(
                result.get("proposed_state")
                or result.get("target_state")
                or result.get("requested_state")
                or result.get("changes")
                or {}
            ),
            changes_real_output=bool(result.get("changes_real_output", True)),
        )

    def prepare_dg4062_offset(
        self,
        offset_v: float,
        *,
        amplitude_vpp: float = 0.020,
        frequency_hz: float = 40_000.0,
        phase_deg: float = 180.0,
    ) -> PreparedInstrumentChange:
        return self.prepare_dg4062_waveform(
            waveform="sine",
            frequency_hz=frequency_hz,
            amplitude_vpp=amplitude_vpp,
            offset_v=offset_v,
            phase_deg=phase_deg,
        )

    def prepare_dg4062_waveform(
        self,
        *,
        waveform: str,
        frequency_hz: float,
        amplitude_vpp: float,
        offset_v: float,
        phase_deg: float,
    ) -> PreparedInstrumentChange:
        arguments = (
            "--channel", "1",
            "--waveform", waveform,
            "--frequency-hz", f"{frequency_hz:.12g}",
            "--amplitude-vpp", f"{amplitude_vpp:.12g}",
            "--offset-v", f"{offset_v:.12g}",
            "--phase-deg", f"{phase_deg:.12g}",
        )
        return self._prepare("dg4062", 1, "dg4062-prepare", arguments)

    def prepare_ds4014e_scale(
        self,
        scale_v_div: float,
        *,
        offset_v: float = 0.0,
    ) -> PreparedInstrumentChange:
        arguments = (
            "--channel", "1",
            "--scale-v-div", f"{scale_v_div:.12g}",
            "--offset-v", f"{offset_v:.12g}",
        )
        return self._prepare(
            "ds4014e", 1, "ds4014e-channel-prepare", arguments
        )

    def prepare_restore(
        self,
        original: PreparedInstrumentChange,
    ) -> PreparedInstrumentChange:
        if not self.can_restore(original.device, original.channel):
            raise InstrumentCliError(
                f"{original.device} CH{original.channel} has no healthy tracked change"
            )
        self.status(original.device)
        state = original.current_state
        if original.device == "dg4062":
            function = str(state.get("function", "")).strip().upper()
            waveform = self._DG_WAVEFORM_NAMES.get(function)
            if waveform is None:
                raise InstrumentCliError(
                    f"unsupported DG4062 restore waveform: {function or 'missing'}"
                )
            required = (
                "frequency_hz",
                "amplitude_unit",
                "amplitude",
                "offset_v",
                "phase_deg",
            )
            missing = [key for key in required if key not in state]
            if missing:
                raise InstrumentCliError(
                    f"DG4062 restore state missing fields: {', '.join(missing)}"
                )
            if str(state["amplitude_unit"]).strip().upper() != "VPP":
                raise InstrumentCliError(
                    "DG4062 restore requires original amplitude_unit=VPP"
                )
            return self.prepare_dg4062_waveform(
                waveform=waveform,
                frequency_hz=float(state["frequency_hz"]),
                amplitude_vpp=float(state["amplitude"]),
                offset_v=float(state["offset_v"]),
                phase_deg=float(state["phase_deg"]),
            )
        if original.device == "ds4014e":
            required = ("scale_v_per_div", "offset_v")
            missing = [key for key in required if key not in state]
            if missing:
                raise InstrumentCliError(
                    f"DS4014E restore state missing fields: {', '.join(missing)}"
                )
            return self.prepare_ds4014e_scale(
                float(state["scale_v_per_div"]),
                offset_v=float(state["offset_v"]),
            )
        raise PermissionError("only DG4062 and DS4014E are supported")

    def _apply(
        self,
        prepared: PreparedInstrumentChange,
        confirmation: str,
        *,
        track_change: bool,
    ) -> tuple[dict, dict]:
        if confirmation != prepared.confirmation_required:
            raise PermissionError("confirmation must exactly match current prepare")
        arguments = prepared.arguments + ("--confirm", confirmation)
        applied = self._call(
            prepared.device, prepared.apply_action, arguments
        )
        channel_key = (prepared.device, prepared.channel)
        if track_change:
            self.changed_channels.add(channel_key)
        else:
            self.changed_channels.discard(channel_key)
        readback = self.status(prepared.device)
        return applied, readback

    def apply(
        self,
        prepared: PreparedInstrumentChange,
        confirmation: str,
    ) -> tuple[dict, dict]:
        return self._apply(
            prepared,
            confirmation,
            track_change=True,
        )

    def apply_restore(
        self,
        prepared: PreparedInstrumentChange,
        confirmation: str,
    ) -> tuple[dict, dict]:
        return self._apply(
            prepared,
            confirmation,
            track_change=False,
        )

    def can_restore(self, device: str, channel: int) -> bool:
        return (
            (device, channel) in self.changed_channels
            and self._healthy.get(device, False)
        )

    @property
    def ds4014e_frozen(self) -> bool:
        return not self._healthy["ds4014e"]
