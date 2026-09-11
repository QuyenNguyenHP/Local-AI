# Create and apply knowledge rules

## Terminal chat

Run the terminal client (Python 3.10 or newer, no extra packages required):

```bash
cd /home/daikai/Local-AI
python3 voice_ai_server/chat.py
```

It defaults to `gemma3:4b`. To use your custom model after creating it:

```bash
python3 voice_ai_server/chat.py --model dq-assistant:latest
```

Ollama must be running; the voice server does not need to be running. Type
`/bye` to exit or `/clear` to reset chat history. The client reuses `voice_ai_server/app/knowledge.py`
and rereads rules and selected notes for each question. It keeps the last five
question/answer pairs in memory, without retaining full reference files.
It uses temperature 0.6, context 8192, and an output limit of 512 tokens.
It preserves the model's default system prompt. It accepts `OLLAMA_MODEL` and
`OLLAMA_URL` from the shell environment, but does not automatically source `.env`.
Add Vietnamese keywords to the rules if you want Vietnamese phrases to match.

The voice server now uses `knowledge_rules.json` to select Markdown files for
each question. This works through your Python voice application; `ollama run`
by itself does not use these rules.

## Edit a rule

Open [knowledge_rules.json](knowledge_rules.json). Each rule has a filename
relative to `voice_ai_server/knowledge/` and a list of keywords. For example:

```json
{"file": "unitree_r1.md", "keywords": ["unitree", "r1", "my robot"]}
```

Add `my robot` to the existing Unitree rule if you want that phrase to select
the file. Use valid JSON: double quotes and no trailing commas or comments.
To add a topic, create its Markdown file and add another object to `rules`.

Matching ignores capitalization and checks whole words or phrases. `R1` matches
`r1`, but does not match `R10`. Phrases use the exact spacing written in the rule.
Any keyword selects its file; multiple rules can match one question. Each file
is included only once. Rules are processed in their listed order.

For example, “How does my ESP32 connect to my server?” loads
`esp32_projects.md` and `servers.md`.

## Add your facts

Fill in the selected Markdown files. Empty template fields do not provide any
information. The application tells the model to acknowledge missing personal
facts and distinguish general technical advice from your actual setup.
Model responses still need checking; instructions do not guarantee accuracy.

## Apply the integration

Restart your running voice server once to load the new Python code. For a server
running in a terminal, stop it with Ctrl+C, then run:

```bash
cd /home/daikai/Local-AI/voice_ai_server
set -a
source .env
set +a
../.venv/bin/python run.py
```

After that, rules and Markdown contents are reread on every question. Editing
them needs no restart or `ollama create`. Keep using the model selected by your
existing `OLLAMA_MODEL`; routing works with both the base and customized model.

## Check which content a question selects

This command prints the selected reference text without calling Ollama:

```bash
cd /home/daikai/Local-AI/voice_ai_server
python3 - <<'PY'
from app.knowledge import matching_notes
print(matching_notes("How does my ESP32 connect to my server?") or "No matching files")
PY
```

## Limits and troubleshooting

- No match: no new notes are sent. Add a keyword or name the topic explicitly.
- Follow-up questions are matched independently. “What about its battery?”
  does not automatically reload the Unitree file; say “What about the R1 battery?”
  Previous chat messages remain available, but full notes are not saved in history.
- `max_chars` caps total Markdown content per request (default 12000 characters).
  Later matching content can be truncated or omitted. This is a character limit,
  not a token budget; notes, history, instructions, and answers must still fit
  the model context. Lower it or shorten notes if context becomes too large.
- Invalid JSON, a missing matched file, or a path outside `knowledge/` produces
  an error instead of silently answering without the expected notes. Fix the
  rules or filename and retry.
