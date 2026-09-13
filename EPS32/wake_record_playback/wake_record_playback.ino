#include <Arduino.h>
#include <string.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "ESP_I2S.h"
#include "ESP_SR.h"  // Also links the ESP-SR component in Arduino builds.
#include "esp_afe_config.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "model_path.h"
#include "esp_heap_caps.h"
#include "secrets.h"
#include "status_display.h"

// ESP32-S3-Touch-LCD-1.85C V1 digital microphone.
constexpr int MIC_WS_PIN = 2;
constexpr int MIC_BCLK_PIN = 15;
constexpr int MIC_DATA_PIN = 39;

// V1 PCM5101 speaker DAC.
constexpr int SPEAKER_BCLK_PIN = 48;
constexpr int SPEAKER_LRCK_PIN = 38;
constexpr int SPEAKER_DATA_PIN = 47;

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr char FIRMWARE_ID[] = "wake-record-voice-ai-v2";

// Speaker playback volume: 100 = original, 50 = half, 200 = twice.
// Values above 100 can clip loud recordings.
constexpr uint16_t PLAYBACK_VOLUME_PERCENT = 200;

// Set this to false if speaker_mic_test shows that the RIGHT level moves.
constexpr bool MIC_IS_LEFT_CHANNEL = false;

// Recording behavior. SILENCE_THRESHOLD is the average absolute 16-bit level.
constexpr uint32_t MAX_RECORD_MS = 8000;
constexpr uint32_t WAIT_FOR_SPEECH_MS = 3000;
constexpr uint32_t FOLLOW_UP_WAIT_MS = 8000;
constexpr uint32_t END_SILENCE_MS = 900;
constexpr uint32_t MIN_SPEECH_MS = 300;
constexpr uint32_t PRE_ROLL_MS = 200;
constexpr uint16_t SILENCE_THRESHOLD = 450;
constexpr size_t READ_FRAMES = 256;
constexpr size_t MAX_RECORD_SAMPLES = (SAMPLE_RATE * MAX_RECORD_MS) / 1000;

// The mic supplies a stereo I2S stream. WakeNet receives the selected mic slot
// as M and ignores the other slot as N.
#if 1
// These macros must be compile-time strings/enum values for ESP_SR.begin().
#define SR_INPUT_CHANNELS SR_CHANNELS_STEREO
#define MIC_I2S_CHANNELS I2S_SLOT_MODE_STEREO
#endif

I2SClass micI2S;
I2SClass speakerI2S;

int16_t *recording = nullptr;
volatile bool recordRequested = false;
volatile bool recognitionPaused = false;
volatile bool feedTaskPaused = false;
bool ready = false;
bool conversationActive = false;
char sessionId[32] = {};
uint32_t conversationNumber = 0;

srmodel_list_t *speechModels = nullptr;
const esp_afe_sr_iface_t *afeHandle = nullptr;
esp_afe_sr_data_t *afeData = nullptr;
TaskHandle_t feedTaskHandle = nullptr;
TaskHandle_t detectTaskHandle = nullptr;

void printMemory(const char *stage) {
  Serial.printf(
      "Memory %s: heap=%u, largest internal=%u, PSRAM free=%u, "
      "largest PSRAM=%u\n",
      stage, static_cast<unsigned>(ESP.getFreeHeap()),
      static_cast<unsigned>(heap_caps_get_largest_free_block(
          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      static_cast<unsigned>(ESP.getFreePsram()),
      static_cast<unsigned>(heap_caps_get_largest_free_block(
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
}

void wakeNetFeedTask(void *parameter) {
  (void)parameter;
  const int frameSamples = afeHandle->get_feed_chunksize(afeData);
  const int channels = afeHandle->get_feed_channel_num(afeData);
  const size_t frameBytes = frameSamples * channels * sizeof(int16_t);
  int16_t *input = static_cast<int16_t *>(heap_caps_malloc(
      frameBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

  if (input == nullptr) {
    Serial.println("WakeNet feed buffer allocation failed.");
    vTaskDelete(nullptr);
  }

  while (true) {
    if (recognitionPaused) {
      feedTaskPaused = true;
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    feedTaskPaused = false;
    const size_t bytesRead = micI2S.readBytes(
        reinterpret_cast<char *>(input), frameBytes);
    if (bytesRead == frameBytes) {
      afeHandle->feed(afeData, input);
    } else {
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
}

void wakeNetDetectTask(void *parameter) {
  (void)parameter;

  while (true) {
    if (recognitionPaused) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    afe_fetch_result_t *result =
        afeHandle->fetch_with_delay(afeData, pdMS_TO_TICKS(100));
    if (result == nullptr || result->ret_value != ESP_OK) continue;

    if (result->wakeup_state == WAKENET_DETECTED) {
      // This board has one physical microphone in a selected stereo I2S slot,
      // so there is no need to wait for microphone-array channel verification.
      Serial.println("Wake word detected. Speak now.");
      recognitionPaused = true;
      recordRequested = true;
    } else if (result->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
      Serial.printf("Wake word verified on channel %d. Speak now.\n",
                    result->trigger_channel_id);
      recognitionPaused = true;
      recordRequested = true;
    }
  }
}

bool initializeWakeNet() {
  speechModels = esp_srmodel_init("model");
  if (speechModels == nullptr) {
    Serial.println("Could not load models from the ESP-SR partition.");
    return false;
  }

  const char *inputFormat = MIC_IS_LEFT_CHANNEL ? "MN" : "NM";
  afe_config_t *config =
      afe_config_init(inputFormat, speechModels, AFE_TYPE_SR, AFE_MODE_LOW_COST);
  if (config == nullptr || !config->wakenet_init) {
    Serial.println("WakeNet model is unavailable in the model partition.");
    if (config != nullptr) afe_config_free(config);
    return false;
  }

  // This path creates only the AFE/VAD/WakeNet pipeline. Unlike ESP_SR.begin(),
  // it does not create MultiNet or build a command FST.
  afeHandle = esp_afe_handle_from_config(config);
  afeData = afeHandle == nullptr ? nullptr : afeHandle->create_from_config(config);
  afe_config_free(config);
  if (afeHandle == nullptr || afeData == nullptr) {
    Serial.println("Could not create the WakeNet audio front end.");
    return false;
  }

  const int expectedChannels = 2;
  if (afeHandle->get_feed_channel_num(afeData) != expectedChannels) {
    Serial.printf("Unexpected WakeNet input channel count: %d\n",
                  afeHandle->get_feed_channel_num(afeData));
    return false;
  }

  if (xTaskCreatePinnedToCore(wakeNetFeedTask, "WakeNet feed", 4096, nullptr,
                              5, &feedTaskHandle, 0) != pdPASS ||
      xTaskCreatePinnedToCore(wakeNetDetectTask, "WakeNet detect", 6144,
                              nullptr, 5, &detectTaskHandle, 1) != pdPASS) {
    Serial.println("Could not start WakeNet tasks.");
    return false;
  }

  return true;
}

bool initializeMicrophone() {
  micI2S.setPins(MIC_BCLK_PIN, MIC_WS_PIN, -1, MIC_DATA_PIN, -1);
  micI2S.setTimeout(1000);
  if (!micI2S.begin(I2S_MODE_STD, SAMPLE_RATE,
                    I2S_DATA_BIT_WIDTH_32BIT, MIC_I2S_CHANNELS,
                    I2S_STD_SLOT_BOTH)) {
    Serial.printf("Microphone initialization failed, error=%d\n",
                  micI2S.lastError());
    return false;
  }

  // WakeNet requires signed 16-bit, 16 kHz samples. The microphone itself is
  // still clocked in its required 32-bit I2S format.
  if (!micI2S.configureRX(SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                          MIC_I2S_CHANNELS, I2S_RX_TRANSFORM_32_TO_16)) {
    Serial.printf("Microphone conversion setup failed, error=%d\n",
                  micI2S.lastError());
    return false;
  }

  return true;
}

bool initializeSpeaker() {
  speakerI2S.setPins(SPEAKER_BCLK_PIN, SPEAKER_LRCK_PIN,
                     SPEAKER_DATA_PIN, -1, -1);
  speakerI2S.setTimeout(1000);
  if (!speakerI2S.begin(I2S_MODE_STD, SAMPLE_RATE,
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO,
                        I2S_STD_SLOT_LEFT)) {
    Serial.printf("Speaker initialization failed, error=%d\n",
                  speakerI2S.lastError());
    return false;
  }

  return true;
}

size_t recordUtterance(uint32_t waitForSpeechMs) {
  int16_t stereo[READ_FRAMES * 2];
  size_t storedSamples = 0;
  bool speechStarted = false;
  uint32_t speechStartMs = 0;
  uint32_t lastLoudMs = millis();
  const uint32_t recordStartMs = millis();

  Serial.println("Recording...");
  statusDisplayShow("Listening", "Speak now");

  while (storedSamples < MAX_RECORD_SAMPLES) {
    statusDisplayPoll();
    const size_t bytesRead = micI2S.readBytes(
        reinterpret_cast<char *>(stereo), sizeof(stereo));
    const size_t framesRead = bytesRead / (sizeof(int16_t) * 2);
    if (framesRead == 0) {
      Serial.printf("Microphone read failed, error=%d\n", micI2S.lastError());
      break;
    }

    uint32_t magnitudeTotal = 0;
    const size_t channel = MIC_IS_LEFT_CHANNEL ? 0 : 1;
    for (size_t frame = 0;
         frame < framesRead && storedSamples < MAX_RECORD_SAMPLES; ++frame) {
      const int16_t sample = stereo[frame * 2 + channel];
      recording[storedSamples++] = sample;
      magnitudeTotal += abs(static_cast<int32_t>(sample));
    }

    const uint16_t level = magnitudeTotal / framesRead;
    const uint32_t now = millis();
    if (level >= SILENCE_THRESHOLD) {
      if (!speechStarted) {
        speechStarted = true;
        speechStartMs = now;
        // Drop the waiting silence but retain a short lead-in so the first
        // consonant is not clipped.
        const size_t preRollSamples = (SAMPLE_RATE * PRE_ROLL_MS) / 1000;
        if (storedSamples > preRollSamples) {
          memmove(recording, recording + storedSamples - preRollSamples,
                  preRollSamples * sizeof(int16_t));
          storedSamples = preRollSamples;
        }
        Serial.println("Speech started.");
      }
      lastLoudMs = now;
    }

    if (!speechStarted && now - recordStartMs >= waitForSpeechMs) {
      Serial.println("No speech heard.");
      return 0;
    }

    if (speechStarted && now - speechStartMs >= MIN_SPEECH_MS &&
        now - lastLoudMs >= END_SILENCE_MS) {
      break;
    }
  }

  Serial.printf("Recorded %.2f seconds.\n",
                static_cast<float>(storedSamples) / SAMPLE_RATE);
  return storedSamples;
}

uint16_t readLe16(const uint8_t *data) {
  return data[0] | (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readLe32(const uint8_t *data) {
  return data[0] | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

void writeLe16(uint8_t *data, uint16_t value) {
  data[0] = value & 0xff;
  data[1] = (value >> 8) & 0xff;
}

void writeLe32(uint8_t *data, uint32_t value) {
  data[0] = value & 0xff;
  data[1] = (value >> 8) & 0xff;
  data[2] = (value >> 16) & 0xff;
  data[3] = (value >> 24) & 0xff;
}

// The Voice AI server expects a conventional mono, 16-bit PCM WAV upload.
void writePcmWavHeader(uint8_t *header, uint32_t pcmBytes) {
  memcpy(header, "RIFF", 4);
  writeLe32(header + 4, 36 + pcmBytes);
  memcpy(header + 8, "WAVEfmt ", 8);
  writeLe32(header + 16, 16);
  writeLe16(header + 20, 1);
  writeLe16(header + 22, 1);
  writeLe32(header + 24, SAMPLE_RATE);
  writeLe32(header + 28, SAMPLE_RATE * sizeof(int16_t));
  writeLe16(header + 32, sizeof(int16_t));
  writeLe16(header + 34, 16);
  memcpy(header + 36, "data", 4);
  writeLe32(header + 40, pcmBytes);
}

bool playWav(HTTPClient &http) {
  WiFiClient *stream = http.getStreamPtr();
  uint8_t header[44];
  if (stream->readBytes(header, sizeof(header)) != sizeof(header) ||
      memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0 ||
      memcmp(header + 36, "data", 4) != 0) {
    Serial.println("Server returned an unsupported WAV file.");
    return false;
  }

  const uint16_t channels = readLe16(header + 22);
  const uint32_t sampleRate = readLe32(header + 24);
  const uint16_t bits = readLe16(header + 34);
  uint32_t remaining = readLe32(header + 40);
  if (channels != 1 || bits != 16 || sampleRate < 8000 || sampleRate > 48000) {
    Serial.printf("Unsupported WAV: %u Hz, %u channels, %u bits.\n",
                  sampleRate, channels, bits);
    return false;
  }

  speakerI2S.end();
  if (!speakerI2S.begin(I2S_MODE_STD, sampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                        I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("Could not configure speaker for response sample rate.");
    return false;
  }

  Serial.printf("Playing AI response at %u Hz...\n", sampleRate);
  statusDisplayShow("Answering", "Playing AI response");
  uint8_t buffer[1024];
  uint32_t played = 0;
  while (remaining > 0) {
    statusDisplayPoll();
    const size_t wanted = min(static_cast<uint32_t>(sizeof(buffer)), remaining);
    const size_t received = stream->readBytes(buffer, wanted);
    if (received == 0) break;

    if (PLAYBACK_VOLUME_PERCENT != 100) {
      int16_t *samples = reinterpret_cast<int16_t *>(buffer);
      for (size_t i = 0; i < received / 2; ++i) {
        int32_t value = static_cast<int32_t>(samples[i]) *
                        PLAYBACK_VOLUME_PERCENT / 100;
        samples[i] = constrain(value, INT16_MIN, INT16_MAX);
      }
    }
    played += speakerI2S.write(buffer, received);
    remaining -= received;
  }
  Serial.printf("Playback finished (%u bytes).\n", played);
  return remaining == 0;
}

// Called from loop only; keep the error visible before resuming WakeNet.
void showAssistantError(const char *message, const char *detail) {
  Serial.printf("Assistant error: %s (%s)\n", message, detail);
  statusDisplayShow(message, detail);
  const uint32_t started = millis();
  while (millis() - started < 5000) {
    statusDisplayPoll();
    delay(10);
  }
}

bool askAssistant(size_t sampleCount) {
  if (WiFi.status() != WL_CONNECTED) {
    showAssistantError("Wi-Fi disconnected", "Check Wi-Fi connection");
    return false;
  }
  statusDisplayShow("Thinking...", "Waiting for AI server");
  // /v1/voice/chat accepts multipart/form-data. Keep the temporary request
  // body in PSRAM, alongside rather than replacing the recording buffer.
  constexpr char boundary[] = "----ESP32VoiceAI7MA4YWxkTrZu0gW";
  const String audioPart = String("--") + boundary + "\r\n" +
      "Content-Disposition: form-data; name=\"audio\"; filename=\"question.wav\"\r\n" +
      "Content-Type: audio/wav\r\n\r\n";
  const String sessionPart = String("\r\n--") + boundary + "\r\n" +
      "Content-Disposition: form-data; name=\"session_id\"\r\n\r\n" +
      sessionId + "\r\n--" + boundary + "--\r\n";
  const size_t pcmBytes = sampleCount * sizeof(int16_t);
  const size_t bodyBytes = audioPart.length() + 44 + pcmBytes + sessionPart.length();
  uint8_t *body = static_cast<uint8_t *>(heap_caps_malloc(
      bodyBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (body == nullptr) {
    Serial.println("Could not allocate the HTTP upload buffer in PSRAM.");
    showAssistantError("Not enough memory", "Cannot send recording");
    return false;
  }

  size_t offset = 0;
  memcpy(body + offset, audioPart.c_str(), audioPart.length());
  offset += audioPart.length();
  writePcmWavHeader(body + offset, pcmBytes);
  offset += 44;
  memcpy(body + offset, recording, pcmBytes);
  offset += pcmBytes;
  memcpy(body + offset, sessionPart.c_str(), sessionPart.length());

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(180000);
  if (!http.begin(VOICE_SERVER_URL)) {
    heap_caps_free(body);
    showAssistantError("API setup failed", "Check server URL");
    return false;
  }
  http.addHeader("Content-Type", String("multipart/form-data; boundary=") + boundary);
#ifdef VOICE_SERVER_API_KEY
  if (strlen(VOICE_SERVER_API_KEY) > 0) {
    http.addHeader("Authorization", String("Bearer ") + VOICE_SERVER_API_KEY);
  }
#endif

  Serial.printf("Uploading %.2f seconds of WAV audio...\n",
                static_cast<float>(sampleCount) / SAMPLE_RATE);
  const int status = http.POST(body, bodyBytes);
  heap_caps_free(body);
  if (status != HTTP_CODE_OK) {
    Serial.printf("Assistant request failed: HTTP %d\n", status);
    http.end();
    const String code = String("HTTP ") + status;
    if (WiFi.status() != WL_CONNECTED) {
      showAssistantError("Wi-Fi disconnected", "Check Wi-Fi connection");
    } else if (status == HTTPC_ERROR_READ_TIMEOUT) {
      showAssistantError("Server timeout", "Please try again later");
    } else if (status < 0) {
      showAssistantError("Cannot reach server", "Check server, IP and port");
    } else if (status == 401 || status == 403) {
      showAssistantError("Access denied", "Check API key");
    } else if (status >= 500) {
      showAssistantError("AI server error", code.c_str());
    } else {
      showAssistantError("Request failed", code.c_str());
    }
    return false;
  }
  const bool played = playWav(http);
  http.end();
  if (!played) {
    showAssistantError("Audio response error", "Invalid WAV or playback failed");
  }
  return played;
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to Wi-Fi %s", WIFI_SSID);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.printf("Starting %s (built %s %s)...\n", FIRMWARE_ID,
                __DATE__, __TIME__);
  statusDisplayInit();

  if (!psramFound()) {
    Serial.println("PSRAM is required. Enable it in Arduino Tools > PSRAM.");
    statusDisplayShow("PSRAM required", "Enable OPI PSRAM");
    return;
  }

  statusDisplayShow("Connecting Wi-Fi...", "Please wait");
  if (!connectWiFi()) {
    Serial.println("Wi-Fi connection failed. Check secrets.h.");
    statusDisplayShow("Wi-Fi failed", "Check secrets.h");
    return;
  }

  // Initialize only the microphone before the speech model.
  if (!initializeMicrophone()) {
    statusDisplayShow("Microphone init failed", "Check Serial Monitor");
    return;
  }
  printMemory("before WakeNet");

  statusDisplayShow("Loading Hi ESP...", "Please wait");
  if (!initializeWakeNet()) {
    statusDisplayShow("WakeNet init failed", "Check model partition");
    return;
  }

  Serial.println("WakeNet initialized.");
  printMemory("after WakeNet");

  if (!initializeSpeaker()) {
    statusDisplayShow("Speaker init failed", "Check Serial Monitor");
    return;
  }

  recording = static_cast<int16_t *>(heap_caps_malloc(
      MAX_RECORD_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (recording == nullptr) {
    Serial.println("Could not allocate the recording buffer in PSRAM.");
    statusDisplayShow("Not enough memory", "Recording buffer failed");
    return;
  }
  printMemory("after audio buffers");

  ready = true;
  statusDisplayShow("Say: Hi ESP", "Ready");
  Serial.printf("Ready. Say 'Hi ESP' (using the %s microphone slot).\n",
                MIC_IS_LEFT_CHANNEL ? "left" : "right");
}

void loop() {
  statusDisplayPoll();
  if (!ready) {
    delay(1000);
    return;
  }

  if (!recordRequested) {
    delay(20);
    return;
  }

  recordRequested = false;
  statusDisplayShow(conversationActive ? "Listening" : "ESP called",
                    conversationActive ? "Ask another question" : "Hi ESP detected");
  const uint32_t pauseStart = millis();
  while (!feedTaskPaused && millis() - pauseStart < 1500) {
    delay(5);
  }
  if (!feedTaskPaused) {
    Serial.println("WakeNet feed task did not pause in time.");
    statusDisplayShow("Microphone busy", "Try Hi ESP again");
    recognitionPaused = false;
    return;
  }

  // The API scopes its conversation history by session_id. Start a fresh
  // session on each wake-word conversation; retain it for follow-ups.
  if (!conversationActive) {
    const uint64_t mac = ESP.getEfuseMac();
    snprintf(sessionId, sizeof(sessionId), "esp32-%04X%08X-%lu",
             static_cast<uint16_t>(mac >> 32), static_cast<uint32_t>(mac),
             static_cast<unsigned long>(++conversationNumber));
    Serial.printf("Voice server session: %s\n", sessionId);
  }
  const size_t sampleCount = recordUtterance(
      conversationActive ? FOLLOW_UP_WAIT_MS : WAIT_FOR_SPEECH_MS);
  if (sampleCount == 0) {
    conversationActive = false;
  } else {
    conversationActive = askAssistant(sampleCount);
  }

  // Avoid the speaker echo immediately re-triggering WakeNet.
  delay(250);
  afeHandle->reset_buffer(afeData);
  if (conversationActive) {
    statusDisplayShow("Listening", "Ask another question");
    Serial.println("Listening for a follow-up...");
    recordRequested = true;
  } else {
    statusDisplayShow("Say: Hi ESP", "Ready");
    recognitionPaused = false;
    Serial.println("Ready. Say 'Hi ESP'.");
  }
}
 
