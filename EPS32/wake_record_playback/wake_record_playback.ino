#include <Arduino.h>
#include <string.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "ESP_I2S.h"
#include "ESP_SR.h"  // Also links the ESP-SR component in Arduino builds.
#include "esp_afe_config.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "model_path.h"
#include "esp_heap_caps.h"
#include "secrets.h"

// ESP32-S3-Touch-LCD-1.85C V1 digital microphone.
constexpr int MIC_WS_PIN = 2;
constexpr int MIC_BCLK_PIN = 15;
constexpr int MIC_DATA_PIN = 39;

// V1 PCM5101 speaker DAC.
constexpr int SPEAKER_BCLK_PIN = 48;
constexpr int SPEAKER_LRCK_PIN = 38;
constexpr int SPEAKER_DATA_PIN = 47;

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr char FIRMWARE_ID[] = "wake-record-ai-stream-v1";

// Wi-Fi setup portal. If saved and secrets.h credentials both fail, connect
// a phone/laptop to this AP and browse to http://192.168.4.1.
constexpr char SETUP_AP_SSID[] = "ESP32-Voice-Setup";
constexpr char SETUP_AP_PASSWORD[] = "configureme";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

// Speaker playback volume: 100 = original, 50 = half, 200 = twice.
// Values above 100 can clip loud recordings.
constexpr uint16_t PLAYBACK_VOLUME_PERCENT = 200;

// Set this to false if speaker_mic_test shows that the RIGHT level moves.
constexpr bool MIC_IS_LEFT_CHANNEL = false;

// Recording behavior. SILENCE_THRESHOLD is the average absolute 16-bit level.
constexpr uint32_t MAX_RECORD_MS = 20000;
constexpr uint32_t WAIT_FOR_SPEECH_MS = 5000;
constexpr uint32_t FOLLOW_UP_WAIT_MS = 8000;
// Allow natural pauses while the user is forming a longer question. Recording
// stops only after 2.8 seconds without speech, or at MAX_RECORD_MS.
constexpr uint32_t END_SILENCE_MS = 2800;
constexpr uint32_t MIN_SPEECH_MS = 300;
constexpr uint32_t PRE_ROLL_MS = 300;
constexpr uint16_t SILENCE_THRESHOLD = 300;
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

  while (storedSamples < MAX_RECORD_SAMPLES) {
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
  uint8_t buffer[1024];
  uint32_t played = 0;
  while (remaining > 0) {
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

void resetServerConversation() {
  HTTPClient http;
  http.begin(VOICE_SERVER_URL);
  const int status = http.sendRequest("DELETE");
  Serial.printf("New conversation (server status %d).\n", status);
  http.end();
}

bool askAssistant(size_t sampleCount) {
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(180000);
  http.begin(VOICE_SERVER_URL);
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("X-Sample-Rate", String(SAMPLE_RATE));
  const char *wantedHeaders[] = {"X-User-Text", "X-Assistant-Text"};
  http.collectHeaders(wantedHeaders, 2);

  Serial.printf("Uploading %.2f seconds of audio...\n",
                static_cast<float>(sampleCount) / SAMPLE_RATE);
  const int status = http.POST(reinterpret_cast<uint8_t *>(recording),
                               sampleCount * sizeof(int16_t));
  if (status != HTTP_CODE_OK) {
    Serial.printf("Assistant request failed: HTTP %d, %s\n", status,
                  http.getString().c_str());
    http.end();
    return false;
  }
  Serial.printf("You: %s\n", http.header("X-User-Text").c_str());
  Serial.printf("Assistant: %s\n", http.header("X-Assistant-Text").c_str());
  const bool played = playWav(http);
  http.end();
  return played;
}

bool tryWiFi(const String &ssid, const String &password) {
  if (ssid.isEmpty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.printf("Connecting to Wi-Fi %s", ssid.c_str());
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - started < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    return false;
  }
  Serial.printf("Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  return value;
}

String setupPage(const String &message = "") {
  String page =
      "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>ESP32 Voice Wi-Fi</title><style>body{font-family:sans-serif;max-width:480px;margin:40px auto;padding:20px}"
      "input,select,button{box-sizing:border-box;width:100%;padding:12px;margin:7px 0}"
      "button{background:#1769aa;color:white;border:0}</style></head><body>"
      "<h2>ESP32 Voice Assistant</h2>";
  if (!message.isEmpty()) page += "<p>" + htmlEscape(message) + "</p>";
  page += "<form method='post' action='/save'><label>Wi-Fi network</label><select name='ssid'>";

  const int count = WiFi.scanNetworks();
  for (int i = 0; i < count; ++i) {
    const String ssid = htmlEscape(WiFi.SSID(i));
    page += "<option value='" + ssid + "'>" + ssid + " (" +
            String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  WiFi.scanDelete();
  page +=
      "</select><label>Wi-Fi password</label><input name='password' type='password' maxlength='63'>"
      "<button type='submit'>Save and connect</button></form>"
      "<p>If this page did not open automatically, browse to <b>192.168.4.1</b>.</p></body></html>";
  return page;
}

bool runWiFiSetupPortal() {
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASSWORD)) {
    Serial.println("Could not start Wi-Fi setup AP.");
    return false;
  }

  const IPAddress apIP = WiFi.softAPIP();
  DNSServer dns;
  WebServer web(80);
  bool credentialsSubmitted = false;
  String submittedSsid;
  String submittedPassword;
  String statusMessage = "Select the Wi-Fi network used by the AI server.";

  dns.start(53, "*", apIP);
  web.on("/", HTTP_GET, [&]() {
    web.send(200, "text/html", setupPage(statusMessage));
  });
  web.on("/save", HTTP_POST, [&]() {
    submittedSsid = web.arg("ssid");
    submittedPassword = web.arg("password");
    if (submittedSsid.isEmpty()) {
      web.send(400, "text/html", setupPage("Please select a Wi-Fi network."));
      return;
    }
    web.send(200, "text/html",
             "<html><meta name='viewport' content='width=device-width'><body>"
             "<h3>Credentials received</h3><p>The ESP32 is testing the connection. "
             "Watch Serial Monitor; this setup network will close when connected.</p></body></html>");
    credentialsSubmitted = true;
  });
  web.onNotFound([&]() {
    web.sendHeader("Location", String("http://") + apIP.toString(), true);
    web.send(302, "text/plain", "");
  });
  web.begin();

  Serial.printf("Wi-Fi setup AP started.\n  Network: %s\n  Password: %s\n  Open: http://%s\n",
                SETUP_AP_SSID, SETUP_AP_PASSWORD, apIP.toString().c_str());

  while (true) {
    dns.processNextRequest();
    web.handleClient();
    if (!credentialsSubmitted) {
      delay(5);
      continue;
    }

    credentialsSubmitted = false;
    delay(500);  // Give the browser time to receive the confirmation page.
    web.stop();
    dns.stop();
    WiFi.softAPdisconnect(true);

    if (tryWiFi(submittedSsid, submittedPassword)) {
      Preferences preferences;
      preferences.begin("voice-wifi", false);
      preferences.putString("ssid", submittedSsid);
      preferences.putString("password", submittedPassword);
      preferences.end();
      Serial.println("Wi-Fi credentials saved in flash.");
      return true;
    }

    statusMessage = "Connection failed. Check the password and try again.";
    Serial.println("Wi-Fi connection failed; restarting setup portal.");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASSWORD);
    dns.start(53, "*", WiFi.softAPIP());
    web.begin();
  }
}

bool connectWiFi() {
  Preferences preferences;
  preferences.begin("voice-wifi", true);
  const String savedSsid = preferences.getString("ssid", "");
  const String savedPassword = preferences.getString("password", "");
  preferences.end();

  if (!savedSsid.isEmpty() && tryWiFi(savedSsid, savedPassword)) return true;

  const String fallbackSsid = WIFI_SSID;
  if (!fallbackSsid.isEmpty() && fallbackSsid != "your-wifi-name" &&
      fallbackSsid != savedSsid && tryWiFi(fallbackSsid, WIFI_PASSWORD)) {
    return true;
  }

  return runWiFiSetupPortal();
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

  if (!connectWiFi()) {
    Serial.println("Wi-Fi setup failed. Reset the board to try again.");
    return;
  }

  // Initialize only the microphone before the speech model.
  if (!initializeMicrophone()) return;
  printMemory("before WakeNet");

  if (!initializeWakeNet()) return;

  Serial.println("WakeNet initialized.");
  printMemory("after WakeNet");

  if (!initializeSpeaker()) {
    return;
  }

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

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi connection lost; starting reconnection/setup.");
    recognitionPaused = true;
    if (!connectWiFi()) {
      delay(1000);
      return;
    }
    afeHandle->reset_buffer(afeData);
    recognitionPaused = false;
    conversationActive = false;
  }

  if (!recordRequested) {
    delay(20);
    return;
  }

  recordRequested = false;
  const uint32_t pauseStart = millis();
  while (!feedTaskPaused && millis() - pauseStart < 1500) {
    delay(5);
  }
  if (!feedTaskPaused) {
    Serial.println("WakeNet feed task did not pause in time.");
    recognitionPaused = false;
    return;
  }

  if (!conversationActive) resetServerConversation();
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
    Serial.println("Listening for a follow-up...");
    recordRequested = true;
  } else {
    recognitionPaused = false;
    Serial.println("Ready. Say 'Hi ESP'.");
  }
}
 
