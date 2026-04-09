from __future__ import annotations

from pathlib import Path
import argparse
import json
import sys
import time

from tools.bootstrap import bootstrap_into

from .config import CHAPTERS, load_settings
from .runner import ExperimentRunner
from .state import record_run_audit, utc_now


def phase_root() -> Path:
    return Path(__file__).resolve().parents[1]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="new-phase-c self-contained chapter-level spec-to-C automation")
    parser.add_argument("--spec-root", type=Path, help="override in-repo spec root")
    parser.add_argument("--runtime-assets-root", type=Path, help="override in-repo runtime-assets root")
    parser.add_argument("--max-auto-rounds", type=int, help="override per-chapter automatic coding round limit")
    subparsers = parser.add_subparsers(dest="command", required=True)

    bootstrap = subparsers.add_parser("bootstrap")
    bootstrap.add_argument("--force", action="store_true")

    init_trial = subparsers.add_parser("init-trial")
    init_trial.add_argument("chapter", choices=CHAPTERS)
    init_trial.add_argument("--force", action="store_true")

    preflight = subparsers.add_parser("preflight-baseline")
    preflight.add_argument("chapter", choices=CHAPTERS)
    preflight.add_argument("--force", action="store_true")

    verify = subparsers.add_parser("verify-chapter")
    verify.add_argument("chapter", choices=CHAPTERS)
    verify.add_argument("--force-baseline", action="store_true")

    run_target = subparsers.add_parser("run-coding-target")
    run_target.add_argument("chapter", choices=CHAPTERS)

    resume = subparsers.add_parser("resume")
    resume.add_argument("chapter", choices=CHAPTERS)

    run_all = subparsers.add_parser("run-all")
    run_all.add_argument("--through", default="ch8", choices=CHAPTERS)

    report = subparsers.add_parser("report")
    report.add_argument("chapter", choices=CHAPTERS)
    report.add_argument("--json", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    settings = load_settings(
        phase_root(),
        spec_repo=args.spec_root,
        runtime_repo=args.runtime_assets_root,
        baseline_fallback_repo=args.runtime_assets_root,
        max_auto_rounds_per_chapter=args.max_auto_rounds,
    )
    runner = ExperimentRunner(settings)
    started_at = utc_now()
    start_monotonic = time.monotonic()
    status = "success"
    exit_code = 0
    workspace_path: str | None = None
    report_path: str | None = None
    chapter = getattr(args, "chapter", None)
    try:
        if args.command == "bootstrap":
            result = bootstrap_into(settings, force=args.force)
            print(f"bootstrap complete: bundles={result['bundle_count']} spec={result['spec_root']} user={result['user_root']}")
            return exit_code

        if args.command == "init-trial":
            workspace = runner.init_trial(args.chapter, force=args.force)
            workspace_path = str(workspace.workspace_dir)
            report_path = str(workspace.report_path)
            print(f"initialized trial: {workspace.root}")
            return exit_code

        if args.command == "preflight-baseline":
            baseline = runner.preflight_baseline(args.chapter, force=args.force)
            print(f"{baseline.chapter}: success={baseline.success} summary={baseline.summary}")
            exit_code = 0 if baseline.success else 1
            status = "success" if exit_code == 0 else "failed"
            return exit_code

        if args.command == "verify-chapter":
            result = runner.verify_chapter(args.chapter, force_baseline=args.force_baseline)
            report_path = str(result.report_path)
            workspace_path = str(settings.paths.trials_root / args.chapter / "workspace")
            print(f"{result.chapter}: success={result.success} summary={result.summary}")
            exit_code = 0 if result.success else 1
            status = "success" if exit_code == 0 else "failed"
            return exit_code

        if args.command == "run-coding-target":
            outcome = runner.run_coding_target(args.chapter)
            report_path = str(outcome.report_path)
            workspace_path = str(settings.paths.trials_root / args.chapter / "workspace")
            print(f"{outcome.chapter}: success={outcome.success} rounds={outcome.rounds} summary={outcome.summary}")
            exit_code = 0 if outcome.success else 1
            status = "success" if exit_code == 0 else "failed"
            return exit_code

        if args.command == "resume":
            outcome = runner.resume(args.chapter)
            report_path = str(outcome.report_path)
            workspace_path = str(settings.paths.trials_root / args.chapter / "workspace")
            print(f"{outcome.chapter}: success={outcome.success} rounds={outcome.rounds} summary={outcome.summary}")
            exit_code = 0 if outcome.success else 1
            status = "success" if exit_code == 0 else "failed"
            return exit_code

        if args.command == "run-all":
            outcomes = runner.run_all(args.through)
            for outcome in outcomes:
                print(f"{outcome.chapter}: success={outcome.success} rounds={outcome.rounds} summary={outcome.summary}")
            if outcomes:
                report_path = str(outcomes[-1].report_path)
                workspace_path = str(settings.paths.trials_root / outcomes[-1].chapter / "workspace")
            exit_code = 0 if outcomes and outcomes[-1].success else 1
            status = "success" if exit_code == 0 else "failed"
            return exit_code

        if args.command == "report":
            payload = runner.build_report(args.chapter)
            report_path = str(payload["report_path"])
            workspace_path = str(settings.paths.trials_root / args.chapter / "workspace")
            if args.json:
                print(json.dumps(payload, ensure_ascii=False, indent=2))
            else:
                print(f"chapter={payload['chapter']}")
                print(f"status={payload['status']}")
                print(f"current_round={payload['current_round']}")
                print(f"report_path={payload['report_path']}")
                print(f"last_failed_case={payload['last_failed_case']}")
                print(f"last_serial_log_path={payload['last_serial_log_path']}")
            return exit_code

        parser.print_help()
        exit_code = 1
        status = "failed"
        return exit_code
    except Exception as exc:
        status = "failed"
        exit_code = 1
        print(str(exc), file=sys.stderr)
        return exit_code
    finally:
        ended_at = utc_now()
        record_run_audit(
            settings.paths.run_audit_root,
            command=args.command,
            chapter=chapter,
            started_at_utc=started_at,
            ended_at_utc=ended_at,
            duration_seconds=time.monotonic() - start_monotonic,
            status=status,
            workspace_path=workspace_path,
            report_path=report_path,
        )


if __name__ == "__main__":
    sys.exit(main())
