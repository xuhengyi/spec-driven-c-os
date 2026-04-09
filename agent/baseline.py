from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any
import ast
import os
import re
import selectors
import shlex
import shutil
import subprocess
import time
import tomllib

from tools.derive_shell_sequences import shell_sequence

from .config import Settings, chapter_number
from .state import read_json, utc_now, write_json

ANSI_ESCAPE_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
STRING_LITERAL_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


@dataclass(slots=True)
class BaselineResult:
    chapter: str
    success: bool
    summary: str
    meta_path: Path
    serial_log_path: Path
    case_summary_path: Path


@dataclass(slots=True)
class QemuCaptureResult:
    command: list[str]
    returncode: int
    stdout: str
    duration_seconds: float
    timed_out: bool
    terminated: bool


def preflight_baseline(settings: Settings, chapter: str, force: bool = False) -> BaselineResult:
    start_monotonic = time.monotonic()
    output_dir = settings.paths.baseline_root / chapter
    meta_path = output_dir / "meta.json"
    serial_log_path = output_dir / "serial.log"
    case_summary_path = output_dir / "case-summary.json"
    if not force and meta_path.exists():
        payload = read_json(meta_path, default={}) or {}
        if _should_reuse_cached_baseline(settings, chapter, payload):
            return BaselineResult(
                chapter=chapter,
                success=bool(payload.get("success")),
                summary=str(payload.get("summary", "cached baseline")),
                meta_path=meta_path,
                serial_log_path=serial_log_path,
                case_summary_path=case_summary_path,
            )

    output_dir.mkdir(parents=True, exist_ok=True)
    build_log_path = output_dir / "build.log"
    embedded_case_summary = settings.paths.runtime_baseline_root / chapter / "case-summary.json"
    embedded_serial_log = settings.paths.runtime_baseline_root / chapter / "serial.log"
    if not embedded_case_summary.exists():
        payload = {
            "chapter": chapter,
            "runtime_repo": str(settings.runtime_repo),
            "baseline_repo": str(settings.paths.runtime_baseline_root / chapter),
            "baseline_repo_kind": "embedded",
            "started_at_utc": utc_now(),
            "ended_at_utc": utc_now(),
            "duration_seconds": round(time.monotonic() - start_monotonic, 3),
            "command": [],
            "stdin_sequence": shell_sequence(chapter),
            "serial_log_path": str(serial_log_path),
            "case_summary_path": str(case_summary_path),
            "build_log_path": str(build_log_path),
            "success": False,
            "summary": f"embedded baseline missing: {embedded_case_summary}",
        }
        write_json(meta_path, payload)
        return BaselineResult(
            chapter=chapter,
            success=False,
            summary=str(payload["summary"]),
            meta_path=meta_path,
            serial_log_path=serial_log_path,
            case_summary_path=case_summary_path,
        )

    shutil.copy2(embedded_case_summary, case_summary_path)
    if embedded_serial_log.exists():
        shutil.copy2(embedded_serial_log, serial_log_path)
    elif not serial_log_path.exists():
        serial_log_path.write_text("", encoding="utf-8")
    build_log_path.write_text(
        "embedded baseline loaded from runtime-assets; no external build step required\n",
        encoding="utf-8",
    )
    runtime_artifacts = runtime_artifacts_for_chapter(settings.runtime_repo, chapter)
    effective_meta = {
        "chapter": chapter,
        "runtime_repo": str(settings.runtime_repo),
        "baseline_repo": str(settings.paths.runtime_baseline_root / chapter),
        "baseline_repo_kind": "embedded",
        "started_at_utc": utc_now(),
        "ended_at_utc": utc_now(),
        "duration_seconds": round(time.monotonic() - start_monotonic, 3),
        "command": [],
        "stdin_sequence": shell_sequence(chapter),
        "serial_log_path": str(serial_log_path),
        "case_summary_path": str(case_summary_path),
        "build_log_path": str(build_log_path),
        "app_asm_path": str(runtime_artifacts["app_asm"]),
        "fs_img_path": str(runtime_artifacts["fs_img"]) if runtime_artifacts.get("fs_img") else None,
        "success": True,
        "summary": "embedded baseline ready",
    }
    write_json(meta_path, effective_meta)
    return BaselineResult(
        chapter=chapter,
        success=bool(effective_meta.get("success")),
        summary=str(effective_meta.get("summary", "embedded baseline ready")),
        meta_path=meta_path,
        serial_log_path=serial_log_path,
        case_summary_path=case_summary_path,
    )


def _should_reuse_cached_baseline(settings: Settings, chapter: str, payload: dict[str, Any]) -> bool:
    if payload.get("success"):
        return True
    return False


def runtime_artifacts_for_chapter(repo_path: Path, chapter: str) -> dict[str, Path]:
    target_dir = repo_path / "user" / chapter
    payload = {
        "target_dir": target_dir,
        "app_asm": target_dir / "app.asm",
    }
    if chapter_number(chapter) >= 6:
        payload["fs_img"] = repo_path / "fs" / "fs.img"
    return payload


def run_qemu_capture(
    settings: Settings,
    repo_path: Path,
    kernel_bin: Path,
    chapter: str,
    *,
    fs_img_path: Path | None,
    input_lines: list[str],
) -> QemuCaptureResult:
    command = qemu_command(settings, repo_path, kernel_bin, chapter, fs_img_path=fs_img_path)
    start = time.monotonic()
    proc = subprocess.Popen(
        command,
        stdin=subprocess.PIPE if input_lines else subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    assert proc.stdout is not None
    os.set_blocking(proc.stdout.fileno(), False)
    selector = selectors.DefaultSelector()
    selector.register(proc.stdout, selectors.EVENT_READ)
    sent_input = False
    raw_output = bytearray()
    terminated = False
    timed_out = False
    start_monotonic = time.monotonic()
    kill_after: float | None = None
    payload = ("\n".join(input_lines) + "\n").encode("utf-8") if input_lines else b""

    try:
        while True:
            now = time.monotonic()
            if input_lines and not sent_input and now - start_monotonic >= settings.shell_startup_delay_seconds:
                assert proc.stdin is not None
                proc.stdin.write(payload)
                proc.stdin.flush()
                proc.stdin.close()
                sent_input = True
                kill_after = now + settings.shell_post_input_seconds

            events = selector.select(timeout=0.2)
            for key, _ in events:
                try:
                    chunk = os.read(key.fd, 4096)
                except BlockingIOError:
                    chunk = b""
                if chunk:
                    raw_output.extend(chunk)

            if proc.poll() is not None:
                while True:
                    try:
                        chunk = os.read(proc.stdout.fileno(), 4096)
                    except BlockingIOError:
                        break
                    if not chunk:
                        break
                    raw_output.extend(chunk)
                break

            if kill_after is not None and now >= kill_after:
                terminated = True
                _terminate_process(proc)
                continue

            if now - start_monotonic >= settings.qemu_timeout_seconds:
                timed_out = True
                _terminate_process(proc)
                continue
    finally:
        selector.close()

    return QemuCaptureResult(
        command=command,
        returncode=proc.returncode or 0,
        stdout=raw_output.decode("utf-8", errors="replace"),
        duration_seconds=time.monotonic() - start,
        timed_out=timed_out,
        terminated=terminated,
    )


def qemu_command(
    settings: Settings,
    repo_path: Path,
    kernel_bin: Path,
    chapter: str,
    *,
    fs_img_path: Path | None,
) -> list[str]:
    qemu = shutil.which("qemu-system-riscv64")
    if qemu is None:
        raise FileNotFoundError("qemu-system-riscv64 not found in PATH")
    command = [
        qemu,
        "-machine",
        "virt",
        "-nographic",
        "-bios",
        str(repo_path / "common" / "rustsbi-qemu.bin"),
        "-kernel",
        str(kernel_bin),
        "-smp",
        "1",
        "-m",
        "64M",
        "-serial",
        "mon:stdio",
    ]
    if chapter_number(chapter) >= 6 and fs_img_path is not None:
        command.extend(
            [
                "-drive",
                f"file={fs_img_path},if=none,format=raw,id=x0",
                "-device",
                "virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0",
            ]
        )
    return command


def build_case_summaries(
    chapter: str,
    log_text: str,
    *,
    cases_path: Path,
    user_root: Path,
) -> list[dict[str, Any]]:
    cases = _chapter_cases(cases_path, chapter)
    if chapter_number(chapter) == 2:
        return _build_batch_case_summaries(cases, log_text, user_root=user_root)
    if chapter_number(chapter) < 5:
        return _build_unordered_case_summaries(cases, log_text, user_root=user_root, chapter=chapter)
    summaries: list[dict[str, Any]] = []
    cursor = 0
    for order, case_name in enumerate(cases):
        patterns = derive_case_patterns(user_root, chapter, case_name, log_text)
        search_cursor = 0 if case_name in {"user_shell", "initproc"} else cursor
        positions: list[int] = []
        missing: list[str] = []
        for pattern in patterns:
            position = log_text.find(pattern, search_cursor)
            if position < 0:
                missing.append(pattern)
            else:
                positions.append(position)
                search_cursor = position + len(pattern)
        passed = bool(patterns) and not missing
        if passed and case_name not in {"user_shell", "initproc"}:
            cursor = search_cursor
        summaries.append(
            {
                "name": case_name,
                "order": order,
                "patterns": patterns,
                "positions": positions,
                "missing_patterns": missing,
                "passed": passed,
                "snippet": snippet_around(log_text, positions[0] if positions else cursor),
            }
        )
    return summaries


def derive_case_patterns(user_root: Path, chapter: str, case_name: str, log_text: str) -> list[str]:
    if case_name == "user_shell":
        return ["Rust user shell", ">> "]
    if case_name == "initproc":
        if "Loaded initproc" in log_text:
            return ["Loaded initproc"]
        if "initproc" in log_text:
            return ["initproc"]
        return ["Rust user shell"]

    patterns: list[str] = []
    case_anchor = 0
    if chapter_number(chapter) >= 5 and case_name in log_text:
        patterns.append(case_name)
        case_anchor = log_text.find(case_name)

    source_path = user_root / "src" / "bin" / f"{case_name}.rs"
    if source_path.exists():
        literals = _extract_string_literals(source_path)
        matched = [literal for literal in literals if literal and literal in log_text and literal not in patterns]
        matched.sort(key=lambda literal: _find_after(log_text, literal, case_anchor))
        for literal in matched:
            if len(patterns) >= 3:
                break
            if _is_informative_literal(literal):
                patterns.append(literal)

    if not patterns:
        fallback = FALLBACK_PATTERNS.get(case_name, [])
        patterns.extend(item for item in fallback if item in log_text)
    return patterns[:3]


def load_case_summary(path: Path) -> dict[str, Any]:
    return read_json(path, default={}) or {}


def case_summary_match_mode(chapter: str) -> str:
    if chapter_number(chapter) in {3, 4}:
        return "unordered"
    return "ordered"


def strip_ansi(text: str) -> str:
    return ANSI_ESCAPE_RE.sub("", text)


def snippet_around(text: str, position: int, width: int = 240) -> str:
    start = max(position - width // 3, 0)
    end = min(position + width, len(text))
    return text[start:end]


def _find_after(text: str, pattern: str, anchor: int) -> int:
    position = text.find(pattern, max(anchor, 0))
    if position >= 0:
        return position
    fallback = text.find(pattern)
    return len(text) if fallback < 0 else len(text) + fallback


def _run_command(command: list[str], *, cwd: Path, timeout_seconds: int) -> dict[str, Any]:
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
            check=False,
        )
        return {
            "command": command,
            "returncode": completed.returncode,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
            "timed_out": False,
        }
    except subprocess.TimeoutExpired as exc:
        return {
            "command": command,
            "returncode": -9,
            "stdout": exc.stdout or "",
            "stderr": exc.stderr or "",
            "timed_out": True,
        }


def _objcopy_kernel(input_elf: Path, output_bin: Path) -> Path:
    objcopy = shutil.which("riscv64-unknown-elf-objcopy") or shutil.which("rust-objcopy")
    if objcopy is None:
        raise FileNotFoundError("objcopy tool not found")
    subprocess.run(
        [objcopy, "-O", "binary", str(input_elf), str(output_bin)],
        check=True,
        capture_output=True,
        text=True,
    )
    return output_bin


def _terminate_process(proc: subprocess.Popen[bytes]) -> None:
    if proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=2)


def _chapter_cases(cases_path: Path, chapter: str) -> list[str]:
    payload = tomllib.loads(cases_path.read_text(encoding="utf-8-sig"))
    return [str(item) for item in payload.get(chapter, {}).get("cases", [])]


def _extract_string_literals(source_path: Path) -> list[str]:
    text = source_path.read_text(encoding="utf-8")
    literals: list[str] = []
    for raw in STRING_LITERAL_RE.findall(text):
        try:
            value = ast.literal_eval(f'"{raw}"')
        except Exception:
            value = raw
        if isinstance(value, str):
            normalized = value.replace("\r", "").strip()
            if normalized and "\0" not in normalized and normalized not in literals:
                literals.append(normalized)
    return literals


def _is_informative_literal(text: str) -> bool:
    stripped = text.strip()
    if len(stripped) < 4:
        return False
    if stripped in {"OK!", "FAILED!", "interrupt"}:
        return False
    alnum = sum(ch.isalnum() for ch in stripped)
    return alnum >= 3


FALLBACK_PATTERNS = {
    "threads_arg": ["thread#", "main thread exited."],
    "race_adder_mutex_blocking": ["time cost is"],
}


def _build_batch_case_summaries(cases: list[str], log_text: str, *, user_root: Path) -> list[dict[str, Any]]:
    summaries: list[dict[str, Any]] = []
    cursor = 0
    for order, case_name in enumerate(cases):
        load_position, next_position = _batch_segment_bounds(log_text, order, cursor)
        if load_position < 0:
            summaries.append(
                {
                    "name": case_name,
                    "order": order,
                    "patterns": [],
                    "positions": [],
                    "missing_patterns": [f"app#{order} segment"],
                    "passed": False,
                    "snippet": snippet_around(log_text, cursor),
                }
            )
            continue
        segment = log_text[load_position:next_position]
        patterns: list[str] = []
        source_strings = _extract_string_literals_for_case(user_root, case_name)
        informative_source_strings = [item for item in source_strings if _is_informative_literal(item)]
        matched_strings = [item for item in informative_source_strings if item in segment]
        matched_strings.sort(key=segment.find)
        patterns.extend(matched_strings[:2])
        outcome_token = _canonical_batch_outcome(segment)
        if outcome_token:
            patterns.append(outcome_token)
        patterns = [item for item in patterns if item]
        patterns = _unique_in_order(patterns)
        passed = bool(patterns)
        missing_patterns: list[str] = []
        if informative_source_strings and not matched_strings:
            passed = False
            missing_patterns.append(informative_source_strings[0])
        if not outcome_token and _batch_should_require_outcome(case_name, informative_source_strings):
            passed = False
            missing_patterns.append("batch outcome token")
        summaries.append(
            {
                "name": case_name,
                "order": order,
                "patterns": patterns,
                "positions": [load_position],
                "missing_patterns": missing_patterns,
                "passed": passed,
                "snippet": segment[:280],
            }
        )
        cursor = next_position
    return summaries


def _build_unordered_case_summaries(
    cases: list[str],
    log_text: str,
    *,
    user_root: Path,
    chapter: str,
) -> list[dict[str, Any]]:
    summaries: list[dict[str, Any]] = []
    for order, case_name in enumerate(cases):
        patterns = derive_case_patterns(user_root, chapter, case_name, log_text)
        positions: list[int] = []
        missing: list[str] = []
        for pattern in patterns:
            position = log_text.find(pattern)
            if position < 0:
                missing.append(pattern)
            else:
                positions.append(position)
        summaries.append(
            {
                "name": case_name,
                "order": order,
                "patterns": patterns,
                "positions": positions,
                "missing_patterns": missing,
                "passed": bool(patterns) and not missing,
                "snippet": snippet_around(log_text, positions[0] if positions else 0),
            }
        )
    return summaries


def _extract_string_literals_for_case(user_root: Path, case_name: str) -> list[str]:
    source_path = user_root / "src" / "bin" / f"{case_name}.rs"
    if source_path.exists():
        return _extract_string_literals(source_path)
    return []


def _batch_segment_bounds(log_text: str, order: int, cursor: int) -> tuple[int, int]:
    start_position = _find_first_any(
        log_text,
        [f"load app{order}", f"Running app {order}:"],
        cursor,
    )
    if start_position < 0:
        return -1, -1
    next_position = _find_first_any(
        log_text,
        [f"load app{order + 1}", f"Running app {order + 1}:"],
        start_position + 1,
    )
    if next_position < 0:
        next_position = len(log_text)
    return start_position, next_position


def _find_first_any(text: str, patterns: list[str], start: int) -> int:
    positions = [text.find(pattern, start) for pattern in patterns]
    positions = [position for position in positions if position >= 0]
    return min(positions) if positions else -1


def _canonical_batch_outcome(segment: str) -> str | None:
    exception_match = re.search(r"Exception\([^)]+\)", segment)
    if exception_match:
        return exception_match.group(0)
    code_match = re.search(r"code\s+-?\d+", segment)
    if code_match:
        return code_match.group(0)
    return None


def _batch_should_require_outcome(case_name: str, informative_source_strings: list[str]) -> bool:
    return bool(case_name and informative_source_strings)


def _unique_in_order(items: list[str]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for item in items:
        if item not in seen:
            seen.add(item)
            result.append(item)
    return result
