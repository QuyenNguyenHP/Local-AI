---
id: home.home-assistant-devices
type: home-automation
status: active
updated: 2026-09-20
confidence: confirmed
tags: [home-assistant, lights, living-room]
---

# Thiết bị Home Assistant

## Đèn phòng khách

Tên người dùng có thể nói:

- đèn phòng khách
- điện phòng khách
- tắt điện phòng khách
- bật điện phòng khách
- đèn T1

Home Assistant entity:
`switch.t1_chieu_sang_switch_3`

Quy tắc:

- Khi người dùng yêu cầu bật đèn hoặc điện phòng khách, gọi tool
  `home_assistant_switch` với `entity_id` là
  `switch.t1_chieu_sang_switch_3` và `state` là `on`.
- Khi người dùng yêu cầu tắt đèn hoặc điện phòng khách, gọi tool
  `home_assistant_switch` với `entity_id` là
  `switch.t1_chieu_sang_switch_3` và `state` là `off`.
- Nếu người dùng không nói rõ phòng hoặc thiết bị, hỏi lại; không tự đoán.

## Thêm công tắc mới

Để AI có thể điều khiển một công tắc mới, quản trị viên phải thêm chính xác
entity ID vào biến `HA_ALLOWED_ENTITIES` trong `voice_ai_server/.env`, ngăn
cách các entity bằng dấu phẩy. Ví dụ:

```dotenv
HA_ALLOWED_ENTITIES=switch.t1_chieu_sang_switch_3,switch.den_phong_ngu,switch.quat_phong_khach
```

Sau khi sửa cấu hình, cần khởi động lại dịch vụ Voice AI. AI chỉ được phép gọi
tool cho các entity có trong allowlist này; không được điều khiển entity ngoài
danh sách.

Khi thêm một thiết bị, tạo một mục riêng theo mẫu dưới đây để AI hiểu các tên
gọi tự nhiên của nó:

```md
## Đèn phòng ngủ

Tên người dùng có thể nói:

- đèn phòng ngủ
- đèn ngủ

Home Assistant entity:
`switch.den_phong_ngu`

Quy tắc:

- Bật: gọi `home_assistant_switch` với `entity_id` là
  `switch.den_phong_ngu` và `state` là `on`.
- Tắt: gọi `home_assistant_switch` với `entity_id` là
  `switch.den_phong_ngu` và `state` là `off`.
```

Tìm entity ID tại Home Assistant: **Settings → Devices & services → Entities**.
Hiện tool chỉ hỗ trợ entity dạng `switch.*` vì nó gọi các dịch vụ
`switch.turn_on` và `switch.turn_off`. Entity dạng `light.*` cần được bổ sung
hỗ trợ trong mã nguồn trước khi đưa vào allowlist. Sau khi cập nhật tài liệu
knowledge, chạy lại bước index knowledge nếu hệ thống đang dùng Qdrant.
