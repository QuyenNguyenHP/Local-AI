---
id: project.esp32-voice-assistant.overview
type: project
status: active
updated: 2026-09-11
confidence: confirmed
tags: [esp32, voice-assistant, embedded-systems]
---

# ESP32 Voice Assistant and Related Projects

## Main ESP32 Interests
Mike uses ESP32 boards for:
- Voice assistants
- RC car control
- Displays
- Sensors
- Audio streaming
- Wake-word detection
- Wi-Fi communication
- Embedded AI clients

## Voice Assistant Architecture

ESP32-S3
├── Microphone
├── Wake word detection
└── Wi-Fi
     ↓
Ubuntu AI Server / RTX 3080
     ↓
Faster-Whisper
     ↓
Text
     ↓
Ollama
     ↓
Qwen / Gemma
     ↓
Text
     ↓
Kokoro TTS
     ↓
PCM audio
     ↓
ESP32-S3
     ↓
I2S
     ↓
Speaker

## Wake Word
Wake word technology explored:
- ESP-SR
- WakeNet
- microWakeWord

Known WakeNet example:
- "Hi ESP"

Future goal:
- Custom wake word

## RC Car Project
Original controller:
- Arduino Nano

Upgrade plan:
- ESP32
- Raspberry Pi Zero
- Camera
- LiDAR

Possible capabilities:
- Remote driving
- Video streaming
- Obstacle detection
- Autonomous movement
- Computer vision

