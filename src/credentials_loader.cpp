#include <Preferences.h>
#include "secrets.h"

Preferences preferences;

void setup() {
  Serial.begin(115200);

  preferences.begin("credentials", false);

  preferences.putString("ssid", SECRET_SSID);
  preferences.putString("password", SECRET_SSID_PASS);
  preferences.putBytes("mac", SECRET_MAC, sizeof(SECRET_MAC));
  preferences.putInt("channel", SECRET_WIFI_CHANNEL);
  preferences.putString("mqtt_token", SECRET_MQTT_TOKEN);

  preferences.end();

  Serial.println("Credentials saved successfully");
}

void loop() {}
