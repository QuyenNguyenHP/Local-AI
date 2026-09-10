# Voice AI Server

Shared voice API: **audio -> faster-whisper -> Ollama AI -> Kokoro -> WAV audio**.
By default, models load and warm up before the server accepts requests. The first startup may need to download model files.

For each question, `app/context.py` uses `build_context()` to reload `../knowledge_rules.json`, select matching Markdown files from `../knowledge/`, and send that context to Ollama. Web-chat shares this knowledge logic. The default model is `dq-assistant:latest`. Changes to knowledge files apply on the next request.

## Source layout

```text
voice_ai_server/
├── run.py               # Start the API and configure CUDA library paths
├── run_gpu.sh           # Optional GPU launcher; python run.py is sufficient
├── chat.py              # Terminal chat with Ollama
├── knowledge_bridge.py  # JSON bridge for web-chat
└── app/
    ├── main.py          # API endpoints and startup warmup
    ├── services.py      # Whisper, Ollama, Kokoro and sessions
    ├── context.py       # Combine knowledge with the question
    ├── knowledge.py     # Select Markdown files by keyword
    ├── progress.py      # Request IDs and processing logs
    └── config.py        # Server configuration
```

Shared data lives in `../knowledge_rules.json` and `../knowledge/`.
Knowledge lookup follows `services.py -> context.build_context() -> knowledge.matching_notes()`.
Web-chat calls `knowledge_bridge.py` to reuse this logic.

Run terminal chat from the project root with `python3 voice_ai_server/chat.py`.

## Install and run

The shared virtual environment is at the project root, `Local-AI/.venv/`.
The server configuration file is `voice_ai_server/.env`.

```bash
cd /home/daikai/Local-AI
# Create only if .venv does not already exist:
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r voice_ai_server/requirements.txt
cd voice_ai_server
# Copy only if .env does not already exist:
[ -f .env ] || cp .env.example .env
# Adjust OLLAMA_MODEL in .env if needed.
python run.py
```

Ollama must be running before startup warmup. If it is not running as a service, start `ollama serve` in a separate terminal.

For subsequent starts:

```bash
cd /home/daikai/Local-AI/voice_ai_server
../.venv/bin/python run.py
```

Interactive API documentation is available at `http://SERVER_IP:8000/docs`.
By default, the server listens on all network interfaces so ESP32 devices, phones and other applications can connect.

## NVIDIA GPU setup for Whisper (Linux)

Faster-Whisper/CTranslate2 requires **CUDA 12 cuBLAS and cuDNN 9 for CUDA 12**.
CUDA 13 libraries do not replace `libcublas.so.12`.
See the [Faster-Whisper GPU guide](https://github.com/SYSTRAN/faster-whisper#gpu).
Check the driver with `nvidia-smi` before starting.

Use PyTorch CUDA 12.8 so Kokoro and Whisper share compatible CUDA/cuDNN libraries.
In a new environment, install in this order:

```bash
cd /home/daikai/Local-AI
.venv/bin/python -m pip install -r voice_ai_server/requirements-gpu.txt
.venv/bin/python -m pip install -r voice_ai_server/requirements.txt
```

If an existing environment contains `nvidia-cudnn-cu13`, uninstall it **before** installing the GPU requirements above, to avoid two cuDNN packages writing to the same directory. Do not reuse the former separate libraries in `.venv/cuda12`.

Set these values in `voice_ai_server/.env`:

```dotenv
WHISPER_DEVICE=cuda
WHISPER_COMPUTE_TYPE=float16
```

Stop the old server, then run:

```bash
cd /home/daikai/Local-AI
source .venv/bin/activate
cd voice_ai_server
python run.py
```

`run.py` discovers NVIDIA libraries in the environment's site-packages, sets `LD_LIBRARY_PATH`, and restarts Python once if necessary before loading the server. There is no need to run `run_gpu.sh` or export paths manually.
Set `WHISPER_COMPUTE_TYPE=int8_float16` to reduce Whisper VRAM usage.
Ollama manages its GPU separately. Git ignores the libraries inside `.venv/`.

## Processing logs

Running `python run.py` displays INFO logs for each stage:
audio received -> Whisper -> conversation history -> knowledge lookup -> Ollama -> Kokoro -> HTTP response.
Each request has an ID such as `[a1b2c3d4]` to distinguish concurrent requests.
Logs include elapsed time, model names, selected Markdown files, character counts and WAV size.
Initial model loading is logged separately; stage duration includes model loading when needed.
Failures include the stage name and traceback.
Logs do not print transcripts, answers or full knowledge documents.

## API

The base URL is `http://SERVER_IP:8000`, for example `http://192.168.1.10:8000`.
When `API_KEY` is set, all endpoints except `GET /healthz` require:

```http
Authorization: Bearer <API_KEY>
```

| Endpoint | Request body | Response body | Purpose |
| --- | --- | --- | --- |
| `GET /healthz` | None | JSON | HTTP server health |
| `POST /v1/audio/transcriptions` | `multipart/form-data` | JSON | Audio to text |
| `POST /v1/chat/completions` | `application/json` | JSON | Text to AI answer |
| `POST /v1/audio/speech` | `application/json` | Binary `audio/wav` | Text to speech |
| `POST /v1/voice/chat` | `multipart/form-data` | WAV or JSON | Complete voice assistant |

For file uploads, browsers use `FormData`; ESP32 clients need a multipart HTTP library or a body with a boundary. The audio field is always named `audio`. Supported formats include WAV, MP3, M4A, WebM and formats supported by the audio backend. For ESP32, use mono PCM WAV, 16-bit, at 16 kHz or 24 kHz.

### `GET /healthz`

No input is required. This checks HTTP availability, not current model or upstream service health.

```bash
curl http://127.0.0.1:8000/healthz
```

```json
{ "status": "ok" }
```

### `POST /v1/audio/transcriptions` - Speech to text

Input: `multipart/form-data`.

| Field | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `audio` | Binary file | Yes | None | Audio file, limited by `MAX_AUDIO_BYTES` (25 MB by default). |
| `language` | String | No | `null` | Whisper language code, such as `en` or `vi`. Omit for automatic detection. |

```bash
curl -X POST http://127.0.0.1:8000/v1/audio/transcriptions \
  -H 'Authorization: Bearer <API_KEY>' \
  -F 'audio=@question.wav' -F 'language=en'
```

JSON response:

```json
{ "text": "How are you?", "language": "en" }
```

### `POST /v1/chat/completions` - AI text and knowledge

Input: `application/json`.

| Field | Type | Required | Limit/default | Description |
| --- | --- | --- | --- | --- |
| `messages` | Object array | Yes | 1-30 items | Conversation messages, each with `role` and `content`. |
| `messages[].role` | String | Yes | `system`, `user`, `assistant` | The last message should be the user question for knowledge lookup. |
| `messages[].content` | String | Yes | None | Message text. |
| `model` | String | No | `OLLAMA_MODEL` | Ollama model name, such as `dq-assistant:latest`. |

```bash
curl -X POST http://127.0.0.1:8000/v1/chat/completions \
  -H 'Authorization: Bearer <API_KEY>' -H 'Content-Type: application/json' \
  -d '{"messages":[{"role":"user","content":"What are my ESP32 projects?"}]}'
```

```json
{
  "choices": [
    { "message": { "role": "assistant", "content": "...AI answer..." } }
  ]
}
```

Before calling Ollama, the server selects knowledge for the last message and adds it to the prompt. Reference notes are not stored in conversation history, to avoid repeatedly expanding the prompt.

### `POST /v1/audio/speech` - Text to speech

Input: `application/json`.

| Field | Type | Required | Default/limit | Description |
| --- | --- | --- | --- | --- |
| `text` | String | Yes | 1-10,000 characters | Text for Kokoro to speak. |
| `voice` | String/null | No | `KOKORO_VOICE` (`af_heart`) | Voice supported by the loaded model. |
| `speed` | Number | No | `1.0`, range `0.5` to `2.0` | Speech speed; lower is slower. This is not a model weight. |

```bash
curl -X POST http://127.0.0.1:8000/v1/audio/speech \
  -H 'Authorization: Bearer <API_KEY>' -H 'Content-Type: application/json' \
  -d '{"text":"Hello.","voice":"af_heart","speed":1.0}' \
  --output reply.wav
```

The response is binary `audio/wav`: 16-bit PCM WAV at 24 kHz. Use curl's `--output` or read the binary body on ESP32.

### `POST /v1/voice/chat` - Complete pipeline

Use this endpoint for a complete Web UI/ESP32 voice request.
Input: `multipart/form-data`.

| Field | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `audio` | Binary file | Yes | None | Recorded speech. |
| `session_id` | String | No | `default` | Conversation ID. Use a separate ID per device to avoid mixing history. |
| `language` | String/null | No | `null` | Whisper language code, or omit for automatic detection. |
| `voice` | String/null | No | `KOKORO_VOICE` | Response voice. |
| `model` | String/null | No | `OLLAMA_MODEL` | Model override for this request. |
| `response_format` | String | No | `audio` | Either `audio` or `json`. |

Processing: `audio -> Whisper -> knowledge -> Ollama -> Kokoro -> response`.

**Binary audio mode for ESP32:**

```bash
curl -X POST http://192.168.1.10:8000/v1/voice/chat \
  -H 'Authorization: Bearer <API_KEY>' \
  -F 'audio=@question.wav' -F 'session_id=esp32-kitchen' -F 'language=en' \
  --output answer.wav
```

Omitting `response_format` selects `audio`. The binary WAV response includes ASCII-safe headers:

```http
Content-Type: audio/wav
X-Transcript-Length: 42
X-Response-Text-Length: 185
Content-Disposition: inline; filename=voice-response.wav
```

On ESP32, read the response as byte chunks and play or save the WAV; do not use `http.getString()` for binary audio.

**JSON mode for debugging or displaying text in a Web UI:**

```bash
curl -X POST http://192.168.1.10:8000/v1/voice/chat \
  -H 'Authorization: Bearer <API_KEY>' \
  -F 'audio=@question.wav' -F 'response_format=json' -F 'session_id=browser-001'
```

```json
{
  "transcript": "Speech recognized by Whisper",
  "language": "en",
  "text": "The answer from Ollama",
  "audio_format": "wav",
  "audio_base64": "UklGRi..."
}
```

`audio_base64` contains the complete WAV encoded as Base64, approximately 33% larger than the binary file. Use JSON when you need text and audio together; otherwise use `audio` to save memory and bandwidth.

### Model weights and configuration

Clients do not send model weights. The server loads faster-whisper weights for STT, the Ollama model for the LLM, and Kokoro weights for TTS. Clients send audio/text and request parameters.

| `.env` variable | Type | Default | Effect |
| --- | --- | --- | --- |
| `WHISPER_MODEL` | String | `small` | Whisper model name or path. Larger models generally need more time and memory. |
| `WHISPER_DEVICE` | String | `auto` | Whisper device, typically `cpu` or `cuda`. |
| `WHISPER_COMPUTE_TYPE` | String | `int8` | Computation precision. Quantization can affect memory, speed and accuracy. |
| `WHISPER_BEAM_SIZE` | Integer | `1` | Decoding search width. |
| `OLLAMA_MODEL` | String | `dq-assistant:latest` | Default model; inspect installed models with `ollama list`. |
| `OLLAMA_URL` | URL | `http://127.0.0.1:11434` | Ollama API address. |
| `OLLAMA_TIMEOUT_SECONDS` | Number | `120` | Maximum wait for Ollama. |
| `OLLAMA_NUM_CTX` | Integer | `4096` | Context window in tokens. |
| `OLLAMA_NUM_PREDICT` | Integer | `256` | Maximum generated tokens. |
| `OLLAMA_KEEP_ALIVE` | String | `30m` | Requested model retention time after a request. |
| `KOKORO_LANG_CODE` | String | `a` | Kokoro pipeline language code. |
| `KOKORO_VOICE` | String | `af_heart` | Default TTS voice. |
| `MAX_AUDIO_BYTES` | Integer | `26214400` | Maximum upload size in bytes. |
| `MAX_HISTORY_MESSAGES` | Integer | `12` | Messages retained in memory per session. |
| `WARMUP_ON_START` | Boolean | `1` | Load and exercise models before serving requests. |

Ollama uses `temperature=0.6`; `num_predict` and `num_ctx` come from `.env`. These are inference settings, not model weights. Higher temperature increases output variation.

### Error codes

| HTTP status | Typical cause |
| --- | --- |
| `401` | Missing or invalid bearer token when `API_KEY` is set. |
| `413` | Empty audio or upload exceeding `MAX_AUDIO_BYTES`. |
| `422` | Invalid request fields, unreadable audio, failed transcription on the transcription endpoint, or invalid response format. |
| `503` | A handled Whisper, Kokoro or Ollama failure, including an unavailable Ollama service. |

### Security and production

Set `API_KEY` in `.env` and send it as a bearer token. For production, use HTTPS through Nginx/Caddy, restrict `CORS_ORIGINS` to specific domains, and replace in-memory sessions with Redis or a database when using multiple instances.

The default Kokoro voice (`af_heart`) speaks English. For other languages, use a supported voice/model or replace the TTS backend.

## Faster response settings

These values are applied in `.env` and `.env.example`:

```dotenv
WHISPER_BEAM_SIZE=1
OLLAMA_NUM_CTX=4096
OLLAMA_NUM_PREDICT=256
```

- Beam size `1` reduces decoding work compared with the former value `5`, with a possible accuracy tradeoff.
- Context size `4096` reduces memory compared with `8192`, helping the model fit on the GPU. Long history and knowledge may exceed this budget; reduce history or `max_chars` in `knowledge_rules.json` if needed.
- The output limit is reduced from `512` to `256` tokens. Long answers can be cut off at the limit.
- The server requests concise answers, usually 1-3 sentences, to reduce Ollama and Kokoro processing time.

Restart with `python run.py` after changing settings. After a request, inspect the `PROCESSOR` column in `ollama ps`. The target is `100% GPU`, but placement depends on model size and available VRAM. Compare stage timings using the same question; model loading can affect the first request when warmup is disabled.

The model selection is unchanged. The API still waits for the full answer and WAV; audio streaming is not implemented. These settings apply to the voice server, including its text chat API. Terminal chat and web-chat retain their own inference settings.

## Startup warmup and warning handling

```dotenv
WARMUP_ON_START=1
OLLAMA_KEEP_ALIVE=30m
# Enable only after Whisper, Kokoro and the selected voice are cached:
HF_HUB_OFFLINE=1
```

- `WARMUP_ON_START=1` runs Kokoro, Whisper and Ollama during startup. Wait for the warmup `ready` log and `Application startup complete` before sending requests. This moves loading work out of the first request rather than eliminating it. Ollama must be running; warmup failure prevents startup. Set `0` to load models on demand.
- `OLLAMA_KEEP_ALIVE=30m` requests that Ollama keep the model loaded for 30 minutes after a request. Memory pressure or model changes can still cause a reload.
- `HF_HUB_OFFLINE=1` uses cached Hugging Face files without network requests. This machine enables it; `.env.example` uses `0` to allow downloads on new machines. Set `0` and restart before downloading an uncached model or voice. This does not replace installation of the spaCy language model.
- Kokoro receives an explicit `repo_id`. Only the two known PyTorch warnings about single-layer LSTM dropout and the deprecated `weight_norm` API are filtered. Other warnings and errors remain visible.

Ollama logs model loading time, prompt processing time, token generation time and generated token count. Responses still wait for complete WAV generation; audio streaming is not implemented.
