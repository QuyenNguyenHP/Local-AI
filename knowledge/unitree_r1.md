# Unitree R1 EDU Knowledge

## Robot
Model: Unitree R1 EDU

## Development Computer
Rear PC:
- NVIDIA Jetson Orin Nano Dev Kit
- ARMv8 CPU
- 6 CPU cores
- Ubuntu 20.04.5
- Kernel 5.10.104-tegra
- ROS 2 Foxy
- CycloneDDS

## Network
Robot access point:
- wlan1: 192.168.12.1

Example Wi-Fi address:
- 10.0.0.145

Direct Ethernet example:
- Robot rear PC: 192.168.123.164
- Laptop: 192.168.123.11

## Robot Software
Communication uses DDS and ROS 2 related interfaces.

Known service namespace:
- /ros_bridge

Known services:
- /config
- /loco
- /motion_switcher
- /robot_state
- /voice

Service type example:
- unitree_api/srv/Generic

Known topic example:
- rt/api/gesture/request

## Known FSM IDs
- 0 = ZeroTorque
- 4 = Stance
- 701 = Lie2StandUp
- 702 = StandUp2Lie
- 811 = Start

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
