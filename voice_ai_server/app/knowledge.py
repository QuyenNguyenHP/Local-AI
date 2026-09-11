"""Load Markdown selected by editable keyword rules on each question."""

import json
import re
from pathlib import Path
from .progress import log


DEFAULT_RULES = Path(__file__).resolve().parents[2] / "knowledge_rules.json"
DEFAULT_KNOWLEDGE_ROOT = Path(__file__).resolve().parents[1] / "knowledge"


def matching_notes(question: str, rules_path: Path = DEFAULT_RULES) -> str:
    rules = json.loads(rules_path.read_text(encoding="utf-8"))
    # Rules remain at the repository root; the Markdown belongs to the voice
    # server so it can be deployed as one self-contained application.
    root = DEFAULT_KNOWLEDGE_ROOT.resolve()
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
        selected_chars = len(clipped)
        remaining -= len(clipped)
        if len(clipped) < len(content):
            clipped += "\n[Note truncated because of the size limit.]"
        log("Knowledge | file=%s, selected=%d characters, truncated=%s", rule["file"], selected_chars, selected_chars < len(content))
        sections.append(f"Source: {rule['file']}\n{clipped}")
    if not sections:
        log("Knowledge | no files matched the keywords")
    return "\n\n".join(sections)
