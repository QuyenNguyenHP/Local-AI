---
id: project.esp32-voice-assistant.hardware-and-wiring
type: project-configuration
status: active
updated: 2026-09-11
confidence: confirmed
tags: [esp32, audio, display, wiring, l298]
---

# ESP32 Voice Assistant Hardware and Wiring

## Preferred Audio Components

- Microphone: INMP441 I2S microphone
- Speaker amplifier: MAX98357 I2S DAC and amplifier
- Speaker: 4 ohm, 3 W

## Displays Explored

- ST7789
- 2.8-inch TFT
- ESP32-S3-Touch-LCD-1.85C

## FireBeetle ESP32 V4.0 Known Wiring

Known assignments from Mike's previous project:

- IO25 / D2 → L298 ENA
- IO26 / D3 → TFT RST
- IO27 / D4 → TFT DC
- IO13 / D7 → L298 IN1
- IO5 / D8 → TFT CS
- IO18 / SCK → TFT SCK
- IO23 / MOSI → TFT SDA / MOSI

## L298 Motor Driver

- IN pins control motor direction.
- ENA and ENB pins control motor speed.
- PWM is applied to ENA or ENB for speed control.

## ESP32 Voltage Rules

- ESP32 logic voltage is 3.3 V.
- Some ESP32 development boards accept 5 V through USB or a regulated 5 V or VIN input.
- Do not apply 5 V directly to normal ESP32 GPIO pins.

## Mike's Embedded Connection Checklist

1. Verify the supply voltage.
2. Verify GPIO logic levels.
3. Verify common ground.
4. Verify I2C, SPI, or I2S pin assignments.
5. Verify current requirements.
6. Do not power high-current loads directly from an ESP32 GPIO.
