# ESP32 PT100 MQTT Gateway

ESP32-based gateway for PT100 temperature sensors using MAX31865.
Publishes telemetry to ThingsBoard via MQTT and supports OTA firmware updates.

## Features

- PT100 / MAX31865 temperature measurement
- WiFi + MQTT (ThingsBoard compatible)
- OTA firmware updates
- Credentials stored securely using Preferences
- Modular and extensible design

## Hardware

- ESP32
- MAX31865 RTD interface
- PT100 sensor (3-wire)

## Firmware Structure

- `main.cpp`: main firmware
- `credentials_loader.cpp`: one-time credential loader
- `secrets.h.example`: credentials template

## Setup

1. Copy `secrets.h.example` → `secrets.h`
2. Fill WiFi and MQTT credentials
3. Flash `credentials_loader.cpp`
4. Flash `main.cpp`

## OTA

OTA updates are handled automatically via ThingsBoard shared attributes.

## License

MIT License
