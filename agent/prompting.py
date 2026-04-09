from __future__ import annotations

from pathlib import Path
from typing import Any
import tomllib

from .config import Settings, chapter_number, previous_chapter
from .workspace import TrialWorkspace


def build_prompt(
    settings: Settings,
    workspace: TrialWorkspace,
    *,
    round_index: int,
    previous_failure: dict[str, Any] | None,
) -> str:
    if previous_failure is None:
        return build_seed_prompt(settings, workspace, round_index=round_index)
    return build_repair_prompt(
        settings,
        workspace,
        round_index=round_index,
        previous_failure=previous_failure,
    )


def build_seed_prompt(settings: Settings, workspace: TrialWorkspace, *, round_index: int) -> str:
    chapter = workspace.chapter
    bundle_path = workspace.phase_dir / "spec" / "bundles" / f"{chapter}.bundle.md"
    cases = _chapter_cases(workspace.phase_dir / "user" / "cases.toml", chapter)
    lines = [
        f"你正在实现 `new-phase-c` 的 `{chapter}` 章节级 C 内核。",
        f"当前轮次：Round {round_index}",
        f"当前工作目录：{workspace.workspace_dir}",
        f"当前报告路径：{workspace.report_path}",
        "",
        "显式 spec 输入只读文件：",
        f"- {bundle_path}",
        "",
        "成功标准：",
        "- 输出一个完整的 `workspace/c-port`。",
        f"- 跑通本章用户态测例：{', '.join(cases)}。",
        f"- judge 会在 `workspace/c-port` 上执行 CMake，并传入 `-DRCORE_CHAPTER={chapter}` 与 `-DAPP_ASM=<repo runtime-assets/user/{chapter}/app.asm>`。",
        "- 章节内核需要能通过 `rcore_<chapter>_kernel` 或 `<chapter>_kernel` 目标构建。",
        "",
        "执行约束：",
        "- 你可以读取 trial 内全部代码和测试文件。",
        "- 只允许将 `.phase-c/spec/**`、`.phase-c/user/**` 与 `new-phase-c` 内上一章已通过的 `workspace/c-port` 作为实现参考。",
        "- 不要读取或参考 `new-phase-c` 目录之外的任何 Rust/C 实现、旧实验目录或其他仓库代码。",
        "- 不要等待交互，不要提问，不要输出计划文档。",
        "- 每轮都必须用中文更新 `report.md`。",
        f"- 本轮必须新增 `## Round {round_index}` 段落。",
        "- `## Round N` 下必须包含：`### 本轮目标`、`### 新建/复制/修改的文件`、`### 遇到的问题`、`### 原因分析`、`### 解决方案`、`### 验证结果`、`### 剩余风险`。",
    ]
    prev = previous_chapter(chapter)
    if prev is not None:
        previous_path = settings.paths.trials_root / prev / "workspace" / "c-port"
        lines.extend(
            [
                "",
                f"上一章通过代码路径：{previous_path}",
                "先复制上一章已通过的 `workspace/c-port` 到当前章节，再按本章 spec 加入新功能。",
            ]
        )
    lines.extend(
        [
            "",
            "现在直接开始实现，不要只停留在分析。",
            "",
        ]
    )
    return "\n".join(lines)


def build_repair_prompt(
    settings: Settings,
    workspace: TrialWorkspace,
    *,
    round_index: int,
    previous_failure: dict[str, Any],
) -> str:
    lines = [build_seed_prompt(settings, workspace, round_index=round_index).rstrip(), "", "上一轮未通过，修复重点如下："]
    lines.append(f"- 失败类型：{previous_failure.get('kind', 'unknown')}")
    lines.append(f"- 摘要：{previous_failure.get('summary', 'unknown failure')}")
    first_failed_case = previous_failure.get("first_failed_case")
    if first_failed_case:
        lines.append(f"- 首个失败 case：{first_failed_case}")
    test_source_path = previous_failure.get("test_source_path")
    if test_source_path:
        lines.append(f"- 对应测试源码：{test_source_path}")
    serial_log_path = previous_failure.get("serial_log_path")
    if serial_log_path:
        lines.append(f"- 当前串口日志：{serial_log_path}")
    baseline_summary_path = previous_failure.get("baseline_summary_path")
    if baseline_summary_path:
        lines.append(f"- Rust 基线摘要：{baseline_summary_path}")
    judge_report_path = previous_failure.get("judge_report_path")
    if judge_report_path:
        lines.append(f"- judge 报告：{judge_report_path}")
    missing_patterns = previous_failure.get("missing_patterns") or []
    if missing_patterns:
        lines.append(f"- 缺失签名：{', '.join(str(item) for item in missing_patterns)}")
    command_excerpt = previous_failure.get("command_excerpt")
    if command_excerpt:
        lines.extend(["", "失败命令摘录：", command_excerpt])
    stderr_excerpt = previous_failure.get("stderr_excerpt")
    if stderr_excerpt:
        lines.extend(["", "失败输出摘录：", stderr_excerpt])
    lines.extend(
        [
            "",
            f"继续修复，并确保本轮新增 `## Round {round_index}` 段落。",
            "- 继续遵守输入边界：不要读取或参考 `new-phase-c` 外部仓库实现。",
            "",
        ]
    )
    return "\n".join(lines)


def _chapter_cases(cases_path: Path, chapter: str) -> list[str]:
    payload = tomllib.loads(cases_path.read_text(encoding="utf-8-sig"))
    chapter_cases = payload.get(chapter, {}).get("cases", [])
    return [str(item) for item in chapter_cases]
