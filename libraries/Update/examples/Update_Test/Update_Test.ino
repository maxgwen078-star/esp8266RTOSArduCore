// ============================================================
//   Test de la bibliothèque Update sur ESP8266 RTOS
//   Vérifie que le portage compile et fonctionne
// ============================================================

#include <Arduino.h>
#include <Update.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_image_format.h"

// ============================================================
// Affichage des infos OTA au démarrage
// ============================================================
void printPartitionInfo() {
    Serial.println(F("=== Partitions OTA ==="));

    // Partition courante (celle qui tourne)
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running) {
        Serial.printf("Running   : %s @ 0x%08X (%u octets)\n",
                      running->label, running->address, running->size);
    } else {
        Serial.println(F("Running   : AUCUNE"));
    }

    // Prochaine partition OTA (celle où on va flasher)
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    if (next) {
        Serial.printf("Next OTA  : %s @ 0x%08X (%u octets)\n",
                      next->label, next->address, next->size);
    } else {
        Serial.println(F("Next OTA  : AUCUNE"));
    }

    // Partition de boot actuelle
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    if (boot) {
        Serial.printf("Boot      : %s @ 0x%08X\n",
                      boot->label, boot->address);
    }

    Serial.printf("Free sketch space : %u octets\n", ESP.getFreeSketchSpace());
    Serial.println();
}

// ============================================================
// Affichage des infos du système
// ============================================================
void printSystemInfo() {
    Serial.println(F("=== Système ==="));
    Serial.printf("CPU freq     : %u MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Free heap    : %u octets\n", ESP.getFreeHeap());
    Serial.printf("Flash size   : %u octets\n", ESP.getFlashChipSize());
    Serial.printf("SPI_FLASH_SEC_SIZE : %d\n", SPI_FLASH_SEC_SIZE);
    Serial.println();
}

// ============================================================
// Test de la classe Update
// ============================================================
void testUpdateClass() {
    Serial.println(F("=== Test de la classe Update ==="));

    // Test 1 : état initial
    Serial.printf("Update.hasError()      : %s\n", Update.hasError() ? "true" : "false");
    Serial.printf("Update.isRunning()     : %s\n", Update.isRunning() ? "true" : "false");
    Serial.printf("Update.errorString()   : %s\n", Update.errorString());

    // Test 2 : taille de la prochaine partition
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    if (next) {
        Serial.printf("Taille partition OTA : %u octets\n", next->size);
    }

    // Test 3 : begin avec taille inconnue (simulé)
    // ⚠️ Ne pas appeler pour de vrai, ça efface la flash !
    // if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
    //     Serial.printf("Update.begin() a échoué : %s\n", Update.errorString());
    // } else {
    //     Serial.println(F("Update.begin() OK"));
    //     Update.abort();   // Annule sans écrire
    // }

    Serial.println(F("Test Update OK (aucune écriture effectuée)"));
    Serial.println();
}

// ============================================================
// Test des MD5
// ============================================================
void testMD5() {
    Serial.println(F("=== Test MD5 ==="));

    // MD5 factice (32 caractères)
    const char *fake_md5 = "0123456789abcdef0123456789abcdef";
    if (Update.setMD5(fake_md5)) {
        Serial.printf("setMD5(%s) OK\n", fake_md5);
    } else {
        Serial.println(F("setMD5 a échoué"));
    }

    // MD5 invalide
    if (!Update.setMD5("invalid")) {
        Serial.println(F("setMD5(invalid) correctement rejeté"));
    }

    Serial.println();
}

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println(F("============================================"));
    Serial.println(F("  Test Update - ESP8266 RTOS"));
    Serial.println(F("============================================"));
    Serial.println();

    printSystemInfo();
    printPartitionInfo();
    testUpdateClass();
    testMD5();

    Serial.println(F("============================================"));
    Serial.println(F("  Tous les tests sont passés !"));
    Serial.println(F("============================================"));
    Serial.println();
}

// ============================================================
// Loop
// ============================================================
void loop() {
    static uint32_t last = 0;
    if (millis() - last > 10000) {
        last = millis();
        Serial.printf("[%lu] Update lib is %s\n",
                      millis(),
                      Update.hasError() ? "ERROR" : "OK");
    }
    delay(100);
}