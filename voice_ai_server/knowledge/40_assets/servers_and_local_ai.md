---
id: assets.servers-and-local-ai
type: asset-inventory
status: active
updated: 2026-09-11
confidence: confirmed
tags: [servers, hosting, local-ai, ollama]
---

# Servers and Local AI Assets

## Main Server / Hosting Environment

### VPS
Operating system:
- Debian 12

Web server:
- Apache 2.4.x

DNS / proxy:
- Hostinger DNS
- Cloudflare

## Known Websites

### dqtech.cloud
Frontend:
- React SPA

Known frontend path:
- /var/www/dqtech.cloud/dist

API:
- Reverse proxied to localhost
- Ports used include 8000 / 8001

### drums.dqtech.cloud
Frontend:
- React

Backend:
- Flask
- Port 8001

### ormin.dqcloud.online
Project:
- Ormin Project V5

Frontend path:
- /home/drums/Ormin_Project_V5/HMI/build

Backend:
- 127.0.0.1:8001

## Common Server Technologies
- Ubuntu
- Debian
- Apache
- Flask
- FastAPI
- Uvicorn
- React
- Docker
- MySQL
- SQLite
- Cloudflare

## Local AI Server

### Hardware
Main AI computer:
- NVIDIA RTX 3080
- 10 GB VRAM

### Ollama
Models used or tested:
- Qwen3:8B
- Gemma

Default Ollama API port:
- 11434

Important endpoints:
- /api/chat
- /api/generate
- /api/tags
- /api/ps
- /api/embed

## Qwen3:8B
Typical use:
- Local chatbot
- Robot agent
- Technical assistant
- RAG-based knowledge assistant

## Recommended Personal AI Architecture

Client
↓
FastAPI
↓
User profile + memory
↓
RAG search
↓
Relevant context
↓
Qwen3:8B via Ollama
↓
Answer / action

## RAG Components
Recommended stack:
- Markdown knowledge files
- Python
- Embedding model
- ChromaDB or FAISS
- Ollama Qwen model

Possible embedding model:
- qwen3-embedding

## Voice AI Pipeline
Microphone
↓
Faster-Whisper
↓
Text
↓
Qwen / Ollama
↓
Text
↓
Kokoro TTS
↓
Audio

## Useful Linux Checks

Check Ollama service:
```bash
systemctl status ollama
```

Start Ollama:
```bash
sudo systemctl start ollama
```

Enable Ollama:
```bash
sudo systemctl enable ollama
```

List installed models:
```bash
ollama list
```

List loaded models:
```bash
ollama ps
```

Run Qwen:
```bash
ollama run qwen3:8b
```

Check port:
```bash
ss -tulpn | grep 11434
```
