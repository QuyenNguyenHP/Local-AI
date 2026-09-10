"""Use the terminal client's exact knowledge prompt without starting its CLI."""
import json
import sys

from app.context import build_context

if __name__ == "__main__":
    question = json.load(sys.stdin)
    print(json.dumps(build_context(question), ensure_ascii=False))
