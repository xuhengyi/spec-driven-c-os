from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any

from .baseline import BaselineResult, preflight_baseline
from .coding_cli import CodingCLIInvocationResult, run_coding_exec
from .config import CHAPTERS, Settings, previous_chapter
from .judge import JudgeResult, run_judge
from .prompting import build_prompt
from .workspace import (
    TrialWorkspace,
    append_intervention_record,
    append_final_report,
    init_trial,
    load_state,
    load_trial,
    patch_state,
    validate_round_report,
)


@dataclass(slots=True)
class RunOutcome:
    chapter: str
    success: bool
    rounds: int
    summary: str
    report_path: Path


class ExperimentRunner:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings

    def init_trial(self, chapter: str, force: bool = False) -> TrialWorkspace:
        return init_trial(self.settings, chapter, force=force)

    def preflight_baseline(self, chapter: str, force: bool = False) -> BaselineResult:
        return preflight_baseline(self.settings, chapter, force=force)

    def verify_chapter(self, chapter: str, force_baseline: bool = False) -> JudgeResult:
        baseline = self.preflight_baseline(chapter, force=force_baseline)
        if not baseline.success:
            return JudgeResult(
                chapter=chapter,
                success=False,
                summary=baseline.summary,
                report_path=baseline.meta_path,
                report_payload={
                    "chapter": chapter,
                    "success": False,
                    "summary": baseline.summary,
                    "baseline_meta_path": str(baseline.meta_path),
                },
            )
        workspace = self._ensure_trial(chapter)
        return run_judge(self.settings, workspace)

    def run_coding_target(self, chapter: str) -> RunOutcome:
        workspace = self._ensure_trial(chapter)
        self._ensure_previous_passed(chapter)
        baseline = self.preflight_baseline(chapter)
        if not baseline.success:
            patch_state(
                workspace,
                status="blocked",
                baseline_meta_path=str(baseline.meta_path),
            )
            return RunOutcome(
                chapter=chapter,
                success=False,
                rounds=0,
                summary=baseline.summary,
                report_path=workspace.report_path,
            )

        state = load_state(workspace)
        precheck = run_judge(self.settings, workspace)
        if precheck.success:
            patch_state(
                workspace,
                status="passed",
                baseline_meta_path=str(baseline.meta_path),
                last_failed_case=None,
                last_serial_log_path=precheck.report_payload.get("serial_log_path"),
            )
            append_final_report(
                workspace,
                ended_at=precheck.report_payload["ended_at_utc"],
                duration_seconds=0.0,
                passed_cases=self._passed_cases(chapter),
                conclusion="章节 judge 已确认本章节通过。",
            )
            return RunOutcome(
                chapter=chapter,
                success=True,
                rounds=int(state.get("current_round", 0)),
                summary=precheck.summary,
                report_path=workspace.report_path,
            )

        previous_failure: dict[str, Any] | None = None if int(state.get("current_round", 0)) == 0 else self._failure_from_judge(precheck)
        round_index = int(state.get("current_round", 0))
        if round_index >= self.settings.max_auto_rounds_per_chapter:
            return self._manual_fix_required_outcome(
                workspace,
                chapter=chapter,
                rounds=round_index,
                baseline_meta_path=str(baseline.meta_path),
                summary=(
                    f"automatic round limit reached ({self.settings.max_auto_rounds_per_chapter}); "
                    "manual fix required before continuing"
                ),
                failure=previous_failure,
            )

        try:
            while True:
                round_index += 1
                prompt = build_prompt(
                    self.settings,
                    workspace,
                    round_index=round_index,
                    previous_failure=previous_failure,
                )
                prompt_path = workspace.prompts_dir / f"{chapter}.round{round_index}.prompt.txt"
                prompt_path.write_text(prompt, encoding="utf-8")
                report_before = workspace.report_path.read_text(encoding="utf-8") if workspace.report_path.exists() else ""
                last_message_path = workspace.logs_dir / f"{chapter}.round{round_index}.last-message.txt"
                coding_result = run_coding_exec(
                    prompt,
                    workspace.root,
                    self.settings.coding_cli,
                    last_message_path,
                )
                self._write_round_log(workspace, chapter, round_index, coding_result)
                report_ok = validate_round_report(workspace.report_path, round_index, report_before)
                judge_result = run_judge(self.settings, workspace)
                success = coding_result.returncode == 0 and report_ok and judge_result.success

                if success:
                    append_final_report(
                        workspace,
                        ended_at=judge_result.report_payload["ended_at_utc"],
                        duration_seconds=self._trial_duration_seconds(load_state(workspace)),
                        passed_cases=self._passed_cases(chapter),
                        conclusion="章节 judge 已确认本章节通过。",
                    )
                    patch_state(
                        workspace,
                        status="passed",
                        current_round=round_index,
                        baseline_meta_path=str(baseline.meta_path),
                        last_failed_case=None,
                        last_serial_log_path=judge_result.report_payload.get("serial_log_path"),
                    )
                    return RunOutcome(
                        chapter=chapter,
                        success=True,
                        rounds=round_index,
                        summary=judge_result.summary,
                        report_path=workspace.report_path,
                    )

                previous_failure = self._pick_failure(
                    chapter,
                    coding_result=coding_result,
                    report_ok=report_ok,
                    judge_result=judge_result,
                    round_index=round_index,
                )
                if round_index >= self.settings.max_auto_rounds_per_chapter:
                    return self._manual_fix_required_outcome(
                        workspace,
                        chapter=chapter,
                        rounds=round_index,
                        baseline_meta_path=str(baseline.meta_path),
                        summary=(
                            f"automatic round limit reached ({self.settings.max_auto_rounds_per_chapter}); "
                            "manual fix required before continuing"
                        ),
                        failure=previous_failure,
                    )
                patch_state(
                    workspace,
                    status="running",
                    current_round=round_index,
                    baseline_meta_path=str(baseline.meta_path),
                    last_failed_case=previous_failure.get("first_failed_case"),
                    last_serial_log_path=previous_failure.get("serial_log_path"),
                )
        except KeyboardInterrupt:
            patch_state(workspace, status="interrupted", current_round=round_index)
            raise

    def resume(self, chapter: str) -> RunOutcome:
        return self.run_coding_target(chapter)

    def run_all(self, through: str) -> list[RunOutcome]:
        outcomes: list[RunOutcome] = []
        for chapter in CHAPTERS:
            outcomes.append(self.run_coding_target(chapter))
            if chapter == through or not outcomes[-1].success:
                break
        return outcomes

    def build_report(self, chapter: str) -> dict[str, Any]:
        workspace = load_trial(self.settings, chapter)
        state = load_state(workspace)
        return {
            "chapter": chapter,
            "status": state.get("status", "unknown"),
            "current_round": state.get("current_round", 0),
            "baseline_meta_path": state.get("baseline_meta_path"),
            "last_failed_case": state.get("last_failed_case"),
            "last_serial_log_path": state.get("last_serial_log_path"),
            "report_path": str(workspace.report_path),
        }

    def _ensure_trial(self, chapter: str) -> TrialWorkspace:
        try:
            return load_trial(self.settings, chapter)
        except FileNotFoundError:
            return init_trial(self.settings, chapter, force=False)

    def _ensure_previous_passed(self, chapter: str) -> None:
        previous = previous_chapter(chapter)
        if previous is None:
            return
        try:
            workspace = load_trial(self.settings, previous)
        except FileNotFoundError as exc:
            raise RuntimeError(f"{chapter} requires previous chapter {previous} to pass first") from exc
        state = load_state(workspace)
        if state.get("status") != "passed":
            raise RuntimeError(f"{chapter} requires previous chapter {previous} to pass first")

    def _failure_from_judge(self, judge_result: JudgeResult) -> dict[str, Any]:
        payload = dict(judge_result.report_payload)
        payload["kind"] = payload.get("failure_kind", "validation")
        payload["judge_report_path"] = str(judge_result.report_path)
        return payload

    def _pick_failure(
        self,
        chapter: str,
        *,
        coding_result: CodingCLIInvocationResult,
        report_ok: bool,
        judge_result: JudgeResult,
        round_index: int,
    ) -> dict[str, Any]:
        if not judge_result.success:
            payload = self._failure_from_judge(judge_result)
            if coding_result.returncode != 0:
                payload["stderr_excerpt"] = (payload.get("stderr_excerpt") or "") + "\n" + coding_result.stderr[-4000:]
            if not report_ok:
                payload["summary"] = str(payload.get("summary", "")).strip() + "；并且 report.md 未新增本轮段落"
            return payload
        if not report_ok:
            return {
                "kind": "report",
                "summary": f"report.md 未新增 `## Round {round_index}` 段落",
                "first_failed_case": None,
                "test_source_path": None,
                "serial_log_path": None,
                "baseline_summary_path": str(self.settings.paths.baseline_root / chapter / "case-summary.json"),
                "judge_report_path": str(judge_result.report_path),
            }
        return {
            "kind": "codex",
            "summary": "codex exec returned non-zero",
            "first_failed_case": None,
            "test_source_path": None,
            "serial_log_path": None,
            "baseline_summary_path": str(self.settings.paths.baseline_root / chapter / "case-summary.json"),
            "judge_report_path": str(judge_result.report_path),
            "stderr_excerpt": coding_result.stderr[-4000:],
        }

    def _passed_cases(self, chapter: str) -> list[str]:
        import tomllib

        payload = tomllib.loads((self.settings.paths.user_root / "cases.toml").read_text(encoding="utf-8-sig"))
        return [str(item) for item in payload.get(chapter, {}).get("cases", [])]

    def _write_round_log(
        self,
        workspace: TrialWorkspace,
        chapter: str,
        round_index: int,
        coding_result: CodingCLIInvocationResult,
    ) -> None:
        path = workspace.logs_dir / f"{chapter}.round{round_index}.codex.log"
        path.write_text(
            "\n".join(
                [
                    f"returncode={coding_result.returncode}",
                    f"duration_seconds={coding_result.duration_seconds:.3f}",
                    "",
                    coding_result.stdout,
                    coding_result.stderr,
                ]
            ),
            encoding="utf-8",
        )

    def _trial_duration_seconds(self, state: dict[str, Any]) -> float:
        started_at = state.get("started_at_utc")
        if not started_at:
            return 0.0
        try:
            started = datetime.fromisoformat(str(started_at))
            return max((datetime.now(started.tzinfo) - started).total_seconds(), 0.0)
        except Exception:
            return 0.0

    def _manual_fix_required_outcome(
        self,
        workspace: TrialWorkspace,
        *,
        chapter: str,
        rounds: int,
        baseline_meta_path: str,
        summary: str,
        failure: dict[str, Any] | None,
    ) -> RunOutcome:
        patch_state(
            workspace,
            status="manual_fix_required",
            current_round=rounds,
            baseline_meta_path=baseline_meta_path,
            last_failed_case=(failure or {}).get("first_failed_case"),
            last_serial_log_path=(failure or {}).get("serial_log_path"),
        )
        append_intervention_record(
            workspace,
            title="自动轮次上限触发，转入人工修复",
            details=[
                f"章节: {chapter}",
                f"自动轮次数: {rounds}",
                f"限制阈值: {self.settings.max_auto_rounds_per_chapter}",
                f"最近失败 case: {(failure or {}).get('first_failed_case') or '无'}",
                f"摘要: {summary}",
            ],
        )
        append_final_report(
            workspace,
            ended_at=datetime.now().astimezone().isoformat(),
            duration_seconds=self._trial_duration_seconds(load_state(workspace)),
            passed_cases=[],
            conclusion=summary,
        )
        return RunOutcome(
            chapter=chapter,
            success=False,
            rounds=rounds,
            summary=summary,
            report_path=workspace.report_path,
        )
