#include <Arduino.h>
#include <SPIFFS.h>

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F("=== Test SPIFFS ==="));

    if (!SPIFFS.begin(true)) {   // true = format si mount échoue
        Serial.println(F("SPIFFS mount failed"));
        return;
    }

    Serial.printf("SPIFFS mounted!\n");
    Serial.printf("  Total : %u octets\n", SPIFFS.totalBytes());
    Serial.printf("  Used  : %u octets\n", SPIFFS.usedBytes());

    // Écriture
    File f = SPIFFS.open("/test.txt", "w");
    if (f) {
        f.println("Hello SPIFFS !");
        f.close();
        Serial.println(F("Ecrit /test.txt OK"));
    }

    // Lecture
    f = SPIFFS.open("/test.txt", "r");
    if (f) {
        Serial.print(F("Contenu : "));
        Serial.println(f.readString());
        f.close();
    }

    SPIFFS.end();
}

void loop() {}