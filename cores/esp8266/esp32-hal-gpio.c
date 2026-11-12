// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp32-hal-gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "esp_attr.h"

#include "rom/gpio.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

//add me ptach
#include "esp8266/eagle_soc.h"
#include "esp8266/pin_mux_register.h"
#include "esp8266/gpio_register.h"


typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void*);
/*
extern void IRAM_ATTR __pinMode(uint8_t pin, uint8_t mode)
{
    switch (mode)
    {
    case INPUT:
        gpio_set_direction(pin,GPIO_MODE_INPUT);
        break;
    
    case INPUT_PULLUP:
        gpio_set_direction(pin,GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin,GPIO_PULLUP_ONLY);
        gpio_pullup_en(pin);
    break;

    case OUTPUT:
        gpio_set_direction(pin,GPIO_MODE_OUTPUT);
        break;

    case OUTPUT_OPEN_DRAIN:
        gpio_set_direction(pin,GPIO_MODE_OUTPUT_OD);
        break;

    case INPUT_PULLDOWN_16:
        gpio_set_direction(pin,GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin,GPIO_PULLDOWN_ONLY);
        gpio_pulldown_en(pin);        
        break;

    default:
        break;
    }
 
}
*/
void IRAM_ATTR __pinMode(uint8_t pin, uint8_t mode)
{
    if(pin > 16) return;
    const uint32_t mux[16] = {
        PERIPHS_IO_MUX_GPIO0_U, PERIPHS_IO_MUX_U0TXD_U, PERIPHS_IO_MUX_GPIO2_U, PERIPHS_IO_MUX_U0RXD_U,
        PERIPHS_IO_MUX_GPIO4_U, PERIPHS_IO_MUX_GPIO5_U, PERIPHS_IO_MUX_SD_CLK_U, PERIPHS_IO_MUX_SD_DATA0_U,
        PERIPHS_IO_MUX_SD_DATA1_U, PERIPHS_IO_MUX_SD_DATA2_U, PERIPHS_IO_MUX_SD_DATA3_U, PERIPHS_IO_MUX_SD_CMD_U,
        PERIPHS_IO_MUX_MTDI_U, PERIPHS_IO_MUX_MTCK_U, PERIPHS_IO_MUX_MTMS_U, PERIPHS_IO_MUX_MTDO_U
    };
    const uint32_t func_gpio[16] = {
        FUNC_GPIO0,FUNC_GPIO1,FUNC_GPIO2,FUNC_GPIO3,FUNC_GPIO4,FUNC_GPIO5,
        FUNC_GPIO6,FUNC_GPIO7,FUNC_GPIO8,FUNC_GPIO9,FUNC_GPIO10,FUNC_GPIO11,
        FUNC_GPIO12,FUNC_GPIO13,FUNC_GPIO14,FUNC_GPIO15
    };

    if(mode == SPECIAL){
        switch(pin){
            case 14: PIN_FUNC_SELECT(mux[pin], FUNC_HSPI_CLK); PIN_PULLUP_EN(mux[pin]); break;
            case 13: PIN_FUNC_SELECT(mux[pin], FUNC_HSPID_MOSI); PIN_PULLUP_EN(mux[pin]); break;
            case 12: PIN_FUNC_SELECT(mux[pin], FUNC_HSPIQ_MISO); PIN_PULLUP_EN(mux[pin]); break;
            case 15: PIN_FUNC_SELECT(mux[pin], FUNC_HSPI_CS0); PIN_PULLUP_EN(mux[pin]); break;
            default: PIN_FUNC_SELECT(mux[pin], func_gpio[pin]); break;
        }
    } else {
        if(pin < 16){
            PIN_FUNC_SELECT(mux[pin], func_gpio[pin]);
            if(pin == 15){
                CLEAR_PERI_REG_MASK(mux[pin], PERIPHS_IO_MUX_PULLDWN);
                CLEAR_PERI_REG_MASK(mux[pin], PERIPHS_IO_MUX_PULLUP);
            } else {
                CLEAR_PERI_REG_MASK(mux[pin], PERIPHS_IO_MUX_PULLDWN);
            }
        }
    }

    switch(mode){
        case INPUT:
            gpio_set_direction(pin, GPIO_MODE_INPUT);
            break;
        case INPUT_PULLUP:
            gpio_set_direction(pin, GPIO_MODE_INPUT);
            gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
            gpio_pullup_en(pin);
            break;
        case OUTPUT:
            gpio_set_direction(pin, GPIO_MODE_OUTPUT);
            break;
        case OUTPUT_OPEN_DRAIN:
            gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
            break;
        case INPUT_PULLDOWN_16:
            gpio_set_direction(pin, GPIO_MODE_INPUT);
            gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);
            gpio_pulldown_en(pin);
            break;
        case SPECIAL:
            gpio_set_direction(pin, GPIO_MODE_OUTPUT);
            break;
        default: break;
    }
}


extern void IRAM_ATTR __digitalWrite(uint8_t pin, uint8_t val)
{
    gpio_set_level(pin,val);
}

extern int IRAM_ATTR __digitalRead(uint8_t pin)
{
    return gpio_get_level(pin);
}


extern void cleanupFunctional(void* arg);

extern void __attachInterruptFunctionalArg(uint8_t pin, voidFuncPtrArg userFunc, void * arg, int intr_type, bool functional)
{
    gpio_set_intr_type(pin,intr_type);
    gpio_install_isr_service(0); // necesary to install irs service, "0" is dummy value, only used in esp32
    gpio_isr_handler_add(pin, userFunc, arg);
}

extern void __attachInterruptArg(uint8_t pin, voidFuncPtrArg userFunc, void * arg, int intr_type)
{
	__attachInterruptFunctionalArg(pin, userFunc, arg, intr_type, false);
}

extern void __attachInterrupt(uint8_t pin, voidFuncPtr userFunc, int intr_type) {
    __attachInterruptFunctionalArg(pin, (voidFuncPtrArg)userFunc, NULL, intr_type, false);
}

extern void __detachInterrupt(uint8_t pin)
{
    gpio_isr_handler_remove(pin);
}


extern void pinMode(uint8_t pin, uint8_t mode) __attribute__ ((weak, alias("__pinMode")));
extern void digitalWrite(uint8_t pin, uint8_t val) __attribute__ ((weak, alias("__digitalWrite")));
extern int digitalRead(uint8_t pin) __attribute__ ((weak, alias("__digitalRead")));
extern void attachInterrupt(uint8_t pin, voidFuncPtr handler, int mode) __attribute__ ((weak, alias("__attachInterrupt")));
extern void attachInterruptArg(uint8_t pin, voidFuncPtrArg handler, void * arg, int mode) __attribute__ ((weak, alias("__attachInterruptArg")));
extern void detachInterrupt(uint8_t pin) __attribute__ ((weak, alias("__detachInterrupt")));

