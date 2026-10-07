#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>
#include <esp_timer.h>

#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h: copy include/secrets.example.h and fill in your values."
#endif

#define LOG(fmt, ...) Serial.printf("[%10lu] " fmt "\n", (unsigned long)millis(), ##__VA_ARGS__)

namespace {

Adafruit_BME280 bme;
WiFiClient netClient;
PubSubClient mqtt(netClient);

String deviceId;
String stateTopic;
String statusTopic;

bool sensorReady = false;
bool wifiWasConnected = false;
bool publishedOnce = false;

uint32_t wifiDownSinceMs = 0;
uint32_t lastMqttAttemptMs = 0;
uint32_t lastSensorAttemptMs = 0;
uint32_t lastPublishMs = 0;

String buildDeviceId() {
  // Last 3 MAC octets (getEfuseMac stores octet i at bits 8*i)
  const uint64_t mac = ESP.getEfuseMac();
  char buf[40];
  snprintf(buf, sizeof(buf), "%s-%02x%02x%02x", DEVICE_ID_PREFIX,
           (unsigned)((mac >> 24) & 0xFF),
           (unsigned)((mac >> 32) & 0xFF),
           (unsigned)((mac >> 40) & 0xFF));
  return String(buf);
}

double round2(double v) { return round(v * 100.0) / 100.0; }

bool initSensor() {
  for (uint8_t addr : BME280_ADDRESSES) {
    if (bme.begin(addr, &Wire)) {
      // Bosch "weather monitoring" profile
      bme.setSampling(Adafruit_BME280::MODE_FORCED,
                      Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::FILTER_OFF);
      LOG("BME280 found at 0x%02X", addr);
      return true;
    }
  }
  LOG("BME280 not found");
  return false;
}

void startWifi() {
  WiFi.persistent(false);
  WiFi.setHostname(deviceId.c_str());  // must precede WiFi.mode()
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiDownSinceMs = millis();
  LOG("Wi-Fi: connecting to \"%s\"", WIFI_SSID);
}

void maintainWifi(uint32_t now) {
  const bool connected = WiFi.status() == WL_CONNECTED;

  if (connected && !wifiWasConnected) {
    LOG("Wi-Fi: connected, IP %s, RSSI %d dBm",
        WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else if (!connected && wifiWasConnected) {
    LOG("Wi-Fi: connection lost");
    wifiDownSinceMs = now;
  }

  if (!connected && now - wifiDownSinceMs >= WIFI_RESTART_TIMEOUT_MS) {
    LOG("Wi-Fi: restarting connection");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiDownSinceMs = now;
  }

  wifiWasConnected = connected;
}

bool connectMqtt() {
  const char* user = MQTT_USER[0] ? MQTT_USER : nullptr;
  const char* pass = MQTT_PASSWORD[0] ? MQTT_PASSWORD : nullptr;

  const bool ok = mqtt.connect(deviceId.c_str(), user, pass,
                               statusTopic.c_str(), 1, true, "offline");
  if (ok) {
    mqtt.publish(statusTopic.c_str(), "online", true);
    LOG("MQTT: connected to %s:%u", MQTT_HOST, MQTT_PORT);
  } else {
    LOG("MQTT: connection failed, state %d", mqtt.state());
  }
  return ok;
}

bool readAndPublish() {
  if (!bme.takeForcedMeasurement()) {
    LOG("BME280: measurement timed out");
    sensorReady = false;
    return false;
  }

  const float t = bme.readTemperature();
  const float h = bme.readHumidity();
  const float p = bme.readPressure();

  if (isnan(t) || isnan(h) || isnan(p)) {
    LOG("BME280: invalid reading");
    sensorReady = false;
    return false;
  }

  JsonDocument doc;
  doc["device"]        = deviceId;
  doc["fw"]            = FW_VERSION;
  doc["temperature_c"] = round2(t);
  doc["humidity_pct"]  = round2(h);
  doc["pressure_hpa"]  = round2(p / 100.0);
  doc["rssi_dbm"]      = WiFi.RSSI();
  doc["uptime_s"]      = (uint64_t)(esp_timer_get_time() / 1000000LL);
  doc["interval_s"]    = PUBLISH_INTERVAL_MS / 1000UL;

  char payload[256];
  const size_t len = serializeJson(doc, payload, sizeof(payload));
  if (len == 0 || len >= sizeof(payload)) {
    LOG("JSON: payload buffer too small");
    return false;
  }

  if (!mqtt.publish(stateTopic.c_str(), reinterpret_cast<const uint8_t*>(payload),
                    len, MQTT_RETAIN_STATE)) {
    LOG("MQTT: publish failed");
    return false;
  }

  LOG("MQTT: %s %s", stateTopic.c_str(), payload);
  return true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  deviceId    = buildDeviceId();
  stateTopic  = String(MQTT_TOPIC_PREFIX) + "/" + deviceId + "/state";
  statusTopic = String(MQTT_TOPIC_PREFIX) + "/" + deviceId + "/status";
  LOG("Boot: %s, firmware %s", deviceId.c_str(), FW_VERSION);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  sensorReady = initSensor();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  mqtt.setBufferSize(512);

  startWifi();

  const uint32_t now = millis();
  lastMqttAttemptMs   = now - MQTT_RECONNECT_INTERVAL_MS;  // first attempt immediately
  lastSensorAttemptMs = now;
}

void loop() {
  const uint32_t now = millis();

  maintainWifi(now);

  if (WiFi.status() == WL_CONNECTED) {
    if (mqtt.connected()) {
      mqtt.loop();
    } else if (now - lastMqttAttemptMs >= MQTT_RECONNECT_INTERVAL_MS) {
      lastMqttAttemptMs = now;
      connectMqtt();
    }
  }

  if (!sensorReady && now - lastSensorAttemptMs >= SENSOR_RETRY_INTERVAL_MS) {
    lastSensorAttemptMs = now;
    sensorReady = initSensor();
  }

  if (sensorReady && mqtt.connected() &&
      (!publishedOnce || now - lastPublishMs >= PUBLISH_INTERVAL_MS)) {
    lastPublishMs = now;
    if (readAndPublish()) publishedOnce = true;
  }

  delay(10);
}
