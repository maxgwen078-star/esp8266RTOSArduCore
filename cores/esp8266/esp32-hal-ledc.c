#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "driver/pwm.h"
#include "driver/adc.h"
#include "esp8266/gpio_register.h"
#include "esp32-hal-gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

static int32_t analogScale = 1023;
static uint32_t analogMap = 0;
static uint16_t analogFreq = 1000;
#define PWM_MAX 8
static uint8_t s_pins[PWM_MAX];
static uint32_t s_hw[PWM_MAX];
static bool s_used[PWM_MAX];
static bool s_inited=false;

static int find_idx(uint8_t pin){ for(int i=0;i<PWM_MAX;i++) if(s_used[i] && s_pins[i]==pin) return i; return -1; }
static int alloc_idx(uint8_t pin){ int f=find_idx(pin); if(f>=0) return f; for(int i=0;i<PWM_MAX;i++) if(!s_used[i]){ s_used[i]=true; s_pins[i]=pin; return i; } return -1; }
static void rebuild(){
    uint32_t duties[PWM_MAX]; uint32_t pins[PWM_MAX]; uint8_t cnt=0;
    for(int i=0;i<PWM_MAX;i++) if(s_used[i]){ duties[cnt]=s_hw[i]; pins[cnt]=s_pins[i]; cnt++; }
    if(!cnt) return;
    pwm_init(analogFreq, duties, cnt, pins);
    pwm_start(); s_inited=true;
}

void analogWriteFreq(uint32_t freq){ if(freq<100) freq=100; if(freq>60000) freq=60000; analogFreq=(uint16_t)freq; if(s_inited) rebuild(); }
void analogWriteRange(uint32_t range){ if(range>=15 && range<=65535) analogScale=(int32_t)range; }
void analogWriteResolution(int res){ if(res>=4 && res<=16) analogScale=(1<<res)-1; }

void analogWriteMode(uint8_t pin, int val, bool openDrain){
    if(pin>16) return;
    if(val<0) val=0; if(val>analogScale) val=analogScale;
    if(val==0){
        int idx=find_idx(pin);
        if(idx>=0){ s_used[idx]=false; analogMap &= ~(1UL<<pin); if(s_inited){ pwm_deinit(); s_inited=false; rebuild(); } }
        pinMode(pin, openDrain?OUTPUT_OPEN_DRAIN:OUTPUT);
        digitalWrite(pin, LOW);
        return;
    }
    if(val>=analogScale){
        int idx=find_idx(pin);
        if(idx>=0){ s_used[idx]=false; analogMap &= ~(1UL<<pin); if(s_inited){ pwm_deinit(); s_inited=false; rebuild(); } }
        pinMode(pin, openDrain?OUTPUT_OPEN_DRAIN:OUTPUT);
        digitalWrite(pin, HIGH);
        return;
    }
    pinMode(pin, openDrain?OUTPUT_OPEN_DRAIN:OUTPUT);
    int idx=alloc_idx(pin);
    if(idx<0) return;
    uint32_t hw = (uint32_t)val*1000UL/(uint32_t)(analogScale?analogScale:1023);
    if(hw>1000) hw=1000;
    s_hw[idx]=hw;
    if(!s_inited || !(analogMap & (1UL<<pin))){
        analogMap|=(1UL<<pin);
        rebuild();
    } else {
        uint8_t hw_idx=0;
        for(int i=0;i<PWM_MAX;i++) if(s_used[i]){ if(i==idx){ pwm_set_duty(hw_idx,hw); pwm_start(); break; } hw_idx++; }
    }
}
void analogWrite(uint8_t pin, int val){ analogWriteMode(pin,val,false); }
int analogRead(uint8_t pin){
    (void)pin;
    uint16_t data=0;
    if(adc_read(&data)==ESP_OK) return (int)data;
    return 0;
}
void analogReadResolution(int bits){ (void)bits; }
void analogSetPinAttenuation(uint8_t pin, uint8_t attenuation){ (void)pin; (void)attenuation; }

void __analogWriteFreq(uint32_t f){ analogWriteFreq(f); }
void __analogWriteRange(uint32_t r){ analogWriteRange(r); }
void __analogWriteResolution(int r){ analogWriteResolution(r); }
void __analogWriteMode(uint8_t p,int v,bool od){ analogWriteMode(p,v,od); }
void __analogWrite(uint8_t p,int v){ analogWrite(p,v); }
int __analogRead(uint8_t p){ return analogRead(p); }

double ledcSetup(uint8_t ch, double freq, uint8_t bits){ (void)ch; analogWriteFreq((uint32_t)freq); analogWriteResolution(bits); return freq; }
void ledcAttachPin(uint8_t pin, uint8_t ch){ (void)ch; pinMode(pin, OUTPUT); }
void ledcWrite(uint8_t ch, uint32_t duty){ (void)ch; (void)duty; }
void ledcDetachPin(uint8_t pin){ int idx=find_idx(pin); if(idx>=0){ s_used[idx]=false; analogMap&=~(1UL<<pin); pwm_deinit(); s_inited=false; rebuild(); } }

#ifdef __cplusplus
}
#endif
