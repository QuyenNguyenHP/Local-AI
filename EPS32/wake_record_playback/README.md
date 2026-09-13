# ESP32 wake-word local AI client

This Arduino sketch targets the ESP32-S3-Touch-LCD-1.85C V1 pinout used by
`speaker_mic_test` and `audio_out_no_tf`. It detects **Hi ESP** locally, records
16 kHz mono PCM WAV, sends it over Wi-Fi to the Voice AI server, and streams the
returned WAV response to the PCM5101 speaker. After each answer it listens for a
follow-up for eight seconds, so the wake phrase is only needed to start a new
conversation.

The computer pipeline is:

```text
ESP32 WakeNet -> WAV/multipart HTTP -> faster-whisper -> Qdrant RAG -> Ollama -> Kokoro -> WAV/HTTP -> ESP32
```

## Start the Voice AI server

From the Voice AI project, start the server as documented there:

```bash
cd "/home/dq/Local AI/voice_ai_server"
../.venv/bin/python run.py
```

The server listens on TCP port 8000. Test it locally with:

```bash
curl http://127.0.0.1:8000/healthz
```

Allow TCP port 8000 through the computer firewall if one is enabled. Ollama and
Qdrant can remain private; only this HTTP server needs to be reachable by the ESP32.

## Configure Wi-Fi

Copy `secrets.example.h` to `secrets.h`, then set the Wi-Fi credentials and the
computer's LAN address:

```cpp
#define WIFI_SSID "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"
#define VOICE_SERVER_URL "http://192.168.1.100:8000/v1/voice/chat"
#define VOICE_SERVER_API_KEY ""
```

Do not use `127.0.0.1` in the sketch: on the ESP32 that address means the ESP32
itself. Keep `secrets.h` private.

## Detailed Arduino IDE setup

The ESP32-S3-Touch-LCD-1.85C has **16 MB flash and 8 MB OPI PSRAM**. Do not
select a 4 MB or 8 MB flash configuration for this board.

### 1. Install or update the Espressif board package

1. In Arduino IDE, open **File > Preferences**.
2. In **Additional Boards Manager URLs**, add:

   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`

   Keep any URLs already present and separate multiple URLs with commas.
3. Open **Tools > Board > Boards Manager**.
4. Search for `esp32`.
5. Install or update **esp32 by Espressif Systems**. Waveshare specifies version
   3.0.2 or newer for this board. A current stable 3.x release contains the
   `ESP_I2S` and `ESP_SR` libraries used by this sketch.
6. Restart Arduino IDE after installing or updating the package.

No separate Library Manager installation is needed for `ESP_SR`; it is supplied
by the Espressif board package. A compile error such as `ESP_SR.h: No such file
or directory` normally means the wrong or an old ESP32 board package is active.

### 2. Select the board and flash settings

Connect the board over USB, then set:

- **Tools > Board**: select the Waveshare ESP32-S3-Touch-LCD-1.85C entry if it
  is available. With board packages that do not list it, use
  **ESP32S3 Dev Module**.
- **Tools > Port**: select the port that appears when the board is connected.
- **Tools > Flash Size**: **16MB (128Mb)**.
- **Tools > PSRAM**: **OPI PSRAM**. On versions that only show Enabled/Disabled,
  select **Enabled**.
- **Tools > Partition Scheme**:
  **ESP SR 16M (3MB APP/7MB SPIFFS/2.9MB MODEL)**.
- **Tools > USB CDC On Boot**: **Enabled**, if this option is present. This makes
  `Serial` output available through the native USB connection.

The ESP-SR partition is essential: it contains a dedicated `model` partition
where the build system uploads the WakeNet model. A normal 16 MB partition
scheme has enough total flash but does not necessarily contain that model
partition.

After changing the partition scheme, it is safest to set **Tools > Erase All
Flash Before Sketch Upload > Enabled** for the first upload. Upload once, then
return this setting to **Disabled** to avoid erasing flash on every upload.

If **ESP SR 16M** is absent, check all three of these conditions:

1. **ESP32S3 Dev Module** or the correct Waveshare S3 board is selected—not a
   classic ESP32, ESP32-C3, or ESP32-S2.
2. **Flash Size** is set to 16 MB.
3. `esp32 by Espressif Systems` is updated to a current 3.x release and Arduino
   IDE has been restarted.

### 3. Confirm that PSRAM works

The sketch allocates about 256 KB for eight seconds of mono recording. During
startup it calls `psramFound()`. Open **Tools > Serial Monitor**, select
**115200 baud**, and reset the board.

Correct startup continues past the PSRAM check. If it prints:

`PSRAM is required. Enable it in Arduino Tools > PSRAM.`

then recheck that **OPI PSRAM** is selected. If the board continuously reboots,
also verify **Flash Size = 16MB** and that the selected board target is an
ESP32-S3.

### 4. Find the microphone's active I2S channel

Before uploading the wake-word sketch:

1. Open `esp32/speaker_mic_test/speaker_mic_test.ino`.
2. Keep the same board, flash, PSRAM, and port selections.
3. Upload the microphone test.
4. Open Serial Monitor at **115200 baud**.
5. Stay quiet for a few readings, then speak or tap gently near the microphone.

The monitor prints lines similar to:

```text
L level=120 peak=850 | R level=2 peak=10
L level=940 peak=6200 | R level=2 peak=12
```

In this example the **left** values increase strongly, so leave this setting in
`wake_record_playback.ino`:

```cpp
constexpr bool MIC_IS_LEFT_CHANNEL = true;
```

If the right level and peak increase while the left side stays near zero,
change it to:

```cpp
constexpr bool MIC_IS_LEFT_CHANNEL = false;
```

Use the channel that reacts consistently to your voice. Absolute numbers are
less important than the change between a quiet room and speaking. WakeNet will
usually never trigger if the inactive channel is selected.

### 5. Upload and operate the AI client

Start the Python bridge first. Open `wake_record_playback.ino`, upload it, and
use a 115200-baud serial monitor. Say **Hi ESP**, wait for `Speak now`, speak,
and pause. The recording ends after about 0.9 seconds of silence. The computer
transcribes it, asks Ollama, synthesizes an answer, and sends the WAV back.

Expected serial output is approximately:

```text
Starting wake-word record/playback...
Ready. Say 'Hi ESP' (using the left microphone slot).
Wake word detected. Speak now.
Recording...
Speech started.
Recorded 2.15 seconds.
Uploading 2.15 seconds of WAV audio...
Playing AI response at 24000 Hz...
Listening for a follow-up...
```

If `API_KEY` is set in `voice_ai_server/.env`, copy its value into
`VOICE_SERVER_API_KEY`. Speak during the follow-up window to continue with the
same server-side conversation. After eight seconds without speech it returns to
WakeNet; the next wake word starts a fresh server session. HTTP failures also
safely return it to wake mode.

If the USB port disappears during upload, hold **BOOT**, press and release
**RESET**, release **BOOT**, select the newly appearing port, and upload again.

## Tuning

- Run `speaker_mic_test` first. If its right channel changes when you speak,
  set `MIC_IS_LEFT_CHANNEL` to `false` in the new sketch.
- If recording stops between words, increase `END_SILENCE_MS`.
- If ambient noise prevents recording from stopping, increase
  `SILENCE_THRESHOLD`. If speech is not detected, decrease it.
- The built-in WakeNet model uses **Hi ESP**. A different phrase requires a
  custom WakeNet model; changing a string in this sketch cannot change it.

The buffer holds at most eight seconds of 16 kHz, signed 16-bit mono audio in
PSRAM. It is intentionally not saved to flash or a TF card.
