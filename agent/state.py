from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
from typing import Any
import json


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def timestamp_slug() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def read_json(path: Path, default: Any | None = None) -> Any:
    if not path.exists():
        return default
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, payload: object) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return path


def record_run_audit(
    audit_root: Path,
    *,
    command: str,
    chapter: str | None,
    started_at_utc: str,
    ended_at_utc: str,
    duration_seconds: float,
    status: str,
    workspace_path: str | None = None,
    report_path: str | None = None,
    extra: dict[str, Any] | None = None,
) -> Path:
    filename_bits = [timestamp_slug(), command]
    if chapter:
        filename_bits.append(chapter)
    path = audit_root / (".".join(filename_bits) + ".json")
    payload: dict[str, Any] = {
        "command": command,
        "chapter": chapter,
        "started_at_utc": started_at_utc,
        "ended_at_utc": ended_at_utc,
        "duration_seconds": round(duration_seconds, 3),
        "status": status,
    }
    if workspace_path is not None:
        payload["workspace_path"] = workspace_path
    if report_path is not None:
        payload["report_path"] = report_path
    if extra:
        payload.update(extra)
    return write_json(path, payload)
