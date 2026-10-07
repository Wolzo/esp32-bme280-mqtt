#pragma once

#include <stdint.h>

constexpr char FW_VERSION[]       = "1.0.0";
constexpr char DEVICE_ID_PREFIX[] = "esp32-bme280";

constexpr int     I2C_SDA_PIN        = 21;
constexpr int     I2C_SCL_PIN        = 22;
constexpr uint8_t BME280_ADDRESSES[] = {0x76, 0x77};

constexpr uint32_t PUBLISH_INTERVAL_MS = 60UL * 1000UL;

constexpr char     MQTT_TOPIC_PREFIX[]   = "home/sensors";
constexpr bool     MQTT_RETAIN_STATE     = true;
constexpr uint16_t MQTT_KEEPALIVE_S      = 30;
constexpr uint16_t MQTT_SOCKET_TIMEOUT_S = 5;

constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 5UL * 1000UL;
constexpr uint32_t SENSOR_RETRY_INTERVAL_MS   = 10UL * 1000UL;
constexpr uint32_t WIFI_RESTART_TIMEOUT_MS    = 30UL * 1000UL;
