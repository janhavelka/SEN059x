#!/usr/bin/env python3
"""Synchronize Version.h, Doxyfile and IDF manifest versions from library.json.

Default behavior:
- when run by PlatformIO as an extra script: sync version outputs if needed and
  inject build metadata defines into the compile environment
- when run standalone: sync version outputs if needed

Standalone commands:
  sync
      Synchronize Version.h, Doxyfile PROJECT_NUMBER and idf_component.yml.
  check
      Exit with code 1 when a version output is out of date; do not write.
  bump patch|minor|major
      Update library.json, then synchronize all version outputs.
  set X.Y.Z
      Set an explicit semantic version, then synchronize all version outputs.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Dict, List, Optional, Tuple

ENV = None
try:
    Import("env")  # type: ignore[name-defined]  # PlatformIO / SCons built-in
    ENV = env  # type: ignore[name-defined]
except Exception:
    ENV = None

# Both package managers accept numeric versions and SemVer prereleases.
NUMBER = r"(?:0|[1-9][0-9]*)"
IDENTIFIER = rf"(?:{NUMBER}|[0-9]*[A-Za-z-][0-9A-Za-z-]*)"
SEMVER_RE = re.compile(
    rf"({NUMBER})\.({NUMBER})\.({NUMBER})(?:-{IDENTIFIER}(?:\.{IDENTIFIER})*)?"
)


def _find_project_root() -> Path:
    if ENV is not None:
        return Path(ENV["PROJECT_DIR"]).resolve()
    return Path(__file__).resolve().parent.parent


def _read_text(path: Path) -> str:
    with open(path, "r", encoding="utf-8") as handle:
        return handle.read()


def _write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(dir=path.parent) as staging:
        temporary = Path(staging) / path.name
        with temporary.open("w", encoding="utf-8", newline="\n") as handle:
            handle.write(content)
        temporary.replace(path)


def _load_library_json(path: Path) -> Dict[str, object]:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def _write_library_json(path: Path, data: Dict[str, object]) -> None:
    _write_text(path, json.dumps(data, indent=2) + "\n")


def _parse_semver(version: str) -> Tuple[int, int, int]:
    match = SEMVER_RE.fullmatch(version)
    if not match:
        raise ValueError(f"Invalid semantic version: {version}")
    return tuple(int(part) for part in match.groups())


def _bump_semver(version: str, part: str) -> str:
    major, minor, patch = _parse_semver(version)
    if part == "major":
        return f"{major + 1}.0.0"
    if part == "minor":
        return f"{major}.{minor + 1}.0"
    if part == "patch":
        return f"{major}.{minor}.{patch + 1}"
    raise ValueError(f"Unknown bump target: {part}")


def _resolve_namespace_dir(project_root: Path) -> Path:
    include_root = project_root / "include"
    if not include_root.exists():
        raise FileNotFoundError(f"Missing include directory: {include_root}")

    candidates = [path for path in include_root.iterdir() if path.is_dir()]
    version_candidates = [path for path in candidates if (path / "Version.h").exists()]

    if len(version_candidates) == 1:
        return version_candidates[0]
    if len(candidates) == 1:
        return candidates[0]
    if len(version_candidates) > 1:
        raise RuntimeError(
            f"Multiple Version.h candidates under {include_root}: "
            + ", ".join(str(path) for path in version_candidates)
        )
    raise RuntimeError(
        f"Unable to resolve namespace directory under {include_root}; found: "
        + ", ".join(path.name for path in candidates)
    )


def _macro_prefix(namespace: str) -> str:
    return "".join(ch if ch.isalnum() else "_" for ch in namespace).upper()


def _get_git_info(project_root: Path) -> Tuple[str, str]:
    try:
        commit_result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=project_root,
            capture_output=True,
            text=True,
            timeout=2,
            check=False,
        )
        commit = commit_result.stdout.strip() if commit_result.returncode == 0 else "unknown"

        status_result = subprocess.run(
            ["git", "status", "--porcelain", "--untracked-files=no"],
            cwd=project_root,
            capture_output=True,
            text=True,
            timeout=2,
            check=False,
        )
        git_status = "dirty" if status_result.stdout.strip() else "clean"
        return commit or "unknown", git_status
    except Exception:
        return "unknown", "unknown"


def _cpp_string_literal(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'\\"{escaped}\\"'


def _append_build_metadata_defines(namespace: str, project_root: Path) -> None:
    if ENV is None:
        return

    git_commit, git_status = _get_git_info(project_root)
    prefix = _macro_prefix(namespace)

    ENV.Append(
        CPPDEFINES=[
            (f"{prefix}_GIT_COMMIT", _cpp_string_literal(git_commit)),
            (f"{prefix}_GIT_STATUS", _cpp_string_literal(git_status)),
        ]
    )


def _render_version_header(namespace: str, version: str) -> str:
    major, minor, patch = _parse_semver(version)
    version_code = major * 10000 + minor * 100 + patch
    prefix = _macro_prefix(namespace)

    return f'''/**
 * @file Version.h
 * @brief Version information for {namespace}.
 *
 * This file is AUTO-GENERATED by scripts/generate_version.py from library.json.
 * DO NOT EDIT MANUALLY. Update version metadata via library.json or
 * scripts/generate_version.py bump/set commands.
 */

#pragma once

#include <stdint.h>

#ifndef {prefix}_VERSION_STRING
#define {prefix}_VERSION_STRING "{version}"
#endif

#ifndef {prefix}_BUILD_DATE
#define {prefix}_BUILD_DATE __DATE__
#endif

#ifndef {prefix}_BUILD_TIME
#define {prefix}_BUILD_TIME __TIME__
#endif

#ifndef {prefix}_BUILD_TIMESTAMP
#define {prefix}_BUILD_TIMESTAMP {prefix}_BUILD_DATE " " {prefix}_BUILD_TIME
#endif

#ifndef {prefix}_GIT_COMMIT
#define {prefix}_GIT_COMMIT "unknown"
#endif

#ifndef {prefix}_GIT_STATUS
#define {prefix}_GIT_STATUS "unknown"
#endif

#ifndef {prefix}_VERSION_FULL
#define {prefix}_VERSION_FULL {prefix}_VERSION_STRING " (" {prefix}_GIT_COMMIT ", " {prefix}_BUILD_TIMESTAMP ", " {prefix}_GIT_STATUS ")"
#endif

namespace {namespace} {{

/// @brief Major version (breaking changes).
static constexpr uint16_t VERSION_MAJOR = {major};

/// @brief Minor version (backward-compatible features).
static constexpr uint16_t VERSION_MINOR = {minor};

/// @brief Patch version (backward-compatible fixes).
static constexpr uint16_t VERSION_PATCH = {patch};

/// @brief Full semantic version string.
static constexpr const char* VERSION = {prefix}_VERSION_STRING;

/// @brief Numeric MAJOR*10000 + MINOR*100 + PATCH. See VERSION for prerelease labels.
static constexpr uint32_t VERSION_CODE = {version_code};

/// @brief Backward-compatible alias used by older repositories.
static constexpr int VERSION_INT = {version_code};

/// @brief Build date string.
static constexpr const char* BUILD_DATE = {prefix}_BUILD_DATE;

/// @brief Build time string.
static constexpr const char* BUILD_TIME = {prefix}_BUILD_TIME;

/// @brief Build timestamp string.
static constexpr const char* BUILD_TIMESTAMP = {prefix}_BUILD_TIMESTAMP;

/// @brief Git commit string.
static constexpr const char* GIT_COMMIT = {prefix}_GIT_COMMIT;

/// @brief Git working tree status string.
static constexpr const char* GIT_STATUS = {prefix}_GIT_STATUS;

/// @brief Full version string with build metadata.
static constexpr const char* VERSION_FULL = {prefix}_VERSION_FULL;

}}  // namespace {namespace}
'''


def _render_doxyfile(current: str, version: str) -> str:
    # Preserve other settings, indentation, and any inline comment. Requiring
    # one assignment also prevents a later duplicate from overriding the value.
    expected, count = re.subn(
        r'^([ \t]*PROJECT_NUMBER[ \t]*=[ \t]*)(?:"[^"\n]*"|[^#\n]*?)([ \t]*(?:#[^\n]*)?)$',
        lambda match: f'{match.group(1)}"{version}"{match.group(2)}',
        current,
        flags=re.MULTILINE,
    )
    if count != 1:
        raise ValueError("Doxyfile must contain exactly one PROJECT_NUMBER assignment")
    return expected


def _render_idf_manifest(current: str, version: str) -> str:
    # Only the root version belongs to this generator. Preserve dependency
    # versions, export filters, comments and all other manually reviewed fields.
    expected, count = re.subn(
        r'^(version[ \t]*:[ \t]*)(?:"[^"\n]*"|\'[^\'\n]*\'|[^#\n]*?)([ \t]*(?:#[^\n]*)?)$',
        lambda match: f'{match.group(1)}"{version}"{match.group(2)}',
        current,
        flags=re.MULTILINE,
    )
    if count != 1:
        raise ValueError("idf_component.yml must contain exactly one root version assignment")
    return expected


def _expected_outputs(project_root: Path, version: Optional[str] = None) -> Dict[Path, str]:
    if version is None:
        library_data = _load_library_json(project_root / "library.json")
        version = str(library_data["version"])
    namespace_dir = _resolve_namespace_dir(project_root)
    namespace = namespace_dir.name

    outputs = {
        namespace_dir / "Version.h": _render_version_header(namespace, version),
    }
    doxyfile = project_root / "Doxyfile"
    outputs[doxyfile] = _render_doxyfile(_read_text(doxyfile), version)
    manifest = project_root / "idf_component.yml"
    outputs[manifest] = _render_idf_manifest(_read_text(manifest), version)

    return outputs


def _sync_outputs(project_root: Path, check_only: bool, quiet: bool) -> bool:
    outputs = _expected_outputs(project_root)
    clean = True

    for path, expected in outputs.items():
        current = _read_text(path) if path.exists() else None
        if current == expected:
            if not quiet:
                print(f"Up to date: {path}")
            continue

        clean = False
        if check_only:
            print(f"Out of date: {path}")
            continue

        _write_text(path, expected)
        print(f"Updated: {path}")

    return clean


def _set_version(project_root: Path, new_version: str) -> str:
    _parse_semver(new_version)
    library_json = project_root / "library.json"
    data = _load_library_json(library_json)
    # Reject malformed output templates before changing the version source.
    # This also validates rendering for the candidate version without writing.
    _expected_outputs(project_root, new_version)
    data["version"] = new_version
    _write_library_json(library_json, data)
    return new_version


def _usage() -> str:
    return (
        "Usage:\n"
        "  scripts/generate_version.py sync\n"
        "  scripts/generate_version.py check\n"
        "  scripts/generate_version.py bump patch|minor|major\n"
        "  scripts/generate_version.py set X.Y.Z"
    )


def main(args: List[str]) -> int:
    project_root = _find_project_root()
    namespace = _resolve_namespace_dir(project_root).name
    _append_build_metadata_defines(namespace, project_root)

    if not args:
        _sync_outputs(project_root, check_only=False, quiet=True)
        return 0

    command = args[0]
    if command == "sync":
        _sync_outputs(project_root, check_only=False, quiet=False)
        return 0
    if command == "check":
        return 0 if _sync_outputs(project_root, check_only=True, quiet=False) else 1
    if command == "bump":
        if len(args) != 2:
            print(_usage(), file=sys.stderr)
            return 2
        current_version = str(_load_library_json(project_root / "library.json").get("version", "0.0.0"))
        new_version = _bump_semver(current_version, args[1])
        _set_version(project_root, new_version)
        _sync_outputs(project_root, check_only=False, quiet=False)
        print(f"Version bumped: {current_version} -> {new_version}")
        return 0
    if command == "set":
        if len(args) != 2:
            print(_usage(), file=sys.stderr)
            return 2
        new_version = _set_version(project_root, args[1])
        _sync_outputs(project_root, check_only=False, quiet=False)
        print(f"Version set: {new_version}")
        return 0

    print(_usage(), file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

if ENV is not None:
    result = main([])
    if result != 0:
        raise SystemExit(result)
