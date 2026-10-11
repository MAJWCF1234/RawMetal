"""Preserve authored script records beyond the geometry editor's UI.

Records use the shared CUSTOM_CAMPAIGN format. This is a bounded passthrough;
the engine parser validates field syntax, target indices, and script semantics.
"""
from __future__ import annotations

MAX_RECORDS = 4096
MAX_RECORD_BYTES = 4096
MAX_TOTAL_BYTES = 512 * 1024
ALLOWED_TAGS = frozenset({
    "TIMED_SEQUENCE", "ACTOR", "ACTOR_KEY", "ACTOR_IDLE_KEY", "ACTOR_AI",
    "ACTOR_GUARD", "ACTOR_PATROL", "ACTOR_WEAPON", "ACTOR_FACTION", "ACTOR_HEALTH", "ACTOR_SUSPEND",
    "ACTOR_PLATFORM", "ACTOR_PRELUDE", "SEQUENCE_SIGHT", "SEQUENCE_CUE",
})


def runtime_records(project: dict) -> list[str]:
    """Validate and return a copy without reordering or rewriting record text."""
    if not isinstance(project, dict):
        raise ValueError("Invalid Depthworks project")
    records = project.get("runtimeRecords", [])
    if not isinstance(records, list):
        raise ValueError("runtimeRecords must be a list of strings")
    if len(records) > MAX_RECORDS:
        raise ValueError(f"runtimeRecords supports at most {MAX_RECORDS} records")
    total_bytes = 0
    for index, record in enumerate(records):
        label = f"runtimeRecords[{index}]"
        if not isinstance(record, str):
            raise ValueError(f"{label} must be a string")
        # splitlines recognizes Unicode line separators as well as CR/LF. Also
        # reject controls that would make an apparently single record ambiguous.
        if any(ord(ch) < 32 or ord(ch) == 127 for ch in record) or record.splitlines() != [record]:
            raise ValueError(f"{label} must be one nonempty line without control characters")
        if any(marker in record for marker in (
                "--- CUSTOM_CAMPAIGN_DATA_START ---", "--- CUSTOM_CAMPAIGN_DATA_END ---",
                "--- MAP_CODE_START ---", "--- MAP_CODE_END ---")):
            raise ValueError(f"{label} cannot contain payload markers")
        tag, separator, _ = record.partition("|")
        if not separator or tag not in ALLOWED_TAGS:
            raise ValueError(f"{label} uses an unsupported authored script tag")
        size = len(record.encode("utf-8"))
        if size > MAX_RECORD_BYTES:
            raise ValueError(f"{label} exceeds {MAX_RECORD_BYTES} UTF-8 bytes")
        total_bytes += size + 1
        if total_bytes > MAX_TOTAL_BYTES:
            raise ValueError(f"runtimeRecords exceeds {MAX_TOTAL_BYTES} UTF-8 bytes")
    return list(records)
