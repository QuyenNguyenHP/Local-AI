---
id: home.home-assistant-devices
type: home-automation
status: active
updated: 2026-09-17
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
