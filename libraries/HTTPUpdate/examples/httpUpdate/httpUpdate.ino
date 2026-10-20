// ============================================================
//   Test HTTPUpdate - ESP8266 RTOS
//   Telecharge un firmware depuis une URL HTTP
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPUpdate.h>

const char* WIFI_SSID = "TON_SSID";
const char* WIFI_PASS = "TON_PASS";

// URL du firmware (remplace par la tienne)
const char* FIRMWARE_URL = "http://192.168.88.100/firmware.bin";

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println(F("============================================"));
    Serial.println(F("  Test HTTPUpdate - ESP8266 RTOS"));
    Serial.println(F("============================================"));

    // --- Connexion WiFi ---
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.printf("Connexion a %s", WIFI_SSID);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
        Serial.print(".");
        delay(500);
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("Echec WiFi"));
        return;
    }

    Serial.print(F("IP : "));
    Serial.println(WiFi.localIP());
    Serial.println();

    // --- Lance la mise a jour ---
    Serial.printf("Telechargement depuis : %s\n", FIRMWARE_URL);

    WiFiClient client;
    httpUpdate.rebootOnUpdate(false);   // On gere le reboot manuellement

    t_httpUpdate_return ret = httpUpdate.update(client, FIRMWARE_URL, "1.0.0");

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("Echec : (%d) %s\n",
                          httpUpdate.getLastError(),
                          httpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println(F("Pas de mise a jour disponible"));
            break;

        case HTTP_UPDATE_OK:
            Serial.println(F("Mise a jour OK ! Redemarrage..."));
            delay(1000);
            ESP.restart();
            break;
    }
}

void loop() {}