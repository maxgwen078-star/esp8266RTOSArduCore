#include "ArduinoOTA.h"
#include <ESPmDNS.h>
#include <MD5Builder.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

ArduinoOTAClass::ArduinoOTAClass(){}
ArduinoOTAClass::~ArduinoOTAClass(){ end(); }
void ArduinoOTAClass::onStart(THandlerFunction fn){ _start_callback=fn; }
void ArduinoOTAClass::onEnd(THandlerFunction fn){ _end_callback=fn; }
void ArduinoOTAClass::onProgress(THandlerFunction_Progress fn){ _progress_callback=fn; }
void ArduinoOTAClass::onError(THandlerFunction_Error fn){ _error_callback=fn; }
void ArduinoOTAClass::setPort(uint16_t port){ if(!_initialized && port) _port=port; }
void ArduinoOTAClass::setHostname(const char* hostname){ if(!_initialized && hostname) _hostname=hostname; }
String ArduinoOTAClass::getHostname(){ return _hostname; }

// ---------------------------------------------------------------------------
// setPassword:
//   - if password is NULL or empty -> disable authentication (like NONOS core)
//   - else -> store MD5(password) as the shared secret
// ---------------------------------------------------------------------------
void ArduinoOTAClass::setPassword(const char* password){
  if(!_initialized && password){
    if(strlen(password) == 0){
      _password = "";           // empty = no auth (matches NONOS core behavior)
    } else {
      MD5Builder md5; md5.begin(); md5.add(password); md5.calculate(); _password=md5.toString();
    }
  }
}

// ---------------------------------------------------------------------------
// setPasswordHash:
//   - same logic: empty string disables auth
// ---------------------------------------------------------------------------
void ArduinoOTAClass::setPasswordHash(const char* password){
  if(!_initialized && password){
    if(strlen(password) == 0){
      _password = "";
    } else {
      _password = password;
    }
  }
}

void ArduinoOTAClass::setRebootOnSuccess(bool reboot){ _rebootOnSuccess=reboot; }

void ArduinoOTAClass::begin(bool useMDNS){
  if(_initialized) return;
  _useMDNS=useMDNS;
  if(!_hostname.length()){
    char tmp[20];
    uint64_t mac = ESP.getEfuseMac();
    sprintf(tmp,"esp8266-%04x", (uint16_t)(mac & 0xFFFF));
    _hostname=tmp;
  }
  if(!_port) _port=8266;
  Serial.printf("[ArduinoOTA] begin port %u host %s\n", _port, _hostname.c_str());
  if(!_udp.begin(_port)){
    Serial.printf("[ArduinoOTA] UDP begin FAILED port %u\n", _port);
    return;
  }
  Serial.printf("[ArduinoOTA] UDP listening on %u\n", _port);
  if(_useMDNS){
    // MDNS already started in web_task, don't re-begin, just add service
    MDNS.addService("arduino","tcp",_port);
    MDNS.addServiceTxt("arduino","tcp","board","esp8266");
    MDNS.addServiceTxt("arduino","tcp","auth_upload", _password.length() ? "yes" : "no");
  }
  _initialized=true; _state=OTA_IDLE;
  Serial.printf("[ArduinoOTA] READY at %s.local:%u\n", _hostname.c_str(), _port);
}
void ArduinoOTAClass::end(){ _initialized=false; _udp.stop(); _state=OTA_IDLE; }

int ArduinoOTAClass::parseInt(){
  char data[16]={0}; uint8_t idx=0;
  while(_udp.peek()==' ') _udp.read();
  for(idx=0; idx<sizeof(data)-1; ++idx){
    int v=_udp.peek(); if(v<'0'||v>'9'){ data[idx]='\0'; return atoi(data); }
    data[idx]=_udp.read();
  }
  return 0;
}
String ArduinoOTAClass::readStringUntil(char end){
  String res; int v;
  while(true){ v=_udp.read(); if(v<0||v=='\0'||v==end) return res; res+=(char)v; }
}
void ArduinoOTAClass::_onRx(){
  Serial.printf("[ArduinoOTA] _onRx state %d packet %d bytes\n", _state, _udp.parsePacket()?0:0);
  // Actually packet already parsed in handle, but we re-parse size? Keep original logic
  if(_state==OTA_IDLE){
    int cmd=parseInt(); if(cmd!=U_FLASH && cmd!=U_FS){ Serial.printf("[ArduinoOTA] bad cmd %d\n", cmd); return; }
    _ota_ip=_udp.remoteIP(); _cmd=cmd; _ota_port=parseInt(); uint16_t udp_port=_udp.remotePort(); _size=parseInt(); _udp.read(); _md5=readStringUntil('\n'); _md5.trim(); 
    Serial.printf("[ArduinoOTA] Invitation cmd %d port %u size %d md5 %s from %s:%u\n", _cmd, _ota_port, _size, _md5.c_str(), _ota_ip.toString().c_str(), udp_port);
    if(_md5.length()!=32){ Serial.println("[ArduinoOTA] bad md5 len"); return; }
    if(_password.length()){
      MD5Builder m; m.begin(); m.add(String(micros())); m.calculate(); _nonce=m.toString();
      char auth[64]; sprintf(auth,"AUTH %s", _nonce.c_str());
      _udp.beginPacket(_ota_ip, udp_port); _udp.write((const uint8_t*)auth, strlen(auth)); _udp.endPacket();
      Serial.printf("[ArduinoOTA] AUTH challenge sent %s\n", auth);
      _state=OTA_WAITAUTH; return;
    } else { 
      _udp.beginPacket(_ota_ip, udp_port); _udp.write((const uint8_t*)"OK",2); _udp.endPacket();
      Serial.println("[ArduinoOTA] OK sent, entering RUNUPDATE");
      _state=OTA_RUNUPDATE; 
    }
  } else if(_state==OTA_WAITAUTH){
    int cmd=parseInt(); if(cmd!=U_AUTH){ _state=OTA_IDLE; return; }
    _udp.read(); String cnonce=readStringUntil(' '); String response=readStringUntil('\n');
    if(cnonce.length()!=32||response.length()!=32){ _state=OTA_IDLE; return; }
    String challenge=_password+":"+_nonce+":"+cnonce;
    MD5Builder md5; md5.begin(); md5.add(challenge); md5.calculate(); String result=md5.toString();
    if(result.equalsConstantTime(response)){
      _udp.beginPacket(_ota_ip, _udp.remotePort()); _udp.write((const uint8_t*)"OK",2); _udp.endPacket();
      _state=OTA_RUNUPDATE;
    } else {
      _udp.beginPacket(_ota_ip, _udp.remotePort()); _udp.write((const uint8_t*)"Authentication Failed",21); _udp.endPacket();
      if(_error_callback) _error_callback(OTA_AUTH_ERROR); _state=OTA_IDLE;
    }
  }
}
void ArduinoOTAClass::_runUpdate(){
  IPAddress ota_ip=_ota_ip;
  uint16_t ota_port=_ota_port;
  Serial.printf("[ArduinoOTA] _runUpdate connecting to %s:%u size %d\n", ota_ip.toString().c_str(), ota_port, _size);
  const esp_partition_t* part=esp_ota_get_next_update_partition(NULL);
  if(!part){ Serial.println("[ArduinoOTA] no next partition"); if(_error_callback) _error_callback(OTA_BEGIN_ERROR); _state=OTA_IDLE; return; }
  Serial.printf("[ArduinoOTA] next partition %s 0x%x\n", part->label, part->address);
  esp_ota_handle_t handle=0;
  esp_err_t err=esp_ota_begin(part, _size, &handle);
  Serial.printf("[ArduinoOTA] esp_ota_begin err %d handle %u\n", err, handle);
  if(err!=ESP_OK){ if(_error_callback) _error_callback(OTA_BEGIN_ERROR); _state=OTA_IDLE; return; }
  _udp.beginPacket(ota_ip, _udp.remotePort()); _udp.write((const uint8_t*)"OK",2); _udp.endPacket(); delay(100);
  if(_start_callback) _start_callback();
  if(_progress_callback) _progress_callback(0,_size);
  WiFiClient client;
  Serial.printf("[ArduinoOTA] connecting TCP to %s:%u\n", ota_ip.toString().c_str(), ota_port);
  if(!client.connect(ota_ip, ota_port)){ Serial.println("[ArduinoOTA] TCP connect failed"); if(_error_callback) _error_callback(OTA_CONNECT_ERROR); esp_ota_end(handle); _state=OTA_IDLE; return; }
  client.setNoDelay(true);
  uint32_t total=0;
  uint8_t buf[1460];
  while(total<(uint32_t)_size && client.connected()){
    int waited=1000;
    while(!client.available() && waited--) delay(1);
    if(!waited){ Serial.println("[ArduinoOTA] timeout"); if(_error_callback) _error_callback(OTA_RECEIVE_ERROR); esp_ota_end(handle); _state=OTA_IDLE; return; }
    size_t avail=client.available(); if(avail>sizeof(buf)) avail=sizeof(buf);
    size_t r=client.read(buf, avail);
    if(r>0){
      err=esp_ota_write(handle, buf, r);
      if(err!=ESP_OK){ Serial.printf("[ArduinoOTA] write err %d\n", err); if(_error_callback) _error_callback(OTA_RECEIVE_ERROR); esp_ota_end(handle); _state=OTA_IDLE; return; }
      total+=r;
      client.print(r);
      if(_progress_callback) _progress_callback(total,_size);
    }
  }
  err=esp_ota_end(handle);
  Serial.printf("[ArduinoOTA] esp_ota_end %d\n", err);
  if(err==ESP_OK){
    err=esp_ota_set_boot_partition(part);
    Serial.printf("[ArduinoOTA] set_boot %d\n", err);
    if(err==ESP_OK){
      client.print("OK"); delay(500); client.stop();
      if(_end_callback) _end_callback();
      if(_rebootOnSuccess){ delay(100); ESP.restart(); }
    } else {
      if(_error_callback) _error_callback(OTA_END_ERROR);
    }
  } else {
    if(_error_callback) _error_callback(OTA_END_ERROR);
  }
  _state=OTA_IDLE;
}
void ArduinoOTAClass::handle(){
  if(!_initialized) return;
  if(_state==OTA_RUNUPDATE){ _runUpdate(); _state=OTA_IDLE; return; }
  int packetSize=_udp.parsePacket();
  if(packetSize){
    Serial.printf("[ArduinoOTA] UDP packet %d from %s\n", packetSize, _udp.remoteIP().toString().c_str());
    _onRx();
  }
}
int ArduinoOTAClass::getCommand(){ return _cmd; }
#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_ARDUINOOTA)
ArduinoOTAClass ArduinoOTA;
#endif