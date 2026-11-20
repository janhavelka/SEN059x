#!/usr/bin/env python3
"""Pack the PlatformIO library and optionally prepare it for a native ESP-IDF build."""

import argparse
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tarfile
from tempfile import TemporaryDirectory

import yaml


ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "scripts" / "fixtures" / "idf_package_consumer"


def package_files():
    library = json.loads((ROOT / "library.json").read_text(encoding="utf-8"))
    idf = yaml.safe_load((ROOT / "idf_component.yml").read_text(encoding="utf-8"))
    names = library["export"]["include"]
    if idf["files"] != {"exclude": ["**/*"], "include": names}:
        raise ValueError("The ESP-IDF and PlatformIO export lists differ")
    if idf["version"] != library["version"]:
        raise ValueError("The package versions differ")
    if len(set(names)) != len(names):
        raise ValueError("Duplicate paths in the package export list")
    required = {"library.json", "idf_component.yml", "CMakeLists.txt", "LICENSE"}
    required.update(f"include/{header}" for header in library["headers"])
    if not required.issubset(names):
        raise ValueError(f"Missing metadata or public headers: {sorted(required - set(names))}")

    sources = {}
    for name in names:
        path = PurePosixPath(name)
        if (path.is_absolute() or ".." in path.parts or path.as_posix() != name
                or any(char in name for char in "\\:*?[]")):
            raise ValueError(f"Expected an exact repository-relative file: {name!r}")
        sources[name] = (ROOT / name).read_bytes()
    return sources


def pack_and_check(archive):
    sources = package_files()
    pio = str(ROOT / "scripts" / "pio.cmd") if os.name == "nt" else "pio"
    subprocess.run([pio, "pkg", "pack", str(ROOT), "--output", str(archive)], check=True)
    files = {}
    with tarfile.open(archive, "r:gz") as packed:
        for member in packed:
            if member.isdir():
                continue
            if not member.isfile() or member.name not in sources or member.name in files:
                raise ValueError(f"Unexpected archive entry: {member.name}")
            with packed.extractfile(member) as content:
                files[member.name] = content.read()
    if files != sources:
        differences = [name for name, data in sources.items() if files.get(name) != data]
        raise ValueError(f"Missing or changed files in PlatformIO archive: {differences}")
    return files


def prepare_idf(destination, files):
    destination = destination.resolve()
    if destination.exists() and (not destination.is_dir() or any(destination.iterdir())):
        raise ValueError(f"Expected a new or empty fixture directory: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(dir=destination.parent) as temporary:
        project = Path(temporary) / "project"
        shutil.copytree(FIXTURE, project)
        for name, data in files.items():
            path = project / "components" / "sen059x" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        if destination.exists():
            destination.rmdir()  # Succeeds only if the directory is still empty.
        project.rename(destination)
    print(f"Prepared ESP-IDF consumer at {destination}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepare-idf", type=Path, metavar="DIRECTORY",
                        help="Place the verified package in an independent ESP-IDF consumer.")
    args = parser.parse_args()
    with TemporaryDirectory(prefix="sen059x-pack-") as temporary:
        files = pack_and_check(Path(temporary) / "sen059x.tar.gz")
    print(f"Verified {len(files)} archive entries against the source files and both manifests.")
    if args.prepare_idf:
        prepare_idf(args.prepare_idf, files)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError,
            tarfile.TarError, yaml.YAMLError) as error:
        sys.exit(f"Package check failed: {error}")
