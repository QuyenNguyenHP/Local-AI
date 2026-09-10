# ESP32-S3 voice AI client

Open `VoiceAI_S3/VoiceAI_S3.ino` in Arduino IDE. This sketch adapts
`/home/daikai/pi-sense-hat/esp32/wake_record_playback` for this repository's
`voice_ai_server`, using the same Waveshare ESP32-S3-Touch-LCD-1.85C **V1** hardware.

Say **Hi ESP**, speak after the wake message, and pause. The device records
16 kHz mono PCM, uploads a multipart WAV to `/v1/voice/chat`, downloads the WAV
answer into PSRAM, and plays it through the onboard PCM5101 DAC. It listens
for a follow-up for eight seconds after each answer. No speech or a request
failure returns it to wake-word detection. Each new wake conversation uses a
new session ID; follow-ups share that ID.

## Configuration

Edit `VoiceAI_S3/secrets.h` with your Wi-Fi SSID/password, server computer's
LAN IP, and optional API key. A placeholder file is provided locally and is
ignored by Git. After a fresh checkout, copy `secrets.example.h` to `secrets.h`.

```cpp
#define WIFI_SSID "your-network"
#define WIFI_PASSWORD "your-password"
#define SERVER_URL "http://192.168.1.10:8000/v1/voice/chat"
#define API_KEY ""
```

Match `API_KEY` to `voice_ai_server/.env` when it is set. The sketch uses HTTP
on your local network. The URL must point at the computer, not `localhost` or
`127.0.0.1`. It does not follow redirects or configure HTTPS certificates.

`config.h` contains language, volume, timeout, reply-size limit and board pins:

| Signal | GPIO |
| --- | --- |
| Microphone WS | 2 |
| Microphone BCLK | 15 |
| Microphone data | 39 |
| PCM5101 BCLK | 48 |
| PCM5101 LRCK | 38 |
| PCM5101 data | 47 |

The microphone defaults to the **right** stereo slot, matching your reference.
The microphone stays in 32-bit stereo I2S mode, with the Arduino driver
converting samples to PCM16 before the active slot is selected.
No external microphone, amplifier or talk button is needed for this board.
The screen and touch interface are not used.

## Arduino IDE settings

Use **esp32 by Espressif Systems 3.3.11** (the locally installed version used
for compilation) and these settings, matching the reference project's setup:

- Board: **ESP32S3 Dev Module**.
- Flash Size: **16MB (128Mb)**.
- PSRAM: **OPI PSRAM**.
- Partition Scheme: **ESP SR 16M (3MB APP/6MB SPIFFS/3.9MB MODEL)**.
- USB CDC On Boot: **Enabled**.
- Serial Monitor: **115200 baud**.

`ESP_I2S`, `ESP_SR` and the ESP-IDF HTTP client come with the board package.
WakeNet needs the `model` partition and its model data uploaded by the Arduino
ESP-SR build flow. A generic partition scheme without it will not work.
Upload using Arduino IDE after filling in `secrets.h`. If the board will not
enter upload mode, hold BOOT, press and release RESET, release BOOT, then
select its USB port again. Changing the partition layout may require erasing
flash; this removes any existing data stored on the board.

The I2S API is documented in the
[Espressif Arduino I2S reference](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2s.html).

## Run the server

From the repository root, with its dependencies/models already installed:

```bash
.venv/bin/python voice_ai_server/run.py
```

Use `HOST=0.0.0.0` and `PORT=8000` in `voice_ai_server/.env` so the board can
reach it. Wait for model warmup to finish, then check from a LAN client:

```bash
curl http://192.168.1.10:8000/healthz
```

Allow inbound TCP 8000 if the computer's firewall blocks it. See
[the server README](../voice_ai_server/README.md) for installation and GPU setup.
The server handles Whisper → Ollama/knowledge → Kokoro. The sketch explicitly
requests `response_format=audio`, with bearer authentication when configured.
It does not use the old bridge's raw PCM upload, DELETE endpoint, or text headers.

## Operation and tuning

- Say **Hi ESP**, wait for `Speak now`, then ask your question.
- Recording stops after about 900 ms of silence, or eight seconds of audio.
- Initial speech must start within three seconds; follow-ups have eight seconds.
- `SILENCE_THRESHOLD`, timing and pre-roll settings are near the top of the sketch.
  Raise the threshold for noisy rooms or lower it for quiet speech.
- Change `MIC_IS_LEFT_CHANNEL` only if the reference microphone test shows the
  left slot is active on your board.
- `VOLUME_PERCENT` defaults to 100 and supports 0–100. Unlike the reference's
  200% gain, this setting does not amplify beyond the source level.
- `LANGUAGE` defaults to `en`; use an empty string for automatic transcription
  language detection. Changing it does not change Kokoro's server-side voice.
- The HTTP timeout is 180 seconds. The response cap is 3 MiB (about 65 seconds
  of mono PCM16 at 24 kHz). Allocation failures or larger responses are reported
  on Serial. WakeNet and audio buffers share the board's 8 MB PSRAM.
- HTTP 401 means the API key is missing/wrong; HTTP 503 means the voice pipeline
  failed. Check the server logs for details. The sketch prints up to 512 bytes
  of an HTTP error response.
- A startup Wi-Fi failure requires resetting after fixing the network. Once
  running, the sketch reconnects before sending a request if necessary.

Playback accepts PCM16 WAV at 8–48 kHz, mono or stereo, and walks RIFF chunks
rather than assuming a fixed header. HTTP chunked transfer encoding is handled
by the ESP-IDF client. Capture/WakeNet are paused during requests and playback;
queued microphone audio is discarded before listening resumes.

## Validation

The sketch translation unit was compiled with the locally installed ESP32
3.3.11 Xtensa compiler using the reference sketch's cached board/include flags.
A host-side test exercised WAV generation, mono/stereo playback conversion,
extra RIFF chunks, and rejection of malformed/truncated WAVs. A full Arduino
firmware link/upload and physical microphone/wake-word/speaker testing remain
to be performed on the board.
