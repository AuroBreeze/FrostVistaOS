#!/usr/bin/env python3
"""Generate website-data/roadmap.json from the current releases.md roadmap."""

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "releases.md"
OUTPUT = ROOT / "website-data" / "roadmap.json"

PHASE_RE = re.compile(r"^##\s+(Phase\s+\d+)\s+-\s+(.+?)(?:\s+<!--\s*id:\s*([^ ]+)\s*-->)?\s*$")
ITEM_RE = re.compile(
    r"^\s*-\s+\[([ xX])\]\s+\*\*(.+?)\*\*"
    r"(?:\s+<!--\s*id:\s*([^ ]+)\s*-->)?\s*:\s*(.+?)\s*$"
)
VALIDATION_RE = re.compile(r"^\s*-\s+\[([ xX])\]\s+(.+?)\s*$")


def clean_markdown(text):
    return text.replace("`", "").strip()


def status_from_items(items):
    statuses = {item["status"] for item in items}
    if statuses == {"complete"}:
        return "complete"
    if "blocked" in statuses:
        return "blocked"
    if "complete" in statuses or "in_progress" in statuses:
        return "in_progress"
    return "planned"


def parse_item(line, line_number):
    match = ITEM_RE.match(line)
    if not match:
        raise ValueError(f"Invalid roadmap item at releases.md:{line_number}")

    checked, title, item_id, description = match.groups()
    if not item_id:
        raise ValueError(f"Roadmap item is missing an id at releases.md:{line_number}")

    return {
        "id": item_id,
        "title": clean_markdown(title),
        "description": description,
        "status": "complete" if checked.lower() == "x" else "planned",
    }


def parse_validation(lines):
    validation = []
    for line in lines:
        match = VALIDATION_RE.match(line)
        if not match:
            continue

        checked, content = match.groups()
        if " -> " in content:
            command, expected = content.rsplit(" -> ", 1)
        else:
            command, expected = content, "PASS"

        command = clean_markdown(command)
        validation.append(
            {
                "command": command,
                "expected": clean_markdown(expected),
                "status": "complete" if checked.lower() == "x" else "not_run",
            }
        )
    return validation


def main():
    lines = SOURCE.read_text(encoding="utf-8").splitlines()
    heading = re.match(r"^#\s+Roadmap\s+\(([^ ]+)\s+-\s+(.+?)\)\s*$", lines[0])
    if not heading:
        raise ValueError("The first releases.md heading is not a roadmap heading")

    version, title = heading.groups()
    summary = lines[2].strip()
    scope = lines[4].strip()
    phases = []
    current = None
    validation_start = None

    for line_number, line in enumerate(lines[6:], 7):
        if line == "## Validation":
            validation_start = line_number
            break

        phase = PHASE_RE.match(line)
        if phase:
            _, phase_title, phase_id = phase.groups()
            if not phase_id:
                raise ValueError(f"Roadmap phase is missing an id at releases.md:{line_number}")
            current = {"id": phase_id, "title": clean_markdown(phase_title), "items": []}
            phases.append(current)
            continue

        if current and ITEM_RE.match(line):
            current["items"].append(parse_item(line, line_number))

    if not phases or any(not phase["items"] for phase in phases):
        raise ValueError("Roadmap must contain phases with at least one item")

    for phase in phases:
        phase["status"] = status_from_items(phase["items"])

    all_items = [item for phase in phases for item in phase["items"]]
    roadmap = {
        "schemaVersion": 2,
        "version": version,
        "title": clean_markdown(title),
        "status": status_from_items(all_items),
        "summary": summary,
        "scope": scope,
        "phases": phases,
        "validation": parse_validation(lines[validation_start - 1:] if validation_start else []),
    }

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(roadmap, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Generated {OUTPUT.relative_to(ROOT)} from {SOURCE.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
