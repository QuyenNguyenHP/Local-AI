# Voice Client Web UI

Web UI cho `../voice_ai_server`: ghi âm từ micro hoặc tải audio lên, sau đó nhận và tự phát WAV trả về. Client dùng audio raw như ESP32: không yêu cầu transcript/text hay audio Base64, nên giảm dữ liệu truyền và RAM phía trình duyệt.

## Chạy

```bash
cd "/home/dq/Local AI/voice_client"
python3 -m http.server 5174
```

Truy cập `http://127.0.0.1:5174`. Không mở `index.html` trực tiếp từ file, vì trình duyệt thường không cho dùng micro trong ngữ cảnh đó.

Với server chạy ở máy khác, nhập URL (ví dụ `http://192.168.1.10:8000`) trong nút bánh răng. Khi truy cập từ điện thoại hay hostname không phải `localhost`, micro yêu cầu HTTPS; hãy đặt UI/server sau HTTPS bằng Nginx hoặc Caddy.

Nếu đã đặt `API_KEY` cho server, nhập cùng key trong cài đặt. Nó chỉ được lưu trong localStorage của trình duyệt này.
