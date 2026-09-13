# DQ AI Chat

A ChatGPT-style React interface with an Express backend that obtains text-only
responses from the local Voice AI API.

## Run

Requires Python 3.10+, Node.js 20.19+ and Ollama with a model installed.

```bash
cd "/home/dq/Local AI/web-chat"
npm install
npm run dev
```

Open http://localhost:5173. The backend runs on http://127.0.0.1:3001.
If Ollama is offline, run `ollama serve`. Install a model if needed with `ollama pull gemma3:4b`.

## Production build

```bash
npm run build
npm start
```

Open http://127.0.0.1:3001. Express serves both the built frontend and API.

Optional shell environment variables:

- `OLLAMA_URL`: default `http://127.0.0.1:11434`; used only to list installed models.
- `OLLAMA_MODEL`: preferred model, default `dq-assistant:latest`.
- `VOICE_AI_URL`: Voice AI API URL, default `http://127.0.0.1:8000`.
- `VOICE_AI_API_KEY`: optional bearer token for the Voice AI API.
- `PORT`: backend port, default `3001`. The development proxy expects port 3001.

Example: `OLLAMA_MODEL=gemma3:4b npm start`

## Features

- Streaming answers, conversation context, and stop generation
- Installed model selection and connection status
- New conversations, search, deletion, and browser-local history
- Markdown, code blocks, tables, and copy response
- Responsive sidebar, keyboard sending, and Vietnamese input
- Clear model connection and generation errors

Conversation history is stored in this browser's localStorage, not in a server database. Clearing browser data removes it. Each text chat request is forwarded to `POST /v1/chat/completions` on `voice_ai_server`; that server owns RAG, Ollama embeddings and Qdrant. It does not invoke Whisper or Kokoro for this endpoint. Changes under `voice_ai_server/knowledge/` take effect after rerunning the indexer.

The server binds to loopback for personal local use. Authentication and multi-user storage are not included.

## Verify

```bash
npm test
npm run build
```
