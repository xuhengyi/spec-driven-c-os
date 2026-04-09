from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import subprocess
import time

from .config import CodingCLIConfig


@dataclass(slots=True)
class CodingCLIInvocationResult:
    returncode: int
    stdout: str
    stderr: str
    duration_seconds: float
    last_message_path: Path


def run_coding_exec(
    prompt: str,
    cwd: Path,
    coding_cli_cfg: CodingCLIConfig,
    last_message_path: Path,
) -> CodingCLIInvocationResult:
    command = [
        token.format(
            cwd=str(cwd),
            model=coding_cli_cfg.model,
            sandbox=coding_cli_cfg.sandbox,
            approval_policy=coding_cli_cfg.approval_policy,
            last_message_path=str(last_message_path),
        )
        for token in coding_cli_cfg.command
    ]
    if coding_cli_cfg.use_json_stream:
        command.append("--json")

    start = time.monotonic()
    try:
        completed = subprocess.run(
            command,
            input=prompt,
            text=True,
            capture_output=True,
            timeout=coding_cli_cfg.round_timeout_seconds,
        )
        return CodingCLIInvocationResult(
            returncode=completed.returncode,
            stdout=completed.stdout,
            stderr=completed.stderr,
            duration_seconds=time.monotonic() - start,
            last_message_path=last_message_path,
        )
    except subprocess.TimeoutExpired as exc:
        return CodingCLIInvocationResult(
            returncode=-9,
            stdout=exc.stdout or "",
            stderr=(exc.stderr or "") + "\ncommand timed out",
            duration_seconds=time.monotonic() - start,
            last_message_path=last_message_path,
        )
