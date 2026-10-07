// Copy to include/secrets.h (git-ignored)
#pragma once

#include <stdint.h>

constexpr char WIFI_SSID[]     = "your-ssid";
constexpr char WIFI_PASSWORD[] = "your-wifi-password";

constexpr char     MQTT_HOST[]     = "192.168.1.10";  // IP or DNS name, no mDNS (.local)
constexpr uint16_t MQTT_PORT       = 1883;
constexpr char     MQTT_USER[]     = "";              // empty for anonymous
constexpr char     MQTT_PASSWORD[] = "";
