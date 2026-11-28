"""HIL tests: general CLI behaviour on the real ESP32."""
import re
import time

import pytest


def test_version(dut):
    out = dut.command("version")
    assert re.fullmatch(r"esp32-can-uart-cli v\d+\.\d+\.\d+", out)


@pytest.mark.parametrize("name", ["help", "version", "uptime", "echo", "sys", "can"])
def test_help_lists_command(dut, name):
    out = dut.command("help")
    assert re.search(rf"^\s*{name}\s", out, re.MULTILINE), f"'{name}' missing in help"


def test_echo_collapses_spaces(dut):
    assert dut.command("echo   hello    world") == "hello world"


def test_empty_line_gives_new_prompt(dut):
    assert dut.command("") == ""


@pytest.mark.parametrize("cmd", ["foo", "HELP", "hel"])
def test_unknown_command(dut, cmd):
    assert dut.command(cmd) == "ERR: unknown command"


def test_twelve_words_ok_thirteen_rejected(dut):
    eleven = " ".join("abcdefghijk")          # echo + 11 = 12 words
    twelve = " ".join("abcdefghijkl")         # echo + 12 = 13 words
    assert dut.command("echo " + eleven) == eleven
    assert dut.command("echo " + twelve) == "ERR: too many arguments"


def _uptime_ms(dut):
    match = re.fullmatch(r"uptime: (\d+) ms", dut.command("uptime"))
    assert match, "unexpected uptime output"
    return int(match.group(1))


def test_uptime_increases(dut):
    first = _uptime_ms(dut)
    time.sleep(0.5)
    second = _uptime_ms(dut)
    assert second - first >= 400


def test_sys_reports_heap_and_stacks(dut):
    out = dut.command("sys")
    assert re.search(r"heap free: \d+ bytes, min ever: \d+ bytes", out)

    stacks = {name: int(free) for name, free in
              re.findall(r"(\w+)\s+stack free \(min\): (\d+) bytes", out)}
    assert set(stacks) == {"uart_rx", "cli", "can_rx"}
    for name, free in stacks.items():
        assert free > 512, f"task '{name}' is close to stack overflow ({free} bytes free)"
