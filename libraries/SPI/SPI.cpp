#include "SPI.h"
#if defined(ESP8266_RTOS)
#include "esp8266/eagle_soc.h"
#include "esp8266/spi_struct.h"
extern "C" {
#include "rom/ets_sys.h"
    void _xt_isr_mask(uint32_t);
    void _xt_isr_unmask(uint32_t);
}

SPIClass SPI;
SPIClass::SPIClass(): _frequency(4000000), _dataMode(0), _bitOrder(1), _initialized(false) {}

void SPIClass::begin()
{
    _xt_isr_mask(1<<ETS_SPI_INUM);
    SPI1.slave.val=0;
    SPI1.pin.val=0;
    SPI1.user.val=0;
    SPI1.user1.val=0;
    SPI1.user2.val=0;
    SPI1.ctrl.val=0;
    SPI1.ctrl1.val=0;
    SPI1.clock.val=0;
    SPI1.user.usr_mosi=1;
    SPI1.user.duplex=1;
    SPI1.user.cs_setup=0;
    SPI1.user.cs_hold=0;

    pinMode(14, SPECIAL); // D5 GPIO14 = HSPI_CLK
    pinMode(13, SPECIAL); // D7 GPIO13 = HSPI_MOSI

    _initialized=true;
    setFrequency(_frequency);
}
void SPIClass::end(){ _xt_isr_unmask(1<<ETS_SPI_INUM); }
void SPIClass::setDataBits(uint16_t bits){ SPI1.user1.usr_mosi_bitlen=bits-1; }
void SPIClass::setFrequency(uint32_t freq){
    _frequency=freq; if(!_initialized) return;
    if(freq>=80000000){ SPI1.clock.clk_equ_sysclk=1; SET_PERI_REG_MASK(PERIPHS_IO_MUX_CONF_U,(1<<9)); return; }
    SPI1.clock.clk_equ_sysclk=0; CLEAR_PERI_REG_MASK(PERIPHS_IO_MUX_CONF_U,(1<<9));
    uint32_t pre,n; if(freq>=40000000){ pre=1; n=2; } else if(freq>=20000000){ pre=2; n=2; } else if(freq>=10000000){ pre=4; n=2; } else if(freq>=5000000){ pre=8; n=2; } else { pre=10; n=2; }
    SPI1.clock.clkdiv_pre=pre-1; SPI1.clock.clkcnt_n=n-1; SPI1.clock.clkcnt_h=(n/2)-1; SPI1.clock.clkcnt_l=n-1;
}
void SPIClass::beginTransaction(SPISettings s){ while(SPI1.cmd.usr); setFrequency(s._clock); }
void SPIClass::endTransaction(){}
void SPIClass::setBitOrder(uint8_t b){}
void SPIClass::setDataMode(uint8_t m){}
void SPIClass::setClockDivider(uint32_t d){ if(d) setFrequency(80000000/d); }
void SPIClass::writeBytes(const uint8_t *data,size_t size){
    if(!data||!size) return;
    while(size){
        size_t chunk=size>64?64:size;
        while(SPI1.cmd.usr);
        setDataBits(chunk*8);
        volatile uint32_t *fifo=SPI1.data_buf;
        for(int i=0;i<16;i++) fifo[i]=0;
        if(((uint32_t)data &3)==0){
            const uint32_t *p=(const uint32_t*)data;
            for(size_t i=0;i<(chunk+3)/4;i++) fifo[i]=p[i];
        } else {
            uint8_t tmp[64]={0}; memcpy(tmp,data,chunk);
            const uint32_t *p=(const uint32_t*)tmp;
            for(size_t i=0;i<(chunk+3)/4;i++) fifo[i]=p[i];
        }
        SPI1.cmd.usr=1; while(SPI1.cmd.usr);
        data+=chunk; size-=chunk;
    }
}
uint8_t SPIClass::transfer(uint8_t d){ writeBytes(&d,1); return 0; }
uint16_t SPIClass::transfer16(uint16_t d){ uint8_t b[2]={(uint8_t)(d>>8),(uint8_t)(d&0xFF)}; writeBytes(b,2); return 0; }
void SPIClass::transfer(void *data,uint32_t s){ writeBytes((uint8_t*)data,s); }
size_t SPIClass::write(uint8_t d){ writeBytes(&d,1); return 1; }
size_t SPIClass::write(const uint8_t *d,size_t s){ writeBytes(d,s); return s; }
size_t SPIClass::write16(uint16_t d){ return write16(d,true); }
size_t SPIClass::write16(uint16_t d,bool msb){ uint8_t b[2]={(uint8_t)(msb?d>>8:d&0xFF),(uint8_t)(msb?d&0xFF:d>>8)}; writeBytes(b,2); return 2; }
size_t SPIClass::write32(uint32_t d){ return write32(d,true); }
size_t SPIClass::write32(uint32_t d,bool msb){ uint8_t b[4]; if(msb){ b[0]=d>>24; b[1]=d>>16; b[2]=d>>8; b[3]=d; } else { b[0]=d; b[1]=d>>8; b[2]=d>>16; b[3]=d>>24; } writeBytes(b,4); return 4; }
void SPIClass::writePattern(uint16_t c,size_t n){ while(n--) write16(c); }
#endif
