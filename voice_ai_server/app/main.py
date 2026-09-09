import base64
from contextlib import asynccontextmanager

import httpx
from fastapi import Depends, FastAPI, File, Form, Header, HTTPException, Request, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, Response
from pydantic import BaseModel, Field

from .config import Settings, get_settings
from .services import OllamaChat, SessionStore, SpeechToText, TextToSpeech


class SpeechRequest(BaseModel):
    text: str = Field(min_length=1, max_length=10_000)
    voice: str | None = None
    speed: float = Field(default=1.0, ge=0.5, le=2.0)


class ChatRequest(BaseModel):
    messages: list[dict[str, str]] = Field(min_length=1, max_length=30)
    model: str | None = None


def require_api_key(authorization: str | None = Header(default=None), settings: Settings = Depends(get_settings)):
    if settings.api_key and authorization != f"Bearer {settings.api_key}":
        raise HTTPException(status_code=401, detail="Missing or invalid API key")


def get_services(request: Request):
    return request.app.state.services


@asynccontextmanager
async def lifespan(app: FastAPI):
    settings = get_settings()
    app.state.services = {
        "settings": settings,
        "stt": SpeechToText(settings),
        "tts": TextToSpeech(settings),
        "chat": OllamaChat(settings),
        "sessions": SessionStore(settings.max_history_messages),
    }
    yield


app = FastAPI(title="Voice AI Server", version="1.0.0", lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=get_settings().allowed_origins, allow_credentials=False, allow_methods=["*"], allow_headers=["*"])


@app.get("/healthz")
async def healthz():
    return {"status": "ok"}


@app.post("/v1/audio/transcriptions", dependencies=[Depends(require_api_key)])
async def transcriptions(audio: UploadFile = File(...), language: str | None = Form(default=None), services=Depends(get_services)):
    data = await audio.read(services["settings"].max_audio_bytes + 1)
    if not data or len(data) > services["settings"].max_audio_bytes:
        raise HTTPException(413, "Audio is empty or exceeds MAX_AUDIO_BYTES")
    try:
        text, detected_language = await services["stt"].transcribe(data, language)
    except Exception as exc:
        raise HTTPException(422, f"Could not transcribe audio: {exc}") from exc
    return {"text": text, "language": detected_language}


@app.post("/v1/audio/speech", dependencies=[Depends(require_api_key)])
async def speech(body: SpeechRequest, services=Depends(get_services)):
    try:
        wav = await services["tts"].synthesize(body.text, body.voice, body.speed)
    except Exception as exc:
        raise HTTPException(503, f"Speech synthesis failed: {exc}") from exc
    return Response(wav, media_type="audio/wav", headers={"Content-Disposition": "inline; filename=response.wav"})


@app.post("/v1/chat/completions", dependencies=[Depends(require_api_key)])
async def chat(body: ChatRequest, services=Depends(get_services)):
    if any(message.get("role") not in {"system", "user", "assistant"} or not isinstance(message.get("content"), str) for message in body.messages):
        raise HTTPException(422, "Each message needs role (system/user/assistant) and string content")
    try:
        answer = await services["chat"].complete(body.messages, body.model)
    except (httpx.HTTPError, ValueError) as exc:
        raise HTTPException(503, f"AI service failed: {exc}") from exc
    return {"choices": [{"message": {"role": "assistant", "content": answer}}]}


@app.post("/v1/voice/chat", dependencies=[Depends(require_api_key)])
async def voice_chat(
    audio: UploadFile = File(...), session_id: str = Form(default="default"), language: str | None = Form(default=None),
    voice: str | None = Form(default=None), model: str | None = Form(default=None), response_format: str = Form(default="audio"), services=Depends(get_services),
):
    data = await audio.read(services["settings"].max_audio_bytes + 1)
    if not data or len(data) > services["settings"].max_audio_bytes:
        raise HTTPException(413, "Audio is empty or exceeds MAX_AUDIO_BYTES")
    try:
        transcript, detected_language = await services["stt"].transcribe(data, language)
        if not transcript:
            raise ValueError("No speech detected")
        messages = services["sessions"].messages(session_id, transcript)
        answer = await services["chat"].complete(messages, model)
        services["sessions"].save(session_id, transcript, answer)
        wav = await services["tts"].synthesize(answer, voice)
    except (httpx.HTTPError, ValueError) as exc:
        raise HTTPException(503, f"Voice pipeline failed: {exc}") from exc
    if response_format == "json":
        return JSONResponse({"transcript": transcript, "language": detected_language, "text": answer, "audio_format": "wav", "audio_base64": base64.b64encode(wav).decode()})
    if response_format != "audio":
        raise HTTPException(422, "response_format must be audio or json")
    # Full text is available in response_format=json; headers stay ASCII-safe.
    return Response(wav, media_type="audio/wav", headers={"X-Transcript-Length": str(len(transcript)), "X-Response-Text-Length": str(len(answer)), "Content-Disposition": "inline; filename=voice-response.wav"})
