import os
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path


def _load_dotenv() -> None:
    """Small dependency-free .env reader; real environment variables win."""
    env_file = Path(__file__).resolve().parents[1] / ".env"
    for line in env_file.read_text(encoding="utf-8").splitlines() if env_file.exists() else []:
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        os.environ.setdefault(key.strip(), value.strip().strip('"').strip("'"))


@dataclass(frozen=True)
class Settings:
    warmup_on_start: bool = True
    ollama_keep_alive: str = "30m"
    api_key: str = ""
    cors_origins: str = "*"
    whisper_model: str = "small"
    whisper_device: str = "auto"
    whisper_compute_type: str = "int8"
    whisper_beam_size: int = 1
    ollama_num_ctx: int = 4096
    ollama_num_predict: int = 256
    ollama_url: str = "http://127.0.0.1:11434"
    # Match web-chat's default. Set OLLAMA_MODEL to override it per deployment.
    ollama_model: str = "dq-assistant:latest"
    ollama_timeout_seconds: float = 120
    qdrant_url: str = "http://127.0.0.1:6333"
    qdrant_api_key: str = ""
    qdrant_collection: str = "local_ai_knowledge"
    ollama_embed_model: str = "embeddinggemma"
    rag_top_k: int = 5
    rag_score_threshold: float = 0.35
    rag_max_chars: int = 9000
    kokoro_lang_code: str = "a"
    kokoro_voice: str = "af_heart"
    max_audio_bytes: int = 25 * 1024 * 1024
    max_history_messages: int = 12

    @property
    def allowed_origins(self) -> list[str]:
        return [item.strip() for item in self.cors_origins.split(",") if item.strip()]


@lru_cache
def get_settings() -> Settings:
    _load_dotenv()
    return Settings(
        warmup_on_start=os.getenv("WARMUP_ON_START", "1").lower() in {"1", "true", "yes"},
        ollama_keep_alive=os.getenv("OLLAMA_KEEP_ALIVE", "30m"),
        api_key=os.getenv("API_KEY", ""), cors_origins=os.getenv("CORS_ORIGINS", "*"),
        whisper_model=os.getenv("WHISPER_MODEL", "small"), whisper_device=os.getenv("WHISPER_DEVICE", "auto"),
        whisper_compute_type=os.getenv("WHISPER_COMPUTE_TYPE", "int8"), ollama_url=os.getenv("OLLAMA_URL", "http://127.0.0.1:11434"),
        ollama_model=os.getenv("OLLAMA_MODEL", "dq-assistant:latest"), ollama_timeout_seconds=float(os.getenv("OLLAMA_TIMEOUT_SECONDS", "120")),
        qdrant_url=os.getenv("QDRANT_URL", "http://127.0.0.1:6333"), qdrant_api_key=os.getenv("QDRANT_API_KEY", ""),
        qdrant_collection=os.getenv("QDRANT_COLLECTION", "local_ai_knowledge"),
        ollama_embed_model=os.getenv("OLLAMA_EMBED_MODEL", "embeddinggemma"),
        rag_top_k=int(os.getenv("RAG_TOP_K", "5")), rag_score_threshold=float(os.getenv("RAG_SCORE_THRESHOLD", "0.35")),
        rag_max_chars=int(os.getenv("RAG_MAX_CHARS", "9000")),
        whisper_beam_size=int(os.getenv("WHISPER_BEAM_SIZE", "1")),
        ollama_num_ctx=int(os.getenv("OLLAMA_NUM_CTX", "4096")),
        ollama_num_predict=int(os.getenv("OLLAMA_NUM_PREDICT", "256")),
        kokoro_lang_code=os.getenv("KOKORO_LANG_CODE", "a"), kokoro_voice=os.getenv("KOKORO_VOICE", "af_heart"),
        max_audio_bytes=int(os.getenv("MAX_AUDIO_BYTES", str(25 * 1024 * 1024))), max_history_messages=int(os.getenv("MAX_HISTORY_MESSAGES", "12")),
    )
