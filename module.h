#ifndef _MODULE_H_
#define _MODULE_H_

#include "types.h"

enum module_e
{
    // scgc2
    MOD_DAC0,

    // scgc3
    MOD_FTM2,
    MOD_ADC1,

    // scgc4
    MOD_EWM,
    MOD_CMT,
    MOD_I2C0,
    MOD_I2C1,
    MOD_UART0,
    MOD_UART1,
    MOD_UART2,
    MOD_USBOTG,
    MOD_CMP,
    MOD_VREF,

    // scgc5
    MOD_LPTIMER,
    MOD_TSI,
    MOD_PORTA,
    MOD_PORTB,
    MOD_PORTC,
    MOD_PORTD,
    MOD_PORTE,

    // scgc6
    MOD_FTFL,
    MOD_DMAMUX,
    MOD_FLEXCAN0,
    MOD_SPI0,
    MOD_SPI1,
    MOD_I2S,
    MOD_CRC,
    MOD_USBDCD,
    MOD_PDB,
    MOD_PIT,
    MOD_FTM0,
    MOD_FTM1,
    MOD_ADC0,
    MOD_RTC,

    // scgc7
    MOD_DMA,
};

struct mod_enable_s
{
    reg32 scgc;
    uint8_t pos;
};

class Module
{
    public:
        static bool enabled(module_e m) { return *_s_enable_map[m].scgc & (1 << _s_enable_map[m].pos); }

    protected:
        static void enable(module_e m) { *_s_enable_map[m].scgc |= (1 << _s_enable_map[m].pos); }
        static void disable(module_e m) { *_s_enable_map[m].scgc &= ~(1 << _s_enable_map[m].pos); }

    private:
        static reg32 _s_scgc_base;
        static reg32 _s_scgc1;
        static reg32 _s_scgc2;
        static reg32 _s_scgc3;
        static reg32 _s_scgc4;
        static reg32 _s_scgc5;
        static reg32 _s_scgc6;
        static reg32 _s_scgc7;

        static mod_enable_s const _s_enable_map[MOD_DMA+1];
};

#endif
