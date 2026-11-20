"""Validate the implementation sequence and its links without a sibling checkout."""

import csv
import json
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
PROMPTS = ROOT / "docs" / "prompts"
LINK = re.compile(r"\[[^\]\n]*\]\(([^()\s]+)\)")
STAGE_FILE = re.compile(r"[0-9]{2}-[a-z0-9-]+\.md")


def headings(markdown):
    for line in markdown.splitlines():
        match = re.match(r"^#{1,6}\s+(.+)$", line)
        if match:
            yield match[1]


def check_links():
    for source in sorted(PROMPTS.glob("*.md")):
        for link in LINK.findall(source.read_text(encoding="utf-8")):
            url = urlsplit(link)
            if url.scheme or url.netloc:
                continue
            target = (source.parent / unquote(url.path)).resolve() if url.path else source
            if not target.is_relative_to(ROOT) or not target.exists():
                raise ValueError(f"{source.name}: missing local link {link}")
            if url.fragment and target.suffix == ".md":
                anchors = {
                    re.sub(r"[^\w -]", "", heading.lower()).replace(" ", "-")
                    for heading in headings(target.read_text(encoding="utf-8"))
                }
                if unquote(url.fragment) not in anchors:
                    raise ValueError(f"{source.name}: missing heading {link}")


def check_sequence():
    manifest = json.loads((PROMPTS / "SEQUENCE.json").read_text(encoding="utf-8"))
    if manifest["schema"] != 1 or not manifest["stages"]:
        raise ValueError("Expected a nonempty version 1 sequence")

    stages = manifest["stages"]
    files = [stage["file"] for stage in stages]
    actual = sorted(path.name for path in PROMPTS.glob("[0-9]*.md"))
    if files != actual:
        raise ValueError("Manifest and numbered prompt files differ")

    index = (PROMPTS / "README.md").read_text(encoding="utf-8")
    if [name for name in LINK.findall(index) if STAGE_FILE.fullmatch(name)] != files:
        raise ValueError("README must link every stage in sequence order")
    progress = (PROMPTS / "PROGRESS.md").read_text(encoding="utf-8")
    if re.findall(r"^\| (\d{2}) \|", progress, re.MULTILINE) != [s["id"] for s in stages]:
        raise ValueError("Progress ledger and manifest stage IDs differ")

    products = {stage["produces"] for stage in stages}
    earlier = {}
    for number, stage in enumerate(stages, start=1):
        name, output = stage["file"], stage["produces"]
        stage_id = f"{number:02d}"
        if stage["id"] != stage_id or not STAGE_FILE.fullmatch(name) or name[:2] != stage_id:
            raise ValueError(f"Expected stage {stage_id}, found {name}")
        dependencies = stage["requires"]
        optional = stage.get("review_if_available", [])
        consumed = dependencies + optional
        if output in earlier or len(consumed) != len(set(consumed)) or set(consumed) - earlier.keys():
            raise ValueError(f"{name}: duplicate output or invalid dependencies {consumed}")

        body = (PROMPTS / name).read_text(encoding="utf-8")
        sections = re.split(r"^## (Inputs|Work|Acceptance|Handoff)$", body, flags=re.MULTILINE)
        if sections[1::2] != ["Inputs", "Work", "Acceptance", "Handoff"]:
            raise ValueError(f"{name}: expected Inputs, Work, Acceptance and Handoff in order")
        inputs, handoff = sections[2], sections[-1]
        if not inputs.strip() or not all(sections[i].strip() for i in (4, 6, 8)):
            raise ValueError(f"{name}: empty stage section")
        if any(f"`{token}`" not in inputs for token in [output, *consumed]) or f"`{output}`" not in handoff:
            raise ValueError(f"{name}: input or handoff differs from manifest")
        ancestors = set(dependencies)
        for dependency in dependencies:
            ancestors.update(earlier[dependency])
        named = set(re.findall(r"`([a-z][a-z0-9-]*)`", inputs)) & products
        if named - ancestors - {output} - set(optional):
            raise ValueError(f"{name}: consumes a handoff outside its dependency chain")
        earlier[output] = ancestors
    return len(stages)


def check_registers():
    with (ROOT / "docs/reference/protocol/registers.csv").open(encoding="utf-8", newline="") as source:
        addresses = [row["address"] for row in csv.DictReader(source)]
    coverage = (PROMPTS / "COVERAGE.md").read_text(encoding="utf-8")
    rows = re.findall(r"^\| `(0x[0-9A-Fa-f]{4})` \|", coverage, re.MULTILINE)
    if sorted(rows) != sorted(addresses):
        raise ValueError("Coverage index must have one row per reference register")
    return len(rows)


if __name__ == "__main__":
    try:
        stage_count = check_sequence()
        register_count = check_registers()
        check_links()
    except (KeyError, OSError, ValueError) as error:
        raise SystemExit(str(error))
    print(f"Verified {stage_count} prompt stages, {register_count} register rows and local links.")
