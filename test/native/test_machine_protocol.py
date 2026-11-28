"""Replay both renderers against the real dispatcher and a clocked, scripted port."""

import argparse
import contextlib
import csv
import io
import json
import re
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PREFIX = b"SEN059X "
NONCE = "bench_A"


def fixture_rows(relative):
    with (ROOT / relative).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def rtu(payload):
    crc = 0xFFFF
    for byte in payload:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return payload + struct.pack("<H", crc)


def request(address, function, register, value):
    return rtu(struct.pack(">BBHH", address, function, register, value))


def read_reply(address, *words):
    payload = b"".join(struct.pack(">H", word) for word in words)
    return rtu(bytes([address, 3, len(payload)]) + payload)


def records(output, machine_only=True):
    def unique_fields(pairs):
        obj = dict(pairs)
        assert len(obj) == len(pairs), pairs
        return obj

    result = []
    for line in output.splitlines(keepends=True):
        if not line.startswith(PREFIX):
            assert PREFIX not in line, line
            if machine_only:
                assert line == b"\n", line
            continue
        assert line.endswith(b"}\n") and len(line) <= 2048 and b"\033" not in line, line
        record = json.loads(line[len(PREFIX):].decode("utf-8"), object_pairs_hook=unique_fields)
        assert record["v"] == 1 and type(record["v"]) is int
        assert re.fullmatch("[0-9a-f]{16}", record["boot"])
        assert record["session"] is None or re.fullmatch(r"[-_A-Za-z0-9]{1,32}", record["session"])
        assert type(record["req"]) is int and 1 <= record["req"] <= 4294967295
        assert record["op"] is None or (type(record["op"]) is int and record["op"] > 0)
        assert isinstance(record["data"], dict) and isinstance(record["cmd"], str)
        if record["phase"] == "accepted":
            assert record["op"] and record["code"] is None and record["detail"] is None
        else:
            assert record["phase"] == "done" and re.fullmatch("[A-Z_]+", record["code"])
            assert type(record["detail"]) is int
        result.append(record)
    return result


class Script:
    def __init__(self, executable, machine, model=None):
        self.executable = executable
        self.machine = machine
        self.steps = []
        self.ids = {}
        if machine:
            self.command("session " + NONCE)
        if model:
            self.command("model " + model)

    def step(self, text):
        self.steps.append(text)
        return len(self.steps)

    def command(self, command):
        req = len(self.ids) + 1
        prefix = f"@{req} " if self.machine else ""
        step = self.step(">" + prefix + command)
        self.ids[step] = req
        return step

    def reply(self, frame):
        self.step("tick 10000")
        return self.step("rx " + frame.hex(" "))

    def run(self, transcript=False):
        run = subprocess.run(
            [self.executable], input=("\n".join(self.steps) + "\n").encode(),
            capture_output=True, timeout=12,
        )
        assert run.returncode == 0, (self.steps, run.stderr.decode(errors="replace"))
        self.output = []
        self.tx = []
        for line in run.stdout.decode("ascii").splitlines():
            kind, _, contents = line.partition(" ")
            if kind == "tx":
                baud, frame = contents.split()
                self.tx.append((int(baud), bytes.fromhex(frame)))
            else:
                assert kind == "out", line
                self.output.append(bytes.fromhex(contents))
        assert len(self.output) == len(self.steps) + 1
        if transcript:
            for step, output in zip(self.steps, self.output[1:]):
                if step.startswith((">", "!")):
                    print("\n> " + step[1:])
                if output:
                    print(output.decode("utf-8").replace("\n> \n", "\n>\n").rstrip("\n> "))
        return self

    def answer(self, command, terminal=None):
        step = command if terminal is None else terminal
        reply, = records(self.output[step])
        assert (reply["req"], reply["session"]) == (self.ids[command], NONCE), reply
        assert (reply["phase"], reply["code"], reply["detail"]) == ("done", "OK", 0), reply
        if terminal is not None:
            admission, = records(self.output[command])
            for key in ("req", "op", "cmd", "session", "boot"):
                assert admission[key] == reply[key], (admission, reply)
            assert admission["phase"] == "accepted" and reply["data"]["disposition"] == "Ok"
        return reply


def replay_row(executable, row, golden, model, vendor, transcript):
    write = row["function"] == "FC06"
    register, word = (int(row[key], 0) for key in ("register", "word"))
    frame = request(255 if vendor else 1, 6 if write else 3, register, word if write else 1)
    reply = frame if write else read_reply(1, word)
    if row["manufacturer_case"] and not vendor:
        case = golden[row["manufacturer_case"]]
        assert frame == bytes.fromhex(case["request_hex"]) and reply == bytes.fromhex(case["response_hex"])
    expected_tx = [(115200, frame)]
    command = row["human_command"]
    decoded = {
        "address": 1, "baud": 9600, "polarity": "high below threshold", "threshold": 100,
        "angle": ("about 60 deg" if model == "SEN0591" else
                  "about 65 deg single, 65 deg dual horizontal, 80 deg dual vertical"),
        "uart-unit": "mm", "noise": "battery supply", "range": "about 50 cm",
    }
    if write:
        key = command.split()[2]
        old_address = request(1, 3, 0x0200, 1)
        address = word if key == "address" else 1
        baud = 9600 if key == "baud" else 115200
        expected_tx.insert(0, (115200, old_address))
        expected_tx.append((baud, request(address, 3, register, 1)))
        if vendor:
            command = command.replace("config set", "broadcast set")
    for machine in (False, True):
        script = Script(executable, machine, model)
        if write:
            if vendor:
                script.command("profile dfrobot-isolated")
            script.command("config read address")
            script.reply(read_reply(1, 1))
            preview = script.command(command.removesuffix(" confirm"))
            refused = script.command(command)
            script.command("maintenance on isolated")
        live = script.command(command)
        if vendor:
            terminal = script.step("tick 10000")
            if key == "address":
                script.command(f"useaddr {address}")
            if key == "baud":
                script.command(f"host baud {baud}")
            verify = script.command("config read " + key)
            verification = script.reply(read_reply(address, word))
            lookup = script.command("result 2")
        else:
            terminal = script.reply(reply)
            if write:
                terminal = script.reply(read_reply(address, word))
        script.run(transcript)
        assert script.tx == expected_tx, (model, command, expected_tx, script.tx)
        count = 2 if write and not vendor else 1
        if not machine:
            out = script.output[terminal].decode()
            assert f"transfers {count}" in out and " OK detail 0" in out, out
            assert f"0x{word:04X}" in out, out
            if write:
                assert b"Preview" in script.output[preview] and b"DISALLOWED" in script.output[refused]
                assert ("verified=false" if vendor else "verified=true") in out
            if vendor:
                assert f"0x{word:04X}".encode() in script.output[verification]
                assert b"verified=false" in script.output[lookup]
            if not write:
                selector = command.split()[-1]
                if register < 0x200:
                    measured = {"processed": "754 mm", "realtime": "751 mm",
                                "temperature": "30.0 °C", "echo": "5750 µs"}
                    assert measured[selector] in out, out
                else:
                    assert str(decoded[selector]) in out, out
            continue
        result = script.answer(live, terminal)
        data = result["data"]
        assert data["transfers"] == count and data["attempts"] >= count and data["elapsedMs"] >= 0
        if write:
            assert script.answer(preview)["data"]["requested"] == word
            denial, = records(script.output[refused])
            assert denial["phase"] == "done" and denial["code"] == "DISALLOWED" and denial["op"] is None
            assert result["cmd"] == ("broadcast set" if vendor else "config set")
            assert (data["key"], data["register"], data["requested"]) == (key, register, word)
            assert data["scope"] == ("send-only" if vendor else "addressed")
            assert data["destination"] == (255 if vendor else 1)
            assert data["acknowledged"] is (not vendor) and data["verified"] is (not vendor)
            assert data["observed"] == (None if vendor else word)
            if vendor:
                assert data["effectUncertain"] is True
                assert script.answer(verify, verification)["data"]["raw"] == word
                assert script.answer(lookup)["data"]["result"] == result
        elif register < 0x200:
            kind = command.split()[1]
            sample = data["sample"]
            assert result["cmd"] == "read" and (sample["model"], sample["address"]) == (model, 1)
            assert (sample["kind"], sample["register"], sample["raw"]) == (kind, register, word)
            assert sample["value"] == (word / 10 if kind == "temperature" else word)
            assert sample["unit"] == {"temperature": "degC", "echo": "us"}.get(kind, "mm")
            assert sample["sampleGeneration"] > 0 and sample["bindingGeneration"] > 0
            assert sample["observationMs"] == 10 and sample["ageMs"] == 0 and sample["quality"]
            assert sample["valid"] is True and sample["fresh"] is True
        else:
            assert result["cmd"] == "config read" and data["raw"] == word
            assert data["key"] == command.split()[2] and data["register"] == register
            assert data["applicability"] in {"rs485", "switch-mode", "uart-output"}
            assert data["trusted"] is True and data["value"] == decoded[data["key"]]


def passive(executable, transcript):
    script = Script(executable, True)
    ids = {cmd: script.command(cmd) for cmd in (
        "help", "ver", "status", "drv", "health", "stats", "settings", "sample", "job status",
        "maintenance", "host", "selftest", "help config set threshold",
    )}
    script.run(transcript)
    assert not script.tx
    startup = script.output[0].decode().splitlines()
    listing = "\n".join(startup[1:startup.index("Selected profile standard")])
    reference = (ROOT / "docs/operator-cli.md").read_text(encoding="utf-8")
    assert f"\n{listing}\n" in reference, "operator reference differs from registry help"
    data = {cmd: script.answer(step)["data"] for cmd, step in ids.items()}
    assert "job status" in data["help"]["commands"] and "config read" in data["help"]["commands"]
    assert data["job status"]["active"] is False
    assert data["stats"]["physicalAttempts"] == 0 and data["stats"]["protocolErrors"] == 0
    assert set(data["sample"]["samples"]) == {"processed", "realtime", "temperature", "echo"}
    assert all(value is None for value in data["sample"]["samples"].values())
    assert len(data["settings"]["settings"]) == 8
    assert all(item["raw"] is None for item in data["settings"]["settings"])
    assert data["health"]["lastErrorMs"] is None and data["health"]["lastOkTimeValid"] is False
    assert data["ver"]["firmwareRevision"] == "abc123" and data["ver"]["dirty"] is False
    assert data["ver"]["framework"] == "native-fw" and data["ver"]["frameworkVersion"] == "1.2"
    assert data["ver"]["target"] == "host-cpu" and data["ver"]["board"] == "test-board"
    assert data["ver"]["model"] is None and data["ver"]["activeOp"] is None
    assert data["ver"]["supportedBaud"] == [4800, 9600, 14400, 19200, 38400, 57600, 76800, 115200]
    assert data["ver"]["address"] == 1 and data["ver"]["baud"] == 115200
    assert data["ver"]["addressPolicy"] == "standard" and data["ver"]["serialFormat"] == "8N1"
    assert data["ver"]["libraryVersion"] and data["ver"]["libraryRevision"]
    assert type(data["ver"]["libraryDirty"]) in (bool, type(None))


def session_and_desktop(terminal):
    nonce = b"A_b-" * 8
    inputs = [
        b"@12 read echo", b"@1 session bad!", b"@1 session " + nonce + b"x",
        b"@1 session " + nonce, b"@2 job status", b"@2 read echo", b"@1 version",
        b"@3 stats", b"color on", b"@4 job status", b'@5 unknown\\"',
        b"@6 read echo" + b" " * 193, b"@7 ver " + b"x" * 65,
        b"@8 config set threshold 750 confirm" + b" " * 193,
        b"@4294967295\x01x version", b"@" + b"0" * 53 + b"42949672959 version", b"@9 stats",
        b"@4294967296 read echo", b"@0 version",
        b"@4294967295 job status", b"@4294967295 read echo", b"@1 session B", b"@2 version",
    ]
    def run(lines):
        proc = subprocess.run([terminal], input=b"\r\n".join(lines) + b"\n",
                              capture_output=True, timeout=10, check=True)
        return records(proc.stdout, machine_only=False)

    replies = run(inputs)
    boot = replies[0]["boot"]
    assert all(r["boot"] == boot for r in replies)
    assert replies[0]["session"] is None and replies[0]["code"] != "OK"
    for record in replies[1:3]:
        assert record["session"] is None and record["code"] != "OK"
    group = [r for r in replies if r["session"] == nonce.decode()]
    assert [r["req"] for r in group] == [*range(1, 10), 4294967295], group
    for record, cmd in zip(group[5:8], ("read", "version", "config set")):
        assert (record["cmd"], record["phase"], record["code"], record["op"]) == (
            cmd, "done", "INVALID_CONFIG", None), record
    assert group[8]["code"] == "OK" and group[8]["data"]["physicalAttempts"] == 0
    assert group[0]["cmd"] == "session" and group[0]["data"]["target"] == "desktop"
    assert group[0]["data"]["framework"] == "standalone"
    assert group[1]["cmd"] == "job status" and group[1]["data"] == group[3]["data"]
    assert group[2]["data"]["protocolErrors"] >= 2 and group[2]["data"]["physicalAttempts"] == 0
    assert group[4]["cmd"] == 'unknown\\"' and group[4]["code"] != "OK"
    assert [(r["session"], r["req"]) for r in replies[-2:]] == [("B", 1), ("B", 2)]
    reboot, = run([b"@1 session B"])
    assert reboot["boot"] != boot and reboot["data"]["activeOp"] is None


def aliases_and_settings(executable, rows):
    script = Script(executable, True, "SEN0592")
    cmds = {cmd: script.command(cmd) for cmd in ("?", "ver", "sample", "stop 99", "setaddr 5", "setbaud 9600")}
    expected_tx = []
    settings = {}
    for row in rows:
        reg, word = int(row["register"], 0), int(row["word"], 0)
        if reg < 0x200 or row["function"] != "FC03":
            continue
        cmd = row["human_command"]
        key = cmd.split()[2]
        # Longest noise description on the longer-range model exercises fully populated output.
        if key == "noise":
            word = 5
        step = script.command("readbaud" if key == "baud" else cmd)
        terminal = script.reply(read_reply(1, word))
        settings[key] = (word, step, terminal)
        expected_tx.append((115200, request(1, 3, reg, 1)))
    shown = script.command("settings")
    diag = script.command("diag")
    done = script.reply(read_reply(1, 1))
    script.run()
    assert script.tx == expected_tx + [(115200, request(1, 3, 0x200, 1))]
    for alias, canonical in (("?", "help"), ("ver", "version"), ("sample", "latest"),
                             ("stop 99", "cancel"), ("setaddr 5", "config set"), ("setbaud 9600", "config set")):
        reply, = records(script.output[cmds[alias]])
        assert reply["cmd"] == canonical and reply["phase"] == "done"
        assert (reply["code"] == "OK") is (alias != "stop 99")
    assert script.answer(diag, done)["cmd"] == "diagnose"
    cached = script.answer(shown)["data"]["settings"]
    assert len(cached) == 8
    for item in cached:
        word, step, terminal = settings[item["key"]]
        original = script.answer(step, terminal)
        assert original["cmd"] == "config read"
        assert item["raw"] == word and item["trusted"] is True
        assert item["endpoint"] == {"address": 1, "baud": 115200}
        assert item["value"] == original["data"]["value"]


def grant_ownership(executable):
    script = Script(executable, True, "SEN0592")
    script.command("readbaud")
    script.reply(read_reply(1, 9))
    script.command("maintenance on isolated")
    script.step(">@1 session bench_B")
    denied = script.step(">@2 config set threshold 750 confirm")
    script.step(">maintenance on isolated")
    human_grant = script.step(">@3 config set threshold 750 confirm")
    script.step(">@4 maintenance on isolated")
    machine_grant = script.step(">config set threshold 750 confirm")
    script.step(">@5 maintenance on isolated")
    accepted = script.step(">@6 config set threshold 750 confirm")
    script.reply(request(1, 6, 0x206, 750))
    done = script.reply(read_reply(1, 750))
    consumed = script.step(">@7 config set threshold 750 confirm")
    script.run()
    assert script.tx == [(115200, request(1, 3, 0x201, 1)), (115200, request(1, 6, 0x206, 750)),
                         (115200, request(1, 3, 0x206, 1))]
    for step in (denied, human_grant, consumed):
        reply, = records(script.output[step])
        assert reply["phase"] == "done" and reply["code"] == "DISALLOWED"
    assert b"DISALLOWED" in script.output[machine_grant]
    assert records(script.output[accepted])[0]["phase"] == "accepted"
    verified, = records(script.output[done])
    assert (verified["session"], verified["req"], verified["data"]["verified"]) == ("bench_B", 6, True)


def reattachment(executable):
    script = Script(executable, True, "SEN0592")
    script.command("config read threshold")
    script.reply(read_reply(1, 100))
    script.command("maintenance on isolated")
    writing = script.command("config set threshold 750 confirm")
    attached = script.step(">@1 session bench_B")
    busy = script.step(">@2 read echo")
    stale = script.step(">@3 cancel 1")
    status = script.step(">@4 job status")
    script.step("limit 0")
    script.step("!@5 cancel 2")
    stalled = script.step("tick 1000")
    resumed = script.step("limit 1")
    lookup = script.step(">@6 result 2")
    late = script.step(">@7 cancel 2")
    script.run()
    assert not script.output[stalled]
    assert script.tx == [(115200, request(1, 3, 0x206, 1)), (115200, request(1, 6, 0x206, 750))]
    accepted, = records(script.output[writing])
    handshake, = records(script.output[attached])
    assert (handshake["session"], handshake["data"]["activeOp"]) == ("bench_B", accepted["op"])
    rejected, = records(script.output[busy])
    assert rejected["code"] == "BUS_BUSY" and rejected["op"] is None
    obsolete, = records(script.output[stale])
    assert obsolete["code"] != "OK"
    state, = records(script.output[status])
    assert state["data"]["active"] is True and state["data"]["op"] == accepted["op"]
    assert state["data"]["session"] == NONCE and state["data"]["deadlineUs"] > 0
    control, original = records(script.output[resumed])
    assert (control["req"], control["session"], control["cmd"], control["code"]) == (5, "bench_B", "cancel", "OK")
    for field in ("req", "cmd", "op", "session", "boot"):
        assert original[field] == accepted[field]
    assert (original["phase"], original["code"], original["data"]["disposition"]) == ("done", "ABORTED", "Cancelled")
    assert original["data"]["effectUncertain"] is True and original["data"]["transfers"] == 1
    assert records(script.output[lookup])[0]["data"]["result"] == original
    assert records(script.output[late])[0]["code"] != "OK"


def stalled_write(executable, *, acknowledged):
    script = Script(executable, True, "SEN0591")
    script.command("config read threshold")
    script.reply(read_reply(1, 100))
    script.command("maintenance on isolated")
    script.step("limit 0")
    start = script.command("config set threshold 750 confirm")
    if acknowledged:
        script.reply(request(1, 6, 0x206, 750))
    script.step(">@1 session bench_B")
    script.step(">@2 cancel 2")
    script.step("tick 10000")
    resumed = script.step("limit 1")
    lookup = script.step(">@3 result 2")
    settings = script.step(">@4 settings")
    script.run()
    assert all(not output for output in script.output[start:resumed])
    baseline = (115200, request(1, 3, 0x206, 1))
    expected = [baseline, (115200, request(1, 6, 0x206, 750))]
    if acknowledged:
        expected.append(baseline)  # Independent FC03 readback was on the wire at cancellation.
    assert script.tx == expected
    accepted, attached, cancelled, terminal = records(script.output[resumed])
    assert [r["cmd"] for r in (accepted, attached, cancelled, terminal)] == [
        "config set", "session", "cancel", "config set"]
    assert accepted["phase"] == "accepted" and attached["data"]["activeOp"] == accepted["op"]
    assert (cancelled["req"], cancelled["session"], cancelled["code"]) == (2, "bench_B", "OK")
    assert cancelled["data"]["stage"] == "active" and cancelled["data"]["settlementRequired"]
    assert terminal["code"] == "ABORTED" and terminal["data"]["disposition"] == "Cancelled"
    for key in ("boot", "session", "req", "op", "cmd"):
        assert terminal[key] == accepted[key]
    data = terminal["data"]
    assert data["acknowledged"] is acknowledged and data["observed"] is None
    assert data["effectUncertain"] and not data["verified"] and data["transfers"] == len(expected) - 1
    assert data["settlementRequired"] and data["cleanupCode"] == "BUS_BUSY"
    assert records(script.output[lookup])[0]["data"]["result"] == terminal
    cached = records(script.output[settings])[0]["data"]["settings"]
    threshold, = (entry for entry in cached if entry["key"] == "threshold")
    assert threshold["raw"] == 100 and threshold["desired"] == 750 and not threshold["trusted"]


def escaped_readback_failure(executable):
    script = Script(executable, False)
    script.step(">@1 session " + "a" * 32)
    script.step(">@2 model SEN0592")
    script.step(">@3 config read address")
    script.reply(read_reply(1, 1))
    script.step(">@4294967291 maintenance on isolated")
    script.step(">@4294967292 config set threshold 5000 confirm")
    script.step("escaped-drain-fault")
    failure = script.reply(request(1, 6, 0x0206, 5000))
    retained = script.step(">@4294967293 result")
    script.run()
    terminal, = records(script.output[failure])
    assert (terminal["code"], terminal["detail"]) == ("IO", -2147483648)
    assert terminal["data"]["message"] == "\x01" * 64
    assert terminal["data"]["acknowledged"] and not terminal["data"]["verified"]
    nested, = records(script.output[retained])
    assert nested["data"]["result"] == terminal


def jobs(executable, machine, transcript):
    script = Script(executable, machine, "SEN0591")
    raw = script.command("readregs 0x0100 3")
    block = script.reply(read_reply(1, 754, 751, 300))
    script.command("readregs 0x0100 3")
    exception = script.reply(rtu(bytes([1, 0x83, 2])))
    watch = script.command("watch temperature 2 20")
    script.reply(read_reply(1, 300))
    script.step("tick 10000")
    watched = script.reply(read_reply(1, 305))
    script.step("tick 1010000")
    latest = script.command("latest temperature")
    script.step("drain-fault")
    fail = script.command("read temperature")
    script.run(transcript)
    assert script.tx == [(115200, request(1, 3, 0x100, 3))] * 2 + [(115200, request(1, 3, 0x102, 1))] * 3
    if not machine:
        assert b"0x0102 = 0x012C" in script.output[block] and b"EXCEPTION detail 2" in script.output[exception]
        assert b"completed 2, failed 0" in script.output[watched]
        assert b"1010 ms" in script.output[latest] and b"IO detail -59" in script.output[fail]
        return
    raw_data = script.answer(raw, block)["data"]
    assert raw_data["words"] == [754, 751, 300] and raw_data["startRegister"] == 0x100
    assert raw_data["count"] == 3 and raw_data["timeoutMs"] > 1000
    bad, = records(script.output[exception])
    assert (bad["code"], bad["detail"], bad["data"]["words"]) == ("EXCEPTION", 2, None)
    counts = script.answer(watch, watched)["data"]
    assert [counts[k] for k in ("requested", "completed", "failed", "skippedIntervals")] == [2, 2, 0, 0]
    sample = script.answer(latest)["data"]["sample"]
    assert sample["raw"] == 305 and sample["ageMs"] == 1010 and sample["fresh"] is False
    accepted, terminal = records(script.output[fail])
    assert accepted["phase"] == "accepted" and accepted["op"] == terminal["op"]
    assert (terminal["code"], terminal["detail"], terminal["data"]["disposition"]) == ("IO", -59, "Failed")
    assert terminal["data"]["sample"] is None and terminal["data"]["previousSample"] == sample


def replay(args):
    rows = fixture_rows("docs/prompts/OPERATIONS.csv")
    golden = {r["case"]: r for r in fixture_rows("docs/reference/protocol/modbus_vectors.csv")}
    assert len(rows) == 20 and len(golden) == 6 and sum(r["function"] == "FC06" for r in rows) == 8
    assert {r["manufacturer_case"] for r in rows if r["manufacturer_case"]} == set(golden)
    session_and_desktop(args.terminal)
    passive(args.fixture, True)
    aliases_and_settings(args.fixture, rows)
    grant_ownership(args.fixture)
    reattachment(args.fixture)
    for acknowledged in (False, True):
        stalled_write(args.fixture, acknowledged=acknowledged)
    escaped_readback_failure(args.fixture)
    for machine in (False, True):
        jobs(args.fixture, machine, True)
    for model in ("SEN0591", "SEN0592"):
        for row in rows:
            scopes = (False, True) if row["function"] == "FC06" else (False,)
            for vendor in scopes:
                show = model == "SEN0592" and row["human_command"].startswith("config set threshold")
                replay_row(args.fixture, row, golden, model, vendor, show)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture")
    parser.add_argument("terminal")
    parser.add_argument("--transcript", action="store_true", help="print representative dispatcher exchanges")
    args = parser.parse_args()
    if not __debug__:
        parser.error("Run the parity checks with Python assertions enabled.")
    transcript = io.StringIO()
    with contextlib.redirect_stdout(transcript):
        replay(args)
    captured = transcript.getvalue()
    examples = (ROOT / "docs/examples/cli-transcript.txt").read_text(encoding="utf-8").strip()
    for excerpt in examples.removeprefix("> ").split("\n\n> "):
        assert "> " + excerpt in captured, "dispatcher transcript changed: " + excerpt.splitlines()[0]
    if args.transcript:
        print(captured, end="")
    print("40 addressed and 16 isolated human/machine pairs passed on the scripted port.")


if __name__ == "__main__":
    main()
