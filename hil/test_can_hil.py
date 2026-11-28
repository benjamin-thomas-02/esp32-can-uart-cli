"""HIL tests: CAN (TWAI) commands on the real ESP32 in self-test/loopback mode."""
import time

import pytest

RX_SETTLE_S = 0.3   # time for can_rx_task to process looped-back frames


# ---------------- self-test ----------------
@pytest.mark.parametrize("frames", [1, 10, 100])
def test_selftest_passes(dut, frames):
    out = dut.command(f"can selftest {frames}", timeout=10)
    assert out == (f"CAN self-test PASS: sent={frames} ok={frames} "
                   f"mismatch=0 timeout=0 tx_err=0")


@pytest.mark.parametrize("arg", ["0", "101", "abc", "5x"])
def test_selftest_rejects_bad_frame_count(dut, arg):
    assert dut.command(f"can selftest {arg}") == \
        "frames must be a number 1-100\nERR: invalid argument"


# ---------------- can send ----------------
@pytest.mark.parametrize("args, expected", [
    ("123 11 22",           "TX ID=0x123 DLC=2 11 22"),
    ("0x7FF",               "TX ID=0x7FF DLC=0"),
    ("0x1ABCDE AA",         "TX ID=0x001ABCDE EXT DLC=1 AA"),
    ("7FF 1 2 3 4 5 6 7 8", "TX ID=0x7FF DLC=8 01 02 03 04 05 06 07 08"),
])
def test_send_valid_frames(dut, args, expected):
    assert dut.command(f"can send {args}") == expected


@pytest.mark.parametrize("args, message", [
    ("",                      "missing CAN ID"),
    ("xyz",                   "CAN ID must be hex, e.g. 0x123"),
    ("0x20000000",            "CAN ID too large (max 0x1FFFFFFF)"),
    ("123 1FF",               "data byte must be hex 00-FF"),
    ("123 1 2 3 4 5 6 7 8 9", "max 8 data bytes"),
])
def test_send_rejects_invalid_input(dut, args, message):
    out = dut.command(f"can send {args}".strip())
    assert out.splitlines()[0] == message
    assert out.endswith("ERR: invalid argument")


# ---------------- stats ----------------
def test_stats_count_sent_and_received_frames(dut):
    dut.command("can stats reset")
    for i in range(3):
        dut.command(f"can send 10{i} 0{i}")
    time.sleep(RX_SETTLE_S)
    assert dut.command("can stats") == "tx_ok=3 tx_err=0 rx=3"


def test_selftest_traffic_is_not_counted(dut):
    dut.command("can stats reset")
    dut.command("can selftest 20", timeout=10)
    time.sleep(RX_SETTLE_S)
    assert dut.command("can stats") == "tx_ok=0 tx_err=0 rx=0"


def test_stats_reset(dut):
    dut.command("can send 1")
    time.sleep(RX_SETTLE_S)
    assert dut.command("can stats reset") == "CAN stats reset"
    assert dut.command("can stats") == "tx_ok=0 tx_err=0 rx=0"


# ---------------- monitor ----------------
@pytest.fixture
def monitor(dut):
    """Turn the CAN monitor on for one test, and always off again afterwards."""
    assert dut.command("can monitor on") == "CAN monitor on"
    yield dut
    dut.command("can monitor off")


def test_monitor_shows_received_frame(monitor):
    out = monitor.command("can send 321 AB")
    out += "\n" + monitor.read_for(0.5)        # RX line may arrive after the prompt
    assert "TX ID=0x321 DLC=1 AB" in out
    assert "[CAN RX] ID=0x321 DLC=1 AB" in out


def test_monitor_off_hides_received_frames(dut):
    assert dut.command("can monitor off") == "CAN monitor off"
    out = dut.command("can send 322 CD") + dut.read_for(0.5)
    assert "TX ID=0x322 DLC=1 CD" in out
    assert "[CAN RX]" not in out


# ---------------- usage ----------------
@pytest.mark.parametrize("cmd", ["can", "can foo"])
def test_can_usage(dut, cmd):
    out = dut.command(cmd)
    assert out.startswith("usage:")
    assert out.endswith("ERR: invalid argument")
