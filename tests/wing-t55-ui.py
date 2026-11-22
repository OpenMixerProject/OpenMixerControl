"""Exercise the real WING OSC server with OpenWING-ui's loopback wire format."""

import json
import socket
import struct
import subprocess
import sys
import time


def field(value):
    return value + bytes(4 - len(value) % 4)


def send(sock, path, tag=b",", payload=b""):
    sock.sendto(field(path) + field(tag) + payload, ("127.0.0.1", 10032))


def receive(sock):
    data, sender = sock.recvfrom(60_000)
    assert sender == ("127.0.0.1", 10032)
    address = data.split(b"\0", 1)[0]
    tag = len(field(address))
    value = data[tag + 4:]
    if data[tag:tag + 2] == b",s":
        value = value.split(b"\0", 1)[0].decode()
    return address, value


def snapshot(sock, path, tag=b",", payload=b""):
    send(sock, path, tag, payload)
    values = dict(receive(sock) for _ in range(100))
    assert len(values) == 100 and b"/state/complete/1" in values
    for param in (b"ch_name", b"fader", b"ch_mute"):
        assert all(b"/set/" + param + b"/" + str(i).encode() in values for i in range(1, 33))
    assert json.loads(values[b"/set/metadata/fader/1"])["readonly"]
    assert not json.loads(values[b"/set/metadata/ch_name/1"])["readonly"]
    return values


def check(sock):
    for attempt in range(20):
        send(sock, b"/get/topology/1")
        try:
            address, value = receive(sock)
            break
        except TimeoutError:
            if attempt == 19:
                raise
            time.sleep(0.05)
    assert address == b"/set/topology/1"
    topology = json.loads(value)
    assert topology["capabilities"] == ["ch_name"] and topology["t55_matrix"]
    assert topology["sources"] == topology["outputs"] == []
    io = topology["local_io"]
    assert not io["transport_available"] and len(io["inputs"]) == 24 and len(io["outputs"]) == 10
    assert io["inputs"][16]["stock_ae_source"] == 0
    assert io["outputs"][-1]["name"] == "AUX OUT 8"

    send(sock, b"/get/t55_matrix/1")
    address, value = receive(sock)
    assert address == b"/set/t55_matrix/1" and json.loads(value) == {"status": "no_catalog"}
    before = snapshot(sock, b"/get/state/1")

    name = "Stimme – 32"
    send(sock, b"/set/ch_name/32", b",s", field(name.encode()))
    assert receive(sock) == (b"/result/ch_name/32", name)
    for invalid in (b"   ", b"bad\nname", b"\xf0\x80\x80\x80", b"a" * 33):
        send(sock, b"/set/ch_name/32", b",s", field(invalid))
        assert receive(sock)[0] == b"/result/ch_name_error/32"

    send(sock, b"/set/fader/32", b",f", struct.pack(">f", 0.31))
    send(sock, b"/set/wing_t55_route/420", b",f", struct.pack(">f", 373))
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as other:
        send(other, b"/set/ch_name/32", b",s", field(b"not the UI"))
    after = snapshot(sock, b"/subscribe/state/1", b",f", struct.pack(">f", 1))
    assert after == before | {b"/set/ch_name/32": name}


if __name__ == "__main__":
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
        client.bind(("127.0.0.1", 10033))
        client.settimeout(0.5)
        with subprocess.Popen([*sys.argv[1:], "--wing-t55-ui-offline"]) as omc:
            try:
                check(client)
            finally:
                omc.terminate()
    print("PASS: loopback OSC topology, snapshots, rename/readback and read-only routing")
