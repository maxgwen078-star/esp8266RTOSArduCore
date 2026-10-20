/*
 Esp.cpp - ESP31B-specific APIs
 Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Arduino.h"
#include "Esp.h"
#include "rom/spi_flash.h"
#include "esp_sleep.h"
#include "esp_spi_flash.h"
#include <memory>
#include "esp8266/eagle_soc.h"
#include "esp_system.h"
#include <esp_partition.h>
extern "C" {
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "esp_heap_caps.h"
#include "esp8266/rom_functions.h"
}
#include <MD5Builder.h>

/**
 * User-defined Literals
 *  usage:
 *
 *   uint32_t = test = 10_MHz; // --> 10000000
 */

unsigned long long operator"" _kHz(unsigned long long x)
{
    return x * 1000;
}

unsigned long long operator"" _MHz(unsigned long long x)
{
    return x * 1000 * 1000;
}

unsigned long long operator"" _GHz(unsigned long long x)
{
    return x * 1000 * 1000 * 1000;
}

unsigned long long operator"" _kBit(unsigned long long x)
{
    return x * 1024;
}

unsigned long long operator"" _MBit(unsigned long long x)
{
    return x * 1024 * 1024;
}

unsigned long long operator"" _GBit(unsigned long long x)
{
    return x * 1024 * 1024 * 1024;
}

unsigned long long operator"" _kB(unsigned long long x)
{
    return x * 1024;
}

unsigned long long operator"" _MB(unsigned long long x)
{
    return x * 1024 * 1024;
}

unsigned long long operator"" _GB(unsigned long long x)
{
    return x * 1024 * 1024 * 1024;
}


EspClass ESP;

void EspClass::deepSleep(uint32_t time_us)
{
    esp_deep_sleep(time_us);
}

void EspClass::restart(void)
{
    esp_restart();
}

/*
#ifdef CONFIG_IDF_TARGET_ESP32
uint32_t EspClass::getHeapSize(void)
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    return info.total_free_bytes + info.total_allocated_bytes;
}

uint32_t EspClass::getMaxAllocHeap(void)
{
    return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
}

uint32_t EspClass::getPsramSize(void)
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    return info.total_free_bytes + info.total_allocated_bytes;
}

uint32_t EspClass::getMaxAllocPsram(void)
{
    return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}

uint32_t EspClass::getFreePsram(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

uint32_t EspClass::getMinFreePsram(void)
{
    return heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
}

#endif
*/


uint32_t EspClass::getFreeHeap(void)
{
    // DRAM seule (MALLOC_CAP_8BIT), même référentiel que getHeapSize()
    return heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

uint32_t EspClass::getMinFreeHeap(void)
{
    // DRAM seule, minimum depuis le boot
    return heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
}


uint32_t EspClass::getHeapSize(void)
{
    extern heap_region_t g_heap_region[];
    extern size_t g_heap_region_num;

    uint32_t total = 0;
    for (size_t i = 0; i < g_heap_region_num; i++) {
        // On ne compte QUE la région accessible en 8-bit (= DRAM, heap malloc)
        // IRAM (MALLOC_CAP_EXEC) est exclue : on ne peut pas y mettre de String
        if (g_heap_region[i].caps & MALLOC_CAP_8BIT)
            total += g_heap_region[i].total_size;
    }
    return total;
}

uint32_t EspClass::getMaxAllocHeap(void)
{
    // Plus grand bloc contigu allouable.
    // Nécessite heap_caps_get_largest_free_block() ajouté au SDK.
    return heap_caps_get_largest_free_block(/*MALLOC_CAP_32BIT*/MALLOC_CAP_8BIT);
}

uint8_t EspClass::getHeapFragmentation(void)
{
    uint32_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint32_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    if (freeHeap == 0) return 0;

    float frag = 100.0f * (1.0f - ((float)maxBlock / (float)freeHeap));
    if (frag < 0.0f)   frag = 0.0f;
    if (frag > 100.0f) frag = 100.0f;

    return (uint8_t)frag;
}

static uint32_t sketchSize(sketchSize_t response) {
    esp_image_metadata_t data;
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) return 0;
    const esp_partition_pos_t running_pos  = {
        .offset = running->address,
        .size = running->size,
    };
    data.start_addr = running_pos.offset;
    esp_image_load(ESP_IMAGE_VERIFY, &running_pos, &data);
    if (response) {
        return running_pos.size - data.image_len;
    } else {
        return data.image_len;
    }
}
    
uint32_t EspClass::getSketchSize () {
    return sketchSize(SKETCH_SIZE_TOTAL);
}

String EspClass::getSketchMD5()
{
    static String result;
    if (result.length()) {
        return result;
    }
    uint32_t lengthLeft = getSketchSize();

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        log_e("Partition could not be found");

        return String();
    }
    const size_t bufSize = SPI_FLASH_SEC_SIZE;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[bufSize]);
    uint32_t offset = 0;
    if(!buf.get()) {
        log_e("Not enough memory to allocate buffer");

        return String();
    }
    MD5Builder md5;
    md5.begin();
    while( lengthLeft > 0) {
        size_t readBytes = (lengthLeft < bufSize) ? lengthLeft : bufSize;
        if (!ESP.flashRead(running->address + offset, reinterpret_cast<uint32_t*>(buf.get()), (readBytes + 3) & ~3)) {
            log_e("Could not read buffer from flash");

            return String();
        }
        md5.add(buf.get(), readBytes);
        lengthLeft -= readBytes;
        offset += readBytes;
    }
    md5.calculate();
    result = md5.toString();
    return result;
}

uint32_t EspClass::getFreeSketchSpace () {
    const esp_partition_t* _partition = esp_ota_get_next_update_partition(NULL);
    if(!_partition){
        return 0;
    }

    return _partition->size;
}

uint32_t EspClass::getChipId(void)
{
    uint32_t chipId = 0;
    uint64_t _chipmacid = 0LL;
    esp_efuse_mac_get_default((uint8_t*)(&_chipmacid));
    
    // Version 32 bits (identique au core Arduino ESP8266)
    chipId = ((_chipmacid >> 16) & 0xFFFFFFFF);
    return chipId;
}

uint8_t EspClass::getChipRevision(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    return chip_info.revision;
}

const char * EspClass::getChipModel(void)
{

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    switch (chip_info.model)
    {
    case CHIP_ESP8266:
        return "ESP8266";
        break;
    case CHIP_ESP32:
        return "ESP32";
        break;
    default:
        break;
    }
    return "Unknown";
}

uint8_t EspClass::getChipCores(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    return chip_info.cores;
}

const char * EspClass::getSdkVersion(void)
{
    return esp_get_idf_version();
}

// ============================================================
//   LECTURE FLASH - ESP8266 RTOS (via g_rom_flashchip)
// ============================================================

uint32_t EspClass::getFlashChipId(void)
{
    // g_rom_flashchip.device_id contient le JEDEC ID complet
    // (manufacturer + device) lu par le bootloader ROM.
    return g_rom_flashchip.device_id;
}

uint8_t EspClass::getFlashChipVendorId(void)
{
    // Le manufacturer est l'octet de poids faible du JEDEC ID
    return (uint8_t)(g_rom_flashchip.device_id & 0xFF);
}

uint32_t EspClass::getFlashChipRealSize(void)
{
    // Taille réelle en octets, lue par le bootloader ROM
    return g_rom_flashchip.chip_size;
}

uint32_t EspClass::getFlashChipSize(void)
{
    // On utilise la taille réelle lue depuis la puce,
    // pas l'en-tête de l'image (qui peut être faux sur RTOS).
    return getFlashChipRealSize();
}

uint32_t EspClass::getFlashChipSpeed(void)
{
    esp_image_header_t fhdr;
    if(flashRead(0x0000, (uint32_t*)&fhdr, sizeof(esp_image_header_t)) && fhdr.magic != ESP_IMAGE_HEADER_MAGIC) {
        return -1;
    }
    return magicFlashChipSpeed(fhdr.spi_speed);
}

FlashMode_t EspClass::getFlashChipMode(void)
{
    esp_image_header_t fhdr;
    if(flashRead(0x0000, (uint32_t*)&fhdr, sizeof(esp_image_header_t)) && fhdr.magic != ESP_IMAGE_HEADER_MAGIC) {
        return FM_UNKNOWN;
    }
    return magicFlashChipMode(fhdr.spi_mode);
}

uint32_t EspClass::magicFlashChipSize(uint8_t byte)
{
    switch(byte & 0x0F) {
    case 0x0: // 8 MBit (1MB)
        return (1_MB);
    case 0x1: // 16 MBit (2MB)
        return (2_MB);
    case 0x2: // 32 MBit (4MB)
        return (4_MB);
    case 0x3: // 64 MBit (8MB)
        return (8_MB);
    case 0x4: // 128 MBit (16MB)
        return (16_MB);
    default: // fail?
        return 0;
    }
}

uint32_t EspClass::magicFlashChipSpeed(uint8_t byte)
{
    switch(byte & 0x0F) {
    case 0x0: // 40 MHz
        return (40_MHz);
    case 0x1: // 26 MHz
        return (26_MHz);
    case 0x2: // 20 MHz
        return (20_MHz);
    case 0xf: // 80 MHz
        return (80_MHz);
    default: // fail?
        return 0;
    }
}

FlashMode_t EspClass::magicFlashChipMode(uint8_t byte)
{
    FlashMode_t mode = (FlashMode_t) byte;
    if(mode > FM_SLOW_READ) {
        mode = FM_UNKNOWN;
    }
    return mode;
}

bool EspClass::flashEraseSector(uint32_t sector)
{
    return spi_flash_erase_sector(sector) == ESP_OK;
}

// Warning: These functions do not work with encrypted flash
bool EspClass::flashWrite(uint32_t offset, uint32_t *data, size_t size)
{
    return spi_flash_write(offset, (uint32_t*) data, size) == ESP_OK;
}

bool EspClass::flashRead(uint32_t offset, uint32_t *data, size_t size)
{
    return spi_flash_read(offset, (uint32_t*) data, size) == ESP_OK;
}

bool EspClass::partitionEraseRange(const esp_partition_t *partition, uint32_t offset, size_t size) 
{
    return esp_partition_erase_range(partition, offset, size) == ESP_OK;
}

bool EspClass::partitionWrite(const esp_partition_t *partition, uint32_t offset, uint32_t *data, size_t size) 
{
    return esp_partition_write(partition, offset, data, size) == ESP_OK;
}

bool EspClass::partitionRead(const esp_partition_t *partition, uint32_t offset, uint32_t *data, size_t size) 
{
    return esp_partition_read(partition, offset, data, size) == ESP_OK;
}

uint64_t EspClass::getEfuseMac(void)
{
    uint64_t _chipmacid = 0LL;
    esp_efuse_mac_get_default((uint8_t*) (&_chipmacid));
    return _chipmacid;
}