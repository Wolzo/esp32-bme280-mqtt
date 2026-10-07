# ESP32 BME280 MQTT Sensor

ESP32 firmware that reads temperature, humidity and pressure from a BME280 (I2C) and publishes them as JSON to an MQTT broker every 60 seconds.

## Hardware

ELEGOO ESP32 board (ESP-WROOM-32) and a BME280 breakout, powered from USB.

| BME280 | ESP32 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

I2C address 0x76 or 0x77 is detected automatically.

## Build and flash

Requires [PlatformIO](https://platformio.org/).

```sh
cp include/secrets.example.h include/secrets.h   # set Wi-Fi and MQTT credentials
pio run -t upload
pio device monitor
```

Other settings (pins, interval, topic prefix) are in `include/config.h`.

## MQTT

`<device_id>` is `esp32-bme280-` plus the last 3 bytes of the MAC address.

| Topic | Payload | Retained |
| --- | --- | --- |
| `home/sensors/<device_id>/state` | JSON (below) | yes |
| `home/sensors/<device_id>/status` | `online` / `offline` (Last Will) | yes |

```json
{
  "device": "esp32-bme280-a1b2c3",
  "fw": "1.0.0",
  "temperature_c": 21.37,
  "humidity_pct": 48.12,
  "pressure_hpa": 1013.25,
  "rssi_dbm": -58,
  "uptime_s": 3600,
  "interval_s": 60
}
```

Pressure is absolute (not sea-level). The payload has no timestamp: consumers should timestamp on receipt.

## Roadmap

Battery operation: deep sleep between measurements and a low-power board, since a development board's regulator and USB bridge draw milliamps even when the ESP32 sleeps.
