"""Use the terminal client's exact knowledge prompt without starting its CLI."""
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from chat import build_context

if __name__ == "__main__":
    question = json.load(sys.stdin)
    print(json.dumps(build_context(question), ensure_ascii=False))
