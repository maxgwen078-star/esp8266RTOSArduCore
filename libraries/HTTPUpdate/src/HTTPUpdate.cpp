/**
 * HTTPUpdate.cpp - Mise a jour OTA via HTTP
 * Adapte pour ESP8266 RTOS Arduino
 */

#include "HTTPUpdate.h"
#include <StreamString.h>

#include <esp_partition.h>
#include <esp_ota_ops.h>

// ============================================================
//  Constructeurs / Destructeur
// ============================================================
HTTPUpdate::HTTPUpdate(void)
        : _httpClientTimeout(8000), _ledPin(-1), _ledOn(HIGH)
{
    _followRedirects = HTTPC_DISABLE_FOLLOW_REDIRECTS;
    _lastError = 0;
}

HTTPUpdate::HTTPUpdate(int httpClientTimeout)
        : _httpClientTimeout(httpClientTimeout), _ledPin(-1), _ledOn(HIGH)
{
    _followRedirects = HTTPC_DISABLE_FOLLOW_REDIRECTS;
    _lastError = 0;
}

HTTPUpdate::~HTTPUpdate(void)
{
}

// ============================================================
//  Methodes publiques
// ============================================================
HTTPUpdateResult HTTPUpdate::update(WiFiClient& client, const String& url,
                                     const String& currentVersion)
{
    HTTPClient http;
    if (!http.begin(client, url)) {
        return HTTP_UPDATE_FAILED;
    }
    return handleUpdate(http, currentVersion, false);
}

HTTPUpdateResult HTTPUpdate::update(HTTPClient& httpClient,
                                     const String& currentVersion)
{
    return handleUpdate(httpClient, currentVersion, false);
}

HTTPUpdateResult HTTPUpdate::update(WiFiClient& client, const String& host,
                                     uint16_t port, const String& uri,
                                     const String& currentVersion)
{
    HTTPClient http;
    if (!http.begin(client, host, port, uri)) {
        return HTTP_UPDATE_FAILED;
    }
    return handleUpdate(http, currentVersion, false);
}

HTTPUpdateResult HTTPUpdate::updateSpiffs(HTTPClient& httpClient,
                                           const String& currentVersion)
{
    return handleUpdate(httpClient, currentVersion, true);
}

HTTPUpdateResult HTTPUpdate::updateSpiffs(WiFiClient& client, const String& url,
                                           const String& currentVersion)
{
    HTTPClient http;
    if (!http.begin(client, url)) {
        return HTTP_UPDATE_FAILED;
    }
    return handleUpdate(http, currentVersion, true);
}

int HTTPUpdate::getLastError(void)
{
    return _lastError;
}

String HTTPUpdate::getLastErrorString(void)
{
    if (_lastError == 0) {
        return String();
    }

    // Erreur venant de Update
    if (_lastError > 0) {
        StreamString error;
        Update.printError(error);
        error.trim();
        return String("Update error: ") + error;
    }

    // Erreur HTTP client
    if (_lastError > -100) {
        return String("HTTP error: ") + HTTPClient::errorToString(_lastError);
    }

    switch (_lastError) {
        case HTTP_UE_TOO_LESS_SPACE:           return "Not Enough space";
        case HTTP_UE_SERVER_NOT_REPORT_SIZE:   return "Server Did Not Report Size";
        case HTTP_UE_SERVER_FILE_NOT_FOUND:    return "File Not Found (404)";
        case HTTP_UE_SERVER_FORBIDDEN:         return "Forbidden (403)";
        case HTTP_UE_SERVER_WRONG_HTTP_CODE:   return "Wrong HTTP Code";
        case HTTP_UE_SERVER_FAULTY_MD5:        return "Wrong MD5";
        case HTTP_UE_BIN_VERIFY_HEADER_FAILED: return "Verify Bin Header Failed";
        case HTTP_UE_BIN_FOR_WRONG_FLASH:      return "New Binary Does Not Fit Flash Size";
        case HTTP_UE_NO_PARTITION:             return "Partition Could Not be Found";
    }

    return String();
}

// ============================================================
//  SHA256 de la partition courante
// ============================================================
static String getSketchSHA256() {
    const size_t HASH_LEN = 32;
    uint8_t sha_256[HASH_LEN] = { 0 };

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        return String();
    }

    // esp_partition_get_sha256 peut ne pas exister dans certains SDK
    // => on retourne une chaine vide si indisponible
    #if defined(ESP_PARTITION_HAS_SHA256)
    if (esp_partition_get_sha256(running, sha_256) != ESP_OK) {
        return String();
    }
    #else
    return String();
    #endif

    char buffer[2 * HASH_LEN + 1];
    for (size_t index = 0; index < HASH_LEN; index++) {
        uint8_t nibble = (sha_256[index] & 0xf0) >> 4;
        buffer[2 * index] = nibble < 10 ? char(nibble + '0')
                                         : char(nibble - 10 + 'A');
        nibble = sha_256[index] & 0x0f;
        buffer[2 * index + 1] = nibble < 10 ? char(nibble + '0')
                                             : char(nibble - 10 + 'A');
    }
    buffer[2 * HASH_LEN] = '\0';
    return String(buffer);
}

// ============================================================
//  handleUpdate : traitement principal
// ============================================================
HTTPUpdateResult HTTPUpdate::handleUpdate(HTTPClient& http,
                                           const String& currentVersion,
                                           bool spiffs)
{
    HTTPUpdateResult ret = HTTP_UPDATE_FAILED;

    // HTTP/1.0 obligatoire pour l'OTA (pas de chunked encoding)
    http.useHTTP10(true);
    http.setTimeout(_httpClientTimeout);
    http.setFollowRedirects(_followRedirects);
    http.setUserAgent("ESP8266-http-Update");
    http.addHeader("Cache-Control", "no-cache");

    // Headers d'info
    http.addHeader("x-ESP8266-STA-MAC", WiFi.macAddress());
    http.addHeader("x-ESP8266-free-space", String(ESP.getFreeSketchSpace()));
    http.addHeader("x-ESP8266-sketch-size", String(ESP.getSketchSize()));
    http.addHeader("x-ESP8266-chip-size", String(ESP.getFlashChipSize()));
    http.addHeader("x-ESP8266-sdk-version", ESP.getSdkVersion());

    // SHA256 (si disponible)
    String sketchSHA256 = getSketchSHA256();
    if (sketchSHA256.length() != 0) {
        http.addHeader("x-ESP8266-sketch-sha256", sketchSHA256);
    }

    // Mode
    if (spiffs) {
        http.addHeader("x-ESP8266-mode", "spiffs");
    } else {
        http.addHeader("x-ESP8266-mode", "sketch");
    }

    // Version courante
    if (currentVersion && currentVersion[0] != 0x00) {
        http.addHeader("x-ESP8266-version", currentVersion);
    }

    // Headers a collecter
    const char * headerkeys[] = { "x-MD5" };
    size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
    http.collectHeaders(headerkeys, headerkeyssize);

    // --- Requete GET ---
    int code = http.GET();
    int len = http.getSize();

    if (code <= 0) {
        log_e("HTTP error: %s\n", http.errorToString(code).c_str());
        _lastError = code;
        http.end();
        return HTTP_UPDATE_FAILED;
    }

    log_d("Header read fin.\n");
    log_d("Server header:\n");
    log_d(" - code: %d\n", code);
    log_d(" - len: %d\n", len);

    if (http.hasHeader("x-MD5")) {
        log_d(" - MD5: %s\n", http.header("x-MD5").c_str());
    }

    log_d("ESP info:\n");
    log_d(" - free Space: %d\n", ESP.getFreeSketchSpace());
    log_d(" - current Sketch Size: %d\n", ESP.getSketchSize());

    if (currentVersion && currentVersion[0] != 0x00) {
        log_d(" - current version: %s\n", currentVersion.c_str());
    }

    switch (code) {
    case HTTP_CODE_OK:
        if (len > 0) {
            bool startUpdate = true;

            if (spiffs) {
                const esp_partition_t* _partition = esp_partition_find_first(
                    ESP_PARTITION_TYPE_DATA,
                    ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                    NULL);
                if (!_partition) {
                    _lastError = HTTP_UE_NO_PARTITION;
                    http.end();
                    return HTTP_UPDATE_FAILED;
                }
                if (len > (int)_partition->size) {
                    log_e("spiffsSize too low (%d) needed: %d\n",
                          _partition->size, len);
                    startUpdate = false;
                }
            } else {
                int sketchFreeSpace = ESP.getFreeSketchSpace();
                if (!sketchFreeSpace) {
                    _lastError = HTTP_UE_NO_PARTITION;
                    http.end();
                    return HTTP_UPDATE_FAILED;
                }
                if (len > sketchFreeSpace) {
                    log_e("FreeSketchSpace too low (%d) needed: %d\n",
                          sketchFreeSpace, len);
                    startUpdate = false;
                }
            }

            if (!startUpdate) {
                _lastError = HTTP_UE_TOO_LESS_SPACE;
                ret = HTTP_UPDATE_FAILED;
            } else {
                WiFiClient * tcp = http.getStreamPtr();

                delay(100);

                int command;
                if (spiffs) {
                    command = U_SPIFFS;
                    log_d("runUpdate spiffs...\n");
                } else {
                    command = U_FLASH;
                    log_d("runUpdate flash...\n");
                }

                // Verif magic byte pour firmware
                if (!spiffs) {
                    if (tcp->peek() != 0xE9) {
                        log_e("Magic header does not start with 0xE9\n");
                        _lastError = HTTP_UE_BIN_VERIFY_HEADER_FAILED;
                        http.end();
                        return HTTP_UPDATE_FAILED;
                    }
                }

                if (runUpdate(*tcp, len, http.header("x-MD5"), command)) {
                    ret = HTTP_UPDATE_OK;
                    log_d("Update ok\n");
                    http.end();

                    if (_rebootOnUpdate && !spiffs) {
                        ESP.restart();
                    }
                } else {
                    ret = HTTP_UPDATE_FAILED;
                    log_e("Update failed\n");
                }
            }
        } else {
            _lastError = HTTP_UE_SERVER_NOT_REPORT_SIZE;
            ret = HTTP_UPDATE_FAILED;
            log_e("Content-Length was 0 or wasn't set by Server?!\n");
        }
        break;

    case HTTP_CODE_NOT_MODIFIED:
        ret = HTTP_UPDATE_NO_UPDATES;
        break;

    case HTTP_CODE_NOT_FOUND:
        _lastError = HTTP_UE_SERVER_FILE_NOT_FOUND;
        ret = HTTP_UPDATE_FAILED;
        break;

    case HTTP_CODE_FORBIDDEN:
        _lastError = HTTP_UE_SERVER_FORBIDDEN;
        ret = HTTP_UPDATE_FAILED;
        break;

    default:
        _lastError = HTTP_UE_SERVER_WRONG_HTTP_CODE;
        ret = HTTP_UPDATE_FAILED;
        log_e("HTTP Code is (%d)\n", code);
        break;
    }

    http.end();
    return ret;
}

// ============================================================
//  runUpdate : ecriture en flash
// ============================================================
bool HTTPUpdate::runUpdate(Stream& in, uint32_t size, String md5, int command)
{
    StreamString error;

    if (!Update.begin(size, command, _ledPin, _ledOn)) {
        _lastError = Update.getError();
        Update.printError(error);
        error.trim();
        log_e("Update.begin failed! (%s)\n", error.c_str());
        return false;
    }

    if (md5.length()) {
        if (!Update.setMD5(md5.c_str())) {
            _lastError = HTTP_UE_SERVER_FAULTY_MD5;
            log_e("Update.setMD5 failed! (%s)\n", md5.c_str());
            return false;
        }
    }

    if (Update.writeStream(in) != size) {
        _lastError = Update.getError();
        Update.printError(error);
        error.trim();
        log_e("Update.writeStream failed! (%s)\n", error.c_str());
        return false;
    }

    if (!Update.end()) {
        _lastError = Update.getError();
        Update.printError(error);
        error.trim();
        log_e("Update.end failed! (%s)\n", error.c_str());
        return false;
    }

    return true;
}

// ============================================================
//  Instance globale
// ============================================================
#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_HTTPUPDATE)
HTTPUpdate httpUpdate;
#endif