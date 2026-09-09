import os
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path


def _load_dotenv() -> None:
    """Small dependency-free .env reader; real environment variables win."""
    for line in Path(".env").read_text(encoding="utf-8").splitlines() if Path(".env").exists() else []:
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        os.environ.setdefault(key.strip(), value.strip().strip('"').strip("'"))


@dataclass(frozen=True)
class Settings:
    api_key: str = ""
    cors_origins: str = "*"
    whisper_model: str = "small"
    whisper_device: str = "auto"
    whisper_compute_type: str = "int8"
    ollama_url: str = "http://127.0.0.1:11434"
    # Match web-chat's default. Set OLLAMA_MODEL to override it per deployment.
    ollama_model: str = "dq-assistant:latest"
    ollama_timeout_seconds: float = 120
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
        api_key=os.getenv("API_KEY", ""), cors_origins=os.getenv("CORS_ORIGINS", "*"),
        whisper_model=os.getenv("WHISPER_MODEL", "small"), whisper_device=os.getenv("WHISPER_DEVICE", "auto"),
        whisper_compute_type=os.getenv("WHISPER_COMPUTE_TYPE", "int8"), ollama_url=os.getenv("OLLAMA_URL", "http://127.0.0.1:11434"),
        ollama_model=os.getenv("OLLAMA_MODEL", "dq-assistant:latest"), ollama_timeout_seconds=float(os.getenv("OLLAMA_TIMEOUT_SECONDS", "120")),
        kokoro_lang_code=os.getenv("KOKORO_LANG_CODE", "a"), kokoro_voice=os.getenv("KOKORO_VOICE", "af_heart"),
        max_audio_bytes=int(os.getenv("MAX_AUDIO_BYTES", str(25 * 1024 * 1024))), max_history_messages=int(os.getenv("MAX_HISTORY_MESSAGES", "12")),
    )
