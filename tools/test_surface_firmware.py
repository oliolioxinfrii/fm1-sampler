#!/usr/bin/env python3
"""Run semantic surface tests against the fm1-base host executable."""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
HOST = ROOT / "surface-firmware/build/host/sampler_host"

def u14(n):
    return [n >> 7 & 127, n & 127]

def fixed(s, n):
    return list(s.encode("ascii")[:n].ljust(n, b"\0"))

def context(mode, page):
    return (mode * 8 * 20 + page) * 6

def message(cmd, payload=()):
    return " ".join(f"{x:02X}" for x in [240, 125, 68, 65, 87, 1, cmd, *payload, 247])

def state(mode=0, page=2, ack=0, value=8192, gen=0, kind=0, light=1):
    payload = [1, *u14(context(mode, page)), *u14(ack), light,
               mode, 0, page, 0, 0, *u14(1), *u14(0), *u14(gen), kind,
               0, 127, 25]
    payload += fixed("COURTYARD", 20) + fixed("ATTACK", 20) + fixed("TEST", 36)
    for _ in range(4):
        payload += [5, *u14(value), *u14(164)] + fixed("ATTACK", 14) + fixed("0.5 s", 14)
    assert len(payload) == 229
    return "sysex " + message(32, payload)

def chunk(offset):
    data = [3] * 756 + u14(0) + u14(16383)
    p = [*u14(context(3, 0)), *u14(1), 2, *u14(len(data)), *u14(offset),
         *data[offset:offset+180]]
    return "sysex " + message(33, p)

lines = [
    "wait 100",
    "sysex " + message(5, [0, 1]),
    "expect_sysex " + message(68, [2]),
    state(), "wait 30", "expect surface_ready 1", "expect surface_knob0 8192",
    "turn K1 1", "wait 30", "expect surface_knob0 8356", "expect surface_pending 1",
    "expect_sysex " + message(69, [*u14(context(0, 2)), 0, 1, 0, 64, 0]),
    state(ack=1, value=8356), "wait 30", "expect surface_pending 0",
    state(mode=3, page=0, ack=1, gen=1, kind=2),
    chunk(720), chunk(360), chunk(0), chunk(180),
    "expect surface_data_len 0", chunk(540), "wait 30", "expect surface_data_len 760",
    state(mode=3, page=0, ack=1, gen=1, kind=2, light=0),
    "wait 30", "expect surface_data_len 760",
    "usb unplug", "wait 100", "expect surface_ready 0",
    "usb plug", "wait 100", "sysex " + message(5, [0, 2]),
    "expect_sysex " + message(68, [2]), state(), "wait 30", "expect surface_ready 1",
]

if __name__ == "__main__":
    if not HOST.is_file():
        raise SystemExit("Build the host first: bash tools/build_surface.sh --host")
    with tempfile.TemporaryDirectory() as directory:
        path = pathlib.Path(directory) / "surface.txt"
        path.write_text("\n".join(lines) + "\n")
        subprocess.run([str(HOST), str(path), directory], check=True)
