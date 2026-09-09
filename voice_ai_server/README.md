# Voice AI Server

API dùng chung cho thiết bị voice: **audio → faster-whisper → Ollama AI → Kokoro → WAV audio**.
Các model được nạp lazy ở request đầu tiên; lần đầu có thể mất thời gian tải model.

Mỗi câu hỏi AI dùng chính hàm `build_context()` của `../chat.py`, giống `web-chat`: nó đọc lại `../knowledge_rules.json`, tìm Markdown phù hợp trong `../knowledge/`, rồi đưa context đó vào Ollama trước khi trả lời. Model mặc định cũng là `dq-assistant:latest`. Thay đổi knowledge có hiệu lực ngay ở request tiếp theo.

## Cài và chạy

```bash
cd "/home/dq/Local AI/voice_ai_server"
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
# Chỉnh OLLAMA_MODEL trong .env nếu cần, rồi bảo đảm Ollama đang chạy:
ollama serve
python run.py
```

Mở tài liệu API tương tác tại `http://SERVER_IP:8000/docs`. Server mặc định lắng nghe mọi network interface để ESP32, điện thoại, hay ứng dụng khác gọi được.

## API

Base URL là `http://SERVER_IP:8000`, ví dụ `http://192.168.1.10:8000`. Các endpoint trừ `GET /healthz` cần header dưới đây khi `API_KEY` có giá trị:

```http
Authorization: Bearer <API_KEY>
```

| Endpoint | Request body | Response body | Dùng khi |
| --- | --- | --- | --- |
| `GET /healthz` | không có | JSON | kiểm tra server sống |
| `POST /v1/audio/transcriptions` | `multipart/form-data` | JSON | chỉ cần audio → text |
| `POST /v1/chat/completions` | `application/json` | JSON | chỉ cần text → AI text |
| `POST /v1/audio/speech` | `application/json` | binary `audio/wav` | chỉ cần text → giọng nói |
| `POST /v1/voice/chat` | `multipart/form-data` | WAV binary hoặc JSON | voice assistant hoàn chỉnh |

`multipart/form-data` là kiểu request gửi file. Browser dùng `FormData`; ESP32 tạo body có boundary hoặc dùng thư viện HTTP multipart. Audio luôn ở field tên `audio`. Server nhận WAV, MP3, M4A, WebM và các định dạng mà backend audio đọc được. Để ít lỗi nhất trên ESP32, dùng WAV PCM mono, 16-bit, 16 kHz hoặc 24 kHz.

### `GET /healthz`

Không cần input. Dùng để kiểm tra HTTP server, không kiểm tra Whisper/Ollama/Kokoro đã tải model hay chưa.

```bash
curl http://127.0.0.1:8000/healthz
```

```json
{ "status": "ok" }
```

### `POST /v1/audio/transcriptions` — Speech to Text

Input: `multipart/form-data`.

| Field | Kiểu | Bắt buộc | Mặc định | Ý nghĩa |
| --- | --- | --- | --- | --- |
| `audio` | file binary | có | — | File audio, tối đa `MAX_AUDIO_BYTES` (25 MB mặc định). |
| `language` | string | không | `null` | Mã ngôn ngữ Whisper, ví dụ `vi`, `en`. Bỏ trống để tự nhận diện. |

```bash
curl -X POST http://127.0.0.1:8000/v1/audio/transcriptions \
  -H 'Authorization: Bearer <API_KEY>' \
  -F 'audio=@question.wav' -F 'language=vi'
```

Output `application/json`:

```json
{ "text": "Bạn khỏe không?", "language": "vi" }
```

### `POST /v1/chat/completions` — AI text và knowledge

Input: `application/json`.

| Field | Kiểu | Bắt buộc | Giới hạn/mặc định | Ý nghĩa |
| --- | --- | --- | --- | --- |
| `messages` | mảng object | có | 1–30 phần tử | Lịch sử chat. Mỗi object có `role` và `content`. |
| `messages[].role` | string | có | `system`, `user`, `assistant` | Vai trò của message. Câu cuối cần là câu hỏi của user để knowledge được gắn đúng. |
| `messages[].content` | string | có | — | Nội dung text. |
| `model` | string | không | `OLLAMA_MODEL` | Tên model Ollama, ví dụ `dq-assistant:latest`. |

```bash
curl -X POST http://127.0.0.1:8000/v1/chat/completions \
  -H 'Authorization: Bearer <API_KEY>' -H 'Content-Type: application/json' \
  -d '{"messages":[{"role":"user","content":"Dự án ESP32 của tôi là gì?"}]}'
```

Output `application/json`:

```json
{
  "choices": [
    { "message": { "role": "assistant", "content": "...câu trả lời AI..." } }
  ]
}
```

Trước khi gọi Ollama, server lấy `content` của message cuối, đọc lại `knowledge_rules.json` và các Markdown khớp keyword trong `../knowledge/`. Nội dung knowledge được chèn vào prompt giống `web-chat`; các message cũ không bị lưu knowledge để tránh prompt phình quá lớn.

### `POST /v1/audio/speech` — Text to Speech

Input `application/json`:

| Field | Kiểu | Bắt buộc | Mặc định/giới hạn | Ý nghĩa |
| --- | --- | --- | --- | --- |
| `text` | string | có | 1–10.000 ký tự | Văn bản Kokoro cần đọc. |
| `voice` | string/null | không | `KOKORO_VOICE` (`af_heart`) | Tên voice Kokoro. Voice phải tồn tại trong model đang dùng. |
| `speed` | number | không | `1.0`, từ `0.5` đến `2.0` | Tốc độ nói; nhỏ hơn là chậm hơn. Không phải “trọng số” model. |

```bash
curl -X POST http://127.0.0.1:8000/v1/audio/speech \
  -H 'Authorization: Bearer <API_KEY>' -H 'Content-Type: application/json' \
  -d '{"text":"Xin chào.","voice":"af_heart","speed":1.0}' \
  --output reply.wav
```

Output là `Content-Type: audio/wav`: WAV PCM 16-bit, sample rate 24 kHz. Không phải JSON; dùng `--output` với curl hoặc đọc HTTP response body dạng binary trên ESP32.

### `POST /v1/voice/chat` — toàn bộ pipeline

Đây là endpoint phù hợp nhất cho Web UI/ESP32. Input `multipart/form-data`:

| Field | Kiểu | Bắt buộc | Mặc định | Ý nghĩa |
| --- | --- | --- | --- | --- |
| `audio` | file binary | có | — | Câu nói đã ghi âm. |
| `session_id` | string | không | `default` | ID cuộc hội thoại. Mỗi thiết bị dùng một ID riêng để không lẫn lịch sử. |
| `language` | string/null | không | `null` | `vi`, `en`,... hoặc bỏ trống để Whisper tự nhận diện. |
| `voice` | string/null | không | `KOKORO_VOICE` | Voice Kokoro cho câu trả lời. |
| `model` | string/null | không | `OLLAMA_MODEL` | Model Ollama dùng riêng cho request này. |
| `response_format` | string | không | `audio` | Chỉ nhận `audio` hoặc `json`. |

Xử lý: `audio → Whisper → knowledge → Ollama → Kokoro → response`.

**Chế độ khuyến nghị cho ESP32: audio raw**

```bash
curl -X POST http://192.168.1.10:8000/v1/voice/chat \
  -H 'Authorization: Bearer <API_KEY>' \
  -F 'audio=@question.wav' -F 'session_id=esp32-kitchen' -F 'language=vi' \
  --output answer.wav
```

Bỏ `response_format` sẽ dùng `audio`. Output là body binary `audio/wav`. Response có thêm header ASCII-safe:

```http
Content-Type: audio/wav
Content-Disposition: inline; filename=voice-response.wav
X-Transcript-Length: 42
X-Response-Text-Length: 185
```

Không dùng `http.getString()` trên ESP32 cho response này: cần đọc HTTP stream/body thành byte chunks và phát/lưu WAV.

**Chế độ JSON, phù hợp debug/Web UI cần hiển thị text**

```bash
curl -X POST http://192.168.1.10:8000/v1/voice/chat \
  -H 'Authorization: Bearer <API_KEY>' \
  -F 'audio=@question.wav' -F 'response_format=json' -F 'session_id=browser-001'
```

Output `application/json`:

```json
{
  "transcript": "Câu nói nhận được từ Whisper",
  "language": "vi",
  "text": "Câu trả lời của Ollama",
  "audio_format": "wav",
  "audio_base64": "UklGRi..."
}
```

`audio_base64` là toàn bộ file WAV đã mã hóa Base64, lớn hơn WAV gốc khoảng 33%. Chỉ dùng khi cần text và audio trong một JSON; nếu chỉ phát âm thanh, dùng `audio` để tiết kiệm RAM/băng thông.

### Trọng số model và các tham số cấu hình

Client **không gửi trọng số (weights) của AI** trong API. Weights là các file model đã được nạp trên server: faster-whisper cho STT, model Ollama cho LLM, và Kokoro cho TTS. Client chỉ gửi audio/text và các tham số request ở trên.

| Biến `.env` | Kiểu | Mặc định | Tác động |
| --- | --- | --- | --- |
| `WHISPER_MODEL` | string | `small` | Kích thước/tên weights Whisper. Model lớn hơn thường chính xác hơn nhưng chậm và tốn RAM/VRAM hơn. |
| `WHISPER_DEVICE` | string | `auto` | Thiết bị chạy Whisper (`auto`, thường `cpu` hoặc `cuda` tùy máy). |
| `WHISPER_COMPUTE_TYPE` | string | `int8` | Kiểu tính toán Whisper; `int8` giảm RAM/tăng tốc CPU, có thể khác nhỏ về độ chính xác. |
| `OLLAMA_MODEL` | string | `dq-assistant:latest` | Tên weights/model Ollama mặc định. Kiểm tra bằng `ollama list`. |
| `OLLAMA_URL` | URL | `http://127.0.0.1:11434` | API Ollama mà voice server gọi. |
| `OLLAMA_TIMEOUT_SECONDS` | number | `120` | Thời gian tối đa đợi Ollama trả lời. |
| `KOKORO_LANG_CODE` | string | `a` | Mã ngôn ngữ/nhóm pipeline của Kokoro. |
| `KOKORO_VOICE` | string | `af_heart` | Voice TTS mặc định, không phải AI weight của Ollama. |
| `MAX_AUDIO_BYTES` | integer | `26214400` | Kích thước audio upload tối đa, tính theo bytes. |
| `MAX_HISTORY_MESSAGES` | integer | `12` | Số messages gần nhất lưu RAM cho mỗi `session_id`. |

Hiện tại Ollama dùng cố định `temperature=0.6`, `num_predict=512`, `num_ctx=8192` trong server. Đây là tham số suy luận: temperature cao tạo câu trả lời đa dạng hơn nhưng kém ổn định; `num_predict` là số token sinh tối đa; `num_ctx` là độ dài context. Chúng không phải trọng số model.

### Mã lỗi

| HTTP status | Ý nghĩa thường gặp |
| --- | --- |
| `401` | Thiếu/sai `Authorization: Bearer ...` khi server có `API_KEY`. |
| `413` | Audio rỗng hoặc vượt `MAX_AUDIO_BYTES`. |
| `422` | Body/field sai kiểu, file không đọc được, audio không thể transcribe, hoặc `response_format` không hợp lệ. |
| `503` | Whisper/Kokoro/Ollama lỗi, chưa tải được model, hoặc Ollama không chạy. |

### Bảo mật và production

Đặt `API_KEY` trong `.env`, mỗi client gửi `Authorization: Bearer <API_KEY>`. Trong production nên chạy sau Nginx/Caddy với HTTPS, đặt `CORS_ORIGINS` về các domain cụ thể thay vì `*`, và dùng Redis/database thay cho session memory nếu chạy nhiều instance.

Kokoro mặc định (`af_heart`) là giọng Anh. Để tiếng Việt tự nhiên hơn, thay TTS backend hoặc cung cấp voice/model Kokoro phù hợp; pipeline vẫn không đổi.
