"""Load Markdown selected by editable keyword rules on each question."""

import json
import re
from pathlib import Path


DEFAULT_RULES = Path(__file__).resolve().parent / "knowledge_rules.json"


def matching_notes(question: str, rules_path: Path = DEFAULT_RULES) -> str:
    rules = json.loads(rules_path.read_text(encoding="utf-8"))
    root = (rules_path.parent / "knowledge").resolve()
    remaining = rules.get("max_chars", 12000)
    if not isinstance(remaining, int) or not 1 <= remaining <= 24000:
        raise ValueError("Knowledge max_chars must be between 1 and 24000")
    sections = []
    seen = set()
    for rule in rules["rules"]:
        matched = any(
            re.search(r"(?<!\w)" + re.escape(keyword) + r"(?!\w)", question, re.I)
            for keyword in rule["keywords"] if keyword.strip()
        )
        if not matched:
            continue
        path = (root / rule["file"]).resolve()
        if not path.is_relative_to(root) or path.suffix != ".md":
            raise ValueError("Knowledge rules must reference Markdown inside knowledge/")
        if path in seen:
            continue
        seen.add(path)
        if remaining <= 0:
            sections.append("[Additional matched notes omitted because of the size limit.]")
            break
        content = path.read_text(encoding="utf-8")
        clipped = content[:remaining]
        remaining -= len(clipped)
        if len(clipped) < len(content):
            clipped += "\n[Note truncated because of the size limit.]"
        sections.append(f"Source: {rule['file']}\n{clipped}")
    return "\n\n".join(sections)
