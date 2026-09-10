#pragma once
#include <stddef.h>

// Copy secrets.example.h to secrets.h and fill in local credentials.
#include "secrets.h"
static const char LANGUAGE[] = "en"; // Empty = automatic Whisper detection.

// Waveshare ESP32-S3-Touch-LCD-1.85C V1, matching wake_record_playback.
constexpr int MIC_WS_PIN = 2;
constexpr int MIC_BCLK_PIN = 15;
constexpr int MIC_DATA_PIN = 39;
constexpr bool MIC_IS_LEFT_CHANNEL = false;
constexpr int SPEAKER_BCLK_PIN = 48;
constexpr int SPEAKER_LRCK_PIN = 38;
constexpr int SPEAKER_DATA_PIN = 47;

constexpr unsigned HTTP_TIMEOUT_MS = 180000;
constexpr size_t MAX_REPLY_BYTES = 3 * 1024 * 1024;
constexpr int VOLUME_PERCENT = 100; // 0..100; start lower if needed.
