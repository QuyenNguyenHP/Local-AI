import asyncio
import base64
import io
import json
from collections import defaultdict
from typing import Any

import httpx

from .config import Settings

from .context import build_context
from .rag import SemanticKnowledge
from .progress import log, stage
from .tools.registry import ToolRegistry
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
                if self.settings.tts_language == "vi":
                    device = self.settings.kokoro_vi_device
                    if device == "auto":
                        import torch
                        device = "cuda" if torch.cuda.is_available() else "cpu"
                    if device not in {"cpu", "cuda"}:
                        raise ValueError("KOKORO_VI_DEVICE must be auto, cpu, or cuda")
                    log("Loading Kokoro Vietnamese model | device=%s", device)
                    from kokoro_vietnamese import KokoroVietnamese
                    self._pipeline = await asyncio.to_thread(
                        KokoroVietnamese, device=device, voice=self.settings.kokoro_voice
                    )
                elif self.settings.tts_language == "en":
                    log("Loading Kokoro English model | language=%s", self.settings.kokoro_lang_code)
                    import warnings
                    # Known upstream warnings only; retain other warnings and errors.
                    warnings.filterwarnings("ignore", message="dropout option adds dropout after all but last recurrent layer.*", category=UserWarning, module=r"torch\.nn\.modules\.rnn")
                    warnings.filterwarnings("ignore", message=r"`torch\.nn\.utils\.weight_norm` is deprecated.*", category=FutureWarning, module=r"torch\.nn\.utils\.weight_norm")
                    from kokoro import KPipeline
                    self._pipeline = await asyncio.to_thread(KPipeline, lang_code=self.settings.kokoro_lang_code, repo_id="hexgrad/Kokoro-82M")
                else:
                    raise ValueError("TTS_LANGUAGE must be en or vi")
            return self._pipeline

    @stage("Kokoro: text -> audio")
    async def synthesize(self, text: str, voice: str | None = None, speed: float = 1.0) -> bytes:
        pipeline = await self._get_pipeline()
        selected_voice = voice or self.settings.kokoro_voice

        def run():
            import numpy as np
            import soundfile as sf
            if self.settings.tts_language == "vi":
                # The Vietnamese package has a different inference API and does
                # not expose Kokoro's speed parameter.
                if voice and voice != self.settings.kokoro_voice:
                    raise ValueError("Vietnamese voice is selected by KOKORO_VOICE; restart after changing it")
                if speed != 1.0:
                    log("Kokoro Vietnamese | speed=%s ignored by this backend", speed)
                audio, _phonemes = pipeline.synthesize(text)
                output = io.BytesIO()
                sf.write(output, audio, 24000, format="WAV", subtype="PCM_16")
                return output.getvalue()
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
        self.tools = ToolRegistry(settings)

    @stage("Chat: knowledge -> Ollama")
    async def complete(self, messages: list[dict[str, Any]], model: str | None = None, allow_tools: bool = True) -> str:
        # Retain only recent conversation turns and enrich only the newest user
        # question with excerpts retrieved from the semantic knowledge index.
        latest_question = messages[-1]["content"]
        start = perf_counter()
        log("Knowledge lookup | question=%d characters", len(latest_question))
        context = await build_context(latest_question, self.rag)
        log("Knowledge lookup | completed in %.2fs, prompt=%d characters", perf_counter() - start, len(context))
        context = (
            "Answer clearly and with enough detail to fully address the question. "
            "Keep simple answers concise, but for technical or complex questions provide "
            "a structured explanation, practical steps, important caveats, and useful examples. "
            + context
        )
        latest_message: dict[str, Any] = {"role": "user", "content": context}
        image = messages[-1].get("image")
        if isinstance(image, bytes):
            # Ollama's native chat API expects bare Base64 image data, not a
            # data URL. Images are accepted only from the image endpoint.
            latest_message["images"] = [base64.b64encode(image).decode("ascii")]
        enriched_messages = [
            *messages[:-1][-10:],
            latest_message,
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
        tool_definitions = self.tools.definitions() if allow_tools else []
        if tool_definitions:
            payload["tools"] = tool_definitions
        start = perf_counter()
        log("Ollama | sending request model=%s, messages=%d", payload["model"], len(enriched_messages))
        timeout = httpx.Timeout(self.settings.ollama_timeout_seconds)
        async with httpx.AsyncClient(timeout=timeout) as client:
            response = await client.post(self.settings.ollama_url.rstrip("/") + "/api/chat", json=payload)
            response.raise_for_status()
        metrics = response.json()
        log("Ollama details | load=%.2fs, prompt processing=%.2fs, generation=%.2fs, tokens=%s", metrics.get("load_duration", 0) / 1e9, metrics.get("prompt_eval_duration", 0) / 1e9, metrics.get("eval_duration", 0) / 1e9, metrics.get("eval_count", 0))
        assistant_message = metrics.get("message", {})
        tool_calls = assistant_message.get("tool_calls", [])
        if tool_calls:
            # One action round is intentional: it prevents an accidental loop
            # and each tool is independently checked by the registry.
            if len(tool_calls) != 1:
                raise ValueError("Only one Home Assistant action may be requested at a time")
            call = tool_calls[0].get("function", {})
            result = await self.tools.execute(call.get("name", ""), call.get("arguments"))
            follow_up = {
                **payload,
                "messages": [
                    *enriched_messages,
                    assistant_message,
                    {"role": "tool", "content": result},
                ],
            }
            async with httpx.AsyncClient(timeout=timeout) as client:
                response = await client.post(self.settings.ollama_url.rstrip("/") + "/api/chat", json=follow_up)
                response.raise_for_status()
            metrics = response.json()
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
