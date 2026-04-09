from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path

CHAPTERS = [f"ch{index}" for index in range(2, 9)]
CHAPTER_INDEX = {chapter: offset for offset, chapter in enumerate(CHAPTERS)}
SHELL_DRIVEN_CHAPTERS = {"ch5", "ch6", "ch7", "ch8"}

PHASE_ROOT = Path(__file__).resolve().parents[1]


def _path_override(env_name: str, fallback: Path) -> Path:
    value = os.environ.get(env_name)
    if value:
        return Path(value).expanduser()
    return fallback


DEFAULT_SPEC_REPO = _path_override("NEW_PHASE_C_SPEC_ROOT", PHASE_ROOT / "spec")
DEFAULT_RUNTIME_REPO = _path_override("NEW_PHASE_C_RUNTIME_ASSETS_ROOT", PHASE_ROOT / "runtime-assets")
DEFAULT_BASELINE_FALLBACK_REPO = DEFAULT_RUNTIME_REPO

DEFAULT_CODING_COMMAND = [
    "codex",
    "--ask-for-approval",
    "{approval_policy}",
    "exec",
    "-c",
    "model_reasoning_effort=\"low\"",
    "--skip-git-repo-check",
    "--sandbox",
    "{sandbox}",
    "--model",
    "{model}",
    "--output-last-message",
    "{last_message_path}",
    "-C",
    "{cwd}",
    "-",
]


@dataclass(slots=True)
class ExperimentPaths:
    phase_root: Path
    spec_root: Path
    user_root: Path
    runtime_assets_root: Path
    runtime_baseline_root: Path
    bundles_root: Path
    trials_root: Path
    artifacts_root: Path
    baseline_root: Path
    logs_root: Path
    run_audit_root: Path
    state_root: Path


@dataclass(slots=True)
class CodingCLIConfig:
    command: list[str]
    model: str
    sandbox: str
    approval_policy: str
    use_json_stream: bool
    round_timeout_seconds: int


@dataclass(slots=True)
class Settings:
    phase_root: Path
    spec_repo: Path
    runtime_repo: Path
    baseline_fallback_repo: Path
    paths: ExperimentPaths
    coding_cli: CodingCLIConfig
    build_timeout_seconds: int
    qemu_timeout_seconds: int
    shell_startup_delay_seconds: float
    shell_post_input_seconds: float
    max_auto_rounds_per_chapter: int


def load_settings(
    phase_root: Path,
    *,
    spec_repo: Path | None = None,
    runtime_repo: Path | None = None,
    baseline_fallback_repo: Path | None = None,
    max_auto_rounds_per_chapter: int | None = None,
) -> Settings:
    phase_root = phase_root.resolve()
    resolved_spec_repo = (spec_repo or DEFAULT_SPEC_REPO).resolve()
    resolved_runtime_repo = (runtime_repo or DEFAULT_RUNTIME_REPO).resolve()
    resolved_baseline_fallback_repo = (baseline_fallback_repo or resolved_runtime_repo).resolve()
    return Settings(
        phase_root=phase_root,
        spec_repo=resolved_spec_repo,
        runtime_repo=resolved_runtime_repo,
        baseline_fallback_repo=resolved_baseline_fallback_repo,
        paths=ExperimentPaths(
            phase_root=phase_root,
            spec_root=phase_root / "spec",
            user_root=phase_root / "user",
            runtime_assets_root=resolved_runtime_repo,
            runtime_baseline_root=resolved_runtime_repo / "baselines",
            bundles_root=phase_root / "spec" / "bundles",
            trials_root=phase_root / "trial-workspaces",
            artifacts_root=phase_root / "artifacts",
            baseline_root=phase_root / "artifacts" / "baselines",
            logs_root=phase_root / "artifacts" / "logs",
            run_audit_root=phase_root / "artifacts" / "runs",
            state_root=phase_root / "artifacts" / "state",
        ),
        coding_cli=CodingCLIConfig(
            command=list(DEFAULT_CODING_COMMAND),
            model="gpt-5-codex",
            sandbox="workspace-write",
            approval_policy="never",
            use_json_stream=False,
            round_timeout_seconds=1800,
        ),
        build_timeout_seconds=900,
        qemu_timeout_seconds=900,
        shell_startup_delay_seconds=2.0,
        shell_post_input_seconds=600.0,
        max_auto_rounds_per_chapter=4 if max_auto_rounds_per_chapter is None else max_auto_rounds_per_chapter,
    )


def chapter_number(chapter: str) -> int:
    validate_chapter(chapter)
    return int(chapter[2:])


def validate_chapter(chapter: str) -> str:
    if chapter not in CHAPTER_INDEX:
        raise ValueError(f"unsupported chapter: {chapter}")
    return chapter


def previous_chapter(chapter: str) -> str | None:
    validate_chapter(chapter)
    offset = CHAPTER_INDEX[chapter]
    if offset == 0:
        return None
    return CHAPTERS[offset - 1]


def chapter_cases_toml_path(settings: Settings) -> Path:
    return settings.paths.user_root / "cases.toml"
