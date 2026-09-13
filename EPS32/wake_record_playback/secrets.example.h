#pragma once

// Copy this file to secrets.h and fill in your Wi-Fi details. The server URL
// must use the computer's LAN address, not 127.0.0.1.
#define WIFI_SSID "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"
#define VOICE_SERVER_URL "http://192.168.1.100:8000/v1/voice/chat"

// Leave empty only when API_KEY is not set in voice_ai_server/.env.
#define VOICE_SERVER_API_KEY ""
