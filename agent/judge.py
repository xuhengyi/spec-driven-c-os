from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any
import shlex
import shutil
import subprocess

from .baseline import load_case_summary, qemu_command, run_qemu_capture, runtime_artifacts_for_chapter, strip_ansi
from .config import Settings, chapter_number
from .state import utc_now, write_json
from .workspace import TrialWorkspace

PREFERRED_CASE_PATTERNS: dict[str, list[str]] = {
    "12forktest": ["forktest pass."],
    "14forktest2": ["forktest2 test passed!"],
    "15matrix": ["fork ok.", "matrix passed."],
    "filetest_simple": ["file_test passed!"],
    "cat_filea": ["Hello, world!"],
}


@dataclass(slots=True)
class JudgeResult:
    chapter: str
    success: bool
    summary: str
    report_path: Path
    report_payload: dict[str, Any]


def run_judge(settings: Settings, workspace: TrialWorkspace) -> JudgeResult:
    chapter = workspace.chapter
    report_path = workspace.judge_dir / f"{chapter}.latest.json"
    started_at = utc_now()
    baseline_meta_path = settings.paths.baseline_root / chapter / "meta.json"
    case_summary_path = settings.paths.baseline_root / chapter / "case-summary.json"
    if not baseline_meta_path.exists() or not case_summary_path.exists():
        payload = _failure_payload(
            chapter,
            started_at,
            "baseline",
            "baseline metadata missing; run preflight-baseline first",
            baseline_summary_path=case_summary_path,
        )
        write_json(report_path, payload)
        return JudgeResult(chapter=chapter, success=False, summary=payload["summary"], report_path=report_path, report_payload=payload)

    c_port_root = workspace.workspace_dir / "c-port"
    if not c_port_root.exists():
        payload = _failure_payload(
            chapter,
            started_at,
            "workspace",
            "workspace/c-port is missing",
            baseline_summary_path=case_summary_path,
        )
        write_json(report_path, payload)
        return JudgeResult(chapter=chapter, success=False, summary=payload["summary"], report_path=report_path, report_payload=payload)

    build_dir = workspace.phase_dir / "build" / chapter
    if build_dir.exists():
        shutil.rmtree(build_dir)
    build_dir.mkdir(parents=True, exist_ok=True)
    artifact_dir = build_dir / "artifacts"
    artifact_dir.mkdir(parents=True, exist_ok=True)

    toolchain_file = c_port_root / "cmake" / "riscv64-toolchain.cmake"
    if not toolchain_file.exists():
        payload = _failure_payload(
            chapter,
            started_at,
            "configure",
            "workspace/c-port/cmake/riscv64-toolchain.cmake is missing",
            baseline_summary_path=case_summary_path,
        )
        write_json(report_path, payload)
        return JudgeResult(chapter=chapter, success=False, summary=payload["summary"], report_path=report_path, report_payload=payload)

    runtime_artifacts = runtime_artifacts_for_chapter(settings.runtime_repo, chapter)
    if not runtime_artifacts["app_asm"].exists():
        payload = _failure_payload(
            chapter,
            started_at,
            "runtime-assets",
            f"embedded app.asm is missing: {runtime_artifacts['app_asm']}",
            baseline_summary_path=case_summary_path,
        )
        write_json(report_path, payload)
        return JudgeResult(chapter=chapter, success=False, summary=payload["summary"], report_path=report_path, report_payload=payload)
    if chapter_number(chapter) >= 6 and not runtime_artifacts.get("fs_img", Path()).exists():
        payload = _failure_payload(
            chapter,
            started_at,
            "runtime-assets",
            f"embedded fs.img is missing: {runtime_artifacts.get('fs_img')}",
            baseline_summary_path=case_summary_path,
        )
        write_json(report_path, payload)
        return JudgeResult(chapter=chapter, success=False, summary=payload["summary"], report_path=report_path, report_payload=payload)

    configure_command = [
        shutil.which("cmake") or "cmake",
        "-S",
        str(c_port_root),
        "-B",
        str(build_dir),
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
        f"-DRCORE_CHAPTER={chapter}",
        f"-DAPP_ASM={runtime_artifacts['app_asm']}",
        f"-DRCORE_ARTIFACT_DIR={artifact_dir}",
    ]
    configure_result = _run_command(configure_command, timeout_seconds=settings.build_timeout_seconds)
    _write_command_log(workspace, chapter, "configure", configure_result)
    if configure_result["returncode"] != 0:
        payload = _failure_payload(
            chapter,
            started_at,
            "configure",
            "cmake configure failed",
            baseline_summary_path=case_summary_path,
            command_excerpt=format_command_result(configure_result),
            stderr_excerpt=str(configure_result["stderr"]).strip()[:4000],
        )
        write_json(report_path, payload)
        return JudgeResult(chapter=chapter, success=False, summary=payload["summary"], report_path=report_path, report_payload=payload)

    target_name = _resolve_target_name(build_dir, chapter)
    if target_name is None:
        payload = _failure_payload(
            chapter,
            started_at,
            "build",
            "no usable chapter kernel target was found after configure",
            baseline_summary_path=case_summary_path,
        )
        write_json(report_path, payload)
        return JudgeResult(chapter=chapter, success=False, summary=payload["summary"], report_path=report_path, report_payload=payload)

    build_command = [shutil.which("cmake") or "cmake", "--build", str(build_dir), "--target", target_name, "-j1"]
    build_result = _run_command(build_command, timeout_seconds=settings.build_timeout_seconds)
    _write_command_log(workspace, chapter, "build", build_result)
    if build_result["returncode"] != 0:
        payload = _failure_payload(
            chapter,
            started_at,
            "build",
            "cmake build failed",
            baseline_summary_path=case_summary_path,
            command_excerpt=format_command_result(build_result),
            stderr_excerpt=str(build_result["stderr"]).strip()[:4000],
        )
        write_json(report_path, payload)
        return JudgeResult(chapter=chapter, success=False, summary=payload["summary"], report_path=report_path, report_payload=payload)

    kernel_bin = artifact_dir / f"{chapter}.bin"
    if not kernel_bin.exists():
        payload = _failure_payload(
            chapter,
            started_at,
            "build",
            f"kernel binary not found at {kernel_bin}",
            baseline_summary_path=case_summary_path,
        )
        write_json(report_path, payload)
        return JudgeResult(chapter=chapter, success=False, summary=payload["summary"], report_path=report_path, report_payload=payload)

    qemu_capture = run_qemu_capture(
        settings,
        settings.runtime_repo,
        kernel_bin,
        chapter,
        fs_img_path=runtime_artifacts.get("fs_img"),
        input_lines=[] if chapter_number(chapter) < 5 else _chapter_input_lines(chapter),
    )
    serial_log_path = workspace.judge_dir / f"{chapter}.serial.log"
    normalized_log = strip_ansi(qemu_capture.stdout)
    serial_log_path.write_text(normalized_log, encoding="utf-8")

    baseline_summary = load_case_summary(case_summary_path)
    evaluation = evaluate_candidate_output(normalized_log, baseline_summary)
    if evaluation["success"]:
        payload = {
            "chapter": chapter,
            "success": True,
            "summary": "all chapter cases passed",
            "failure_kind": None,
            "first_failed_case": None,
            "missing_patterns": [],
            "unexpected_patterns": evaluation["unexpected_patterns"],
            "test_source_path": None,
            "serial_log_path": str(serial_log_path),
            "baseline_summary_path": str(case_summary_path),
            "started_at_utc": started_at,
            "ended_at_utc": utc_now(),
            "configure_command": configure_command,
            "build_command": build_command,
            "qemu_command": qemu_capture.command,
            "target_name": target_name,
        }
        write_json(report_path, payload)
        return JudgeResult(chapter=chapter, success=True, summary=payload["summary"], report_path=report_path, report_payload=payload)

    first_failed_case = evaluation["first_failed_case"]
    payload = _failure_payload(
        chapter,
        started_at,
        "validation",
        f"{first_failed_case} did not match the cached baseline signatures",
        baseline_summary_path=case_summary_path,
        first_failed_case=first_failed_case,
        missing_patterns=evaluation["missing_patterns"],
        unexpected_patterns=evaluation["unexpected_patterns"],
        serial_log_path=serial_log_path,
        test_source_path=settings.paths.user_root / "src" / "bin" / f"{first_failed_case}.rs" if first_failed_case else None,
        command_excerpt="\n".join(
            [
                f"$ {shlex.join(configure_command)}",
                f"$ {shlex.join(build_command)}",
                f"$ {shlex.join(qemu_capture.command)}",
            ]
        ),
    )
    write_json(report_path, payload)
    return JudgeResult(chapter=chapter, success=False, summary=payload["summary"], report_path=report_path, report_payload=payload)


def evaluate_candidate_output(log_text: str, baseline_summary: dict[str, Any]) -> dict[str, Any]:
    cases = list(baseline_summary.get("cases", []))
    match_mode = str(baseline_summary.get("match_mode", "ordered"))
    if match_mode == "unordered":
        return _evaluate_unordered_candidate_output(log_text, cases)
    cursor = 0
    unexpected_patterns = [token for token in ("panicked at", "assertion failed", "kernel panic") if token in log_text]
    for case in cases:
        case_name = str(case.get("name") or "")
        patterns = _preferred_case_patterns(case_name, case.get("patterns", []))
        if not patterns:
            return {
                "success": False,
                "first_failed_case": case_name,
                "missing_patterns": [],
                "unexpected_patterns": unexpected_patterns,
            }
        search_cursor = 0 if case_name in {"user_shell", "initproc"} else cursor
        missing: list[str] = []
        for pattern in patterns:
            position = log_text.find(pattern, search_cursor)
            if position < 0:
                missing.append(pattern)
            else:
                search_cursor = position + len(pattern)
        if missing:
            return {
                "success": False,
                "first_failed_case": case_name,
                "missing_patterns": missing,
                "unexpected_patterns": unexpected_patterns,
            }
        if case_name not in {"user_shell", "initproc"}:
            cursor = search_cursor
    if unexpected_patterns:
        return {
            "success": False,
            "first_failed_case": None,
            "missing_patterns": [],
            "unexpected_patterns": unexpected_patterns,
        }
    return {
        "success": True,
        "first_failed_case": None,
        "missing_patterns": [],
        "unexpected_patterns": [],
    }


def _preferred_case_patterns(case_name: str, fallback_patterns: Any) -> list[str]:
    patterns = [str(item) for item in fallback_patterns]
    preferred = PREFERRED_CASE_PATTERNS.get(case_name)
    if preferred is None:
        return patterns
    merged: list[str] = []
    if case_name and case_name not in {"user_shell", "initproc"}:
        merged.append(case_name)
    for item in preferred:
        if item not in merged:
            merged.append(item)
    return merged


def _evaluate_unordered_candidate_output(log_text: str, cases: list[dict[str, Any]]) -> dict[str, Any]:
    unexpected_patterns = [token for token in ("panicked at", "assertion failed", "kernel panic") if token in log_text]
    for case in cases:
        patterns = [str(item) for item in case.get("patterns", [])]
        if not patterns:
            return {
                "success": False,
                "first_failed_case": case.get("name"),
                "missing_patterns": [],
                "unexpected_patterns": unexpected_patterns,
            }
        missing = [pattern for pattern in patterns if pattern not in log_text]
        if missing:
            return {
                "success": False,
                "first_failed_case": str(case.get("name") or ""),
                "missing_patterns": missing,
                "unexpected_patterns": unexpected_patterns,
            }
    if unexpected_patterns:
        return {
            "success": False,
            "first_failed_case": None,
            "missing_patterns": [],
            "unexpected_patterns": unexpected_patterns,
        }
    return {
        "success": True,
        "first_failed_case": None,
        "missing_patterns": [],
        "unexpected_patterns": [],
    }


def format_command_result(result: dict[str, Any]) -> str:
    return "\n".join(
        [
            f"$ {shlex.join(result['command'])}",
            f"returncode={result['returncode']}",
            "stdout:",
            str(result["stdout"]).strip(),
            "stderr:",
            str(result["stderr"]).strip(),
        ]
    ).strip()


def _run_command(command: list[str], *, timeout_seconds: int, cwd: Path | None = None) -> dict[str, Any]:
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


def _resolve_target_name(build_dir: Path, chapter: str) -> str | None:
    help_result = _run_command(
        [shutil.which("cmake") or "cmake", "--build", str(build_dir), "--target", "help"],
        timeout_seconds=120,
    )
    targets = help_result["stdout"]
    preferred = [f"rcore_{chapter}_kernel", f"{chapter}_kernel"]
    for candidate in preferred:
        if candidate in targets:
            return candidate
    return None


def _write_command_log(workspace: TrialWorkspace, chapter: str, label: str, result: dict[str, Any]) -> None:
    path = workspace.judge_dir / f"{chapter}.{label}.log"
    path.write_text(format_command_result(result) + "\n", encoding="utf-8")


def _chapter_input_lines(chapter: str) -> list[str]:
    from tools.derive_shell_sequences import shell_sequence

    return shell_sequence(chapter)


def _failure_payload(
    chapter: str,
    started_at_utc: str,
    failure_kind: str,
    summary: str,
    *,
    baseline_summary_path: Path | None,
    first_failed_case: str | None = None,
    missing_patterns: list[str] | None = None,
    unexpected_patterns: list[str] | None = None,
    serial_log_path: Path | None = None,
    test_source_path: Path | None = None,
    command_excerpt: str | None = None,
    stderr_excerpt: str | None = None,
) -> dict[str, Any]:
    return {
        "chapter": chapter,
        "success": False,
        "summary": summary,
        "failure_kind": failure_kind,
        "first_failed_case": first_failed_case,
        "missing_patterns": missing_patterns or [],
        "unexpected_patterns": unexpected_patterns or [],
        "test_source_path": str(test_source_path) if test_source_path else None,
        "serial_log_path": str(serial_log_path) if serial_log_path else None,
        "baseline_summary_path": str(baseline_summary_path) if baseline_summary_path else None,
        "started_at_utc": started_at_utc,
        "ended_at_utc": utc_now(),
        "command_excerpt": command_excerpt,
        "stderr_excerpt": stderr_excerpt,
    }
