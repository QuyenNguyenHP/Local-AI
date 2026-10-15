# Voice AI Server

## Tài liệu nội dung trình bày với khách hàng

**Phiên bản:** 1.0
**Ngày:** 15/10/2026
**Mô hình ngôn ngữ:** `Qwen3:8B-Q4_K_M`

---

## 1. Tổng quan giải pháp

Voice AI Server là nền tảng trợ lý AI chạy trên hạ tầng riêng, cung cấp khả năng hội thoại bằng văn bản và giọng nói qua một API thống nhất.

Hệ thống tiếp nhận câu hỏi từ trình duyệt, ứng dụng terminal, thiết bị di động hoặc thiết bị nhúng. Nội dung được xử lý trên máy chủ AI, kết hợp với kho kiến thức riêng của doanh nghiệp và trả về câu trả lời bằng văn bản hoặc âm thanh.

Hai luồng xử lý chính:

- **Text chat:** câu hỏi văn bản → tìm kiếm kiến thức → mô hình ngôn ngữ → câu trả lời văn bản.
- **Voice chat:** âm thanh → nhận dạng giọng nói → tìm kiếm kiến thức → mô hình ngôn ngữ → tổng hợp giọng nói.

Giải pháp phù hợp với các trường hợp cần:

- Trợ lý hỏi đáp dựa trên tài liệu nội bộ.
- Giao diện điều khiển bằng giọng nói.
- AI hoạt động trong mạng nội bộ.
- Kiểm soát dữ liệu, mô hình và lịch sử hội thoại.
- Tích hợp AI vào website, ứng dụng hoặc thiết bị IoT.

---

## 2. Giá trị mang lại cho khách hàng

### Dữ liệu được kiểm soát

Mô hình, dữ liệu tri thức và các dịch vụ xử lý có thể chạy trên máy chủ do khách hàng quản lý. Hệ thống không bắt buộc phải gửi nội dung hội thoại đến một dịch vụ AI công cộng.

### Câu trả lời dựa trên kiến thức riêng

Trước khi tạo câu trả lời, hệ thống tìm các đoạn tài liệu liên quan trong kho kiến thức. Cơ chế này giúp AI sử dụng thông tin của doanh nghiệp thay vì chỉ dựa vào kiến thức có sẵn trong mô hình.

### Một máy chủ phục vụ nhiều loại thiết bị

FastAPI cung cấp giao diện HTTP chung cho:

- Web chat.
- Terminal voice client.
- Điện thoại và máy tính bảng.
- ESP32 hoặc thiết bị nhúng khác.
- Ứng dụng doanh nghiệp cần tích hợp AI.

### Có thể mở rộng theo nhu cầu

Các thành phần nhận dạng giọng nói, mô hình ngôn ngữ, cơ sở dữ liệu vector và tổng hợp giọng nói được tách biệt. Doanh nghiệp có thể thay đổi từng thành phần mà không phải thiết kế lại toàn bộ hệ thống.

---

## 3. Kiến trúc tổng thể

```text
┌─────────────────────────────────────────────────────────────┐
│                         CLIENTS                             │
│                                                             │
│  Web Chat     Terminal Voice     Mobile App     ESP32/IoT   │
└──────────┬─────────────┬──────────────┬──────────────┬───────┘
           │             │              │              │
           └─────────────┴────── HTTP / HTTPS ─────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────┐
│               VOICE AI SERVER — FASTAPI :8000              │
│                                                             │
│  Authentication │ API routing │ Sessions │ Request logging  │
└──────────┬──────────────────┬───────────────────┬────────────┘
           │                  │                   │
           ▼                  ▼                   ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│ Speech Services │  │ AI and RAG      │  │ Optional Tools  │
│                 │  │                 │  │                 │
│ faster-whisper  │  │ Ollama          │  │ Home Assistant  │
│ Kokoro TTS      │  │ Qwen3 8B        │  │ Allowed devices │
└─────────────────┘  │ embeddinggemma  │  └─────────────────┘
                     │ Qdrant          │
                     └─────────────────┘
```

### Thành phần phía client

Client chỉ cần gửi văn bản, hình ảnh hoặc file âm thanh đến API. Client không cần lưu model AI.

Terminal Voice Client có thể thực hiện wake word ngay trên laptop. Âm thanh nền được xử lý cục bộ; chương trình chỉ gửi câu hỏi sau khi nhận diện wake word.

### Thành phần API

FastAPI là cổng truy cập duy nhất từ client. API chịu trách nhiệm:

- Xác thực API key.
- Kiểm tra dữ liệu đầu vào.
- Điều phối các dịch vụ AI.
- Quản lý lịch sử hội thoại theo session.
- Trả về JSON hoặc file WAV.
- Ghi log theo từng giai đoạn xử lý.

### Thành phần AI nội bộ

- **faster-whisper:** chuyển giọng nói thành văn bản.
- **embeddinggemma:** tạo vector ngữ nghĩa cho câu hỏi và tài liệu.
- **Qdrant:** lưu trữ và tìm kiếm các vector kiến thức.
- **Qwen3:8B-Q4_K_M:** hiểu câu hỏi và tạo câu trả lời.
- **Kokoro:** chuyển câu trả lời thành giọng nói.
- **Ollama:** quản lý và chạy mô hình ngôn ngữ cùng mô hình embedding.

---

## 4. Pipeline xử lý hội thoại bằng giọng nói

```text
Audio từ client
      │
      ▼
Kiểm tra định dạng và dung lượng
      │
      ▼
faster-whisper
Nhận dạng giọng nói thành văn bản
      │
      ▼
Nạp lịch sử theo session_id
      │
      ▼
embeddinggemma + Qdrant
Tìm các đoạn kiến thức liên quan
      │
      ▼
Xây dựng prompt gồm câu hỏi, lịch sử và kiến thức
      │
      ▼
Qwen3:8B-Q4_K_M
Tạo câu trả lời
      │
      ├──────────────► Trả về JSON text
      │
      ▼
Kokoro
Tổng hợp giọng nói
      │
      ▼
Trả về WAV hoặc JSON có audio Base64
```

### Bước 1: tiếp nhận âm thanh

Endpoint `/v1/voice/chat` tiếp nhận âm thanh qua `multipart/form-data`. Các định dạng được hỗ trợ gồm WAV, MP3, M4A, WebM và các định dạng mà backend âm thanh có thể đọc.

Thiết bị ESP32 nên gửi WAV mono, PCM 16-bit, tần số lấy mẫu 16 kHz hoặc 24 kHz.

### Bước 2: nhận dạng giọng nói

faster-whisper chuyển âm thanh thành transcript. Client có thể chỉ định ngôn ngữ như `en` hoặc `vi`, hoặc để hệ thống tự nhận diện.

### Bước 3: nạp ngữ cảnh hội thoại

Mỗi client sử dụng một `session_id`. Máy chủ dùng mã này để lấy các lượt hội thoại gần nhất và giúp AI hiểu các câu hỏi nối tiếp.

Mặc định hệ thống giữ tối đa 12 message cho mỗi session trong bộ nhớ.

### Bước 4: tìm kiếm tri thức

Câu hỏi mới nhất được chuyển thành vector bằng `embeddinggemma`. Qdrant so sánh vector này với kho tài liệu và trả về các đoạn có độ tương đồng cao nhất.

Hệ thống chỉ trả về tài liệu có metadata `status: active`.

### Bước 5: tạo câu trả lời

Máy chủ kết hợp:

- Câu hỏi hiện tại.
- Lịch sử hội thoại gần nhất.
- Các đoạn kiến thức liên quan.
- Chỉ dẫn hệ thống.

Prompt hoàn chỉnh được gửi đến `Qwen3:8B-Q4_K_M` qua Ollama để tạo câu trả lời.

### Bước 6: tạo âm thanh phản hồi

Nếu client yêu cầu phản hồi giọng nói, Kokoro chuyển câu trả lời thành WAV. Nếu client chỉ cần nội dung để hiển thị, API có thể trả về JSON gồm transcript và câu trả lời.

### Đặc điểm thời gian phản hồi

Phiên bản hiện tại chờ hoàn tất toàn bộ câu trả lời và file WAV trước khi gửi response. Hệ thống chưa hỗ trợ streaming audio theo từng phần.

---

## 5. Pipeline Text Chat

```text
Tin nhắn từ client
      │
      ▼
Kiểm tra danh sách message
      │
      ▼
Tìm kiếm kiến thức trong Qdrant
      │
      ▼
Thêm kiến thức liên quan vào prompt
      │
      ▼
Qwen3:8B-Q4_K_M qua Ollama
      │
      ▼
JSON text response
```

Text chat không chạy Whisper và Kokoro. Vì vậy, luồng này sử dụng ít tài nguyên hơn voice chat.

Endpoint `/v1/chat/completions` hỗ trợ phản hồi hoàn chỉnh hoặc streaming text qua Server-Sent Events.

---

## 6. Retrieval-Augmented Generation — RAG

### Mục tiêu

RAG giúp mô hình sử dụng thông tin mới hoặc thông tin riêng mà không cần huấn luyện lại Qwen.

### Quy trình lập chỉ mục

```text
knowledge/**/*.md
      │
      ▼
Đọc nội dung và YAML frontmatter
      │
      ▼
Chia tài liệu thành các đoạn nhỏ
      │
      ▼
embeddinggemma tạo vector
      │
      ▼
Lưu vector và metadata vào Qdrant
```

Tài liệu nguồn được lưu dưới dạng Markdown trong thư mục `knowledge/`.

YAML frontmatter có thể lưu metadata phục vụ quản lý tài liệu. Metadata cũng được đưa vào nội dung embedding để cải thiện khả năng truy xuất.

### Quy trình truy vấn

```text
Câu hỏi mới nhất
      │
      ▼
Tạo query embedding
      │
      ▼
Qdrant similarity search
      │
      ▼
Lọc theo status và score threshold
      │
      ▼
Chọn các đoạn phù hợp nhất
      │
      ▼
Đưa vào prompt của Qwen
```

Các tham số chính:

| Tham số                |  Giá trị mặc định | Ý nghĩa                                      |
| ----------------------- | ---------------------: | ---------------------------------------------- |
| `RAG_TOP_K`           |                  `5` | Số đoạn kiến thức tối đa được lấy   |
| `RAG_SCORE_THRESHOLD` |               `0.35` | Ngưỡng tương đồng tối thiểu            |
| `RAG_MAX_CHARS`       |               `9000` | Giới hạn tổng số ký tự đưa vào prompt |
| `QDRANT_COLLECTION`   | `local_ai_knowledge` | Tên collection chứa dữ liệu                |
| `OLLAMA_EMBED_MODEL`  |     `embeddinggemma` | Mô hình tạo embedding                       |

Khi thay đổi mô hình embedding, cần lập chỉ mục lại toàn bộ kho kiến thức để đảm bảo vector tài liệu và vector truy vấn có cùng không gian biểu diễn.

---

## 7. Mô hình Qwen3:8B-Q4_K_M

### Vai trò

`Qwen3:8B-Q4_K_M` là mô hình ngôn ngữ chính của hệ thống. Mô hình thực hiện:

- Hiểu câu hỏi và ngữ cảnh hội thoại.
- Tổng hợp thông tin từ các đoạn tài liệu được truy xuất.
- Tạo câu trả lời tự nhiên.
- Hỗ trợ tool calling khi được cấu hình.

### Ý nghĩa tên model

- **Qwen3:** dòng mô hình ngôn ngữ Qwen thế hệ 3.
- **8B:** kiến trúc có khoảng 8 tỷ tham số.
- **Q4_K_M:** định dạng lượng tử hóa 4-bit dùng để giảm dung lượng và nhu cầu bộ nhớ khi chạy inference.

### Cách triển khai

Ollama quản lý model và cung cấp API inference nội bộ. FastAPI không tải trực tiếp trọng số Qwen; nó gửi prompt đến Ollama tại địa chỉ mặc định `http://127.0.0.1:11434`.

Model được chọn bằng biến môi trường:

```dotenv
OLLAMA_MODEL=<tên model Ollama trỏ tới Qwen3:8B-Q4_K_M>
```

Tên model hiển thị trong Ollama có thể là một alias riêng, nhưng model nền sử dụng cho triển khai này là `Qwen3:8B-Q4_K_M`.

### Tham số inference

| Cấu hình             | Giá trị hiện tại/mặc định | Tác dụng                                            |
| ---------------------- | -------------------------------: | ----------------------------------------------------- |
| Temperature            |                          `0.6` | Điều chỉnh mức độ đa dạng của câu trả lời |
| `OLLAMA_NUM_CTX`     |                         `4096` | Kích thước context window                          |
| `OLLAMA_NUM_PREDICT` |                         `1024` | Số token sinh tối đa theo cấu hình               |
| `OLLAMA_THINK`       |                        `false` | Không yêu cầu xuất nội dung suy luận            |
| `OLLAMA_KEEP_ALIVE`  |                          `30m` | Yêu cầu Ollama giữ model trong bộ nhớ            |

Các giá trị này có thể điều chỉnh theo GPU, độ dài tài liệu và yêu cầu thời gian phản hồi.

---

## 8. Các mô hình và dịch vụ bổ trợ

### faster-whisper

Chức năng: Speech-to-Text.

Các cấu hình chính:

| Cấu hình               | Giá trị mặc định |
| ------------------------ | --------------------- |
| `WHISPER_MODEL`        | `small`             |
| `WHISPER_DEVICE`       | `auto`              |
| `WHISPER_COMPUTE_TYPE` | `int8`              |
| `WHISPER_BEAM_SIZE`    | `1`                 |

Máy chủ có thể dùng CUDA để tăng tốc Whisper. Cấu hình GPU phù hợp là CUDA 12, cuBLAS cho CUDA 12 và cuDNN 9.

### embeddinggemma

Chức năng: tạo embedding cho tài liệu và câu hỏi. Cùng một model phải được sử dụng ở cả thời điểm lập chỉ mục và thời điểm truy vấn.

### Qdrant

Chức năng: lưu vector, metadata và thực hiện tìm kiếm tương đồng.

Qdrant nên chỉ lắng nghe trên localhost hoặc mạng Docker riêng. Không nên công khai trực tiếp dịch vụ này ra Internet.

### Kokoro

Chức năng: Text-to-Speech.

Với tiếng Anh, cấu hình mặc định sử dụng Kokoro-82M và voice `af_heart`.

Với tiếng Việt, hệ thống có thể sử dụng checkpoint Kokoro-Vietnamese và một voice phù hợp như `diem_trinh`.

---

## 9. Các API chính

Base URL:

```text
http://SERVER_IP:8000
```

| Method   | Endpoint                     | Chức năng                                | Kết quả              |
| -------- | ---------------------------- | ------------------------------------------ | ---------------------- |
| `GET`  | `/healthz`                 | Kiểm tra API đang hoạt động           | JSON                   |
| `POST` | `/v1/audio/transcriptions` | Chuyển âm thanh thành văn bản         | JSON text              |
| `POST` | `/v1/chat/completions`     | Text chat với RAG và Qwen                | JSON hoặc text stream |
| `POST` | `/v1/images/chat`          | Phân tích hình ảnh bằng model vision  | JSON text              |
| `POST` | `/v1/audio/speech`         | Chuyển văn bản thành giọng nói       | WAV                    |
| `POST` | `/v1/voice/chat`           | Pipeline trợ lý giọng nói hoàn chỉnh | WAV hoặc JSON         |

### Ví dụ voice chat

```bash
curl -X POST http://SERVER_IP:8000/v1/voice/chat \
  -H 'Authorization: Bearer <API_KEY>' \
  -F 'audio=@question.wav' \
  -F 'session_id=meeting-room-01' \
  -F 'language=en' \
  -F 'response_format=json'
```

Phản hồi:

```json
{
  "transcript": "What services do you provide?",
  "language": "en",
  "text": "The answer generated by Qwen using retrieved knowledge.",
  "audio_format": "wav",
  "audio_base64": "UklGRi..."
}
```

### Ví dụ text chat

```bash
curl -X POST http://SERVER_IP:8000/v1/chat/completions \
  -H 'Authorization: Bearer <API_KEY>' \
  -H 'Content-Type: application/json' \
  -d '{
    "messages": [
      {"role": "user", "content": "What services do you provide?"}
    ]
  }'
```

---

## 10. Quản lý hội thoại

Voice chat sử dụng `session_id` để phân biệt hội thoại của từng người dùng hoặc thiết bị.

Ví dụ:

```text
terminal-sales-01
meeting-room-a
esp32-reception
mobile-user-123
```

Luồng xử lý session:

```text
Nhận câu hỏi và session_id
      │
      ▼
Lấy các message gần nhất của session
      │
      ▼
Thêm câu hỏi hiện tại
      │
      ▼
Gửi context đến Qwen
      │
      ▼
Lưu câu hỏi và câu trả lời mới
```

Phiên bản hiện tại lưu session trong bộ nhớ của tiến trình:

- Restart server sẽ xóa lịch sử session.
- Nhiều server instance sẽ không tự chia sẻ session.
- Triển khai production nhiều instance nên sử dụng Redis hoặc cơ sở dữ liệu.

---

## 11. Wake word và Terminal Voice Client

Terminal Voice Client chạy trên laptop của người dùng và kết nối đến AI server qua mạng LAN.

Luồng hoạt động:

```text
Microphone luôn lắng nghe cục bộ
      │
      ▼
openWakeWord nhận diện "Hey Jarvis"
      │
      ▼
Client ghi âm câu hỏi đến khi im lặng
      │
      ▼
Chuyển âm thanh thành WAV 16 kHz mono
      │
      ▼
Gửi /v1/voice/chat đến AI server
      │
      ▼
Hiển thị transcript và câu trả lời
```

Đặc điểm bảo mật quan trọng:

- Wake word được xử lý trên laptop client.
- Âm thanh nền không được gửi liên tục đến server.
- Client chỉ gửi đoạn ghi âm sau khi wake word được phát hiện.
- Cùng một `session_id` được giữ trong suốt thời gian chạy để tiếp tục hội thoại.

---

## 12. Tích hợp Home Assistant — tùy chọn

Hệ thống có thể cho phép Qwen điều khiển một danh sách nhỏ các switch trong Home Assistant.

Quy trình bảo vệ:

- Qwen không nhìn thấy Home Assistant token.
- Server chỉ cho phép các entity nằm trong `HA_ALLOWED_ENTITIES`.
- Model không được chạy shell command.
- Model không được gọi URL tùy ý.
- Server xác thực entity và trạng thái trước khi gọi Home Assistant API.

Ví dụ cấu hình:

```dotenv
HA_ENABLED=1
HA_URL=https://home.example.com
HA_TOKEN=<dedicated-token>
HA_ALLOWED_ENTITIES=switch.meeting_room_light
```

Đây là tính năng tùy chọn. Hệ thống vẫn hoạt động như một trợ lý hỏi đáp khi không bật Home Assistant.

---

## 13. Bảo mật

### Xác thực API

Khi cấu hình `API_KEY`, mọi endpoint ngoài `/healthz` yêu cầu header:

```http
Authorization: Bearer <API_KEY>
```

### Phân vùng mạng

Biên truy cập khuyến nghị:

```text
Internet hoặc LAN
      │
      ▼
HTTPS reverse proxy
      │
      ▼
FastAPI :8000
      │
      ├── Ollama :11434 — private
      └── Qdrant :6333 — private
```

Chỉ FastAPI hoặc reverse proxy nên được client truy cập. Ollama và Qdrant nên nằm trong localhost hoặc mạng private.

### HTTPS

Nếu client truy cập qua Internet hoặc mạng không tin cậy, cần đặt Nginx hoặc Caddy phía trước FastAPI để cung cấp HTTPS.

### CORS

Trong production, `CORS_ORIGINS` nên giới hạn ở các domain thực tế thay vì cho phép mọi nguồn.

### Log và dữ liệu nhạy cảm

Log mặc định ghi:

- Request ID.
- Tên model.
- Giai đoạn xử lý.
- Thời gian xử lý.
- File kiến thức được chọn.
- Kích thước dữ liệu và file WAV.

Log không in toàn bộ transcript, câu trả lời hoặc nội dung tài liệu tri thức.

---

## 14. Triển khai

### Mô hình triển khai trên một máy chủ

```text
Server PC
├── FastAPI Voice AI Server
├── Ollama
│   ├── Qwen3:8B-Q4_K_M
│   └── embeddinggemma
├── Qdrant
├── faster-whisper
├── Kokoro
└── Local knowledge and model cache
```

### Triển khai trực tiếp

FastAPI, Whisper và Kokoro chạy trong Python virtual environment. Ollama và Qdrant chạy như các dịch vụ riêng trên cùng máy.

### Triển khai bằng Docker

Docker Compose có thể quản lý:

- Voice AI Server.
- Qdrant.
- Model cache volume.

Ollama tiếp tục chạy trên host để quản lý GPU và model. Container truy cập Ollama qua `host.docker.internal`.

### GPU

- Ollama tự quản lý việc sử dụng GPU cho Qwen.
- faster-whisper có thể sử dụng CUDA.
- Kokoro có thể sử dụng CPU hoặc GPU tùy backend.
- Mức sử dụng VRAM thực tế phụ thuộc model, context window, compute type và các tiến trình đang chạy đồng thời.

Không nên cam kết số request mỗi giây hoặc độ trễ cụ thể nếu chưa benchmark trên phần cứng triển khai thực tế.

---

## 15. Khởi động và warmup

Khi `WARMUP_ON_START=1`, máy chủ tải và chạy thử các model trước khi bắt đầu nhận request.

Lợi ích:

- Phát hiện lỗi model hoặc dịch vụ ngay khi khởi động.
- Giảm độ trễ bất thường của request đầu tiên.
- Xác nhận Ollama, Whisper và Kokoro đã sẵn sàng.

Warmup không loại bỏ thời gian inference. Nó chỉ chuyển chi phí tải model khỏi request đầu tiên.

`OLLAMA_KEEP_ALIVE=30m` yêu cầu Ollama giữ model trong bộ nhớ trong 30 phút sau request. Ollama vẫn có thể dỡ model khi thiếu tài nguyên hoặc khi cần chạy model khác.

---

## 16. Giám sát và xử lý lỗi

Mỗi request có một ID riêng để phân biệt các request chạy đồng thời.

Voice request được ghi nhận theo các giai đoạn:

```text
Audio received
Whisper
Conversation history
Knowledge lookup
Qwen through Ollama
Kokoro
HTTP response
```

Mã lỗi chính:

| HTTP status | Ý nghĩa thường gặp                                 |
| ----------: | ------------------------------------------------------- |
|     `401` | API key thiếu hoặc không hợp lệ                    |
|     `413` | Audio rỗng hoặc vượt quá dung lượng cho phép    |
|     `422` | Dữ liệu đầu vào hoặc định dạng không hợp lệ |
|     `503` | Whisper, Qdrant, Ollama hoặc Kokoro không sẵn sàng  |

Log có thời gian cho từng stage, giúp xác định thành phần gây chậm hoặc lỗi.

---

## 17. Giới hạn hiện tại

Các giới hạn cần trình bày rõ với khách hàng:

- Voice response chưa hỗ trợ audio streaming.
- Session đang được lưu trong memory.
- Chất lượng câu trả lời phụ thuộc vào chất lượng tài liệu và kết quả retrieval.
- Qwen có thể tạo thông tin không chính xác nếu không có đủ context phù hợp.
- RAG giảm nhưng không loại bỏ hoàn toàn hiện tượng hallucination.
- Hiệu năng phụ thuộc vào GPU, VRAM, cấu hình model và độ dài prompt.
- Mô hình Q4 ưu tiên giảm yêu cầu bộ nhớ; chất lượng có thể khác model có độ chính xác trọng số cao hơn.
- Voice chat phải chờ STT, RAG, LLM và TTS hoàn tất nên chậm hơn text chat.
- API key không thay thế HTTPS khi giao tiếp qua mạng không tin cậy.
- Các tác vụ điều khiển thiết bị cần allowlist và cơ chế xác nhận phù hợp với mức độ rủi ro.

---

## 18. Khả năng mở rộng

Các hướng phát triển có thể đề xuất:

### Hạ tầng

- Redis hoặc database cho session dùng chung.
- Reverse proxy với HTTPS và rate limiting.
- Centralized logging và monitoring.
- Nhiều worker hoặc nhiều server instance.
- Hàng đợi riêng cho các tác vụ audio dài.

### AI và dữ liệu

- Quản trị phiên bản tài liệu trong knowledge base.
- Phân quyền truy cập tài liệu theo người dùng hoặc nhóm.
- Trích dẫn nguồn trực tiếp trong câu trả lời.
- Đánh giá chất lượng retrieval và câu trả lời.
- Thêm reranker cho kết quả tìm kiếm.

### Trải nghiệm người dùng

- Streaming text response.
- Streaming audio response.
- Voice activity detection nâng cao.
- Wake-word model tùy chỉnh theo thương hiệu.
- Giao diện quản trị tài liệu và session.

### Tích hợp

- CRM, ERP hoặc helpdesk.
- Home Assistant và hệ thống IoT.
- Website chăm sóc khách hàng.
- Kiosk hoặc thiết bị lễ tân.
- Ứng dụng mobile riêng.

---

## 19. Các trường hợp sử dụng đề xuất

### Trợ lý kiến thức nội bộ

Nhân viên hỏi về quy trình, sản phẩm, tài liệu kỹ thuật hoặc chính sách nội bộ bằng văn bản hoặc giọng nói.

### Trợ lý chăm sóc khách hàng

Hệ thống trả lời câu hỏi dựa trên tài liệu sản phẩm đã được kiểm duyệt.

### Trợ lý tại phòng họp hoặc quầy lễ tân

Thiết bị nhận wake word, ghi âm câu hỏi và gửi tới máy chủ AI trung tâm.

### Điều khiển thiết bị có giới hạn

AI có thể thực hiện một số thao tác được cho phép trước, chẳng hạn bật hoặc tắt đèn trong Home Assistant.

### Giao diện giọng nói cho thiết bị nhúng

ESP32 thu âm và gửi audio đến server. Thiết bị không cần chạy model AI cục bộ.

---

## 20. Thông điệp chính khi trình bày

1. Voice AI Server cung cấp một cổng API chung cho text, voice, image và thiết bị IoT.
2. `Qwen3:8B-Q4_K_M` tạo câu trả lời trên hạ tầng do khách hàng kiểm soát.
3. Qdrant và `embeddinggemma` giúp câu trả lời dựa trên tài liệu riêng.
4. faster-whisper và Kokoro hoàn thiện pipeline hội thoại bằng giọng nói.
5. Client không cần chứa model; thiết bị chỉ gửi dữ liệu và nhận kết quả.
6. Các dịch vụ model và vector được giữ trong mạng private.
7. Kiến trúc module cho phép thay thế hoặc mở rộng từng thành phần.
8. Hệ thống hiện phù hợp với triển khai nội bộ, pilot và các ứng dụng cần kiểm soát dữ liệu.

---

## 21. Gợi ý cấu trúc slide

Phần này chỉ là gợi ý tổ chức nội dung. Người thiết kế có thể thay đổi theo thời lượng và đối tượng khách hàng.

1. **Voice AI Server** — tên giải pháp và thông điệp tổng quan.
2. **Nhu cầu của khách hàng** — dữ liệu riêng, hội thoại tự nhiên và khả năng tích hợp.
3. **Giải pháp đề xuất** — text chat, voice chat và RAG trên hạ tầng riêng.
4. **System topology** — client, FastAPI và các dịch vụ private.
5. **Voice pipeline** — từ audio đến câu trả lời bằng giọng nói.
6. **Knowledge pipeline** — Markdown, embeddinggemma và Qdrant.
7. **Qwen3:8B-Q4_K_M** — vai trò và cách Ollama vận hành model.
8. **API và tích hợp** — các endpoint và loại client.
9. **Security architecture** — API key, HTTPS và network isolation.
10. **Deployment** — máy chủ đơn hoặc Docker.
11. **Current limitations** — các giới hạn kỹ thuật cần minh bạch.
12. **Roadmap and use cases** — hướng mở rộng và ứng dụng phù hợp.

---

## 22. Câu hỏi thường gặp từ khách hàng

### Dữ liệu có được gửi ra ngoài không?

Kiến trúc hỗ trợ chạy Qwen, Whisper, Qdrant và Kokoro trên máy chủ riêng. Việc có truy cập Internet hay không phụ thuộc cấu hình triển khai, quy trình tải model và các tích hợp bên ngoài được bật.

### Có thể chạy hoàn toàn offline không?

Có thể chạy offline sau khi package và model cần thiết đã được tải và lưu trong cache. Cần tắt hoặc tránh các tích hợp phụ thuộc dịch vụ Internet.

### Có thể cập nhật kiến thức mà không huấn luyện lại model không?

Có. Người quản trị cập nhật các file Markdown và chạy lại indexer. Qwen không cần fine-tune lại.

### Hệ thống hỗ trợ tiếng Việt không?

Whisper có thể nhận dạng tiếng Việt. Hệ thống cũng có backend Kokoro-Vietnamese. Chất lượng cần được đánh giá bằng nội dung và giọng nói thực tế của khách hàng.

### Client có cần GPU không?

Không. Client chỉ thu âm, gửi request và hiển thị hoặc phát kết quả. GPU nằm trên máy chủ AI nếu cấu hình tăng tốc được sử dụng.

### Có thể dùng nhiều client cùng lúc không?

API hỗ trợ các request có request ID riêng. Năng lực đồng thời thực tế cần được benchmark trên phần cứng server và cấu hình model cụ thể.

### Có thể thay Qwen bằng model khác không?

Có. Model mặc định được chọn qua biến `OLLAMA_MODEL`. Model thay thế cần tương thích với yêu cầu text generation và tool calling nếu tính năng tool được sử dụng.

### Hệ thống có lưu toàn bộ hội thoại không?

Phiên bản hiện tại chỉ giữ số lượng message gần nhất trong memory cho mỗi session. Việc lưu trữ lâu dài cần database và chính sách dữ liệu riêng.

### Câu trả lời có luôn chính xác không?

Không có mô hình ngôn ngữ nào đảm bảo chính xác tuyệt đối. RAG giúp tăng tính liên quan, nhưng cần tài liệu tốt, kiểm thử và cơ chế xác minh cho các nội dung quan trọng.

---

## 23. Phạm vi demo đề xuất

Một buổi demo khách hàng có thể sử dụng quy trình sau:

1. Kiểm tra `/healthz` để xác nhận server đang hoạt động.
2. Mở web chat và hỏi một câu có thông tin trong knowledge base.
3. Hiển thị tài liệu Markdown được dùng làm nguồn tri thức.
4. Dùng Terminal Voice Client và nói wake word.
5. Đặt một câu hỏi bằng tiếng Anh.
6. Hiển thị transcript và câu trả lời của Qwen.
7. Đặt câu hỏi nối tiếp để minh họa session history.
8. Mở log để cho thấy thời gian của Whisper, RAG, Qwen và Kokoro.
9. Giải thích rằng Qdrant và Ollama chỉ chạy trong mạng private.
10. Nếu phù hợp, demo một Home Assistant entity nằm trong allowlist.

Không nên sử dụng câu hỏi demo chưa được kiểm tra trước. Nên chuẩn bị tài liệu, câu hỏi và kết quả mong đợi để bảo đảm buổi trình bày tập trung vào giá trị của giải pháp.

---

## 24. Kết luận

Voice AI Server kết hợp nhận dạng giọng nói, RAG, mô hình ngôn ngữ và tổng hợp giọng nói trong một nền tảng có thể triển khai trên hạ tầng riêng.

Giải pháp sử dụng `Qwen3:8B-Q4_K_M` để tạo câu trả lời, `embeddinggemma` và Qdrant để khai thác kiến thức doanh nghiệp, faster-whisper để nhận dạng lời nói và Kokoro để tạo phản hồi âm thanh.

Kiến trúc API thống nhất giúp nhiều loại client sử dụng cùng một năng lực AI, trong khi mô hình module tạo điều kiện thay đổi model, bổ sung dữ liệu và mở rộng tích hợp theo nhu cầu thực tế.

Để chuyển từ pilot sang production, các bước quan trọng gồm benchmark trên phần cứng mục tiêu, triển khai HTTPS, bổ sung persistent sessions, xác định chính sách dữ liệu và xây dựng bộ đánh giá chất lượng câu trả lời.

---

## Nguồn thông tin kỹ thuật

Nội dung tài liệu được tổng hợp từ:

- `voice_ai_server/README.md`
- `voice_ai_server/app/main.py`
- `voice_ai_server/app/services.py`
- `voice_ai_server/app/config.py`
- Cấu hình model do dự án cung cấp: `Qwen3:8B-Q4_K_M`

---

# Voice AI Server — English Version

## Customer presentation content

**Version:** 1.0
**Date:** October 15, 2026
**Language model:** `Qwen3:8B-Q4_K_M`

---

## 1. Solution overview

Voice AI Server is a privately deployable AI assistant platform that provides text and voice conversations through one unified API.

The platform accepts requests from web browsers, terminal applications, mobile devices, and embedded hardware. The AI server processes each request, retrieves relevant information from the organization's private knowledge base, and returns a text or audio response.

The platform provides two main processing paths:

- **Text chat:** text message → knowledge retrieval → language model → text response.
- **Voice chat:** audio → speech recognition → knowledge retrieval → language model → speech synthesis.

Typical use cases include:

- Question answering based on internal documents.
- Voice-controlled user interfaces.
- AI services operating inside a private network.
- Controlled management of data, models, and conversation history.
- Integration with websites, business applications, and IoT devices.

---

## 2. Customer value

### Controlled data environment

The models, knowledge base, and processing services can run on infrastructure controlled by the customer. The platform does not require conversation content to be sent to a public AI provider.

### Answers based on private knowledge

Before generating an answer, the server retrieves relevant passages from the customer's knowledge base. This allows the AI to use organization-specific information rather than relying only on the language model's built-in knowledge.

### One server for multiple client types

The FastAPI service provides a common HTTP interface for:

- Web chat.
- Terminal voice clients.
- Smartphones and tablets.
- ESP32 and other embedded devices.
- Business applications that need AI functions.

### Modular architecture

Speech recognition, language generation, vector storage, and speech synthesis are separate components. Each component can be changed or upgraded without redesigning the complete platform.

---

## 3. System architecture

```text
┌─────────────────────────────────────────────────────────────┐
│                         CLIENTS                             │
│                                                             │
│  Web Chat     Terminal Voice     Mobile App     ESP32/IoT   │
└──────────┬─────────────┬──────────────┬──────────────┬───────┘
           │             │              │              │
           └─────────────┴────── HTTP / HTTPS ─────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────┐
│               VOICE AI SERVER — FASTAPI :8000              │
│                                                             │
│  Authentication │ API routing │ Sessions │ Request logging  │
└──────────┬──────────────────┬───────────────────┬────────────┘
           │                  │                   │
           ▼                  ▼                   ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│ Speech Services │  │ AI and RAG      │  │ Optional Tools  │
│                 │  │                 │  │                 │
│ faster-whisper  │  │ Ollama          │  │ Home Assistant  │
│ Kokoro TTS      │  │ Qwen3 8B        │  │ Allowed devices │
└─────────────────┘  │ embeddinggemma  │  └─────────────────┘
                     │ Qdrant          │
                     └─────────────────┘
```

### Client layer

Clients send text, images, or audio files to the API. They do not store or execute the main AI models.

The Terminal Voice Client can detect a wake word locally on the user's laptop. Background audio remains on the client; the application only sends a recorded question after detecting the wake word.

### API layer

FastAPI is the only service that clients need to access. It handles:

- API-key authentication.
- Input validation.
- AI service orchestration.
- Session-based conversation history.
- JSON and WAV responses.
- Processing logs for each request stage.

### Private AI services

- **faster-whisper:** converts speech into text.
- **embeddinggemma:** creates semantic vectors for questions and documents.
- **Qdrant:** stores and searches knowledge vectors.
- **Qwen3:8B-Q4_K_M:** understands questions and generates responses.
- **Kokoro:** converts generated text into speech.
- **Ollama:** hosts the language and embedding models.

---

## 4. Voice conversation pipeline

```text
Audio from client
      │
      ▼
Validate format and file size
      │
      ▼
faster-whisper
Convert speech to text
      │
      ▼
Load history for session_id
      │
      ▼
embeddinggemma + Qdrant
Retrieve relevant knowledge
      │
      ▼
Build a prompt with the question, history, and knowledge
      │
      ▼
Qwen3:8B-Q4_K_M
Generate the answer
      │
      ├──────────────► Return JSON text
      │
      ▼
Kokoro
Synthesize speech
      │
      ▼
Return WAV or JSON with Base64 audio
```

### Step 1: receive audio

The `/v1/voice/chat` endpoint accepts audio through `multipart/form-data`. Supported formats include WAV, MP3, M4A, WebM, and other formats supported by the audio backend.

ESP32 devices should send mono PCM WAV audio at 16-bit and 16 kHz or 24 kHz.

### Step 2: recognize speech

faster-whisper converts the recording into a transcript. A client may specify a language such as `en` or `vi`, or allow automatic language detection.

### Step 3: load conversation context

Each client uses a `session_id`. The server uses this identifier to retrieve recent conversation turns so the AI can understand follow-up questions.

By default, the system keeps up to 12 messages for each session in memory.

### Step 4: retrieve knowledge

`embeddinggemma` converts the latest question into a vector. Qdrant compares it with indexed document vectors and returns the most relevant passages.

Only documents with `status: active` metadata are eligible for retrieval.

### Step 5: generate an answer

The server combines:

- The current question.
- Recent conversation history.
- Relevant knowledge passages.
- System instructions.

The completed prompt is sent to `Qwen3:8B-Q4_K_M` through Ollama.

### Step 6: generate the audio response

When a client requests a spoken response, Kokoro converts the answer into WAV audio. A display-oriented client can request JSON containing the transcript and generated answer.

### Response timing

The current voice endpoint waits for the complete answer and WAV file before returning the HTTP response. Incremental audio streaming is not currently implemented.

---

## 5. Text chat pipeline

```text
Client messages
      │
      ▼
Validate the message list
      │
      ▼
Retrieve knowledge from Qdrant
      │
      ▼
Add relevant knowledge to the prompt
      │
      ▼
Qwen3:8B-Q4_K_M through Ollama
      │
      ▼
JSON text response
```

Text chat does not invoke Whisper or Kokoro, so it requires fewer processing stages than voice chat.

The `/v1/chat/completions` endpoint supports either a complete JSON response or text streaming through Server-Sent Events.

---

## 6. Retrieval-Augmented Generation — RAG

### Purpose

RAG gives the language model access to current and private information without requiring Qwen to be retrained.

### Indexing process

```text
knowledge/**/*.md
      │
      ▼
Read content and YAML frontmatter
      │
      ▼
Split documents into smaller chunks
      │
      ▼
Create vectors with embeddinggemma
      │
      ▼
Store vectors and metadata in Qdrant
```

Source documents are stored as Markdown files under `knowledge/`. YAML frontmatter provides document metadata and is also included in the embedding context for each chunk.

### Query process

```text
Latest user question
      │
      ▼
Create query embedding
      │
      ▼
Qdrant similarity search
      │
      ▼
Filter by status and score threshold
      │
      ▼
Select the most relevant passages
      │
      ▼
Add the passages to the Qwen prompt
```

Main settings:

| Setting                 |                Default | Purpose                                    |
| ----------------------- | ---------------------: | ------------------------------------------ |
| `RAG_TOP_K`           |                  `5` | Maximum number of retrieved passages       |
| `RAG_SCORE_THRESHOLD` |               `0.35` | Minimum similarity score                   |
| `RAG_MAX_CHARS`       |               `9000` | Maximum retrieved text added to the prompt |
| `QDRANT_COLLECTION`   | `local_ai_knowledge` | Vector collection name                     |
| `OLLAMA_EMBED_MODEL`  |     `embeddinggemma` | Embedding model                            |

Changing the embedding model requires rebuilding the knowledge index so document and query vectors use the same representation space.

---

## 7. Qwen3:8B-Q4_K_M

### Role in the system

`Qwen3:8B-Q4_K_M` is the primary language model. It performs the following functions:

- Understands questions and conversation context.
- Combines information from retrieved documents.
- Generates natural-language answers.
- Supports tool calling when configured.

### Model name

- **Qwen3:** the third-generation Qwen model family.
- **8B:** an architecture with approximately eight billion parameters.
- **Q4_K_M:** a 4-bit quantization format that reduces storage and memory requirements during inference.

### Deployment

Ollama manages the model and provides the internal inference API. FastAPI does not load Qwen weights directly; it sends prompts to Ollama at the default address `http://127.0.0.1:11434`.

The model is selected through:

```dotenv
OLLAMA_MODEL=<Ollama model name pointing to Qwen3:8B-Q4_K_M>
```

Ollama may display a custom alias, but the underlying deployment model for this project is `Qwen3:8B-Q4_K_M`.

### Inference settings

| Setting                | Current/default value | Purpose                                    |
| ---------------------- | --------------------: | ------------------------------------------ |
| Temperature            |               `0.6` | Controls response variation                |
| `OLLAMA_NUM_CTX`     |              `4096` | Context-window size                        |
| `OLLAMA_NUM_PREDICT` |              `1024` | Configured maximum generated tokens        |
| `OLLAMA_THINK`       |             `false` | Does not request thinking output           |
| `OLLAMA_KEEP_ALIVE`  |               `30m` | Requests that Ollama keep the model loaded |

These values can be adjusted according to GPU capacity, document length, and response-time requirements.

---

## 8. Supporting models and services

### faster-whisper

Purpose: Speech-to-Text.

| Setting                  | Default   |
| ------------------------ | --------- |
| `WHISPER_MODEL`        | `small` |
| `WHISPER_DEVICE`       | `auto`  |
| `WHISPER_COMPUTE_TYPE` | `int8`  |
| `WHISPER_BEAM_SIZE`    | `1`     |

The server can use CUDA to accelerate Whisper. The documented GPU configuration uses CUDA 12, CUDA 12 cuBLAS, and cuDNN 9.

### embeddinggemma

Purpose: creates embeddings for documents and user questions. The same model must be used during indexing and retrieval.

### Qdrant

Purpose: stores vectors and metadata and performs similarity search.

Qdrant should only listen on localhost or a private Docker network. It should not be directly exposed to the Internet.

### Kokoro

Purpose: Text-to-Speech.

The default English configuration uses Kokoro-82M with the `af_heart` voice. Vietnamese speech can use the Kokoro-Vietnamese checkpoint with a suitable voice such as `diem_trinh`.

---

## 9. Main API endpoints

Base URL:

```text
http://SERVER_IP:8000
```

| Method   | Endpoint                     | Purpose                            | Response            |
| -------- | ---------------------------- | ---------------------------------- | ------------------- |
| `GET`  | `/healthz`                 | Check API availability             | JSON                |
| `POST` | `/v1/audio/transcriptions` | Convert audio to text              | JSON text           |
| `POST` | `/v1/chat/completions`     | RAG and Qwen text chat             | JSON or text stream |
| `POST` | `/v1/images/chat`          | Analyze images with a vision model | JSON text           |
| `POST` | `/v1/audio/speech`         | Convert text into speech           | WAV                 |
| `POST` | `/v1/voice/chat`           | Complete voice assistant pipeline  | WAV or JSON         |

### Voice-chat example

```bash
curl -X POST http://SERVER_IP:8000/v1/voice/chat \
  -H 'Authorization: Bearer <API_KEY>' \
  -F 'audio=@question.wav' \
  -F 'session_id=meeting-room-01' \
  -F 'language=en' \
  -F 'response_format=json'
```

Example response:

```json
{
  "transcript": "What services do you provide?",
  "language": "en",
  "text": "The answer generated by Qwen using retrieved knowledge.",
  "audio_format": "wav",
  "audio_base64": "UklGRi..."
}
```

### Text-chat example

```bash
curl -X POST http://SERVER_IP:8000/v1/chat/completions \
  -H 'Authorization: Bearer <API_KEY>' \
  -H 'Content-Type: application/json' \
  -d '{
    "messages": [
      {"role": "user", "content": "What services do you provide?"}
    ]
  }'
```

---

## 10. Conversation management

Voice chat uses `session_id` to keep conversations from different users and devices separate.

Example identifiers:

```text
terminal-sales-01
meeting-room-a
esp32-reception
mobile-user-123
```

```text
Receive question and session_id
      │
      ▼
Load recent messages for the session
      │
      ▼
Add the current question
      │
      ▼
Send the context to Qwen
      │
      ▼
Save the new question and answer
```

The current version stores sessions in process memory:

- Restarting the server clears session history.
- Multiple server instances do not automatically share sessions.
- A multi-instance production deployment should use Redis or a database.

---

## 11. Wake word and Terminal Voice Client

The Terminal Voice Client runs on the user's laptop and connects to the AI server through the local network.

```text
The microphone listens locally
      │
      ▼
openWakeWord detects "Hey Jarvis"
      │
      ▼
The client records until it detects silence
      │
      ▼
Convert the recording to 16 kHz mono WAV
      │
      ▼
Send /v1/voice/chat to the AI server
      │
      ▼
Display the transcript and AI answer
```

Important privacy characteristics:

- The laptop performs wake-word detection locally.
- Background audio is not continuously uploaded.
- The client only sends the recording after wake-word detection.
- The client keeps one `session_id` while it is running so the conversation can continue.

---

## 12. Optional Home Assistant integration

The server can allow Qwen to control a limited list of Home Assistant switches.

Safety boundaries include:

- Qwen cannot access the Home Assistant token.
- The server only permits entities listed in `HA_ALLOWED_ENTITIES`.
- The model cannot execute shell commands.
- The model cannot request arbitrary URLs.
- The server validates the entity and desired state before calling Home Assistant.

Example configuration:

```dotenv
HA_ENABLED=1
HA_URL=https://home.example.com
HA_TOKEN=<dedicated-token>
HA_ALLOWED_ENTITIES=switch.meeting_room_light
```

This integration is optional. The server continues to operate as a question-answering assistant when Home Assistant is disabled.

---

## 13. Security

### API authentication

When `API_KEY` is configured, every endpoint except `/healthz` requires:

```http
Authorization: Bearer <API_KEY>
```

### Network boundaries

```text
Internet or LAN
      │
      ▼
HTTPS reverse proxy
      │
      ▼
FastAPI :8000
      │
      ├── Ollama :11434 — private
      └── Qdrant :6333 — private
```

Only FastAPI or its reverse proxy should be accessible to clients. Ollama and Qdrant should remain on localhost or a private network.

### HTTPS

Deploy Nginx or Caddy in front of FastAPI when clients connect over the Internet or an untrusted network.

### CORS

Production deployments should restrict `CORS_ORIGINS` to approved domains instead of allowing every origin.

### Logs and sensitive data

Default logs include:

- Request ID.
- Model name.
- Processing stage.
- Stage duration.
- Selected knowledge files.
- Data and WAV sizes.

The logs do not print complete transcripts, generated answers, or knowledge documents.

---

## 14. Deployment

### Single-server deployment

```text
Server PC
├── FastAPI Voice AI Server
├── Ollama
│   ├── Qwen3:8B-Q4_K_M
│   └── embeddinggemma
├── Qdrant
├── faster-whisper
├── Kokoro
└── Local knowledge and model cache
```

### Direct deployment

FastAPI, Whisper, and Kokoro run inside a Python virtual environment. Ollama and Qdrant run as separate services on the same machine.

### Docker deployment

Docker Compose can manage:

- Voice AI Server.
- Qdrant.
- The shared model-cache volume.

Ollama remains on the host to manage the GPU and model files. The application container connects to it through `host.docker.internal`.

### GPU responsibilities

- Ollama manages GPU execution for Qwen.
- faster-whisper can use CUDA.
- Kokoro can use CPU or GPU depending on the selected backend.
- Actual VRAM usage depends on the models, context window, compute type, and concurrent workloads.

Performance claims such as requests per second or response latency should only be made after benchmarking on the target hardware.

---

## 15. Startup and model warmup

With `WARMUP_ON_START=1`, the server loads and tests its models before accepting requests.

Benefits include:

- Detecting model or service failures during startup.
- Reducing unusual latency for the first request.
- Confirming that Ollama, Whisper, and Kokoro are ready.

Warmup does not eliminate inference time. It moves model-loading work out of the first user request.

`OLLAMA_KEEP_ALIVE=30m` asks Ollama to retain the model in memory for 30 minutes after a request. Ollama may still unload it because of memory pressure or model changes.

---

## 16. Monitoring and error handling

Each request receives a unique ID so concurrent requests can be traced independently.

A voice request records the following stages:

```text
Audio received
Whisper
Conversation history
Knowledge lookup
Qwen through Ollama
Kokoro
HTTP response
```

Main status codes:

| HTTP status | Typical meaning                                   |
| ----------: | ------------------------------------------------- |
|     `401` | Missing or invalid API key                        |
|     `413` | Empty audio or file exceeds the configured limit  |
|     `422` | Invalid input or unsupported format               |
|     `503` | Whisper, Qdrant, Ollama, or Kokoro is unavailable |

Per-stage timing in the logs helps identify slow or failing components.

---

## 17. Current limitations

The following limitations should be communicated clearly:

- Voice responses do not currently support audio streaming.
- Sessions are stored in memory.
- Answer quality depends on source-document quality and retrieval results.
- Qwen may generate incorrect information when relevant context is unavailable.
- RAG reduces but does not eliminate hallucinations.
- Performance depends on the GPU, VRAM, model configuration, and prompt length.
- Q4 quantization prioritizes lower memory requirements and may differ in quality from higher-precision weights.
- Voice chat is slower than text chat because it runs STT, RAG, LLM, and TTS stages.
- An API key does not replace HTTPS on an untrusted network.
- Device-control operations require an allowlist and safeguards appropriate to their risk.

---

## 18. Expansion options

### Infrastructure

- Redis or a database for shared sessions.
- HTTPS reverse proxy and rate limiting.
- Centralized logging and monitoring.
- Multiple application workers or server instances.
- A separate queue for long audio-processing tasks.

### AI and data

- Knowledge-document version management.
- User or group access controls for documents.
- Direct source citations in responses.
- Retrieval and answer-quality evaluation.
- A reranker for retrieved passages.

### User experience

- Streaming text responses.
- Streaming audio responses.
- Advanced voice activity detection.
- A custom branded wake-word model.
- Administration interfaces for documents and sessions.

### Integrations

- CRM, ERP, and helpdesk systems.
- Home Assistant and IoT platforms.
- Customer-support websites.
- Kiosks and reception devices.
- Dedicated mobile applications.

---

## 19. Suggested use cases

### Internal knowledge assistant

Employees ask about procedures, products, technical documentation, or internal policies using text or speech.

### Customer-support assistant

The system answers customer questions using approved product and support documents.

### Meeting-room or reception assistant

A device detects a wake word, records a question, and sends it to the central AI server.

### Restricted device control

The AI performs selected operations that have been explicitly approved, such as switching a Home Assistant light on or off.

### Voice interface for embedded devices

An ESP32 records and uploads audio. The embedded device does not need to run an AI model locally.

---

## 20. Key presentation messages

1. Voice AI Server provides one API for text, voice, image, and IoT clients.
2. `Qwen3:8B-Q4_K_M` generates responses on customer-controlled infrastructure.
3. Qdrant and `embeddinggemma` ground answers in private documents.
4. faster-whisper and Kokoro provide the speech pipeline.
5. Clients do not store AI models; they send requests and receive results.
6. Language-model and vector-database services remain on the private network.
7. The modular architecture supports model replacement and new integrations.
8. The current platform is suitable for internal deployments, pilots, and controlled-data applications.

---

## 21. Suggested slide structure

1. **Voice AI Server** — solution name and overview.
2. **Customer requirements** — private data, natural conversation, and integration.
3. **Proposed solution** — text chat, voice chat, and private RAG.
4. **System topology** — clients, FastAPI, and private services.
5. **Voice pipeline** — audio input through spoken response.
6. **Knowledge pipeline** — Markdown, embeddinggemma, and Qdrant.
7. **Qwen3:8B-Q4_K_M** — model role and Ollama deployment.
8. **APIs and integrations** — endpoints and client types.
9. **Security architecture** — API key, HTTPS, and network isolation.
10. **Deployment** — direct installation or Docker.
11. **Current limitations** — technical boundaries and expectations.
12. **Roadmap and use cases** — expansion paths and suitable applications.

---

## 22. Customer FAQ

### Is data sent outside the customer environment?

The architecture supports running Qwen, Whisper, Qdrant, and Kokoro on private infrastructure. Internet access depends on deployment configuration, model-download procedures, and enabled external integrations.

### Can the system operate completely offline?

Yes. After all required packages and models have been downloaded and cached, the platform can operate offline if integrations that require Internet access are disabled.

### Can the knowledge base be updated without retraining Qwen?

Yes. An administrator updates the Markdown files and rebuilds the vector index. Qwen does not need to be fine-tuned again.

### Does the platform support Vietnamese?

Whisper can recognize Vietnamese, and the platform provides a Kokoro-Vietnamese backend. Quality should be tested with the customer's actual content, speakers, and audio equipment.

### Does the client device need a GPU?

No. The client records input, sends the request, and displays or plays the response. GPU processing takes place on the AI server when acceleration is enabled.

### Can multiple clients use the server simultaneously?

The API assigns separate request IDs to concurrent requests. Actual concurrency must be benchmarked with the target hardware and model configuration.

### Can Qwen be replaced with another model?

Yes. `OLLAMA_MODEL` selects the default language model. A replacement must support the required text-generation features and tool calling if tools are enabled.

### Does the platform permanently store conversations?

The current version only keeps a limited number of recent messages in memory for each session. Long-term retention requires a database and a defined data policy.

### Are the answers always correct?

No language model can guarantee perfect accuracy. RAG improves relevance, but important use cases still require good source documents, testing, and appropriate verification.

---

## 23. Suggested customer demo

1. Call `/healthz` to confirm that the server is available.
2. Open the web chat and ask a question whose answer exists in the knowledge base.
3. Show the Markdown document used as the knowledge source.
4. Start the Terminal Voice Client and say the wake word.
5. Ask a question in English.
6. Show the transcript and Qwen response.
7. Ask a follow-up question to demonstrate session history.
8. Open the logs and show the timing for Whisper, RAG, Qwen, and Kokoro.
9. Explain that Qdrant and Ollama remain on the private network.
10. If appropriate, demonstrate one Home Assistant entity from the allowlist.

Demo questions should be tested in advance. Prepare the source documents, questions, and expected answers so the session remains focused on customer value.

---

## 24. Conclusion

Voice AI Server combines speech recognition, RAG, language generation, and speech synthesis in a platform that can run on private infrastructure.

The platform uses `Qwen3:8B-Q4_K_M` to generate answers, `embeddinggemma` and Qdrant to retrieve private knowledge, faster-whisper to recognize speech, and Kokoro to produce spoken responses.

One unified API allows different clients to use the same AI services. The modular design also allows the customer to change models, update knowledge, and add integrations as requirements evolve.

Moving from a pilot to production should include benchmarking on the target hardware, HTTPS deployment, persistent session storage, a defined data policy, and a formal answer-quality evaluation process.

---

## Technical sources

This document is based on:

- `voice_ai_server/README.md`
- `voice_ai_server/app/main.py`
- `voice_ai_server/app/services.py`
- `voice_ai_server/app/config.py`
- Project model configuration: `Qwen3:8B-Q4_K_M`
