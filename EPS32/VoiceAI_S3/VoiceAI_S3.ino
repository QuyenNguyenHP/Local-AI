#include <Arduino.h>
#include <string.h>
#include <WiFi.h>
#include <esp_http_client.h>
#include <esp_system.h>
#include "ESP_I2S.h"
#include "ESP_SR.h"  // Also links the ESP-SR component in Arduino builds.
#include "esp_afe_config.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "model_path.h"
#include "esp_heap_caps.h"
#include "config.h"

#if !CONFIG_IDF_TARGET_ESP32S3
#error Select an ESP32-S3 board.
#endif

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr unsigned RECORD_RATE = SAMPLE_RATE;
constexpr char FIRMWARE_ID[] = "waveshare-local-ai-v1";

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
#define MIC_I2S_CHANNELS I2S_SLOT_MODE_STEREO

I2SClass micI2S;
I2SClass speakerI2S;

int16_t *recording = nullptr;
volatile bool recordRequested = false;
volatile bool recognitionPaused = false;
volatile bool feedTaskPaused = false;
volatile bool detectTaskPaused = false;
bool ready = false;
bool conversationActive = false;

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
    return;
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
      detectTaskPaused = true;
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    detectTaskPaused = false;
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

size_t recordUtterance(uint32_t waitForSpeechMs) {
  int16_t stereo[READ_FRAMES * 2];
  size_t storedSamples = 0;
  bool speechStarted = false;
  uint32_t speechStartMs = 0;
  uint32_t lastLoudMs = millis();
  const uint32_t recordStartMs = millis();

  Serial.println("Recording...");

  while (storedSamples < MAX_RECORD_SAMPLES) {
    const size_t bytesRead = micI2S.readBytes(
        reinterpret_cast<char *>(stereo), sizeof(stereo));
    const size_t framesRead = bytesRead / (sizeof(int16_t) * 2);
    if (framesRead == 0) {
      Serial.printf("Microphone read failed, error=%d\n", micI2S.lastError());
      return 0;
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

    // Keep only pre-roll while waiting, preserving the full recording budget.
    const size_t preRoll = SAMPLE_RATE * PRE_ROLL_MS / 1000;
    if (!speechStarted && storedSamples > preRoll) {
      memmove(recording, recording + storedSamples - preRoll, preRoll * sizeof(int16_t));
      storedSamples = preRoll;
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
  return speechStarted ? storedSamples : 0;
}

String sessionId;
static const char BOUNDARY[] = "----ESP32S3VoiceBoundary9f32";
static_assert(VOLUME_PERCENT >= 0 && VOLUME_PERCENT <= 100, "Invalid volume");


// The HTTP client removes chunked transfer framing before calling this sink.
struct Reply {
  uint8_t *data = nullptr;
  size_t size = 0;
  size_t capacity = 0;
  bool failed = false;
};

esp_err_t receiveHttp(esp_http_client_event_t *event) {
  Reply *reply = static_cast<Reply *>(event->user_data);
  if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) return ESP_OK;
  size_t count = static_cast<size_t>(event->data_len);
  if (reply->failed || count > MAX_REPLY_BYTES - reply->size) {
    reply->failed = true;
    return ESP_FAIL;
  }
  size_t needed = reply->size + count;
  if (needed > reply->capacity) {
    size_t capacity = (needed + 16383) & ~size_t(16383);
    if (capacity > MAX_REPLY_BYTES) capacity = MAX_REPLY_BYTES;
    void *next = heap_caps_realloc(reply->data, capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!next) { reply->failed = true; return ESP_FAIL; }
    reply->data = static_cast<uint8_t *>(next);
    reply->capacity = capacity;
  }
  memcpy(reply->data + reply->size, event->data, count);
  reply->size = needed;
  return ESP_OK;
}

void put16(uint8_t *p, uint16_t n) { p[0] = n; p[1] = n >> 8; }
void put32(uint8_t *p, uint32_t n) { put16(p, n); put16(p + 2, n >> 16); }
uint16_t get16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
uint32_t get32(const uint8_t *p) { return uint32_t(get16(p)) | (uint32_t(get16(p + 2)) << 16); }

void wavHeader(uint8_t *p, size_t bytes) {
  memcpy(p, "RIFF", 4); put32(p + 4, bytes + 36);
  memcpy(p + 8, "WAVEfmt ", 8); put32(p + 16, 16);
  put16(p + 20, 1); put16(p + 22, 1);
  put32(p + 24, RECORD_RATE); put32(p + 28, RECORD_RATE * 2);
  put16(p + 32, 2); put16(p + 34, 16);
  memcpy(p + 36, "data", 4); put32(p + 40, bytes);
}

String field(const char *name, const String &value) {
  return String("--") + BOUNDARY + "\r\nContent-Disposition: form-data; name=\"" +
         name + "\"\r\n\r\n" + value + "\r\n";
}

bool connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi failed. Check config.h; check Wi-Fi and reset if startup failed.");
    return false;
  }
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  return true;
}

bool playReply(const uint8_t *wav, size_t length) {
  // Walk RIFF chunks instead of assuming every WAV has a 44-byte header.
  if (length < 12 || memcmp(wav, "RIFF", 4) || memcmp(wav + 8, "WAVE", 4)) return false;
  size_t end = size_t(get32(wav + 4)) + 8;
  if (end < 12 || end > length) return false;
  uint16_t format = 0, channels = 0, bits = 0, align = 0;
  uint32_t rate = 0;
  const uint8_t *pcm = nullptr;
  size_t pcmBytes = 0;
  for (size_t pos = 12; pos + 8 <= end;) {
    uint32_t size = get32(wav + pos + 4);
    const uint8_t *chunk = wav + pos;
    pos += 8;
    if (size > end - pos) return false;
    if (!memcmp(chunk, "fmt ", 4) && size >= 16) {
      format = get16(wav + pos); channels = get16(wav + pos + 2);
      rate = get32(wav + pos + 4); align = get16(wav + pos + 12);
      bits = get16(wav + pos + 14);
    } else if (!memcmp(chunk, "data", 4)) {
      pcm = wav + pos; pcmBytes = size;
    }
    pos += size;
    if (size & 1) { if (pos == end) return false; ++pos; }
  }
  if (!pcm || !pcmBytes || format != 1 || bits != 16 ||
      (channels != 1 && channels != 2) || align != channels * 2 ||
      pcmBytes % align || rate < 8000 || rate > 48000) return false;

  speakerI2S.setPins(SPEAKER_BCLK_PIN, SPEAKER_LRCK_PIN, SPEAKER_DATA_PIN, -1);
  if (!speakerI2S.begin(I2S_MODE_STD, rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) return false;
  Serial.printf("Playing %u Hz reply...\n", unsigned(rate));
  int16_t stereo[512];
  bool ok = true;
  for (size_t pos = 0; pos < pcmBytes && ok;) {
    size_t frames = (pcmBytes - pos) / align;
    if (frames > 256) frames = 256;
    for (size_t i = 0; i < frames; ++i) {
      int16_t left = static_cast<int16_t>(get16(pcm + pos));
      int16_t right = channels == 2 ? static_cast<int16_t>(get16(pcm + pos + 2)) : left;
      stereo[i * 2] = int32_t(left) * VOLUME_PERCENT / 100;
      stereo[i * 2 + 1] = int32_t(right) * VOLUME_PERCENT / 100;
      pos += align;
    }
    size_t bytes = frames * 4, sent = 0;
    while (sent < bytes) {
      size_t n = speakerI2S.write(reinterpret_cast<uint8_t *>(stereo) + sent, bytes - sent);
      if (!n) { ok = false; break; }
      sent += n;
    }
    delay(1);
  }
  // Feed silence to drain the queued final samples before deleting the channel.
  memset(stereo, 0, sizeof(stereo));
  for (int i = 0; i < 16 && ok; ++i) ok = speakerI2S.write(reinterpret_cast<uint8_t *>(stereo), sizeof(stereo)) == sizeof(stereo);
  speakerI2S.end();
  return ok;
}

bool askAssistant(size_t sampleCount) {
  if (!connectWifi()) return false;
  String prefix = field("session_id", sessionId) + field("response_format", "audio");
  if (LANGUAGE[0]) prefix += field("language", LANGUAGE);
  prefix += String("--") + BOUNDARY + "\r\nContent-Disposition: form-data; name=\"audio\"; filename=\"recording.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String suffix = String("\r\n--") + BOUNDARY + "--\r\n";
  size_t pcmBytes = sampleCount * sizeof(int16_t);
  size_t bodySize = prefix.length() + 44 + pcmBytes + suffix.length();
  uint8_t *body = static_cast<uint8_t *>(heap_caps_malloc(bodySize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!body) { Serial.println("Not enough PSRAM for upload."); return false; }
  memcpy(body, prefix.c_str(), prefix.length());
  uint8_t *wav = body + prefix.length();
  wavHeader(wav, pcmBytes);
  memcpy(wav + 44, recording, pcmBytes);
  memcpy(wav + 44 + pcmBytes, suffix.c_str(), suffix.length());
  Serial.printf("Uploading %u bytes. Waiting for AI...\n", unsigned(pcmBytes));
  Reply reply;
  esp_http_client_config_t config = {};
  config.url = SERVER_URL;
  config.method = HTTP_METHOD_POST;
  config.timeout_ms = HTTP_TIMEOUT_MS;
  config.event_handler = receiveHttp;
  config.user_data = &reply;
  config.disable_auto_redirect = true;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) { Serial.println("HTTP initialization failed."); free(body); return false; }
  String contentType = String("multipart/form-data; boundary=") + BOUNDARY;
  String authorization = String("Bearer ") + API_KEY;
  esp_http_client_set_header(client, "Content-Type", contentType.c_str());
  esp_http_client_set_header(client, "Accept", "audio/wav");
  if (API_KEY[0]) esp_http_client_set_header(client, "Authorization", authorization.c_str());
  esp_http_client_set_post_field(client, reinterpret_cast<const char *>(body), bodySize);
  esp_err_t result = esp_http_client_perform(client);
  int status = esp_http_client_get_status_code(client);
  bool complete = esp_http_client_is_complete_data_received(client);
  esp_http_client_cleanup(client);
  free(body);
  bool played = false;
  if (result != ESP_OK || reply.failed || !complete) {
    Serial.printf("Transfer failed: %s; HTTP %d. Check server logs, timeout, and PSRAM/reply limit.\n", esp_err_to_name(result), status);
  } else if (status != 200) {
    Serial.printf("Server HTTP %d: ", status);
    if (reply.data) Serial.write(reply.data, reply.size < 512 ? reply.size : 512);
    Serial.println();
  } else if (!(played = playReply(reply.data, reply.size))) {
    Serial.println("Playback failed: expected PCM16 mono/stereo WAV, 8–48 kHz, or I2S output failed.");
  }
  free(reply.data);
  return played;
}
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.printf("Starting %s (built %s %s)...\n", FIRMWARE_ID,
                __DATE__, __TIME__);

  if (!psramFound()) {
    Serial.println("PSRAM is required. Enable it in Arduino Tools > PSRAM.");
    return;
  }

  if (strncmp(SERVER_URL, "http://", 7)) {
    Serial.println("Configure an http:// LAN server URL in config.h.");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (!connectWifi()) {
    Serial.println("Wi-Fi connection failed. Check secrets.h.");
    return;
  }

  // Initialize only the microphone before the speech model.
  if (!initializeMicrophone()) return;
  printMemory("before WakeNet");

  if (!initializeWakeNet()) return;

  Serial.println("WakeNet initialized.");
  printMemory("after WakeNet");

  recording = static_cast<int16_t *>(heap_caps_malloc(
      MAX_RECORD_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (recording == nullptr) {
    Serial.println("Could not allocate the recording buffer in PSRAM.");
    return;
  }
  printMemory("after audio buffers");

  ready = true;
  Serial.printf("Ready. Say 'Hi ESP' (using the %s microphone slot).\n",
                MIC_IS_LEFT_CHANNEL ? "left" : "right");
}

void loop() {
  if (!ready) {
    delay(1000);
    return;
  }

  if (!recordRequested) {
    delay(20);
    return;
  }

  recordRequested = false;
  const uint32_t pauseStart = millis();
  while ((!feedTaskPaused || !detectTaskPaused) && millis() - pauseStart < 1500) {
    delay(5);
  }
  if (!feedTaskPaused || !detectTaskPaused) {
    Serial.println("WakeNet feed task did not pause in time.");
    recognitionPaused = false;
    return;
  }

  if (!conversationActive) {
    // The local server has no DELETE endpoint; a fresh ID starts fresh history.
    sessionId = String("esp32s3-") + WiFi.macAddress() + "-" + String(esp_random(), HEX);
    sessionId.replace(":", "");
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
  // RX DMA kept running while the server/speaker were active. Discard queued
  // samples and speaker echo before follow-up recording or WakeNet resumes.
  int16_t discard[READ_FRAMES * 2];
  for (int i = 0; i < 16; ++i) {
    micI2S.readBytes(reinterpret_cast<char *>(discard), sizeof(discard));
  }
  afeHandle->reset_buffer(afeData);
  if (conversationActive) {
    Serial.println("Listening for a follow-up...");
    recordRequested = true;
  } else {
    recognitionPaused = false;
    Serial.println("Ready. Say 'Hi ESP'.");
  }
}
