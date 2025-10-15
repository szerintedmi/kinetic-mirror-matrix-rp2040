#!/usr/bin/env python3
"""
Smoke test script for Core Mirror Control Deck firmware.

Cycles through homing, motion bounds, and limit fault scenarios while logging
build metadata so lab operators can trace results back to a firmware revision.
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys
import time
from typing import Iterable, List, Optional

try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover - guidance for operators when pyserial missing
    serial = None  # type: ignore


ROOT = pathlib.Path(__file__).resolve().parents[1]
CONFIG_VERSION_FILE = ROOT / "config" / "version.txt"


def read_git_hash() -> str:
    try:
        output = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, stderr=subprocess.STDOUT
        )
        return output.decode("ascii", errors="ignore").strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def read_config_version() -> str:
    try:
        text = CONFIG_VERSION_FILE.read_text(encoding="ascii")
        return text.strip() or "unknown"
    except FileNotFoundError:
        return "unknown"


def require_serial_module() -> None:
    if serial is None:  # type: ignore
        print("pyserial not available. Install it with `pip install pyserial`.", file=sys.stderr)
        sys.exit(2)


class SerialSession:
    def __init__(self, port: str, baud: int, timeout: float) -> None:
        self._port = port
        self._baud = baud
        self._timeout = timeout
        self._handle: Optional[serial.Serial] = None  # type: ignore[attr-defined]

    def __enter__(self) -> "SerialSession":
        self.open()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:  # type: ignore[override]
        self.close()

    def open(self) -> None:
        if self._handle is not None:
            return
        self._handle = serial.Serial(  # type: ignore[attr-defined]
            self._port,
            baudrate=self._baud,
            timeout=self._timeout,
            write_timeout=self._timeout,
        )
        time.sleep(0.1)
        self._drain_boot_messages()

    def close(self) -> None:
        if self._handle is None:
            return
        try:
            self._handle.close()
        finally:
            self._handle = None

    def send_command(self, command: str, quiet: bool = False) -> List[str]:
        if self._handle is None:
            raise RuntimeError("Serial port not open")
        payload = (command.strip() + "\n").encode("ascii")
        self._handle.write(payload)
        self._handle.flush()

        lines: List[str] = []
        stop_time = time.monotonic() + self._timeout
        while time.monotonic() < stop_time:
            raw = self._handle.readline()
            if not raw:
                if lines:
                    break
                continue
            line = raw.decode("ascii", errors="ignore").strip()
            if line:
                lines.append(line)
            if len(lines) >= 1 and lines[0].startswith("CTRL:") and time.monotonic() > stop_time - 0.1:
                break
        if not quiet:
            print(f"> {command}")
            for entry in lines:
                print(f"< {entry}")
        return lines

    def _drain_boot_messages(self) -> None:
        if self._handle is None:
            return

        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            raw = self._handle.readline()
            if not raw:
                break
            line = raw.decode("ascii", errors="ignore").strip()
            if line:
                print(f"[boot] {line}")


def extract_status_line(lines: Iterable[str]) -> Optional[str]:
    for line in lines:
        if line.startswith("STATUS:"):
            return line
    return None


def wait_for_idle(session: SerialSession, channel: int, timeout: float, poll_interval: float = 0.2) -> str:
    deadline = time.monotonic() + timeout
    next_report = time.monotonic()
    while time.monotonic() < deadline:
        status = extract_status_line(session.send_command(f"STATUS:{channel}", quiet=True))
        if status:
            if "STATE=IDLE" in status:
                print(f"[status] {status}")
                return status
            now = time.monotonic()
            if now >= next_report:
                print(f"[status] {status}")
                next_report = now + 2.0
        time.sleep(poll_interval)
    raise TimeoutError(f"Motor channel {channel} did not return to IDLE within {timeout} seconds")


def run_homing(session: SerialSession, channel: int, idle_timeout: float) -> None:
    print(f"\n[homing] Channel {channel}")
    session.send_command(f"HOME:{channel}")
    wait_for_idle(session, channel, idle_timeout)
    print("[homing] complete")


def run_motion_bounds(session: SerialSession, channel: int, limit: int, idle_timeout: float) -> None:
    print(f"\n[bounds] Exercising +/-{limit} steps on channel {channel}")
    session.send_command(f"WAKE:{channel}")
    session.send_command(f"MOVE:{channel},{limit}")
    wait_for_idle(session, channel, idle_timeout)
    session.send_command(f"MOVE:{channel},{-limit}")
    wait_for_idle(session, channel, idle_timeout)
    session.send_command(f"SLEEP:{channel}")
    print("[bounds] complete")


def run_fault_injection(session: SerialSession, channel: int, limit: int) -> None:
    print(f"\n[fault] Injecting limit violation on channel {channel}")
    response = session.send_command(f"MOVE:{channel},{limit + 400}")
    clipped = any("MOVE:LIMIT_CLIPPED=1" in line for line in response)
    if not clipped:
        raise RuntimeError("Expected limit clipping indicator missing from MOVE response")
    status = extract_status_line(session.send_command(f"STATUS:{channel}", quiet=True))
    if status:
        print(f"[fault-status] {status}")
        if "ERR=ERR_LIMIT" not in status:
            raise RuntimeError("STATUS response did not surface ERR_LIMIT after fault injection")
    print("[fault] limit violation acknowledged")


def parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="RP2040 Core Mirror Control smoke test runner")
    parser.add_argument("--port", required=True, help="Serial port connected to the RP2040 control deck")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    parser.add_argument("--channel", type=int, default=0, help="Motor channel to exercise")
    parser.add_argument("--limit", type=int, default=1200, help="Soft limit (steps) to verify during motion bounds")
    parser.add_argument("--timeout", type=float, default=1.0, help="Serial read timeout in seconds")
    parser.add_argument(
        "--idle-timeout",
        type=float,
        default=25.0,
        help="Seconds to wait for a channel to reach IDLE during scripted flows (default: 25.0)",
    )
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> int:
    args = parse_args(argv)
    require_serial_module()

    print("=== Core Mirror Control Deck Smoke Test ===")
    print(f"Firmware git hash : {read_git_hash()}")
    print(f"Config version    : {read_config_version()}")
    print("-------------------------------------------")

    try:
        with SerialSession(args.port, args.baud, args.timeout) as session:
            run_homing(session, args.channel, args.idle_timeout)
            run_motion_bounds(session, args.channel, args.limit, args.idle_timeout)
            run_fault_injection(session, args.channel, args.limit)
    except TimeoutError as exc:
        print(f"[error] {exc}")
        print("Hint: Increase --idle-timeout or verify mechanics before retrying.")
        return 1

    print("\nAll smoke test sequences completed successfully.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
