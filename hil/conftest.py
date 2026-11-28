"""Shared HIL fixtures: a connection to the ESP32 CLI over serial."""
import time

import pytest
import serial

PROMPT = b"\r\n> "          # printed by cli_task after every command
BANNER = b"type 'help'"      # last part of the boot log line from app_main


def pytest_addoption(parser):
    parser.addoption("--port", default="/dev/ttyUSB0", help="Serial port of the ESP32")
    parser.addoption("--baud", default=115200, type=int, help="UART baud rate")
    parser.addoption("--no-reset", action="store_true",
                     help="Do not reset the board before testing")


class Dut:
    """Device Under Test: sends CLI commands to the ESP32 and returns the replies."""

    def __init__(self, port, baud):
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = baud
        self.ser.timeout = 2.0
        self.ser.dtr = False      # keep GPIO0 high  -> normal boot, not download mode
        self.ser.rts = False      # keep EN high     -> chip keeps running
        self.ser.open()

    def close(self):
        self.ser.close()

    def reset(self):
        """Hardware reset via RTS (EN pin), then wait for the boot banner."""
        self.ser.rts = True       # pull EN low  -> chip held in reset
        time.sleep(0.1)
        self.ser.rts = False      # release EN   -> chip boots
        self.ser.timeout = 5.0
        data = self.ser.read_until(BANNER)
        self.ser.timeout = 2.0
        if BANNER not in data:
            raise RuntimeError(f"Board did not boot. Last output: {data[-200:]!r}")
        time.sleep(0.5)           # let the remaining boot log lines finish
        self.ser.reset_input_buffer()

    def sync(self):
        """Press Enter until we see a prompt, so we start at a clean line."""
        for _ in range(3):
            self.ser.reset_input_buffer()
            self.ser.write(b"\r")
            if self.ser.read_until(PROMPT).endswith(PROMPT):
                return
        raise RuntimeError("No CLI prompt from the board")

    def command(self, cmd, timeout=3.0):
        """Send one command line and return its output (echo and prompt removed)."""
        self.ser.reset_input_buffer()
        self.ser.write(cmd.encode() + b"\r")
        self.ser.timeout = timeout
        raw = self.ser.read_until(PROMPT)
        self.ser.timeout = 2.0
        if not raw.endswith(PROMPT):
            raise TimeoutError(f"No prompt after {cmd!r}. Got: {raw!r}")
        text = raw[: -len(PROMPT)].decode(errors="replace")
        _echo, _sep, body = text.partition("\r\n")   # first line = our own echo
        return body.replace("\r\n", "\n").strip()

    def read_for(self, seconds):
        """Collect any output that arrives during the next `seconds`."""
        end = time.monotonic() + seconds
        chunks = []
        while time.monotonic() < end:
            waiting = self.ser.in_waiting
            if waiting:
                chunks.append(self.ser.read(waiting))
            else:
                time.sleep(0.02)
        return b"".join(chunks).decode(errors="replace").replace("\r\n", "\n")


@pytest.fixture(scope="session")
def dut(request):
    """One connection + one reset for the whole test session."""
    board = Dut(request.config.getoption("--port"), request.config.getoption("--baud"))
    try:
        if not request.config.getoption("--no-reset"):
            board.reset()
        board.sync()
        yield board
    finally:
        board.close()
