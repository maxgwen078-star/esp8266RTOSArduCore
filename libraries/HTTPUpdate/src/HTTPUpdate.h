/**
 * HTTPUpdate.h - Mise a jour OTA via HTTP
 * Adapte pour ESP8266 RTOS Arduino
 */

#ifndef ___HTTP_UPDATE_H___
#define ___HTTP_UPDATE_H___

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <Update.h>

// ============================================================
//  Codes d'erreur specifiques
// ============================================================
#define HTTP_UE_TOO_LESS_SPACE              (-100)
#define HTTP_UE_SERVER_NOT_REPORT_SIZE      (-101)
#define HTTP_UE_SERVER_FILE_NOT_FOUND       (-102)
#define HTTP_UE_SERVER_FORBIDDEN            (-103)
#define HTTP_UE_SERVER_WRONG_HTTP_CODE      (-104)
#define HTTP_UE_SERVER_FAULTY_MD5           (-105)
#define HTTP_UE_BIN_VERIFY_HEADER_FAILED    (-106)
#define HTTP_UE_BIN_FOR_WRONG_FLASH         (-107)
#define HTTP_UE_NO_PARTITION                (-108)

enum HTTPUpdateResult {
    HTTP_UPDATE_FAILED,
    HTTP_UPDATE_NO_UPDATES,
    HTTP_UPDATE_OK
};

typedef HTTPUpdateResult t_httpUpdate_return;

class HTTPUpdate
{
public:
    HTTPUpdate(void);
    HTTPUpdate(int httpClientTimeout);
    ~HTTPUpdate(void);

    void rebootOnUpdate(bool reboot) {
        _rebootOnUpdate = reboot;
    }

    void setFollowRedirects(followRedirects_t follow) {
        _followRedirects = follow;
    }

    void setLedPin(int ledPin = -1, uint8_t ledOn = HIGH) {
        _ledPin = ledPin;
        _ledOn = ledOn;
    }

    t_httpUpdate_return update(WiFiClient& client, const String& url,
                                const String& currentVersion = "");

    t_httpUpdate_return update(WiFiClient& client, const String& host,
                                uint16_t port, const String& uri = "/",
                                const String& currentVersion = "");

    t_httpUpdate_return updateSpiffs(WiFiClient& client, const String& url,
                                      const String& currentVersion = "");

    t_httpUpdate_return update(HTTPClient& httpClient,
                                const String& currentVersion = "");

    t_httpUpdate_return updateSpiffs(HTTPClient &httpClient,
                                      const String &currentVersion = "");

    int getLastError(void);
    String getLastErrorString(void);

protected:
    t_httpUpdate_return handleUpdate(HTTPClient& http,
                                      const String& currentVersion,
                                      bool spiffs = false);
    bool runUpdate(Stream& in, uint32_t size, String md5,
                   int command = U_FLASH);

    int _lastError;
    bool _rebootOnUpdate = true;

private:
    int _httpClientTimeout;
    followRedirects_t _followRedirects;
    int _ledPin;
    uint8_t _ledOn;
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_HTTPUPDATE)
extern HTTPUpdate httpUpdate;
#endif

#endif /* ___HTTP_UPDATE_H___ */