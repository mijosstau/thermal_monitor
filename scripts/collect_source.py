#!/usr/bin/env python3
"""Collect project source files into one text file with file-name headers."""

from __future__ import annotations

import argparse
from pathlib import Path


DEFAULT_OUTPUT = "source_bundle.txt"

INCLUDED_SUFFIXES = {
    ".c",
    ".cpp",
    ".h",
    ".hpp",
    ".cmake",
    ".md",
    ".py",
    ".sh",
    ".template",
    ".txt",
}

INCLUDED_NAMES = {
    "CMakeLists.txt",
    "Makefile",
    "meta-data",
}

EXCLUDED_DIRS = {
    ".git",
    ".codex",
    ".agents",
    ".claude",
    "build",
    "__pycache__",
}

EXCLUDED_FILES = {
    DEFAULT_OUTPUT,
    "CMakeLists.txt.user",
}


def should_include(path: Path, root: Path, output: Path) -> bool:
    rel = path.relative_to(root)
    if any(part in EXCLUDED_DIRS for part in rel.parts):
        return False
    if path.resolve() == output.resolve():
        return False
    if path.name in EXCLUDED_FILES:
        return False
    return path.name in INCLUDED_NAMES or path.suffix in INCLUDED_SUFFIXES


def collect_files(root: Path, output: Path) -> list[Path]:
    return sorted(
        path
        for path in root.rglob("*")
        if path.is_file() and should_include(path, root, output)
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Collect project source files into one text file."
    )
    parser.add_argument(
        "-o",
        "--output",
        default=DEFAULT_OUTPUT,
        help=f"output file path, default: {DEFAULT_OUTPUT}",
    )
    parser.add_argument(
        "--root",
        default=".",
        help="project root, default: current directory",
    )
    args = parser.parse_args()

    root = Path(args.root).resolve()
    output = Path(args.output)
    if not output.is_absolute():
        output = root / output

    files = collect_files(root, output)

    with output.open("w", encoding="utf-8") as out:
        out.write(f"Source bundle for: {root}\n")
        out.write(f"Files: {len(files)}\n\n")

        for path in files:
            rel = path.relative_to(root)
            out.write("=" * 100 + "\n")
            out.write(f"FILE: {rel}\n")
            out.write("=" * 100 + "\n\n")
            try:
                out.write(path.read_text(encoding="utf-8"))
            except UnicodeDecodeError:
                out.write(path.read_text(encoding="utf-8", errors="replace"))
            out.write("\n\n")

    print(f"Wrote {len(files)} files to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
