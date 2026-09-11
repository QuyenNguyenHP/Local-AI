# ESP32 Projects

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

## Preferred Audio Components
Microphone:
- INMP441 I2S microphone

Speaker amplifier:
- MAX98357 I2S DAC / amplifier

Speaker:
- 4 ohm
- 3 W

## Wake Word
Wake word technology explored:
- ESP-SR
- WakeNet
- microWakeWord

Known WakeNet example:
- "Hi ESP"

Future goal:
- Custom wake word

## Display
Displays explored:
- ST7789
- 2.8-inch TFT
- ESP32-S3-Touch-LCD-1.85C

## FireBeetle ESP32 V4.0 Known Wiring
Known assignments from previous project:
- IO25 / D2 → L298 ENA
- IO26 / D3 → TFT RST
- IO27 / D4 → TFT DC
- IO13 / D7 → L298 IN1
- IO5 / D8 → TFT CS
- IO18 / SCK → TFT SCK
- IO23 / MOSI → TFT SDA / MOSI

## L298 Motor Driver
Important principle:
- IN pins control motor direction
- ENA / ENB pins control motor speed
- PWM is applied to ENA or ENB for speed control

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

## ESP32 Voltage Notes
ESP32 logic voltage:
- 3.3 V

Some ESP32 development boards can accept:
- 5 V through USB or regulated 5 V / VIN input

Do not apply 5 V directly to normal GPIO pins.

## General Embedded Design Preference
Before connecting modules:
1. Verify supply voltage
2. Verify GPIO logic level
3. Verify common ground
4. Verify I2C / SPI / I2S pin assignment
5. Verify current requirements
6. Avoid powering high-current loads directly from ESP32 GPIO
