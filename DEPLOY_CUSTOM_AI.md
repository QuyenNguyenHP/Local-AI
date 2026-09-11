# Deploy your customized Gemma 3 AI

This guide creates `dq-assistant:latest` from your installed `gemma3:4b` model and connects it to your local voice assistant. Customization here means setting its personality, instructions, and generation defaults; it does not retrain the model or automatically teach it your files.

All commands below run in an Ubuntu terminal. Your actual project path contains a space before `_Voice_Assistant`; keep the quotes in the commands.

## 1. Check the existing Ollama service

```bash
systemctl status ollama --no-pager
ollama list
```

The service should show `active (running)` and the model list should contain `gemma3:4b`. If the service is stopped, run:

```bash
sudo systemctl start ollama
```

Only if the base model is missing, download it:

```bash
ollama pull gemma3:4b
```

Use the system service as your Ollama server. Do not also run `ollama serve`: a second server caused your earlier `address already in use` failure on port 11434. The service is already enabled to start at boot.

## 2. Write your customization

Run this block to create a `Modelfile`. Running it again replaces that file, so save any edits first.

```bash
cd "/home/dq/Local _Voice_Assistant/Local AI"
cat > Modelfile <<'EOF'
FROM gemma3:4b

PARAMETER temperature 0.6
PARAMETER num_ctx 8192
PARAMETER num_predict 192

SYSTEM """
You are Mike's personal AI assistant.

You should answer technical questions clearly and practically.

About Mike:

- Mike is an automation engineer.
- He works with Linux, Python, ESP32, Raspberry Pi and industrial automation.
- He frequently works with AI, computer vision and robotics.
- He prefers concise technical explanations.
- When explaining something complicated, start with the practical concept before going into theory.

When information is uncertain, say so instead of inventing facts.
"""
EOF
```

Edit the text inside `SYSTEM` to change the name, tone, language, or responsibilities. `FROM` selects the base model; `SYSTEM` sets its instructions; `PARAMETER` sets generation defaults. These directives and the create/run commands follow the [official Ollama Modelfile reference](https://docs.ollama.com/modelfile).

### Keep personal knowledge in Markdown files

Store your editable information separately from model behavior:

```text
Local AI/
├── Modelfile
├── DEPLOY_CUSTOM_AI.md
└── voice_ai_server/
    └── knowledge/
    ├── about_me.md
    ├── company.md
    ├── drums.md
    ├── unitree_r1.md
    ├── esp32_projects.md
    ├── servers.md
    └── notes.md
```

These files are provided alongside this guide. `about_me.md` contains Mike's
supplied background and preferences. The other files provide sections to fill
in; blank fields and comments are placeholders, not confirmed information.
Add dates to information that changes and distinguish confirmed facts from
plans or open questions.

For example, edit your personal profile with:

```bash
cd "/home/dq/Local _Voice_Assistant/Local AI"
nano voice_ai_server/knowledge/about_me.md
```

Keep general behavior in `Modelfile`, such as answering clearly and acknowledging
uncertainty. Keep detailed personal and project facts in these topic files. The
profile in the Modelfile example above can stay as a short baseline; if you later
move it entirely into Markdown, first connect the files to the application.

The voice server now loads matching files using [knowledge_rules.json](knowledge_rules.json).
See [KNOWLEDGE_RULES.md](KNOWLEDGE_RULES.md) for editing rules, testing matches,
and restarting the server to apply the integration. Ollama itself does not read
the folder; the Python application adds the selected text to each request.
Knowledge edits do not require rebuilding the model.

## 3. Create and test your custom model

```bash
cd "/home/dq/Local _Voice_Assistant/Local AI"
ollama create dq-assistant:latest -f Modelfile
ollama run dq-assistant:latest "Introduce yourself in one sentence."
```

The answer should describe it as Mike's personal AI assistant, although wording can vary. The model tag remains `dq-assistant:latest`. To chat interactively:

```bash
ollama run dq-assistant:latest
```

Type `/bye` to leave the chat. Inspect the saved configuration with:

```bash
ollama show --modelfile dq-assistant:latest
```

## 4. Test the local HTTP API

```bash
curl --fail-with-body --max-time 120 http://127.0.0.1:11434/api/chat \
  -H 'Content-Type: application/json' \
  -d '{"model":"dq-assistant:latest","messages":[{"role":"user","content":"Introduce yourself in one sentence."}],"stream":false}'
```

A successful response contains the answer in `message.content` and `done: true`.

## 5. Connect your existing voice assistant

Your application reads `OLLAMA_MODEL` from its environment. It also sends its own `SYSTEM_PROMPT` on every chat request, so set that prompt to your desired personality as well. Changing only the Modelfile can otherwise produce different behavior between the terminal and the voice app.

Open the server environment file:

```bash
cd "/home/dq/Local _Voice_Assistant/voice_assisstant_server"
cp -n .env .env.before-custom-ai
nano .env
```

If `.env` does not exist, first create it with `cp .env.example .env` and configure the other voice settings using the server README. Replace existing entries for these keys, preserving the other settings:

```bash
OLLAMA_URL=http://127.0.0.1:11434
OLLAMA_MODEL=dq-assistant:latest
OLLAMA_TIMEOUT=120
OLLAMA_KEEP_ALIVE=30m
OLLAMA_NUM_PREDICT=192
OLLAMA_NUM_CTX=8192
SYSTEM_PROMPT="You are Mike's personal AI assistant.

You should answer technical questions clearly and practically.

About Mike:

- Mike is an automation engineer.
- He works with Linux, Python, ESP32, Raspberry Pi and industrial automation.
- He frequently works with AI, computer vision and robotics.
- He prefers concise technical explanations.
- When explaining something complicated, start with the practical concept before going into theory.

When information is uncertain, say so instead of inventing facts."
```

The existing `ai.py` explicitly sets temperature to `0.3`, overriding the Modelfile. Before starting the voice server, edit `voice_assisstant_server/ai.py` and change the temperature entry inside `options` to:

```python
"temperature": 0.6,
```

The server sends context/output limits from the environment variables above, so `OLLAMA_NUM_CTX=8192` keeps the voice app aligned with the Modelfile. The existing output limit of 192 tokens remains unchanged. The multiline, double-quoted `SYSTEM_PROMPT` above can be loaded by the Bash `source .env` command below.

Stop any existing foreground voice server with Ctrl+C in its terminal, then start it with the updated environment:

```bash
cd "/home/dq/Local _Voice_Assistant/voice_assisstant_server"
set -a
source .env
set +a
.venv/bin/python server.py
```

This assumes the voice server's Python environment, Whisper, and Piper assets are already installed. See the [voice server setup guide](../voice_assisstant_server/README.md) for its dependencies and audio request examples; use the system-managed Ollama service from step 1 above.

From a second terminal:

```bash
curl --fail-with-body http://127.0.0.1:8765/health
```

The health route checks that the HTTP server responds. Also speak a question through your existing microphone client to verify the complete transcription → customized model → speech flow.

## 6. Update or roll back

After editing `Local AI/Modelfile`, rebuild:

```bash
cd "/home/dq/Local _Voice_Assistant/Local AI"
ollama create dq-assistant:latest -f Modelfile
```

If you changed personality instructions, update `SYSTEM_PROMPT` in the voice server `.env` too, then restart the voice server. Restarting also clears its in-memory conversation history. Ollama itself does not need a restart for each rebuild.

To return the voice application to the base model, set `OLLAMA_MODEL=gemma3:4b`, restore your previous `SYSTEM_PROMPT`, and restart the voice server. Your base model remains available.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| Service repeatedly restarts | Run `journalctl -u ollama -n 40 --no-pager` and `ss -ltnp 'sport = :11434'`. If a duplicate manual Ollama server owns the port, stop it in its terminal with Ctrl+C. |
| Model not found | Run `ollama list`. Create `dq-assistant:latest` using step 3 and check the spelling in `.env`. |
| Voice assistant uses the old personality | Check `.env` `SYSTEM_PROMPT`, reload the environment, and restart the voice server. |
| Slow first reply | Your service was using CPU inference during the September 7, 2026 check. Initial model loading adds latency. If requests time out, increase `OLLAMA_TIMEOUT`; shorter output limits can reduce generation time. |
| Replies stop too early | Increase `OLLAMA_NUM_PREDICT` in the voice server environment and restart it. For terminal use, increase the Modelfile value and rebuild. |
| Model does not know your documents | Check knowledge_rules.json keywords and the matching Markdown file. Only files matching the current question are loaded. |

## Deployment checklist

- [ ] Ollama service is active.
- [ ] `ollama list` contains `dq-assistant:latest`.
- [ ] A terminal prompt and the HTTP API both return an answer.
- [ ] Voice server `.env` selects the custom model and matching system prompt.
- [ ] Voice server has restarted with the new environment.
- [ ] A spoken question completes the full voice assistant flow.
