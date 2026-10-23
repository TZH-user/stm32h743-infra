"""Transport abstractions, serial fake, and guarded pyserial backend (UTF-8)."""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass
import queue
import struct
import threading
import time
import zlib


class TransportError(RuntimeError):
    pass


class TransportDisconnected(TransportError):
    pass


class Transport:
    name = "transport"

    @property
    def is_open(self) -> bool:
        raise NotImplementedError

    def open(self) -> None:
        raise NotImplementedError

    def close(self) -> None:
        raise NotImplementedError

    def write(self, data: bytes) -> None:
        raise NotImplementedError

    def read_available(self) -> bytes:
        raise NotImplementedError


class SerialTransport(Transport):
    """Real serial backend; construction is harmless and opening is gated."""

    name = "real-serial"

    def __init__(
        self,
        port: str,
        *,
        baudrate: int = 2_000_000,
        allow_real_serial: bool = False,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.allow_real_serial = allow_real_serial
        self._serial = None
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._tx: queue.Queue[bytes] = queue.Queue(maxsize=64)
        self._rx: queue.Queue[bytes] = queue.Queue(maxsize=256)
        self._worker_error: Exception | None = None

    @property
    def is_open(self) -> bool:
        return bool(self._serial and self._serial.is_open and self._worker_error is None)

    def open(self) -> None:
        if not self.allow_real_serial:
            raise PermissionError(
                "real serial is disabled; start with explicit --allow-real-serial authorization"
            )
        if not self.port.strip():
            raise ValueError("serial port is required")
        import serial

        self._serial = serial.Serial(
            port=self.port,
            baudrate=self.baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0,
            write_timeout=0.5,
        )
        self._stop.clear()
        self._worker_error = None
        self._thread = threading.Thread(
            target=self._io_worker,
            name="SignalHostSerialIo",
            daemon=True,
        )
        self._thread.start()

    def _io_worker(self) -> None:
        try:
            while not self._stop.is_set():
                wrote = False
                try:
                    payload = self._tx.get_nowait()
                except queue.Empty:
                    payload = b""
                if payload:
                    self._serial.write(payload)
                    wrote = True
                waiting = int(self._serial.in_waiting)
                if waiting:
                    chunk = self._serial.read(waiting)
                    try:
                        self._rx.put(chunk, timeout=0.1)
                    except queue.Full as exc:
                        raise TransportError("serial RX queue is full") from exc
                elif not wrote:
                    time.sleep(0.002)
        except Exception as exc:
            self._worker_error = exc
            self._stop.set()

    def close(self) -> None:
        self._stop.set()
        if self._thread is not None and self._thread.is_alive():
            self._thread.join(timeout=1.0)
        if self._serial is not None:
            self._serial.close()

    def write(self, data: bytes) -> None:
        if not self.is_open:
            detail = f": {self._worker_error}" if self._worker_error else ""
            raise TransportDisconnected(f"serial transport is closed{detail}")
        try:
            self._tx.put_nowait(data)
        except queue.Full as exc:
            raise TransportError("serial TX queue is full") from exc

    def read_available(self) -> bytes:
        chunks: list[bytes] = []
        while True:
            try:
                chunks.append(self._rx.get_nowait())
            except queue.Empty:
                break
        if self._worker_error is not None and not chunks:
            raise TransportDisconnected(f"serial I/O worker failed: {self._worker_error}")
        return b"".join(chunks)


@dataclass(slots=True)
class FakeFaults:
    queue_full_for: set[str] | None = None
    reject_for: set[str] | None = None
    drop_response_for: set[str] | None = None
    disconnect_for: set[str] | None = None
    export_crc_error_at: int | None = None

    def contains(self, group: str, command: str) -> bool:
        values = getattr(self, group) or set()
        return any(command.startswith(prefix) for prefix in values)


class SerialFake(Transport):
    """Stateful protocol fake. It never enumerates or opens Windows ports."""

    name = "serial-fake"

    def __init__(self, *, faults: FakeFaults | None = None, chunk_size: int = 0) -> None:
        self.faults = faults or FakeFaults()
        self.chunk_size = chunk_size
        self._open = False
        self._rx: deque[bytes] = deque()
        self.writes: list[str] = []
        self.state = 0
        self.exclusive = 0
        self.can_start = 0
        self.candidate = 0
        self.pending = 0
        self.applied = 0
        self.points = 0
        self.expected_points = 0
        self.active_sequence = 1
        self.previous_sequence = 0
        self.pending_sequence = 0
        self.captured_points: list[tuple[float, float, int, float, int]] = []

    @property
    def is_open(self) -> bool:
        return self._open

    def open(self) -> None:
        self._open = True

    def close(self) -> None:
        self._open = False

    def _queue(self, *lines: str) -> None:
        payload = "\r\n".join(lines).encode("ascii") + b"\r\n"
        if self.chunk_size > 0:
            for start in range(0, len(payload), self.chunk_size):
                self._rx.append(payload[start : start + self.chunk_size])
        else:
            self._rx.append(payload)

    def _cal_snapshot(self) -> str:
        return (
            f"[CTRL] CAL state={self.state} error=0 can_start={self.can_start} "
            f"mode=1 source=2 cand={self.candidate} pend={self.pending} "
            f"applied={self.applied}"
        )

    def _status(self) -> str:
        return "\r\n".join(
            (
                f"[CAL] STATUS v=3 state={self.state} error=0 "
                f"exclusive={self.exclusive} can_start={self.can_start}",
                f"[CAL] SESSION worker_active=0 reason=none "
                f"points={self.points}/{self.expected_points}",
                f"[CAL] SELECT mode=1 source=2 item=1 candidate={self.candidate}",
                f"[CAL] PROFILE configured={self.applied} active={self.applied} "
                f"pending={self.pending}",
                f"[CAL] PROFILE2 previous=0 undo_available={self.applied} "
                f"enabled={self.applied} in_use={self.applied} applied={self.applied}",
                "[CAL] ADC valid=0 seq=0 dc_uv=0 rms_uv=0 peak_uv=0",
                "[CAL] LAST output_mv=0 mean_mc=0 min=0 max=0",
                "[CAL] IO low=0 high=0 dir=0 samples=0",
                "[CAL] HW relay=0x0 dac_code=0 adc_run=0",
            )
        )

    def _handle_point(self, command: str) -> None:
        if self.state != 5:
            self._queue("[CTRL] ERR CAL EXT POINT state or busy")
            return
        reference_mv = int(command.split()[-1])
        self.points += 1
        mean_millicodes = 2_119_000 - 412 * reference_mv
        mean = mean_millicodes / 1000.0
        minimum = max(0, min(4095, int(mean) - 4))
        maximum = max(minimum, min(4095, int(mean) + 5))
        flags = 0
        self.captured_points.append(
            (reference_mv / 1000.0, mean, 0, 0.0, flags)
        )
        payload = struct.pack(
            "<6i", self.points, reference_mv, mean_millicodes, minimum, maximum, flags
        )
        crc = zlib.crc32(payload) & 0xFFFFFFFF
        self._queue(
            "[CTRL] OK CAL EXT POINT queued",
            f"[CAL] EXT n={self.points} ref_mv={reference_mv} mean={mean:.3f} "
            f"min={minimum} max={maximum} flags=0x0 crc={crc:08X}",
        )

    @staticmethod
    def _float_bits(value: float) -> int:
        return struct.unpack("<I", struct.pack("<f", value))[0]

    def _handle_export(self, command: str) -> None:
        parts = command.split()
        if len(parts) not in {2, 3}:
            self._queue("[CTRL] ERR CAL EXPORT FORMAT")
            return
        if len(parts) == 2 or parts[2] == "LATEST":
            sequence = self.active_sequence
        else:
            try:
                sequence = int(parts[2], 10)
            except ValueError:
                self._queue("[CTRL] ERR CAL EXPORT FORMAT")
                return
            if not 1 <= sequence <= 0xFFFFFFFF:
                self._queue("[CTRL] ERR CAL EXPORT FORMAT")
                return
        if sequence != self.active_sequence:
            self._queue("[CTRL] ERR CAL EXPORT session not found")
            return

        default_points = (
            (-1.0, -0.75, 1000, -5.5, 0),
            (0.0, 0.125, 1000, 0.0, 1),
            (1.0, 1.0, 10000, 5.5, 0),
        )
        points = tuple(self.captured_points) or default_points
        lines = [
            f"[CTRL] OK CAL EXPORT queued sequence={sequence}",
            f"[CAL] EXPORT B s={sequence} v=1 mo=1 so=2 it=1 "
            f"ep={len(points)} np={len(points)} c=A1B2C3D4",
        ]
        for index, (reference, measured, frequency_hz, phase_deg, flags) in enumerate(points):
            reference_bits = self._float_bits(reference)
            measured_bits = self._float_bits(measured)
            phase_bits = self._float_bits(phase_deg)
            payload = struct.pack(
                "<IIIII",
                reference_bits,
                measured_bits,
                frequency_hz,
                phase_bits,
                flags,
            )
            crc = zlib.crc32(payload) & 0xFFFFFFFF
            if self.faults.export_crc_error_at == index:
                crc ^= 0x00000001
            lines.append(
                f"[CAL] EXPORT P s={sequence} i={index} r={reference_bits:08X} "
                f"m={measured_bits:08X} f={frequency_hz} p={phase_bits:08X} "
                f"g={flags:08X} c={crc:08X}"
            )
        lines.append(
            f"[CAL] EXPORT E s={sequence} n={len(points)} c=5A6B7C8D"
        )
        self._queue(*lines)

    def write(self, data: bytes) -> None:
        if not self._open:
            raise TransportDisconnected("serial fake is closed")
        command = data.decode("ascii").strip().upper()
        self.writes.append(command)
        if self.faults.contains("disconnect_for", command):
            self._open = False
            raise TransportDisconnected(f"fake disconnect during {command}")
        if self.faults.contains("drop_response_for", command):
            return
        if self.faults.contains("queue_full_for", command):
            self._queue("[CTRL] ERR UI QUEUE FULL")
            return
        if self.faults.contains("reject_for", command):
            self._queue(f"[CTRL] ERR UI {command}", self._cal_snapshot())
            return

        if command == "OUTPUT LOG":
            self._queue("[CTRL] OK OUTPUT log")
        elif command == "PING":
            self._queue("[CTRL] PONG")
        elif command == "STATUS":
            self._queue(
                "[CTRL] STATUS tick=100 main=0 phase=0 output=log requested=log",
                self._cal_snapshot(),
            )
        elif command == "RTOS":
            self._queue(
                "[CTRL] RTOS tick=100 tasks=8 heap=131072 min_heap=120000",
                "[CTRL] STACK display=1024 touch=1024 printf=2048 main=2048 phase=2048",
            )
        elif command == "CAL STATUS":
            self._queue(self._status())
        elif command.startswith("CAL EXPORT"):
            self._handle_export(command)
        elif command == "CAL EXT ENTER":
            self.state, self.exclusive, self.can_start = 3, 1, 1
            self._queue("[CTRL] OK UI CAL EXT ENTER", self._cal_snapshot())
        elif command.startswith("CAL EXT START"):
            parts = command.split()
            self.expected_points = int(parts[3]) if len(parts) == 4 else 0
            self.points = 0
            self.captured_points.clear()
            self.state, self.can_start = 5, 0
            self._queue("[CTRL] OK UI CAL EXT START", self._cal_snapshot())
        elif command.startswith("CAL EXT POINT"):
            self._handle_point(command)
        elif command == "CAL EXT FINISH":
            self.state = 6
            self._queue("[CTRL] OK UI CAL EXT FINISH", self._cal_snapshot())
        elif command.startswith("CAL IMPORT ADC"):
            parts = command.split()
            try:
                if len(parts) != 9:
                    raise ValueError("CAL IMPORT ADC requires five fields and CRC")
                fields = tuple(int(value) for value in parts[3:8])
                expected_crc = int(parts[8], 16)
                actual_crc = zlib.crc32(struct.pack("<5i", *fields)) & 0xFFFFFFFF
            except (ValueError, IndexError, struct.error):
                self._queue("[CTRL] ERR CAL IMPORT FORMAT")
                return
            if actual_crc != expected_crc:
                self._queue(f"[CTRL] ERR CAL IMPORT CRC {actual_crc:08X}")
                return
            self.candidate = 1
            self._queue("[CTRL] OK UI CAL IMPORT ADC", self._cal_snapshot())
        elif command == "CAL CONFIRM":
            self.candidate, self.pending = 0, 1
            self.pending_sequence = self.active_sequence + 1
            self._queue("[CTRL] OK UI CAL CONFIRM", self._cal_snapshot())
        elif command == "CAL APPLY":
            self.pending, self.applied = 0, 1
            self.previous_sequence = self.active_sequence
            self.active_sequence = self.pending_sequence or self.active_sequence + 1
            self._queue("[CTRL] OK UI CAL APPLY", self._cal_snapshot())
        elif command == "CAL UNDO":
            if not self.applied or self.previous_sequence == 0:
                self._queue("[CTRL] ERR UI CAL UNDO", self._cal_snapshot())
                return
            self.active_sequence = self.previous_sequence
            self.previous_sequence = 0
            self.applied = 0
            self._queue("[CTRL] OK UI CAL UNDO", self._cal_snapshot())
        elif command == "CAL EXT EXIT":
            self.state, self.exclusive, self.can_start = 0, 0, 0
            self._queue("[CTRL] OK UI CAL EXT EXIT", self._cal_snapshot())
        else:
            self._queue("[CTRL] ERR UNKNOWN")

    def read_available(self) -> bytes:
        if not self._open:
            return b""
        return self._rx.popleft() if self._rx else b""


class ReplayTransport(Transport):
    """Read RX records from a deterministic UTF-8 replay file."""

    name = "log-replay"

    def __init__(self, records: list[tuple[str, str]]) -> None:
        self.records = deque(records)
        self._open = False
        self.writes: list[str] = []

    @classmethod
    def from_file(cls, path: str) -> "ReplayTransport":
        records: list[tuple[str, str]] = []
        with open(path, "r", encoding="utf-8") as stream:
            for number, line in enumerate(stream, 1):
                text = line.rstrip("\r\n")
                if not text or text.startswith("#"):
                    continue
                if "|" not in text:
                    raise ValueError(f"invalid replay record at line {number}")
                direction, payload = text.split("|", 1)
                records.append((direction, payload))
        return cls(records)

    @property
    def is_open(self) -> bool:
        return self._open

    def open(self) -> None:
        self._open = True

    def close(self) -> None:
        self._open = False

    def write(self, data: bytes) -> None:
        if not self._open:
            raise TransportDisconnected("replay transport is closed")
        self.writes.append(data.decode("ascii").strip())

    def read_available(self) -> bytes:
        if not self._open or not self.records:
            return b""
        direction, payload = self.records.popleft()
        if direction == "DISCONNECT":
            self._open = False
            raise TransportDisconnected(payload or "replay disconnect")
        if direction != "RX":
            return b""
        return payload.encode("ascii", errors="replace") + b"\r\n"
