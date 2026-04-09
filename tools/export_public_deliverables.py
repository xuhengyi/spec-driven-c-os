from __future__ import annotations

import hashlib
import json
import re
import shutil
from pathlib import Path

CHAPTERS = [f"ch{index}" for index in range(2, 9)]

ROOT = Path(__file__).resolve().parents[1]
DELIVERABLE_ROOT = ROOT / "deliverables"
SOURCE_ARTIFACT_ROOT = ROOT / "artifacts" / "state" / "manual-verify-builds"
SUMMARY_PATH = ROOT / "artifacts" / "state" / "manual-verification-summary.json"
REPORT_ROOT = ROOT / "trial-workspaces"

BYTE_REPLACEMENTS = [
    (b"/home/xu-hy22/", b"/repo-root___/"),
    (b"Graduation/", b"workspace_/"),
    (b"new-phase-c/", b"public-repo/"),
]


def parse_report(report_path: Path) -> dict[str, object]:
    text = report_path.read_text(encoding="utf-8")
    ended_at = _extract_line_value(text, "结束时间")
    duration = _extract_line_value(text, "总耗时")
    cases_text = _extract_line_value(text, "最终通过的 case 列表")
    conclusion = _normalize_conclusion(_extract_line_value(text, "当前章节结论"))
    cases = [item.strip() for item in cases_text.split(",") if item.strip()]
    return {
        "ended_at": ended_at,
        "duration": duration,
        "cases": cases,
        "conclusion": conclusion,
    }


def _extract_line_value(text: str, label: str) -> str:
    match = re.search(rf"^- {re.escape(label)}: (.+)$", text, flags=re.MULTILINE)
    if not match:
        raise ValueError(f"missing field '{label}' in report")
    return match.group(1).strip()


def _normalize_conclusion(text: str) -> str:
    return text.replace("外部 judge", "章节 judge")


def sanitize_bytes(data: bytes) -> bytes:
    for old, new in BYTE_REPLACEMENTS:
        if len(old) != len(new):
            raise ValueError(f"replacement length mismatch: {old!r} -> {new!r}")
        data = data.replace(old, new)
    return data


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_binary_copy(src: Path, dst: Path) -> None:
    data = sanitize_bytes(src.read_bytes())
    dst.write_bytes(data)


def write_text(path: Path, text: str) -> None:
    path.write_text(text.rstrip() + "\n", encoding="utf-8")


def boot_command(chapter: str) -> str:
    lines = [
        "qemu-system-riscv64 \\",
        "  -machine virt \\",
        "  -nographic \\",
        "  -bios \"runtime-assets/common/rustsbi-qemu.bin\" \\",
        f"  -kernel \"deliverables/chapters/{chapter}/{chapter}.bin\" \\",
        "  -smp 1 \\",
        "  -m 64M \\",
    ]
    if int(chapter[2:]) >= 6:
        lines.extend(
            [
                "  -serial mon:stdio \\",
                "  -drive \"file=runtime-assets/fs/fs.img,if=none,format=raw,id=x0\" \\",
                "  -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0",
            ]
        )
    else:
        lines.append("  -serial mon:stdio")
    return "\n".join(lines)


def verify_command(chapter: str) -> str:
    return f"python3 -m agent.cli verify-chapter {chapter}"


def build_command(chapter: str) -> str:
    return "\n".join(
        [
            "cmake -S \"trial-workspaces/{chapter}/workspace/c-port\" \\".format(chapter=chapter),
            "  -B \"out/{chapter}\" \\".format(chapter=chapter),
            "  -DCMAKE_TOOLCHAIN_FILE=\"trial-workspaces/{chapter}/workspace/c-port/cmake/riscv64-toolchain.cmake\" \\".format(chapter=chapter),
            f"  -DRCORE_CHAPTER={chapter} \\",
            "  -DAPP_ASM=\"$PWD/runtime-assets/user/{chapter}/app.asm\" \\".format(chapter=chapter),
            "  -DRCORE_ARTIFACT_DIR=\"$PWD/out/{chapter}/artifacts\"".format(chapter=chapter),
            "",
            "cmake --build \"out/{chapter}\" --target rcore_{chapter}_kernel -j1".format(chapter=chapter),
        ]
    )


def export_chapter(chapter: str, summary: dict[str, object]) -> dict[str, object]:
    source_dir = SOURCE_ARTIFACT_ROOT / chapter / "artifacts"
    report = parse_report(REPORT_ROOT / chapter / "report.md")
    output_dir = DELIVERABLE_ROOT / "chapters" / chapter
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    copied = []
    for suffix in ("bin", "elf"):
        src = source_dir / f"{chapter}.{suffix}"
        dst = output_dir / src.name
        write_binary_copy(src, dst)
        copied.append(dst.name)

    map_src = source_dir / f"{chapter}.map"
    map_dst = output_dir / map_src.name
    shutil.copy2(map_src, map_dst)
    copied.append(map_dst.name)

    manifest = {
        "chapter": chapter,
        "success": bool(summary["success"]),
        "duration_seconds": summary["duration_seconds"],
        "terminated": summary["terminated"],
        "timed_out": summary["timed_out"],
        "cases": report["cases"],
        "case_count": len(report["cases"]),
        "ended_at": report["ended_at"],
        "duration_text": report["duration"],
        "conclusion": report["conclusion"],
        "source_dir": f"trial-workspaces/{chapter}/workspace/c-port",
        "deliverable_dir": f"deliverables/chapters/{chapter}",
        "files": copied,
        "sha256": {name: sha256_file(output_dir / name) for name in copied},
    }
    write_text(output_dir / "manifest.json", json.dumps(manifest, ensure_ascii=False, indent=2))

    return manifest


def main() -> int:
    if not SUMMARY_PATH.exists() or not SOURCE_ARTIFACT_ROOT.exists():
        raise SystemExit(
            "source artifacts for deliverable export are not present in the slim public snapshot; "
            "regenerate them locally before rerunning tools/export_public_deliverables.py"
        )
    summary = json.loads(SUMMARY_PATH.read_text(encoding="utf-8"))
    root_payload = {}
    for chapter in CHAPTERS:
        root_payload[chapter] = export_chapter(chapter, summary[chapter])

    overview = "\n".join(
        [
            "# Deliverables",
            *[
                "- `{chapter}`: `{count}` cases passed, source at `trial-workspaces/{chapter}/workspace/c-port`, deliverables at `deliverables/chapters/{chapter}`".format(
                    chapter=chapter,
                    count=root_payload[chapter]["case_count"],
                )
                for chapter in CHAPTERS
            ],
        ]
    )
    write_text(DELIVERABLE_ROOT / "README.md", overview)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
