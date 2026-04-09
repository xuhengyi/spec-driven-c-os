from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any
import re
import shutil

from .config import Settings, previous_chapter
from .state import read_json, utc_now, write_json

IGNORED_COPY_DIRS = {".git", "__pycache__", ".pytest_cache", "target"}

TRIAL_AGENTS = """# Automated Trial Instructions

- 这是 `new-phase-c` 的自动化章节级 trial。
- 只修改 `workspace/**` 与 `report.md`。
- 将 `.phase-c/spec/**` 和 `.phase-c/user/**` 视为只读输入。
- 只允许参考 `new-phase-c` 内部的 spec、测试和上一章已通过代码，不要读取外部仓库实现。
- 你可以读取 trial 下的全部代码和测试文件，不需要额外请求权限。
- 不要等待用户交互，不要提问，不要启动规划流程。
- 每一轮都必须用中文更新 `report.md`，并新增对应的 `## Round N` 段落。
"""


@dataclass(slots=True)
class TrialWorkspace:
    chapter: str
    root: Path
    workspace_dir: Path
    phase_dir: Path
    logs_dir: Path
    prompts_dir: Path
    judge_dir: Path
    report_path: Path
    state_path: Path


def init_trial(settings: Settings, chapter: str, force: bool = False) -> TrialWorkspace:
    started_at = utc_now()
    root = settings.paths.trials_root / chapter
    if root.exists():
        if not force:
            raise FileExistsError(f"trial already exists: {root}")
        shutil.rmtree(root)
    root.mkdir(parents=True, exist_ok=True)
    workspace = load_trial(settings, chapter)
    _install_frozen_inputs(settings, workspace)
    _write_agents_file(workspace)
    _write_initial_report(settings, workspace, started_at=started_at)
    write_json(
        workspace.state_path,
        {
            "chapter": chapter,
            "status": "initialized",
            "current_round": 0,
            "started_at_utc": started_at,
            "bundle_path": str(workspace.phase_dir / "spec" / "bundles" / f"{chapter}.bundle.md"),
            "baseline_meta_path": None,
            "last_failed_case": None,
            "last_serial_log_path": None,
            "report_path": str(workspace.report_path),
            "updated_at": utc_now(),
        },
    )
    return workspace


def load_trial(settings: Settings, chapter: str) -> TrialWorkspace:
    root = settings.paths.trials_root / chapter
    if not root.exists():
        raise FileNotFoundError(f"trial does not exist: {root}")
    workspace_dir = root / "workspace"
    phase_dir = root / ".phase-c"
    logs_dir = phase_dir / "logs"
    prompts_dir = phase_dir / "prompts"
    judge_dir = phase_dir / "judge"
    for path in (workspace_dir, phase_dir, logs_dir, prompts_dir, judge_dir):
        path.mkdir(parents=True, exist_ok=True)
    return TrialWorkspace(
        chapter=chapter,
        root=root,
        workspace_dir=workspace_dir,
        phase_dir=phase_dir,
        logs_dir=logs_dir,
        prompts_dir=prompts_dir,
        judge_dir=judge_dir,
        report_path=root / "report.md",
        state_path=phase_dir / "state.json",
    )


def load_state(workspace: TrialWorkspace) -> dict[str, Any]:
    return read_json(workspace.state_path, default={}) or {}


def patch_state(workspace: TrialWorkspace, **updates: Any) -> dict[str, Any]:
    state = load_state(workspace)
    state.update(updates)
    state["updated_at"] = utc_now()
    write_json(workspace.state_path, state)
    return state


def validate_round_report(report_path: Path, round_index: int, previous_text: str) -> bool:
    if not report_path.exists():
        return False
    current = report_path.read_text(encoding="utf-8")
    if current == previous_text:
        return False
    return re.search(rf"^## Round {round_index}\s*$", current, flags=re.MULTILINE) is not None


def append_final_report(
    workspace: TrialWorkspace,
    *,
    ended_at: str,
    duration_seconds: float,
    passed_cases: list[str],
    conclusion: str,
) -> None:
    text = workspace.report_path.read_text(encoding="utf-8") if workspace.report_path.exists() else ""
    final_section = "\n".join(
        [
            "## 最终结果",
            f"- 结束时间: {ended_at}",
            f"- 总耗时: {duration_seconds:.3f} 秒",
            f"- 最终通过的 case 列表: {', '.join(passed_cases)}",
            f"- 当前章节结论: {conclusion}",
            "",
        ]
    )
    pattern = re.compile(r"^## 最终结果\s*$", flags=re.MULTILINE)
    if pattern.search(text):
        text = pattern.split(text, maxsplit=1)[0].rstrip() + "\n\n" + final_section
    else:
        text = text.rstrip() + "\n\n" + final_section
    workspace.report_path.write_text(text, encoding="utf-8")


def append_intervention_record(
    workspace: TrialWorkspace,
    *,
    title: str,
    details: list[str],
    recorded_at: str | None = None,
) -> None:
    text = workspace.report_path.read_text(encoding="utf-8") if workspace.report_path.exists() else ""
    stamp = recorded_at or utc_now()
    section = "\n".join(
        [
            "## 人工介入记录",
            f"### {title}",
            f"- 时间: {stamp}",
            *[f"- {detail}" for detail in details],
            "",
        ]
    )
    pattern = re.compile(r"^## 最终结果\s*$", flags=re.MULTILINE)
    if pattern.search(text):
        head, tail = pattern.split(text, maxsplit=1)
        workspace.report_path.write_text(
            head.rstrip() + "\n\n" + section + "## 最终结果\n" + tail.lstrip("\n"),
            encoding="utf-8",
        )
        return
    workspace.report_path.write_text(text.rstrip() + "\n\n" + section, encoding="utf-8")


def _install_frozen_inputs(settings: Settings, workspace: TrialWorkspace) -> None:
    spec_dst = workspace.phase_dir / "spec"
    user_dst = workspace.phase_dir / "user"
    for dst, src in ((spec_dst, settings.paths.spec_root), (user_dst, settings.paths.user_root)):
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(src, dst, ignore=_ignore_copy_dirs)


def _ignore_copy_dirs(directory: str, names: list[str]) -> set[str]:
    return {name for name in names if name in IGNORED_COPY_DIRS}


def _write_agents_file(workspace: TrialWorkspace) -> None:
    (workspace.root / "AGENTS.md").write_text(TRIAL_AGENTS, encoding="utf-8")


def _write_initial_report(settings: Settings, workspace: TrialWorkspace, *, started_at: str) -> None:
    previous = previous_chapter(workspace.chapter)
    previous_line = (
        f"- 上一章代码来源路径: {settings.paths.trials_root / previous / 'workspace' / 'c-port'}"
        if previous
        else "- 上一章代码来源路径: 无"
    )
    bundle_path = workspace.phase_dir / "spec" / "bundles" / f"{workspace.chapter}.bundle.md"
    content = "\n".join(
        [
            f"# {workspace.chapter} 实现报告",
            "",
            "## 基本信息",
            f"- 章节: {workspace.chapter}",
            f"- 开始时间: {started_at}",
            f"- 工作目录: {workspace.workspace_dir}",
            f"- spec bundle 路径: {bundle_path}",
            previous_line,
            "",
        ]
    )
    workspace.report_path.write_text(content, encoding="utf-8")
