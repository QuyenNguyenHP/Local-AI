#!/usr/bin/env python3
"""Terminal chat with Ollama and the voice assistant's Markdown routing."""

import argparse
import json
import os
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

from app.context import build_context


def ask(question, history, model, url, on_chunk=None):
    context = build_context(question)
    payload = {
        "model": model,
        "messages": [*history, {"role": "user", "content": context}],
        "stream": True,
        "options": {"temperature": 0.6, "num_ctx": 8192, "num_predict": 512},
    }
    request = Request(
        url.rstrip("/") + "/api/chat",
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
    )
    chunks = []
    done = False
    with urlopen(request, timeout=180) as response:
        for line in response:
            if not line.strip():
                continue
            result = json.loads(line)
            if "error" in result:
                raise ValueError(f"Ollama streaming error: {result['error']}")
            content = result.get("message", {}).get("content", "")
            if content:
                chunks.append(content)
                if on_chunk is not None:
                    on_chunk(content)
            if result.get("done"):
                done = True
                break
    if not done:
        raise ValueError("Response interrupted; partial answer was not saved to history")
    answer = "".join(chunks).strip()
    if not answer:
        raise ValueError("Ollama returned an empty answer")
    history.extend([
        {"role": "user", "content": question},
        {"role": "assistant", "content": answer},
    ])
    del history[:-10]
    return answer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", default=os.getenv("OLLAMA_MODEL", "gemma3:4b"))
    parser.add_argument("--url", default=os.getenv("OLLAMA_URL", "http://127.0.0.1:11434"))
    args = parser.parse_args()
    history = []
    print(f"Model: {args.model} | /bye: exit | /clear: clear history")
    print("Rules and Markdown files are reloaded for each question.")
    while True:
        try:
            question = input("\nYou: ").strip()
            if question in {"/bye", "/exit"}:
                break
            if question == "/clear":
                history.clear()
                print("History cleared.")
                continue
            if not question:
                continue
            print("\nAI: ", end="", flush=True)
            try:
                ask(
                    question, history, args.model, args.url,
                    on_chunk=lambda text: print(text, end="", flush=True),
                )
            finally:
                print()
        except (EOFError, KeyboardInterrupt):
            print("\nGoodbye!")
            break
        except HTTPError as exc:
            print(f"Ollama HTTP error {exc.code}: {exc.read().decode('utf-8', errors='replace')}")
        except (URLError, TimeoutError, OSError, ValueError, KeyError, TypeError) as exc:
            print(f"Error: {exc}. Check Ollama, the model, and knowledge_rules.json.")


if __name__ == "__main__":
    main()
