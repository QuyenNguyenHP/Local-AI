---
id: project.unitree-r1.overview
type: project
status: active
updated: 2026-09-11
confidence: confirmed
tags: [unitree-r1, robotics, ros2, dds]
---

# Unitree R1 EDU Project Overview

## Robot
Model: Unitree R1 EDU

## Existing Capabilities
Mike has already achieved:
- Live video
- Face recognition
- Audio playback
- Base movement
- Upper-body joint control

Upper-body control:
- 13 joints

## Planned AI Architecture

Microphone
↓
Faster-Whisper
↓
Text
↓
Robot Agent
↓
Qwen local LLM
↓
Decision
├── Text response → Kokoro TTS → Speaker
└── Robot action → DDS / robot control

Camera
↓
YOLO
↓
Detection results
↓
Robot Agent

## Planned Capabilities
- Object detection
- Human detection
- Phone / knife / camera detection
- Object tracking
- Stable tracking ID
- Distance estimation
- Head and neck tracking
- Autonomous navigation
- Go-to-pose
- Obstacle avoidance
- SLAM
- Upper-body action macros
- Voice interaction

## AI Models Considered
Speech-to-text:
- Faster-Whisper Small

Language model:
- Qwen2.5 3B Q4
- Qwen3 family models for larger computers

Text-to-speech:
- Kokoro-82M

Computer vision:
- YOLO

## Important Design Preference
Robot action control does not necessarily need ROS 2 high-level planning if Unitree's existing DDS/control services can perform the required action directly.
