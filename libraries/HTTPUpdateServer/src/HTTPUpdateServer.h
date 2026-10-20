#ifndef __HTTP_UPDATE_SERVER_H
#define __HTTP_UPDATE_SERVER_H

#include <StreamString.h>
#include <Update.h>
#include <WebServer.h>

#include "esp_partition.h"
#include "esp_ota_ops.h"

// ============================================================
//  Structure : infos sur les partitions disponibles
// ============================================================
struct OtaCapabilities {
    bool has_firmware;      // Partition OTA suivante disponible
    bool has_spiffs;        // Partition SPIFFS disponible
    uint32_t firmware_size; // Taille max firmware
    uint32_t spiffs_size;   // Taille SPIFFS
};

// ============================================================
//  Detection des partitions disponibles
// ============================================================
static OtaCapabilities detectOtaCapabilities() {
    OtaCapabilities caps = { false, false, 0, 0 };

    // Firmware : verifier la prochaine partition OTA
    const esp_partition_t *ota = esp_ota_get_next_update_partition(NULL);
    if (ota) {
        caps.has_firmware = true;
        caps.firmware_size = ota->size;
    }

    // SPIFFS : chercher une partition de type DATA/SPIFFS
    const esp_partition_t *spiffs = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
        NULL);
    if (spiffs) {
        caps.has_spiffs = true;
        caps.spiffs_size = spiffs->size;
    }

    return caps;
}

// ============================================================
//  Page HTML : header (toujours affiche)
// ============================================================
static const char PAGE_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang='fr'>
<head>
    <meta charset='utf-8'>
    <meta name='viewport' content='width=device-width,initial-scale=1'/>
    <title>Mise a jour OTA</title>
    <style>
        body{font-family:Arial;background:#121212;color:#e0e0e0;padding:20px;margin:0}
        .container{max-width:520px;margin:0 auto;background:#1e1e1e;
                   padding:20px;border-radius:8px;border:1px solid #333}
        h1{color:#4fc3f7;margin-top:0}
        h2{color:#ffca28;font-size:15px;margin:20px 0 5px 0}
        hr{border:1px solid #333;margin:20px 0}
        input[type=file]{margin:10px 0;padding:8px;background:#2a2a2a;
                         color:#fff;border:1px solid #444;border-radius:3px;
                         width:100%;box-sizing:border-box}
        input[type=submit]{background:#1565c0;color:#fff;border:none;
                           padding:10px 20px;border-radius:4px;
                           cursor:pointer;font-weight:bold}
        input[type=submit]:hover{background:#1976d2}
        a{color:#4fc3f7;text-decoration:none}
        .info{color:#888;font-size:12px;margin-top:8px}
        .warn{color:#ff9800;font-size:13px;padding:10px;
              background:#2a1a00;border-left:3px solid #ff9800;margin:15px 0}
        .err{color:#ef5350;font-size:13px;padding:10px;
             background:#2a0a0a;border-left:3px solid #ef5350;margin:15px 0}
        .ok{color:#66bb6a;font-size:13px}
    </style>
</head>
<body>
    <div class='container'>
        <h1>Mise a jour OTA</h1>
        <p><a href='/'>&larr; Retour</a></p>
)rawliteral";

// ============================================================
//  Page HTML : formulaire firmware
// ============================================================
static const char PAGE_FORM_FIRMWARE[] PROGMEM = R"rawliteral(
        <hr>
        <h2>Firmware (programme)</h2>
        <form method='POST' action='' enctype='multipart/form-data'>
            <input type='file' accept='.bin,.bin.gz' name='firmware' required>
            <input type='submit' value='Envoyer le firmware'>
        </form>
        <div class='info'>Taille max : {SIZE} Ko</div>
)rawliteral";

// ============================================================
//  Page HTML : formulaire SPIFFS
// ============================================================
static const char PAGE_FORM_SPIFFS[] PROGMEM = R"rawliteral(
        <hr>
        <h2>SPIFFS (systeme de fichiers)</h2>
        <form method='POST' action='' enctype='multipart/form-data'>
            <input type='file' accept='.bin,.bin.gz,.image' name='filesystem'>
            <input type='submit' value='Envoyer le SPIFFS'>
        </form>
        <div class='info'>Taille max : {SIZE} Ko</div>
)rawliteral";

// ============================================================
//  Page HTML : message aucune partition
// ============================================================
static const char PAGE_NO_PARTITION[] PROGMEM = R"rawliteral(
        <div class='err'>
            <b>Erreur :</b> aucune partition disponible pour la mise a jour.<br>
            Verifie la table de partitions dans ton projet.
        </div>
)rawliteral";

// ============================================================
//  Page HTML : message SPIFFS indisponible
// ============================================================
static const char PAGE_NO_SPIFFS[] PROGMEM = R"rawliteral(
        <div class='warn'>
            <b>SPIFFS :</b> partition non disponible.<br>
            La mise a jour du systeme de fichiers est desactivee.
        </div>
)rawliteral";

// ============================================================
//  Page HTML : footer
// ============================================================
static const char PAGE_FOOTER[] PROGMEM = R"rawliteral(
    </div>
</body>
</html>)rawliteral";

// ============================================================
//  Page HTML : succes
// ============================================================
static const char PAGE_SUCCESS[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset='utf-8'>
<meta http-equiv="refresh" content="10;URL=/">
<style>
body{font-family:Arial;background:#121212;color:#e0e0e0;padding:40px;text-align:center}
h2{color:#66bb6a}
a{color:#4fc3f7}
</style>
</head><body>
<h2>&#10004; Mise a jour reussie !</h2>
<p>Redemarrage dans 10 secondes...</p>
<p><a href='/'>Retour</a></p>
</body></html>)rawliteral";

// ============================================================
//  Page HTML : erreur
// ============================================================
static const char PAGE_ERROR_HEAD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset='utf-8'>
<style>
body{font-family:Arial;background:#121212;color:#e0e0e0;padding:40px;text-align:center}
h2{color:#ef5350}
a{color:#4fc3f7}
</style>
</head><body>
<h2>&#10006; Erreur de mise a jour</h2>
<p>
)rawliteral";

static const char PAGE_ERROR_TAIL[] PROGMEM = R"rawliteral(
</p>
<p><a href='/update'>Reessayer</a></p>
</body></html>)rawliteral";

// ============================================================
//  Classe HTTPUpdateServer
// ============================================================
class HTTPUpdateServer {
public:
    HTTPUpdateServer(bool serial_debug = false) {
        _serial_output = serial_debug;
        _server = NULL;
        _username = emptyString;
        _password = emptyString;
        _authenticated = false;
    }

    // --- Setup sans auth ---
    void setup(WebServer *server) {
        setup(server, emptyString, emptyString);
    }

    void setup(WebServer *server, const String& path) {
        setup(server, path, emptyString, emptyString);
    }

    // --- Setup avec auth ---
    void setup(WebServer *server, const String& username, const String& password) {
        setup(server, "/update", username, password);
    }

    // --- Setup complet ---
    void setup(WebServer *server, const String& path,
               const String& username, const String& password) {

        _server = server;
        _username = username;
        _password = password;

        // ============================================================
        //  GET /update -> formulaire adapte aux partitions
        // ============================================================
        _server->on(path.c_str(), HTTP_GET, [&]() {
            if (_username != emptyString && _password != emptyString
                && !_server->authenticate(_username.c_str(), _password.c_str())) {
                return _server->requestAuthentication();
            }

            // Detecter les partitions disponibles
            OtaCapabilities caps = detectOtaCapabilities();

            _server->setContentLength(CONTENT_LENGTH_UNKNOWN);
            _server->send(200, "text/html", "");
            _server->sendContent_P(PAGE_HEADER);

            // --- Firmware disponible ? ---
            if (caps.has_firmware) {
                // On doit remplacer {SIZE} par la taille reelle
                // donc on envoie morceau par morceau
                String s = FPSTR(PAGE_FORM_FIRMWARE);
                s.replace("{SIZE}", String(caps.firmware_size / 1024));
                _server->sendContent(s);
            } else {
                _server->sendContent_P(PAGE_NO_PARTITION);
            }

            // --- SPIFFS disponible ? ---
            if (caps.has_spiffs) {
                String s = FPSTR(PAGE_FORM_SPIFFS);
                s.replace("{SIZE}", String(caps.spiffs_size / 1024));
                _server->sendContent(s);
            } else {
                _server->sendContent_P(PAGE_NO_SPIFFS);
            }

            _server->sendContent_P(PAGE_FOOTER);
        });

        // ============================================================
        //  POST /update -> upload
        // ============================================================
        _server->on(path.c_str(), HTTP_POST,
            [&]() {
                // --- Handler de fin d'upload ---
                if (!_authenticated) {
                    return _server->requestAuthentication();
                }

                if (Update.hasError()) {
                    // Envoyer une page d'erreur propre
                    _server->setContentLength(CONTENT_LENGTH_UNKNOWN);
                    _server->send(200, "text/html", "");
                    _server->sendContent_P(PAGE_ERROR_HEAD);
                    _server->sendContent(_updaterError.length() ? _updaterError : "Erreur inconnue");
                    _server->sendContent_P(PAGE_ERROR_TAIL);
                } else {
                    _server->client().setNoDelay(true);
                    _server->send_P(200, PSTR("text/html"), PAGE_SUCCESS);
                    delay(100);
                    _server->client().stop();
                    ESP.restart();
                }
            },
            [&]() {
                // --- Handler de reception des chunks ---
                HTTPUpload& upload = _server->upload();

                if (upload.status == UPLOAD_FILE_START) {
                    _updaterError.clear();
                    if (_serial_output) Serial.setDebugOutput(true);

                    _authenticated = (_username == emptyString
                                   || _password == emptyString
                                   || _server->authenticate(_username.c_str(),
                                                            _password.c_str()));
                    if (!_authenticated) {
                        if (_serial_output) Serial.println(F("[OTA] Non authentifie"));
                        return;
                    }

                    if (_serial_output) {
                        Serial.printf("[OTA] Debut : %s (champ=%s)\n",
                                      upload.filename.c_str(),
                                      upload.name.c_str());
                    }

                    // ====================================================
                    //  Cas 1 : SPIFFS
                    // ====================================================
                    if (upload.name == "filesystem") {
                        const esp_partition_t *spiffs = esp_partition_find_first(
                            ESP_PARTITION_TYPE_DATA,
                            ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                            NULL);

                        if (!spiffs) {
                            _updaterError = "Partition SPIFFS non disponible";
                            if (_serial_output) Serial.println(F("[OTA] ERREUR: pas de SPIFFS"));
                            return;   // Ne PAS appeler Update.begin
                        }

                        if (_serial_output) {
                            Serial.printf("[OTA] SPIFFS cible : %s (%u Ko)\n",
                                          spiffs->label, spiffs->size / 1024);
                        }

                        if (!Update.begin(spiffs->size, U_SPIFFS)) {
                            _setUpdaterError();
                        }
                    }
                    // ====================================================
                    //  Cas 2 : Firmware
                    // ====================================================
                    else {
                        const esp_partition_t *ota = esp_ota_get_next_update_partition(NULL);

                        if (!ota) {
                            _updaterError = "Partition OTA non disponible";
                            if (_serial_output) Serial.println(F("[OTA] ERREUR: pas de partition OTA"));
                            return;
                        }

                        if (_serial_output) {
                            Serial.printf("[OTA] OTA cible : %s (%u Ko)\n",
                                          ota->label, ota->size / 1024);
                        }

                        if (!Update.begin(ota->size, U_FLASH)) {
                            _setUpdaterError();
                        }
                    }
                }
                else if (_authenticated && upload.status == UPLOAD_FILE_WRITE
                         && !_updaterError.length()) {
                    if (_serial_output) Serial.print(F("."));
                    if (Update.write(upload.buf, upload.currentSize)
                        != upload.currentSize) {
                        _setUpdaterError();
                    }
                }
                else if (_authenticated && upload.status == UPLOAD_FILE_END
                         && !_updaterError.length()) {
                    if (Update.end(true)) {
                        if (_serial_output) {
                            Serial.printf("\n[OTA] MAJ OK : %u octets\n[OTA] Redemarrage...\n",
                                          upload.totalSize);
                        }
                    } else {
                        _setUpdaterError();
                    }
                    if (_serial_output) Serial.setDebugOutput(false);
                }
                else if (_authenticated && upload.status == UPLOAD_FILE_ABORTED) {
                    Update.end();
                    if (_serial_output) Serial.println(F("[OTA] Upload annule"));
                }
                delay(0);
            });
    }

    void updateCredentials(const String& username, const String& password) {
        _username = username;
        _password = password;
    }

protected:
    void _setUpdaterError() {
        if (_serial_output) Update.printError(Serial);
        StreamString str;
        Update.printError(str);
        _updaterError = str.c_str();
    }

private:
    bool _serial_output;
    WebServer *_server;
    String _username;
    String _password;
    bool _authenticated;
    String _updaterError;
};

#endif