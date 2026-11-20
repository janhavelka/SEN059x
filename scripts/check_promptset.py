"""Validate the implementation sequence and its links without a sibling checkout."""

import csv
import json
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
PROMPTS = ROOT / "docs" / "prompts"
LINK = re.compile(r"\[[^\]\n]*\]\(([^()\s]+)\)")
STAGE_FILE = re.compile(r"[0-9]{2}A?-[a-z0-9-]+\.md")
STAGE_SECTIONS = ["Start", "Inputs", "Work", "Acceptance", "Delegation and self-audit", "Handoff"]
BENCH_OUTPUTS = {"bench-measurements", "bench-commissioning", "bench-shared-bus"}


def headings(markdown):
    for line in markdown.splitlines():
        match = re.match(r"^#{1,6}\s+(.+)$", line)
        if match:
            yield match[1]


def check_links():
    # External project pages can link into a renumbered stage, too.
    sources = list(ROOT.glob("*.md"))
    for directory in ("docs", "examples", ".github"):
        sources.extend((ROOT / directory).rglob("*.md"))
    for source in sources:
        inside_promptset = source.is_relative_to(PROMPTS)
        for link in LINK.findall(source.read_text(encoding="utf-8")):
            url = urlsplit(link)
            if url.scheme or url.netloc:
                continue
            target = (source.parent / unquote(url.path)).resolve() if url.path else source
            if not inside_promptset and not target.is_relative_to(PROMPTS):
                continue
            if not target.is_relative_to(ROOT) or not target.exists():
                raise ValueError(f"{source.name}: missing local link {link}")
            if url.fragment and target.suffix == ".md":
                anchors = {
                    re.sub(r"[^\w -]", "", heading.lower()).replace(" ", "-")
                    for heading in headings(target.read_text(encoding="utf-8"))
                }
                if unquote(url.fragment) not in anchors:
                    raise ValueError(f"{source.name}: missing heading {link}")


def stage_rows(name):
    source = (PROMPTS / name).read_text(encoding="utf-8")
    lines = iter(source.splitlines())
    if not any(re.match(r"^\|\s*Stage\s*\|", line) for line in lines):
        raise ValueError(f"{name} has no stage table")
    next(lines, None)  # Markdown delimiter row.
    rows = []
    for line in lines:
        if not line.startswith("|"):
            break
        rows.append([column.strip() for column in line.strip().strip("|").split("|")])
    return rows


def check_navigation(stages):
    ids = {stage["produces"]: stage["id"] for stage in stages}
    index = []
    ledger = []
    for stage in stages:
        number, output = stage["id"], stage["produces"]
        requires = ", ".join(ids[token] for token in stage["requires"])
        optional = ", ".join(ids[token] for token in stage.get("review_if_available", []))
        index.append([number, f"[{stage['title']}]({stage['file']})",
                      requires or "Initial scaffold", f"`{output}`", optional or "—"])
        ledger.append([number, f"`{output}`"])
    if stage_rows("README.md") != index:
        raise ValueError("README stage index disagrees with the sequence (ID, link/title, dependencies, output or optional evidence)")

    progress = stage_rows("PROGRESS.md")
    if [row[:2] for row in progress] != ledger:
        raise ValueError("Progress ledger stage IDs or outputs disagree with the sequence")
    statuses = {"planned", "in progress", "partial", "blocked", "complete"}
    for row in progress:
        if len(row) != 3 or row[2].casefold() not in statuses:
            raise ValueError(f"Progress ledger stage {row[0]} has an invalid status")

    startup = (PROMPTS / "START_HERE.md").read_text(encoding="utf-8")
    paths = re.findall(r"docs/prompts/(\d{2}A?-[a-z0-9-]+\.md)\b", startup)
    files = {stage["file"] for stage in stages}
    if stages[0]["file"] not in paths or set(paths) - files:
        raise ValueError("START_HERE needs the current first prompt and valid numbered paths")


def check_audit_instructions(name, body, scope):
    """Check the requested audit actions in each independently dispatched prompt."""
    words = " ".join(body.lower().replace("–", "-").split())
    opening = rf"re-?read (?:the )?original prompts? {scope.replace('–', '-')} in full"
    if not re.search(opening, words):
        raise ValueError(f"{name}: must reread original prompt range {scope} in full")

    topics = ("requirements coverage", "implementation correctness", "edge cases", "tests",
              "scope", "code simplicity")
    assignments = [sentence for sentence in re.split(r"[.!?]", words)
                   if "parallel" in sentence and re.search(r"sub-?agents?\b", sentence)]
    if not any(all(topic in assignment for topic in topics) for assignment in assignments):
        raise ValueError(f"{name}: must assign all six audit topics to parallel subagents")
    actions = {
        "independent audit": r"fresh.{0,30}independent audit",
        "inspect actual code and diff": r"(?:verify|check|reproduce).{0,75}actual code.{0,20}diff",
        "avoid relying on previous summaries": r"(?:not|rather than|instead of).{0,30}(?:previous|earlier|prior) summar",
        "fix confirmed issues in scope": r"fix.{0,35}confirmed.{0,60}original scope",
        "simplify corrections": r"(?:simple|readable).{0,60}(?:remove|reduce|avoid) unnecessary complexity",
        "verify until complete": r"(?:repeat|rerun).{0,35}verification.{0,65}until.{0,80}(?:satisf|complete)",
        "report findings, fixes and checks": r"concise report.{0,40}findings.{0,30}fixes.{0,45}verification",
    }
    for action, pattern in actions.items():
        if not re.search(pattern, words):
            raise ValueError(f"{name}: missing audit instruction to {action}")


def check_checkpoint(stage, originals, inputs):
    """Match the checkpoint to the exact uninterrupted run of original prompts before it."""
    name = stage["file"]
    source_ids = [source["id"] for source in originals]
    if not source_ids or stage["audits"] != source_ids or stage["id"] != source_ids[-1] + "A":
        raise ValueError(f"{name}: audit sources must match the preceding original prompt range")
    scope = source_ids[0] if len(source_ids) == 1 else f"{source_ids[0]}–{source_ids[-1]}"
    range_lines = [line for line in inputs.splitlines() if line.startswith("Original prompts ")]
    if len(range_lines) != 1 or not re.match(rf"Original prompts {scope}\b", range_lines[0]):
        raise ValueError(f"{name}: Inputs must identify original prompt range {scope}")
    citations = re.findall(r"\[([^\]]+)\]\(([^)]+)\)", range_lines[0])
    if citations != [(source["id"], source["file"]) for source in originals]:
        raise ValueError(f"{name}: original prompt links must match the entire range in order")

    covered = {source["produces"] for source in originals}
    if covered & BENCH_OUTPUTS:
        if covered != BENCH_OUTPUTS or set(stage.get("review_if_available", [])) != covered:
            raise ValueError(f"{name}: physical audit must review the three optional bench outputs")
    elif not covered.issubset(stage["requires"]):
        raise ValueError(f"{name}: software audit must require every original output in its range")
    return scope


def check_sequence():
    manifest = json.loads((PROMPTS / "SEQUENCE.json").read_text(encoding="utf-8"))
    if manifest["schema"] != 2 or not manifest["stages"]:
        raise ValueError("Expected a nonempty version 2 sequence")

    stages = manifest["stages"]
    files = [stage["file"] for stage in stages]
    actual = sorted(path.name for path in PROMPTS.glob("[0-9]*.md"))
    if files != actual:
        raise ValueError("Manifest and numbered prompt files differ")

    products = {stage["produces"] for stage in stages}
    earlier = {}
    used = set()
    # Consumers of a group's terminal output must also pass that group's audit.
    # Consumers of earlier outputs may run independently of work later in the group.
    software_audits = {}
    pending = []
    original_number = 0
    for stage in stages:
        name, output = stage["file"], stage["produces"]
        is_audit = "audits" in stage
        if not is_audit:
            original_number += 1
            pending.append(stage)
        stage_id = f"{original_number:02d}" + ("A" if is_audit else "")
        prefix = f"{stage_id}-audit-" if is_audit else f"{stage_id}-"
        if stage["id"] != stage_id or not STAGE_FILE.fullmatch(name) or not name.startswith(prefix):
            raise ValueError(f"Expected stage {stage_id}, found {name}")
        dependencies = stage["requires"]
        optional = stage.get("review_if_available", [])
        consumed = dependencies + optional
        if output in earlier or len(consumed) != len(set(consumed)) or set(consumed) - earlier.keys():
            raise ValueError(f"{name}: duplicate output or invalid dependencies {consumed}")
        used.update(consumed)

        body = (PROMPTS / name).read_text(encoding="utf-8")
        if body.splitlines()[:1] != [f"# {stage_id} — {stage['title']}"]:
            raise ValueError(f"{name}: stage heading disagrees with the manifest ID or title")
        sections = re.split(r"^## (.+)$", body, flags=re.MULTILINE)
        if sections[1::2] != STAGE_SECTIONS or not all(s.strip() for s in sections[2::2]):
            raise ValueError(f"{name}: missing, empty or out-of-order stage section")
        content = dict(zip(sections[1::2], sections[2::2]))
        inputs, handoff = content["Inputs"], content["Handoff"]
        scope = check_checkpoint(stage, pending, inputs) if is_audit else stage_id
        check_audit_instructions(name, body, scope)
        for section, targets in (("Start", {"COMMON.md", "PROGRESS.md"}),
                                 ("Delegation and self-audit", {"AUDIT.md"}),
                                 ("Handoff", {"PROGRESS.md"})):
            paths = {urlsplit(link).path for link in LINK.findall(content[section])}
            if missing := targets - paths:
                raise ValueError(f"{name}: {section} must link to {', '.join(sorted(missing))}")
        if any(f"`{token}`" not in inputs for token in [output, *consumed]) or f"`{output}`" not in handoff:
            raise ValueError(f"{name}: input or handoff differs from manifest")
        ancestors = set(dependencies)
        for dependency in dependencies:
            ancestors.update(earlier[dependency])
        named = set(re.findall(r"`([a-z][a-z0-9-]*)`", inputs)) & products
        if named - ancestors - {output} - set(optional):
            raise ValueError(f"{name}: consumes a handoff outside its dependency chain")
        if ancestors.intersection(optional):
            raise ValueError(f"{name}: optional evidence is already a required prerequisite")
        if not is_audit:
            required_reviews = {software_audits[dep] for dep in dependencies if dep in software_audits}
            if required_reviews - ancestors:
                raise ValueError(f"{name}: bypasses software audit {sorted(required_reviews - ancestors)}")
        else:
            if not BENCH_OUTPUTS.intersection(optional):
                software_audits[pending[-1]["produces"]] = output
            pending.clear()
        earlier[output] = ancestors
    if pending or stages[-1]["produces"] != "delivery-audit":
        raise ValueError("The final independent audit must cover the remaining original prompts")
    delivery = next((stage for stage in stages if stage["produces"] == "delivery-review"), None)
    physical = BENCH_OUTPUTS | {"bench-audit"}
    if delivery is None or not physical.issubset(delivery.get("review_if_available", [])):
        raise ValueError("Delivery review must retain the three bench outputs and their audit as optional evidence")
    if earlier["delivery-audit"] & physical:
        raise ValueError("Software delivery must remain possible without physical audit evidence")
    if set(software_audits.values()) - earlier["delivery-audit"] - {"delivery-audit"}:
        raise ValueError("The final delivery must require all software audits")
    # All consumers occur later. If every nonfinal stage has a consumer, every
    # output reaches the final handoff through required or optional evidence.
    if unused := products - used - {stages[-1]["produces"]}:
        raise ValueError(f"No path to the delivery stage for outputs {sorted(unused)}")
    check_navigation(stages)
    return len(stages)


def csv_rows(path):
    with path.open(encoding="utf-8", newline="") as source:
        return list(csv.DictReader(source))


def common_write_word(key, value):
    """Encode a standard-profile example valid on both SKUs (DFRobot table 2)."""
    if key == "uart-unit":
        if value not in ("mm", "us"):
            raise ValueError(f"{key}: unknown unit {value}")
        return int(value == "us")
    if not re.fullmatch(r"[0-9]+", value):
        raise ValueError(f"{key}: expected decimal CLI value")
    number = int(value)
    if key == "baud":
        rates = (4800, 9600, 14400, 19200, 38400, 57600, 76800, 115200)
        if number not in rates:
            raise ValueError(f"{key}: unsupported rate {number}")
        return 2 + rates.index(number)

    limits = {"address": (1, 247), "polarity": (0, 1), "threshold": (30, 3000),
              "angle": (1, 4), "noise": (1, 5), "range": (1, 4)}
    if key not in limits:
        raise ValueError(f"Unknown setting key {key}")
    minimum, maximum = limits[key]
    if not minimum <= number <= maximum:
        raise ValueError(f"{key}: {number} is outside the shared standard-profile range")
    return number


def check_operations():
    protocol = ROOT / "docs/reference/protocol"
    inventory = {row["address"]: row["access"] for row in csv_rows(protocol / "registers.csv")}
    vectors = {row["case"]: row for row in csv_rows(protocol / "modbus_vectors.csv")}
    coverage = (PROMPTS / "COVERAGE.md").read_text(encoding="utf-8")
    rows = re.findall(r"^\| `(0x[0-9A-F]{4})` \| (R|RW) \| ([^|]+)\| `([^`]+)`", coverage, re.MULTILINE)
    if len(rows) != len(inventory) or {addr: access for addr, access, _, _ in rows} != inventory:
        raise ValueError("Coverage table must match every reference address and access mode")

    cli = (PROMPTS / "CLI_CONTRACT.md").read_text(encoding="utf-8")
    cli_rows = re.findall(r"^\| `(0x[0-9A-F]{4})` \| `([a-z-]+)` \|", cli, re.MULTILINE)
    cli_settings = dict(cli_rows)
    writable = {addr for addr, access in inventory.items() if access == "RW"}
    if set(cli_settings) != writable or len(cli_rows) != len(writable):
        raise ValueError("CLI contract must name exactly one setting for each writable register")

    expected = {}
    for addr, access, command_cell, selector in rows:
        functions = ["FC03", "FC06"] if access == "RW" else ["FC03"]
        commands = re.findall(r"`([^`]+)`", command_cell)
        if len(commands) != len(functions):
            raise ValueError(f"{addr}: missing or extra command in coverage table")
        for function, command in zip(functions, commands):
            expected[addr, function] = (selector, command)

    cases = csv_rows(PROMPTS / "OPERATIONS.csv")
    seen_vectors = set()
    for row in cases:
        addr, function = row["register"], row["function"]
        key = (addr, function)
        if key not in expected:
            raise ValueError(f"{addr}/{function}: duplicate or undocumented operation")
        selector, command = expected.pop(key)
        if not re.fullmatch(r"0x[0-9A-F]{4}", row["word"]):
            raise ValueError(f"{addr}/{function}: expected a 16-bit test word")
        expected_word = int(row["word"], 16)
        tokens = row["human_command"].split()
        if function == "FC06":
            if len(tokens) != 5 or tokens[:2] != ["config", "set"] or tokens[-1] != "confirm":
                raise ValueError(f"{addr}: expected a confirmed named setting and value")
            if tokens[2] != cli_settings[addr]:
                raise ValueError(f"{addr}: operation uses a different setting key from the CLI contract")
            if common_write_word(tokens[2], tokens[3]) != expected_word:
                raise ValueError(f"{addr}: CLI setting value disagrees with the expected wire word")
            tokens = tokens[:3]
        if row["selector"] != selector or " ".join(tokens) != command:
            raise ValueError(f"{addr}/{function}: mismatch with public selector or CLI command")
        case = row["manufacturer_case"]
        if case:
            if case not in vectors or case in seen_vectors:
                raise ValueError(f"{addr}/{function}: duplicate or unknown manufacturer case {case}")
            seen_vectors.add(case)
            request = bytes.fromhex(vectors[case]["request_hex"])
            response = bytes.fromhex(vectors[case]["response_hex"])
            word = request[4:6] if function == "FC06" else response[3:5]
            published = (request[1], int.from_bytes(request[2:4], "big"), int.from_bytes(word, "big"))
            if published != (int(function[2:], 16), int(addr, 16), expected_word):
                raise ValueError(f"{addr}/{function}: word or request differs from manufacturer case {case}")

    if expected:
        raise ValueError(f"Missing operations {sorted(expected)}")
    if seen_vectors != set(vectors):
        raise ValueError(f"Missing manufacturer cases {sorted(set(vectors) - seen_vectors)}")
    return len(inventory), len(cases)


if __name__ == "__main__":
    try:
        stage_count = check_sequence()
        register_count, operation_count = check_operations()
        check_links()
    except (KeyError, OSError, ValueError) as error:
        raise SystemExit(str(error))
    print(f"Verified {stage_count} audited stages, {register_count} registers, {operation_count} operations and local links.")
