/*
  ESP32 PT100 MQTT Gateway with OTA

  - Reads temperature from PT100 sensors via MAX31865
  - Publishes telemetry to ThingsBoard using MQTT
  - Supports OTA firmware updates
  - WiFi credentials stored using Preferences

  Author: AndMe16
  License: MIT
*/

/////////////////////////////////////
// Includes
/////////////////////////////////////

#include <Adafruit_MAX31865.h>

// Network & MQTT
#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>

// OTA
#include <Espressif_Updater.h>

// Storage
#include <Preferences.h>

// JSON
#include <ArduinoJson.h>

/////////////////////////////////////
// Persistent Storage
/////////////////////////////////////

Preferences preferences;

/////////////////////////////////////
// Device Configuration
/////////////////////////////////////

constexpr int DEVICE_TYPE_COLD_STORAGE = 1;
constexpr int DEVICE_TYPE_GENERIC = 2;

constexpr int DEVICE_TYPE = DEVICE_TYPE_COLD_STORAGE;

const char* firmwareTitle = nullptr;
constexpr char FIRMWARE_VERSION[] PROGMEM = "1.3.0";

/////////////////////////////////////
// OTA Configuration
/////////////////////////////////////

constexpr uint8_t OTA_RETRY_LIMIT = 24U;
constexpr uint16_t OTA_PACKET_SIZE = 4096U;

/////////////////////////////////////
// SPI Configuration (MAX31865)
/////////////////////////////////////

constexpr uint8_t PIN_SPI_CS  = 18;
constexpr uint8_t PIN_SPI_MOSI = 17;
constexpr uint8_t PIN_SPI_MISO = 16;
constexpr uint8_t PIN_SPI_CLK = 4;

/////////////////////////////////////
// WiFi Configuration (loaded from Preferences)
/////////////////////////////////////

String wifiSSID;
String wifiPassword;
unsigned char routerMAC[6];
int wifiChannel;

/////////////////////////////////////
// MQTT / ThingsBoard
/////////////////////////////////////

String mqttToken;
constexpr char MQTT_SERVER[] PROGMEM = "mqtt.thingsboard.cloud";
constexpr uint16_t MQTT_PORT = 1883U;
constexpr uint16_t MQTT_BUFFER_SIZE = 512U;

/////////////////////////////////////
// Serial
/////////////////////////////////////

constexpr uint32_t SERIAL_BAUDRATE = 115200U;

/////////////////////////////////////
// PT100 Configuration
/////////////////////////////////////

constexpr uint16_t PT100_REFERENCE_RESISTOR = 430;
constexpr uint16_t PT100_NOMINAL_RESISTANCE = 100;

/////////////////////////////////////
// Timing
/////////////////////////////////////

unsigned long lastTelemetryMillis = 0;
unsigned long lastConnectionCheckMillis = 0;

constexpr unsigned long TELEMETRY_INTERVAL_MS = 900000; // 15 minutes
constexpr unsigned long CONNECTION_CHECK_MS = 1000;

bool sendInitialTelemetry = true;

/////////////////////////////////////
// Runtime Variables
/////////////////////////////////////

float temperatureCelsius;
bool mqttConnected = false;

/////////////////////////////////////
// Hardware & Clients
/////////////////////////////////////

Adafruit_MAX31865 pt100(
  PIN_SPI_CS,
  PIN_SPI_MOSI,
  PIN_SPI_MISO,
  PIN_SPI_CLK
);

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard thingsboard(mqttClient, MQTT_BUFFER_SIZE);
Espressif_Updater otaUpdater;

/////////////////////////////////////
// JSON Buffers
/////////////////////////////////////

StaticJsonDocument<512> telemetryJson;
StaticJsonDocument<512> attributesJson;
char jsonBuffer[512];

/////////////////////////////////////
// WiFi Functions
/////////////////////////////////////

void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str(), wifiChannel, routerMAC);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
}

bool ensureWiFiConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  connectWiFi();
  return true;
}

/////////////////////////////////////
// OTA Callbacks
/////////////////////////////////////

void onOtaCompleted(const bool& success) {
  if (success) {
    Serial.println("OTA completed successfully, restarting...");
    esp_restart();
  } else {
    Serial.println("OTA update failed");
  }
}

void onOtaProgress(const size_t& chunk, const size_t& total) {
  static int counter = 0;
  if (++counter >= 10) {
    float percent = (chunk * 100.0f) / total;
    thingsboard.sendAttributeData("OTA_Progress", percent);
    counter = 0;
  }
}

/////////////////////////////////////
// Setup
/////////////////////////////////////

void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  delay(1000);

  preferences.begin("credentials", true);

  wifiSSID     = preferences.getString("ssid", "");
  wifiPassword = preferences.getString("password", "");
  preferences.getBytes("mac", routerMAC, sizeof(routerMAC));
  wifiChannel  = preferences.getInt("channel", 0);
  mqttToken    = preferences.getString("mqtt_token", "");

  preferences.end();

  firmwareTitle = (DEVICE_TYPE == DEVICE_TYPE_COLD_STORAGE)
                    ? "PT100_Cold_Storage"
                    : "PT100_Generic";

  connectWiFi();
  pt100.begin(MAX31865_3WIRE);
}

/////////////////////////////////////
// Main Loop
/////////////////////////////////////

void loop() {
  unsigned long now = millis();

  if (now - lastConnectionCheckMillis >= CONNECTION_CHECK_MS) {

    ensureWiFiConnection();

    if (!thingsboard.connected()) {
      mqttConnected = false;

      if (!thingsboard.connect(MQTT_SERVER, mqttToken.c_str(), MQTT_PORT)) {
        Serial.println("MQTT connection failed");
        return;
      }

      mqttConnected = true;

      thingsboard.Firmware_Send_Info(firmwareTitle, FIRMWARE_VERSION);
      thingsboard.Firmware_Send_State("UPDATED");

      OTA_Update_Callback otaCallback(
        &onOtaProgress,
        &onOtaCompleted,
        firmwareTitle,
        FIRMWARE_VERSION,
        &otaUpdater,
        OTA_RETRY_LIMIT,
        OTA_PACKET_SIZE
      );

      thingsboard.Subscribe_Firmware_Update(otaCallback);
    }

    lastConnectionCheckMillis = now;
  }

  if ((now - lastTelemetryMillis >= TELEMETRY_INTERVAL_MS) || sendInitialTelemetry) {
    sendInitialTelemetry = false;

    temperatureCelsius = pt100.temperature(
      PT100_NOMINAL_RESISTANCE,
      PT100_REFERENCE_RESISTOR
    );

    if (mqttConnected) {
      telemetryJson["temperature_celsius"] = temperatureCelsius;
      serializeJson(telemetryJson, jsonBuffer);
      thingsboard.sendTelemetryJson(jsonBuffer);
      telemetryJson.clear();
    }

    lastTelemetryMillis = now;
  }

  thingsboard.loop();
}
