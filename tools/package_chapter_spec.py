from __future__ import annotations

from argparse import ArgumentParser
from pathlib import Path
import re

CHAPTERS = [f"ch{index}" for index in range(2, 9)]

SPEC_ALIAS_MAP = {
    "console": "console",
    "rcore-console": "console",
    "linker": "linker",
    "kernel-alloc": "kernel-alloc",
    "kernel-context": "kernel-context",
    "kernel-vm": "kernel-vm",
    "syscall": "syscall",
    "task-manage": "task-manage",
    "rcore-task-manage": "task-manage",
    "rcore-task-manage-core": "task-manage-core",
    "rcore-task-manage-relations": "task-manage-relations",
    "easy-fs": "easy-fs",
    "easy-fs-storage": "easy-fs-storage",
    "easy-fs-vfs": "easy-fs-vfs",
    "signal": "signal",
    "signal-defs": "signal-defs",
    "signal-impl": "signal-impl",
    "sync": "sync",
    "sync-up-cell": "sync-up-cell",
    "sync-blocking-primitives": "sync-blocking-primitives",
    "syscall-abi": "syscall-abi",
    "syscall-kernel-dispatch": "syscall-kernel-dispatch",
    "syscall-user-api": "syscall-user-api",
    "kernel-context-foreign": "kernel-context-foreign",
    "kernel-context-local": "kernel-context-local",
}


def build_all_bundles(spec_root: Path, bundle_root: Path, chapters: list[str] | None = None) -> list[Path]:
    selected = chapters or CHAPTERS
    outputs: list[Path] = []
    for chapter in selected:
        outputs.append(build_chapter_bundle(spec_root, bundle_root, chapter))
    return outputs


def build_chapter_bundle(spec_root: Path, bundle_root: Path, chapter: str) -> Path:
    chapter_dir = spec_root / "specs" / chapter
    if not chapter_dir.exists():
        raise FileNotFoundError(f"missing chapter spec directory: {chapter_dir}")
    spec_text = (chapter_dir / "spec.md").read_text(encoding="utf-8")
    dependency_ids = extract_dependency_spec_ids(spec_text)
    parts = [
        f"# {chapter} bundle",
        "",
        "## Current Chapter",
        "",
        f"### {chapter}/spec.md",
        f"Source: spec/specs/{chapter}/spec.md",
        "",
        spec_text.rstrip(),
        "",
        "## Direct Dependency Specs",
        "",
    ]
    chapter_design = chapter_dir / "design.md"
    if chapter_design.exists():
        parts[8:8] = [
            f"### {chapter}/design.md",
            f"Source: spec/specs/{chapter}/design.md",
            "",
            chapter_design.read_text(encoding="utf-8").rstrip(),
            "",
        ]
    if not dependency_ids:
        parts.extend(["(No mapped direct dependency specs.)", ""])
    for spec_id in dependency_ids:
        dep_dir = spec_root / "specs" / spec_id
        if not dep_dir.exists():
            continue
        parts.extend(
            [
                f"### {spec_id}/spec.md",
                f"Source: spec/specs/{spec_id}/spec.md",
                "",
                (dep_dir / "spec.md").read_text(encoding="utf-8").rstrip(),
                "",
            ]
        )
        design_path = dep_dir / "design.md"
        if design_path.exists():
            parts.extend(
                [
                    f"### {spec_id}/design.md",
                    f"Source: spec/specs/{spec_id}/design.md",
                    "",
                    design_path.read_text(encoding="utf-8").rstrip(),
                    "",
                ]
            )
    output = bundle_root / f"{chapter}.bundle.md"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(parts), encoding="utf-8")
    return output


def extract_dependency_spec_ids(spec_text: str) -> list[str]:
    dependency_section = _dependency_section(spec_text)
    seen: set[str] = set()
    results: list[str] = []
    for token in re.findall(r"`([^`]+)`", dependency_section):
        mapped = SPEC_ALIAS_MAP.get(token.strip())
        if mapped and mapped not in seen:
            seen.add(mapped)
            results.append(mapped)
    return results


def _dependency_section(spec_text: str) -> str:
    marker = "## Dependencies"
    start = spec_text.find(marker)
    if start < 0:
        return ""
    next_heading = re.search(r"^##\s+", spec_text[start + len(marker) :], flags=re.MULTILINE)
    if not next_heading:
        return spec_text[start:]
    end = start + len(marker) + next_heading.start()
    return spec_text[start:end]


def main() -> int:
    parser = ArgumentParser(description="Build chapter bundle markdown files from copied OpenSpec sources.")
    parser.add_argument("--spec-root", type=Path, required=True)
    parser.add_argument("--bundle-root", type=Path, required=True)
    parser.add_argument("--chapters", nargs="*", default=CHAPTERS)
    args = parser.parse_args()
    build_all_bundles(args.spec_root, args.bundle_root, args.chapters)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
