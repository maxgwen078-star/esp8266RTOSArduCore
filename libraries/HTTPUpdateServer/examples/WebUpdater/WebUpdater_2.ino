#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "HTTPUpdateServer.h"

const char* ssid = "TON_SSID";
const char* password = "TON_PASS";

WebServer server(80);
HTTPUpdateServer httpUpdater(true);   // true = debug série

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n=== Test HTTPUpdateServer ==="));

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.printf("Connexion a %s", ssid);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    Serial.print("IP : ");
    Serial.println(WiFi.localIP());

    httpUpdater.setup(&server, "/update", "admin", "admin");
    server.begin();

    Serial.printf("Ouvre http://%s/update\n", WiFi.localIP().toString().c_str());
}

void loop() {
    server.handleClient();
}