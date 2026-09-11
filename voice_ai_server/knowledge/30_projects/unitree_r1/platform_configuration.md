---
id: project.unitree-r1.platform-configuration
type: project-configuration
status: active
updated: 2026-09-11
confidence: confirmed
tags: [unitree-r1, jetson, network, ros2, dds]
---

# Unitree R1 EDU Platform Configuration

## Development Computer

The Unitree R1 EDU rear computer is configured with:

- NVIDIA Jetson Orin Nano Dev Kit
- ARMv8 CPU with 6 CPU cores
- Ubuntu 20.04.5
- Kernel 5.10.104-tegra
- ROS 2 Foxy
- CycloneDDS

## Unitree R1 Network

- Robot access point on wlan1: 192.168.12.1
- Example Wi-Fi address: 10.0.0.145
- Direct Ethernet robot rear PC: 192.168.123.164
- Direct Ethernet laptop: 192.168.123.11

## Unitree R1 Software Interfaces

Communication uses DDS and ROS 2 related interfaces.

- Known service namespace: /ros_bridge
- Known services: /config, /loco, /motion_switcher, /robot_state, and /voice
- Service type example: unitree_api/srv/Generic
- Known topic example: rt/api/gesture/request

## Unitree R1 Known FSM IDs

- 0 = ZeroTorque
- 4 = Stance
- 701 = Lie2StandUp
- 702 = StandUp2Lie
- 811 = Start
