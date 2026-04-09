from __future__ import annotations

from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from agent.config import Settings
from tools.package_chapter_spec import build_all_bundles

def bootstrap_into(settings: Settings, *, force: bool = False) -> dict[str, object]:
    if settings.spec_repo.resolve() != settings.paths.spec_root.resolve():
        _copy_spec(settings.spec_repo, settings.paths.spec_root)
    elif not settings.paths.spec_root.exists():
        raise FileNotFoundError(f"in-repo spec root not found: {settings.paths.spec_root}")
    if not settings.paths.user_root.exists():
        raise FileNotFoundError(f"in-repo user root not found: {settings.paths.user_root}")
    bundle_paths = build_all_bundles(settings.paths.spec_root, settings.paths.bundles_root)
    for directory in (
        settings.paths.trials_root,
        settings.paths.artifacts_root,
        settings.paths.baseline_root,
        settings.paths.logs_root,
        settings.paths.run_audit_root,
        settings.paths.state_root,
    ):
        directory.mkdir(parents=True, exist_ok=True)
    return {
        "spec_root": str(settings.paths.spec_root),
        "user_root": str(settings.paths.user_root),
        "bundle_count": len(bundle_paths),
    }


def _copy_spec(spec_source_root: Path, spec_root: Path) -> None:
    resolved_root, specs_dir = _resolve_spec_source(spec_source_root)
    if spec_root.exists():
        shutil.rmtree(spec_root)
    target_specs = spec_root / "specs"
    target_specs.mkdir(parents=True, exist_ok=True)
    for src_dir in sorted(path for path in specs_dir.iterdir() if path.is_dir()):
        copied = False
        dst_dir = target_specs / src_dir.name
        dst_dir.mkdir(parents=True, exist_ok=True)
        for filename in ("spec.md", "design.md"):
            src_file = src_dir / filename
            if src_file.exists():
                shutil.copy2(src_file, dst_dir / filename)
                copied = True
        if not copied:
            shutil.rmtree(dst_dir)
    project_md = resolved_root / "project.md"
    if project_md.exists():
        shutil.copy2(project_md, spec_root / "project.md")
    else:
        generated = "\n".join(
            [
                "# new-phase-c project",
                "",
                "该 `project.md` 由 `tools/bootstrap.py` 自动生成，因为源 spec 目录中未提供顶层 `project.md`。",
                "",
                "## Included specs",
                "",
                *[f"- {path.name}" for path in sorted(path for path in specs_dir.iterdir() if path.is_dir())],
                "",
            ]
        )
        (spec_root / "project.md").write_text(generated, encoding="utf-8")
    (spec_root / "bundles").mkdir(parents=True, exist_ok=True)


def _resolve_spec_source(spec_source_root: Path) -> tuple[Path, Path]:
    resolved = spec_source_root.resolve()
    if (resolved / "specs").is_dir():
        return resolved, resolved / "specs"
    if resolved.is_dir() and any((resolved / name).is_dir() for name in ("ch2", "ch3", "kernel-alloc")):
        return resolved.parent, resolved
    raise FileNotFoundError(f"spec source root not found or invalid: {resolved}")
