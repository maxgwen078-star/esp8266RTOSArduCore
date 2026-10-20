#ifndef __ARDUINO_OTA_RTOS_H
#define __ARDUINO_OTA_RTOS_H
#include <WiFi.h>
#include <WiFiUdp.h>
#include <functional>
typedef enum { OTA_IDLE, OTA_WAITAUTH, OTA_RUNUPDATE } ota_state_t;
typedef enum { OTA_AUTH_ERROR, OTA_BEGIN_ERROR, OTA_CONNECT_ERROR, OTA_RECEIVE_ERROR, OTA_END_ERROR } ota_error_t;
class ArduinoOTAClass {
 public:
  typedef std::function<void(void)> THandlerFunction;
  typedef std::function<void(ota_error_t)> THandlerFunction_Error;
  typedef std::function<void(unsigned int, unsigned int)> THandlerFunction_Progress;
  ArduinoOTAClass();
  ~ArduinoOTAClass();
  void setPort(uint16_t port);
  void setHostname(const char* hostname);
  String getHostname();
  void setPassword(const char* password);
  void setPasswordHash(const char* password);
  void setRebootOnSuccess(bool reboot);
  void onStart(THandlerFunction fn);
  void onEnd(THandlerFunction fn);
  void onError(THandlerFunction_Error fn);
  void onProgress(THandlerFunction_Progress fn);
  void begin(bool useMDNS=true);
  void end();
  void handle();
  int getCommand();
 private:
  void _runUpdate();
  void _onRx();
  int parseInt();
  String readStringUntil(char end);
  int _port=8266;
  String _password;
  String _hostname;
  String _nonce;
  WiFiUDP _udp;
  bool _initialized=false;
  bool _rebootOnSuccess=true;
  bool _useMDNS=true;
  ota_state_t _state=OTA_IDLE;
  int _size=0;
  int _cmd=0;
  uint16_t _ota_port=0;
  IPAddress _ota_ip;
  String _md5;
  THandlerFunction _start_callback=nullptr;
  THandlerFunction _end_callback=nullptr;
  THandlerFunction_Error _error_callback=nullptr;
  THandlerFunction_Progress _progress_callback=nullptr;
};
#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_ARDUINOOTA)
extern ArduinoOTAClass ArduinoOTA;
#endif
#define U_FLASH 0
#define U_FS 100
#define U_AUTH 200
#endif
