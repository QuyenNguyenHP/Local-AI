import asyncio
import io
import json
import sys
from collections import defaultdict
from pathlib import Path

import httpx

from .config import Settings

# Reuse the terminal and web-chat knowledge path verbatim.  services.py is
# <workspace>/voice_ai_server/app/services.py, so parents[2] is the workspace.
WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
if str(WORKSPACE_ROOT) not in sys.path:
    sys.path.insert(0, str(WORKSPACE_ROOT))
from chat import build_context  # noqa: E402


class SpeechToText:
    """Lazy model loader so the HTTP server can start before models download."""

    def __init__(self, settings: Settings):
        self.settings = settings
        self._model = None
        self._lock = asyncio.Lock()

    async def _get_model(self):
        async with self._lock:
            if self._model is None:
                from faster_whisper import WhisperModel
                self._model = await asyncio.to_thread(
                    WhisperModel,
                    self.settings.whisper_model,
                    device=self.settings.whisper_device,
                    compute_type=self.settings.whisper_compute_type,
                )
            return self._model

    async def transcribe(self, audio: bytes, language: str | None = None) -> tuple[str, str | None]:
        model = await self._get_model()

        def run():
            segments, info = model.transcribe(
                io.BytesIO(audio), language=language, vad_filter=True, beam_size=5
            )
            return " ".join(segment.text.strip() for segment in segments).strip(), info.language

        return await asyncio.to_thread(run)


class TextToSpeech:
    def __init__(self, settings: Settings):
        self.settings = settings
        self._pipeline = None
        self._lock = asyncio.Lock()

    async def _get_pipeline(self):
        async with self._lock:
            if self._pipeline is None:
                from kokoro import KPipeline
                self._pipeline = await asyncio.to_thread(KPipeline, lang_code=self.settings.kokoro_lang_code)
            return self._pipeline

    async def synthesize(self, text: str, voice: str | None = None, speed: float = 1.0) -> bytes:
        pipeline = await self._get_pipeline()
        selected_voice = voice or self.settings.kokoro_voice

        def run():
            import numpy as np
            import soundfile as sf
            pieces = []
            for _, _, audio in pipeline(text, voice=selected_voice, speed=speed):
                pieces.append(audio)
            if not pieces:
                raise ValueError("Kokoro produced no audio")
            output = io.BytesIO()
            sf.write(output, np.concatenate(pieces), 24000, format="WAV", subtype="PCM_16")
            return output.getvalue()

        return await asyncio.to_thread(run)


class OllamaChat:
    def __init__(self, settings: Settings):
        self.settings = settings

    async def complete(self, messages: list[dict[str, str]], model: str | None = None) -> str:
        # This is intentionally the same policy as web-chat/server/app.js:
        # retain only recent conversation turns and enrich *only* the newest
        # user question. build_context reloads knowledge_rules.json and matching
        # Markdown files every call, so edits apply with no server restart.
        latest_question = messages[-1]["content"]
        context = await asyncio.to_thread(build_context, latest_question)
        enriched_messages = [
            *messages[:-1][-10:],
            {"role": "user", "content": context},
        ]
        payload = {
            "model": model or self.settings.ollama_model,
            "messages": enriched_messages,
            "stream": False,
            "options": {"temperature": 0.6, "num_ctx": 8192, "num_predict": 512},
        }
        timeout = httpx.Timeout(self.settings.ollama_timeout_seconds)
        async with httpx.AsyncClient(timeout=timeout) as client:
            response = await client.post(self.settings.ollama_url.rstrip("/") + "/api/chat", json=payload)
            response.raise_for_status()
        answer = response.json().get("message", {}).get("content", "").strip()
        if not answer:
            raise ValueError("Ollama returned an empty answer")
        return answer


class SessionStore:
    """Ephemeral session memory. Use Redis/database when multiple server instances are deployed."""

    def __init__(self, max_messages: int):
        self.max_messages = max_messages
        self._sessions: dict[str, list[dict[str, str]]] = defaultdict(list)

    def messages(self, session_id: str, user_text: str) -> list[dict[str, str]]:
        return [*self._sessions[session_id][-self.max_messages:], {"role": "user", "content": user_text}]

    def save(self, session_id: str, user_text: str, assistant_text: str) -> None:
        history = self._sessions[session_id]
        history.extend(({"role": "user", "content": user_text}, {"role": "assistant", "content": assistant_text}))
        del history[:-self.max_messages]
