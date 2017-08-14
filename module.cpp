#include "module.h"
#include "types.h"

reg32 Module::_s_scgc_base = (reg32)0x40048028;
reg32 Module::_s_scgc1 = _s_scgc_base;
reg32 Module::_s_scgc2 = _s_scgc_base + 1;
reg32 Module::_s_scgc3 = _s_scgc_base + 2;
reg32 Module::_s_scgc4 = _s_scgc_base + 3;
reg32 Module::_s_scgc5 = _s_scgc_base + 4;
reg32 Module::_s_scgc6 = _s_scgc_base + 5;
reg32 Module::_s_scgc7 = _s_scgc_base + 6;

mod_enable_s const Module::_s_enable_map[MOD_DMA+1] =
{
    mod_enable_s{_s_scgc2, 12},  // DAC0

    mod_enable_s{_s_scgc3, 24},  // FTM2
    mod_enable_s{_s_scgc3, 27},  // ADC1

    mod_enable_s{_s_scgc4,  1},  // EWM
    mod_enable_s{_s_scgc4,  2},  // CMT
    mod_enable_s{_s_scgc4,  6},  // I2C0
    mod_enable_s{_s_scgc4,  7},  // I2C1
    mod_enable_s{_s_scgc4, 10},  // UART0
    mod_enable_s{_s_scgc4, 11},  // UART1
    mod_enable_s{_s_scgc4, 12},  // UART2
    mod_enable_s{_s_scgc4, 18},  // USBOTG
    mod_enable_s{_s_scgc4, 19},  // CMP
    mod_enable_s{_s_scgc4, 20},  // VREF

    mod_enable_s{_s_scgc5,  0},  // LPTIMER
    mod_enable_s{_s_scgc5,  5},  // TSI
    mod_enable_s{_s_scgc5,  9},  // PORTA
    mod_enable_s{_s_scgc5, 10},  // PORTB
    mod_enable_s{_s_scgc5, 11},  // PORTC
    mod_enable_s{_s_scgc5, 12},  // PORTD
    mod_enable_s{_s_scgc5, 13},  // PORTE

    mod_enable_s{_s_scgc6,  0},  // FTFL
    mod_enable_s{_s_scgc6,  1},  // DMAMUX
    mod_enable_s{_s_scgc6,  4},  // FLEXCAN0
    mod_enable_s{_s_scgc6, 12},  // SPI0
    mod_enable_s{_s_scgc6, 13},  // SPI1
    mod_enable_s{_s_scgc6, 15},  // I2S
    mod_enable_s{_s_scgc6, 18},  // CRC
    mod_enable_s{_s_scgc6, 21},  // USBDCD
    mod_enable_s{_s_scgc6, 22},  // PDB
    mod_enable_s{_s_scgc6, 23},  // PIT
    mod_enable_s{_s_scgc6, 24},  // FTM0
    mod_enable_s{_s_scgc6, 25},  // FTM1
    mod_enable_s{_s_scgc6, 27},  // ADC0
    mod_enable_s{_s_scgc6, 29},  // RTC

    mod_enable_s{_s_scgc7,  1},  // DMA
};

