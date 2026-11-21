"""Read the same common source manifest consumed by the ESP-IDF targets."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "main/shared_sources.txt"


def source_groups():
    groups = {}
    seen = set()
    for number, line in enumerate(MANIFEST.read_text().splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        group, relative = line.split(maxsplit=1)
        path = (ROOT / relative).resolve()
        if not path.is_relative_to(ROOT) or not path.is_file():
            raise ValueError(f"Invalid shared source at line {number}: {relative}")
        if path in seen:
            raise ValueError(f"Duplicate shared source at line {number}: {relative}")
        seen.add(path)
        groups.setdefault(group, []).append(path)
    return groups


def shared_sources(*groups):
    available = source_groups()
    return [path for group in groups for path in available[group]]
