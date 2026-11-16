#!/usr/bin/env python3
"""Non-destructive discovery/state wire probe against a running bodyless OMC.

Usage: python3 files/test-osc-state.py [server IP]. Changes DEBUG_VALUE only.
Run on the server host (or pass its address); bind local UDP port 10033 first.
"""
import json
import socket
import struct
import sys
import time


def osc_string(value):
    data = value.encode() + b"\0"
    return data + b"\0" * (-len(data) % 4)


def packet(path, value=None):
    return osc_string(path) + osc_string("," if value is None else ",f") + (
        b"" if value is None else struct.pack(">f", value))


def decode(data):
    end = data.index(0)
    path = data[:end].decode()
    offset = (end + 4) & ~3
    end = data.index(0, offset)
    kind = data[offset:end].decode()
    offset = (end + 4) & ~3
    if kind == ",s":
        return path, data[offset:data.index(0, offset)].decode()
    return path, struct.unpack_from(">f", data, offset)[0]


with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024 * 1024)
    sock.bind(("0.0.0.0", 10033))
    sock.settimeout(10)
    target = (sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1", 10032)

    def send(path, value=None):
        sock.sendto(packet(path, value), target)

    def until(path):
        values = {}
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            key, value = decode(sock.recv(65536))
            values[key] = value
            if key == path:
                return values
        raise AssertionError("missing " + path)

    send("/get/topology/1")
    topology = json.loads(until("/set/topology/1")["/set/topology/1"])
    assert topology["dcas"] == 8 and topology["mute_groups"] == 6
    assert topology["fanout"] is True
    assert topology["colors"] == ["#000000", "#ff0000", "#00ff00", "#ffff00",
                                   "#0000ff", "#ff00ff", "#00ffff", "#ffffff"]
    assert next(g for g in topology["strips"] if g["kind"] == "aux")["start"] == 32
    assert next(g for g in topology["strips"] if g["kind"] == "bus")["eq_bands"] == []
    routing = topology["routing"]
    for mode, count in (("channels", 40), ("outputs", 40), ("effects", 24), ("sockets", 208)):
        assert sum(g["count"] for g in routing[mode]["dests"]) == count
        assert all(g["start"] > 0 for g in routing[mode]["sources"])
    assert routing["effects"]["dests"][0]["start"] == 40
    assert routing["channels"]["sources"] != topology["strips"]

    # Metadata and snapshot requests must coexist, even when issued together.
    send("/get/metadata/1")
    send("/subscribe/state/1", 1)
    initial = until("/state/complete/1")
    assert "/metadata/complete/1" in initial
    assert json.loads(initial["/set/metadata/ch_phase/1"])["audio_status"] == "state_only_no_dispatch"
    assert initial["/subscribe/state/1"] == 1
    assert "/set/ch_gain/1" in initial and "/set/ch_gain/81" in initial
    send("/set/debug/1", 17)
    assert until("/set/debug/1")["/set/debug/1"] == 17
    # Invalid addresses/types/indices and truncated messages must not crash OMC.
    for path in ("/", "/get", "/set/ch_gain/0", "/set/ch_gain/999999999999999999999999"):
        send(path, 1)
    sock.sendto(b"/bad" * 300, target)
    sock.sendto(b"/no-nul-or-format", target)
    send("/get/state/1")
    assert until("/state/complete/1")["/set/debug/1"] == 17
    send("/subscribe/state/1", 0)
    assert until("/subscribe/state/1")["/subscribe/state/1"] == 0
    send("/set/debug/1", 18)
    sock.settimeout(0.3)
    try:
        data = sock.recv(65536)
        raise AssertionError("feedback after unsubscribe: " + str(decode(data)))
    except socket.timeout:
        pass
    print("PASS topology/native routing/palette/metadata/snapshot/changes/unsubscribe/malformed input")
