# Terminal Voice Client

Client chạy trong terminal theo luồng:

`micro → wake word cục bộ → ghi âm đến khi im lặng → /v1/voice/chat → hiện text`

Âm thanh nền chỉ được xử lý tại máy. Client chỉ gửi câu hỏi sau khi wake word được nhận diện. Một `session_id` được giữ nguyên trong suốt tiến trình nên server tiếp tục hội thoại từ các lượt trước.

## Cài đặt

Ubuntu/Debian cần PortAudio:

```bash
sudo apt install libportaudio2 portaudio19-dev
cd /home/daikai/Local-AI/terminal_voice_client
python3 -m venv .venv
bash install.sh
```

Script cài đặt dùng ONNX Runtime và chủ động bỏ `tflite-runtime` không cần
thiết, do TFLite hiện không có wheel cho Python 3.12 trên Linux.

Nếu package chưa có model tích hợp, tải các model chuẩn một lần:

```bash
.venv/bin/python -c "from openwakeword.utils import download_models; download_models()"
```

## Chạy

Khởi động `voice_ai_server`, sau đó:

```bash
cd /home/daikai/Local-AI/terminal_voice_client
.venv/bin/python voice_client.py --server http://127.0.0.1:8000
```

Mặc định wake word là `hey_jarvis` và ngôn ngữ câu hỏi là tiếng Anh. Có thể cấu hình:

```bash
.venv/bin/python voice_client.py \
  --server http://192.168.1.10:8000 \
  --api-key YOUR_KEY \
  --wake-word hey_jarvis \
  --language en \
  --session-id phong-khac`h
```

Xem danh sách micro rồi chọn bằng tên hoặc index:

```bash
.venv/bin/python voice_client.py --list-devices
.venv/bin/python voice_client.py --device 2
```

Các biến môi trường tương ứng: `VOICE_SERVER_URL`, `VOICE_API_KEY`, `VOICE_LANGUAGE`, `VOICE_SESSION_ID`, `WAKE_WORD`, `WAKE_THRESHOLD`, `MIC_DEVICE`.

Các wake-word model có sẵn tùy phiên bản openWakeWord, thường gồm `hey_jarvis`, `alexa` và `hey_mycroft`. Một cụm tiếng Việt tùy ý cần model openWakeWord đã được huấn luyện; truyền đường dẫn model đó qua `--wake-word`.
