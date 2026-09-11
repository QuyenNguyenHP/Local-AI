import asyncio
import io
import json
from collections import defaultdict

import httpx

from .config import Settings

from .context import build_context
from .rag import SemanticKnowledge
from .progress import log, stage
from time import perf_counter


class SpeechToText:
    """Lazy model loader so the HTTP server can start before models download."""

    def __init__(self, settings: Settings):
        self.settings = settings
        self._model = None
        self._lock = asyncio.Lock()

    async def _get_model(self):
        async with self._lock:
            if self._model is None:
                log("Loading Whisper | model=%s device=%s compute=%s", self.settings.whisper_model, self.settings.whisper_device, self.settings.whisper_compute_type)
                from faster_whisper import WhisperModel
                self._model = await asyncio.to_thread(
                    WhisperModel,
                    self.settings.whisper_model,
                    device=self.settings.whisper_device,
                    compute_type=self.settings.whisper_compute_type,
                )
            return self._model

    @stage("Whisper: audio -> text")
    async def transcribe(self, audio: bytes, language: str | None = None) -> tuple[str, str | None]:
        model = await self._get_model()

        def run():
            segments, info = model.transcribe(
                io.BytesIO(audio), language=language, vad_filter=True, beam_size=self.settings.whisper_beam_size
            )
            return " ".join(segment.text.strip() for segment in segments).strip(), info.language

        result = await asyncio.to_thread(run)
        log("Whisper | language=%s, text=%d characters", result[1], len(result[0]))
        return result


class TextToSpeech:
    def __init__(self, settings: Settings):
        self.settings = settings
        self._pipeline = None
        self._lock = asyncio.Lock()

    async def _get_pipeline(self):
        async with self._lock:
            if self._pipeline is None:
                log("Loading Kokoro model | language=%s", self.settings.kokoro_lang_code)
                import warnings
                # Known upstream warnings only; retain other warnings and errors.
                warnings.filterwarnings("ignore", message="dropout option adds dropout after all but last recurrent layer.*", category=UserWarning, module=r"torch\.nn\.modules\.rnn")
                warnings.filterwarnings("ignore", message=r"`torch\.nn\.utils\.weight_norm` is deprecated.*", category=FutureWarning, module=r"torch\.nn\.utils\.weight_norm")
                from kokoro import KPipeline
                self._pipeline = await asyncio.to_thread(KPipeline, lang_code=self.settings.kokoro_lang_code, repo_id="hexgrad/Kokoro-82M")
            return self._pipeline

    @stage("Kokoro: text -> audio")
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

        wav = await asyncio.to_thread(run)
        log("Kokoro | voice=%s, WAV=%d bytes", selected_voice, len(wav))
        return wav


class OllamaChat:
    def __init__(self, settings: Settings):
        self.settings = settings
        self.rag = SemanticKnowledge(settings)

    @stage("Chat: knowledge -> Ollama")
    async def complete(self, messages: list[dict[str, str]], model: str | None = None) -> str:
        # Retain only recent conversation turns and enrich only the newest user
        # question with excerpts retrieved from the semantic knowledge index.
        latest_question = messages[-1]["content"]
        start = perf_counter()
        log("Knowledge lookup | question=%d characters", len(latest_question))
        context = await build_context(latest_question, self.rag)
        log("Knowledge lookup | completed in %.2fs, prompt=%d characters", perf_counter() - start, len(context))
        context = "Keep your answer concise, usually 1 to 3 short sentences. " + context
        enriched_messages = [
            *messages[:-1][-10:],
            {"role": "user", "content": context},
        ]
        payload = {
            "model": model or self.settings.ollama_model,
            "messages": enriched_messages,
            "stream": False,
            # The voice API needs a spoken answer, not a response that spends
            # the entire token budget on Qwen's hidden reasoning field.
            "think": False,
            "keep_alive": self.settings.ollama_keep_alive,
            "options": {"temperature": 0.6, "num_ctx": self.settings.ollama_num_ctx, "num_predict": self.settings.ollama_num_predict},
        }
        start = perf_counter()
        log("Ollama | sending request model=%s, messages=%d", payload["model"], len(enriched_messages))
        timeout = httpx.Timeout(self.settings.ollama_timeout_seconds)
        async with httpx.AsyncClient(timeout=timeout) as client:
            response = await client.post(self.settings.ollama_url.rstrip("/") + "/api/chat", json=payload)
            response.raise_for_status()
        metrics = response.json()
        log("Ollama details | load=%.2fs, prompt processing=%.2fs, generation=%.2fs, tokens=%s", metrics.get("load_duration", 0) / 1e9, metrics.get("prompt_eval_duration", 0) / 1e9, metrics.get("eval_duration", 0) / 1e9, metrics.get("eval_count", 0))
        answer = metrics.get("message", {}).get("content", "").strip()
        if not answer:
            raise ValueError("Ollama returned an empty answer")
        log("Ollama | completed in %.2fs, answer=%d characters", perf_counter() - start, len(answer))
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
