# T-Display S3 Air Quality Monitor

ESP-IDF firmware for a Sensirion SEN55 air quality sensor on a LilyGo T-Display S3. Displays PM2.5 on the built-in LCD with color-coded AQI severity, and publishes all eight SEN55 readings to Home Assistant via MQTT.

## What it does

- Reads PM1.0, PM2.5, PM4.0, PM10, temperature, humidity, VOC index, and NOx index from the SEN55 at 1 Hz
- Displays PM2.5 in large text on a 320x170 landscape screen, background color changes with AQI severity (green/yellow/orange/red)
- Publishes all readings to MQTT with Home Assistant auto-discovery

## Hardware

- [LilyGo T-Display S3](https://www.lilygo.cc/products/t-display-s3) (non-touch, ESP32-S3, 16MB flash, 8MB PSRAM)
- [Sensirion SEN55](https://sensirion.com/products/catalog/SEN55) environmental sensor module

## Wiring

The SEN55 connects to the T-Display S3 via I2C (4 wires + SEL to GND).

```
  T-Display S3                    SEN55
  ┌──────────┐        ┌─────────────────────────┐
  │          │        │  (view from connector)   │
  │      5V  ├────────┤  1  VDD (5V)             │
  │     GND  ├───┬────┤  2  GND                  │
  │  GPIO 17 ├───│────┤  3  SDA                  │
  │  GPIO 18 ├───│────┤  4  SCL                  │
  │          │   └────┤  5  SEL (GND = I2C)      │
  │          │        │  6  NC                    │
  └──────────┘        └─────────────────────────┘
```

| SEN55 Pin | Signal | T-Display S3 |
|-----------|--------|--------------|
| 1 | VDD | 5V |
| 2 | GND | GND |
| 3 | SDA | GPIO 17 |
| 4 | SCL | GPIO 18 |
| 5 | SEL | GND (selects I2C mode) |
| 6 | NC | - |

No external pull-up resistors are needed. The ESP32-S3's internal pull-ups (~45k) are enabled and work at the SEN55's 10 kHz I2C clock speed. The SEN55's I2C lines are 3.3V compatible despite its 5V supply.

## Build and flash

Requires [ESP-IDF v6.0](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/).

```sh
# Copy and edit credentials
cp main/credentials.h.template main/credentials.h
# Fill in WIFI_SSID, WIFI_PASS, MQTT_BROKER_URI

idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

The T-Display S3 uses the ESP32-S3's built-in USB — any USB-C cable works.

## Home Assistant

Eight sensor entities are auto-discovered via MQTT:

| Entity | Unit | Device class |
|--------|------|-------------|
| PM1.0 | ug/m3 | `pm1` |
| PM2.5 | ug/m3 | `pm25` |
| PM4.0 | ug/m3 | - |
| PM10 | ug/m3 | `pm10` |
| Temperature | C | `temperature` |
| Humidity | % | `humidity` |
| VOC Index | - | `volatile_organic_compounds_part` |
| NOx Index | - | - |

MQTT topics follow the pattern `aqm/<device_id>/sensor/<entity>`, where `<device_id>` is derived from the ESP32's MAC address.

## License

Unlicensed / private project.
