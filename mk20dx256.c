/*******************************************************************************
 * Teensyduino Core Library
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2017 PJRC.COM, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * 2. If the Software is incorporated into a build system that allows selection
 * among a list of target devices, then similar target devices manufactured by
 * PJRC.COM must be included in the list of target devices and selectable in the
 * same manner.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/

#include <errno.h>
#include <stdint.h>

// Flash Security Setting. On Teensy 3.2, you can lock the MK20 chip to prevent
// anyone from reading your code.  You CAN still reprogram your Teensy while
// security is set, but the bootloader will be unable to respond to auto-reboot
// requests from Arduino. Pressing the program button will cause a full chip
// erase to gain access, because the bootloader chip is locked out.  Normally,
// erase occurs when uploading begins, so if you press the Program button
// accidentally, simply power cycling will run your program again.  When
// security is locked, any Program button press causes immediate full erase.
// Special care must be used with the Program button, because it must be made
// accessible to initiate reprogramming, but it must not be accidentally
// pressed when Teensy Loader is not being used to reprogram.  To set lock the
// security change this to 0xDC.  Teensy 3.0 and 3.1 do not support security lock.
#define FSEC 0xDE

// Flash Options
#define FOPT 0xF9

extern unsigned long _stext;
extern unsigned long _etext;
extern unsigned long _sdata;
extern unsigned long _edata;
extern unsigned long _sbss;
extern unsigned long _ebss;
extern unsigned long _estack;

extern int main(void);

static void start_clocks(void);
void __libc_init_array(void);

void fault_isr(void)
{
#define SIM_SCGC5  (*(volatile uint32_t *)0x40048038)
#define SIM_SCGC5_PORTC  ((uint32_t)0x00000800)
#define PORTC_PCR5  (*(volatile uint32_t *)0x4004B014)
#define PORT_PCR_MUX(n)  ((uint32_t)(((n) & 7) << 8))
#define PORT_PCR_DSE     ((uint32_t)0x00000040)
#define PORT_PCR_SRE     ((uint32_t)0x00000004)
#define GPIOC_PDDR  (*(volatile uint32_t *)0x400FF094)
#define GPIOC_PSOR  (*(volatile uint32_t *)0x400FF084)
#define GPIOC_PTOR  (*(volatile uint32_t *)0x400FF08C)

    // Blink pin 13 LED on the Teensy3
    SIM_SCGC5 |= SIM_SCGC5_PORTC;
    PORTC_PCR5 = PORT_PCR_MUX(1) | PORT_PCR_DSE | PORT_PCR_SRE;
    GPIOC_PDDR |= (1 << 5);

    while (1)
    {
        uint32_t us = 1000000;
        GPIOC_PSOR = (1 << 5);

        for (int i = 0; i < 4; i++)
        {
            us *= F_CPU / 2000000;

            __asm__ volatile
                (
                 "L_%=_delay_usecs:\n\t"
                 "subs %0, #1\n\t"
                 "bne L_%=_delay_usecs\n\t" : "+r" (us)
                );

            us = 100000;
            GPIOC_PTOR = (1 << 5);
        }
    }

    while (1);
}

void unused_isr(void) { fault_isr(); }

// ARM Core System Handler Vectors
void ResetHandler(void);  // Set as the Initial Program Counter
void nmi_isr(void)             __attribute__ ((weak, alias("unused_isr")));
void hard_fault_isr(void)      __attribute__ ((weak, alias("fault_isr")));
void memmanage_fault_isr(void) __attribute__ ((weak, alias("fault_isr")));
void bus_fault_isr(void)       __attribute__ ((weak, alias("fault_isr")));
void usage_fault_isr(void)     __attribute__ ((weak, alias("fault_isr")));
void svcall_isr(void)          __attribute__ ((weak, alias("unused_isr")));
void debugmonitor_isr(void)    __attribute__ ((weak, alias("unused_isr")));
void pendablesrvreq_isr(void)  __attribute__ ((weak, alias("unused_isr")));
void systick_isr(void)         __attribute__ ((weak, alias("unused_isr")));

// Freescale Kinetis K20 - MK20DX256VLH7 - ARM Cortex-M4
// ARM NVIC Handler Vectors

// eDMA
void dma_ch0_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void dma_ch1_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void dma_ch2_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void dma_ch3_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void dma_ch4_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void dma_ch5_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void dma_ch6_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void dma_ch7_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void dma_ch8_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void dma_ch9_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void dma_ch10_isr(void)  __attribute__ ((weak, alias("unused_isr")));
void dma_ch11_isr(void)  __attribute__ ((weak, alias("unused_isr")));
void dma_ch12_isr(void)  __attribute__ ((weak, alias("unused_isr")));
void dma_ch13_isr(void)  __attribute__ ((weak, alias("unused_isr")));
void dma_ch14_isr(void)  __attribute__ ((weak, alias("unused_isr")));
void dma_ch15_isr(void)  __attribute__ ((weak, alias("unused_isr")));
void dma_error_isr(void) __attribute__ ((weak, alias("unused_isr")));

// FTFL
void flash_cmd_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void flash_error_isr(void) __attribute__ ((weak, alias("unused_isr")));

// PMC
void low_voltage_isr(void) __attribute__ ((weak, alias("unused_isr")));

// LLWU
void wakeup_isr(void)      __attribute__ ((weak, alias("unused_isr")));

// WDOG or EWM
void watchdog_isr(void)    __attribute__ ((weak, alias("unused_isr")));

// I2C
void i2c0_isr(void) __attribute__ ((weak, alias("unused_isr")));
void i2c1_isr(void) __attribute__ ((weak, alias("unused_isr")));

// SPI
void spi0_isr(void) __attribute__ ((weak, alias("unused_isr")));
void spi1_isr(void) __attribute__ ((weak, alias("unused_isr")));

// FlexCAN
void can0_message_isr(void) __attribute__ ((weak, alias("unused_isr")));
void can0_bus_off_isr(void) __attribute__ ((weak, alias("unused_isr")));
void can0_error_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void can0_tx_warn_isr(void) __attribute__ ((weak, alias("unused_isr")));
void can0_rx_warn_isr(void) __attribute__ ((weak, alias("unused_isr")));
void can0_wakeup_isr(void)  __attribute__ ((weak, alias("unused_isr")));

// I2S
void i2s0_tx_isr(void) __attribute__ ((weak, alias("unused_isr")));
void i2s0_rx_isr(void) __attribute__ ((weak, alias("unused_isr")));

// UART
void uart0_lon_isr(void)    __attribute__ ((weak, alias("unused_isr")));
void uart0_status_isr(void) __attribute__ ((weak, alias("unused_isr")));
void uart0_error_isr(void)  __attribute__ ((weak, alias("unused_isr")));
void uart1_status_isr(void) __attribute__ ((weak, alias("unused_isr")));
void uart1_error_isr(void)  __attribute__ ((weak, alias("unused_isr")));
void uart2_status_isr(void) __attribute__ ((weak, alias("unused_isr")));
void uart2_error_isr(void)  __attribute__ ((weak, alias("unused_isr")));

// ADC
void adc0_isr(void) __attribute__ ((weak, alias("unused_isr")));
void adc1_isr(void) __attribute__ ((weak, alias("unused_isr")));

// CMP
void cmp0_isr(void) __attribute__ ((weak, alias("unused_isr")));
void cmp1_isr(void) __attribute__ ((weak, alias("unused_isr")));
void cmp2_isr(void) __attribute__ ((weak, alias("unused_isr")));

// FTM
void ftm0_isr(void) __attribute__ ((weak, alias("unused_isr")));
void ftm1_isr(void) __attribute__ ((weak, alias("unused_isr")));
void ftm2_isr(void) __attribute__ ((weak, alias("unused_isr")));

// CMT
void cmt_isr(void) __attribute__ ((weak, alias("unused_isr")));

// RTC
void rtc_alarm_isr(void)   __attribute__ ((weak, alias("unused_isr")));
void rtc_seconds_isr(void) __attribute__ ((weak, alias("unused_isr")));

// PIT
void pit0_isr(void) __attribute__ ((weak, alias("unused_isr")));
void pit1_isr(void) __attribute__ ((weak, alias("unused_isr")));
void pit2_isr(void) __attribute__ ((weak, alias("unused_isr")));
void pit3_isr(void) __attribute__ ((weak, alias("unused_isr")));

// PDB
void pdb_isr(void) __attribute__ ((weak, alias("unused_isr")));

// USB OTG
void usb_isr(void) __attribute__ ((weak, alias("unused_isr")));

// USB Charger Detect
void usb_charge_isr(void) __attribute__ ((weak, alias("unused_isr")));

// DAC
void dac0_isr(void) __attribute__ ((weak, alias("unused_isr")));

// TSI
void tsi0_isr(void) __attribute__ ((weak, alias("unused_isr")));

// MCG
void mcg_isr(void) __attribute__ ((weak, alias("unused_isr")));

// LPTMR
void lptmr_isr(void) __attribute__ ((weak, alias("unused_isr")));

// PORT
void porta_isr(void) __attribute__ ((weak, alias("unused_isr")));
void portb_isr(void) __attribute__ ((weak, alias("unused_isr")));
void portc_isr(void) __attribute__ ((weak, alias("unused_isr")));
void portd_isr(void) __attribute__ ((weak, alias("unused_isr")));
void porte_isr(void) __attribute__ ((weak, alias("unused_isr")));

// Can only be pended or cleared via the NVIC registers
void software_isr(void) __attribute__ ((weak, alias("unused_isr")));

__attribute__ ((section(".flashconfig"), used))
uint8_t const flashconfigbytes[16] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, FSEC, FOPT, 0xFF, 0xFF
};

#define NVIC_NUM_INTERRUPTS  95

__attribute__ ((section(".dmabuffers"), used, aligned(512)))
void (* _VectorsRam[NVIC_NUM_INTERRUPTS + 16])(void);

__attribute__ ((section(".vectors"), used))
void (* const _VectorsFlash[NVIC_NUM_INTERRUPTS + 16])(void) =
{
    // ARM core
    (void (*)(void))((unsigned long)&_estack), //  0 ARM: Initial Stack Pointer
    ResetHandler,         //  1 ARM: Initial Program Counter
    nmi_isr,              //  2 ARM: Non-maskable Interrupt (NMI)
    hard_fault_isr,       //  3 ARM: Hard Fault
    memmanage_fault_isr,  //  4 ARM: MemManage Fault
    bus_fault_isr,        //  5 ARM: Bus Fault
    usage_fault_isr,      //  6 ARM: Usage Fault

    fault_isr,            //  7 --
    fault_isr,            //  8 --
    fault_isr,            //  9 --
    fault_isr,            // 10 --

    svcall_isr,           // 11 ARM: Supervisor call (SVCall)
    debugmonitor_isr,     // 12 ARM: Debug Monitor

    fault_isr,            // 13 --

    pendablesrvreq_isr,   // 14 ARM: Pendable req serv(PendableSrvReq)
    systick_isr,          // 15 ARM: System tick timer (SysTick)

    // eDMA
    dma_ch0_isr,   // 16 DMA channel 0 transfer complete
    dma_ch1_isr,   // 17 DMA channel 1 transfer complete
    dma_ch2_isr,   // 18 DMA channel 2 transfer complete
    dma_ch3_isr,   // 19 DMA channel 3 transfer complete
    dma_ch4_isr,   // 20 DMA channel 4 transfer complete
    dma_ch5_isr,   // 21 DMA channel 5 transfer complete
    dma_ch6_isr,   // 22 DMA channel 6 transfer complete
    dma_ch7_isr,   // 23 DMA channel 7 transfer complete
    dma_ch8_isr,   // 24 DMA channel 8 transfer complete
    dma_ch9_isr,   // 25 DMA channel 9 transfer complete
    dma_ch10_isr,  // 26 DMA channel 10 transfer complete
    dma_ch11_isr,  // 27 DMA channel 11 transfer complete
    dma_ch12_isr,  // 28 DMA channel 12 transfer complete
    dma_ch13_isr,  // 29 DMA channel 13 transfer complete
    dma_ch14_isr,  // 30 DMA channel 14 transfer complete
    dma_ch15_isr,  // 31 DMA channel 15 transfer complete
    dma_error_isr, // 32 DMA error interrupt channel

    unused_isr, // 33 --

    // FTFL
    flash_cmd_isr,   // 34 Flash Memory Command complete
    flash_error_isr, // 35 Flash Read collision

    // PMC
    low_voltage_isr, // 36 Low-voltage detect/warning

    // LLWU
    wakeup_isr, // 37 Low Leakage Wakeup

    // WDOG or EWM
    watchdog_isr, // 38 Both EWM and WDOG interrupt

    unused_isr, // 39 --

    // I2C
    i2c0_isr, // 40 I2C0
    i2c1_isr, // 41 I2C1

    // SPI
    spi0_isr, // 42 SPI0
    spi1_isr, // 43 SPI1

    unused_isr, // 44 --

    // FlexCAN
    can0_message_isr, // 45 CAN OR'ed Message buffer (0-15)
    can0_bus_off_isr, // 46 CAN Bus Off
    can0_error_isr,   // 47 CAN Error
    can0_tx_warn_isr, // 48 CAN Transmit Warning
    can0_rx_warn_isr, // 49 CAN Receive Warning
    can0_wakeup_isr,  // 50 CAN Wake Up

    // I2S
    i2s0_tx_isr, // 51 I2S0 Transmit
    i2s0_rx_isr, // 52 I2S0 Receive

    unused_isr, // 53 --
    unused_isr, // 54 --
    unused_isr, // 55 --
    unused_isr, // 56 --
    unused_isr, // 57 --
    unused_isr, // 58 --
    unused_isr, // 59 --

    // UART
    uart0_lon_isr,    // 60 UART0 CEA709.1-B (LON) status
    uart0_status_isr, // 61 UART0 status
    uart0_error_isr,  // 62 UART0 error
    uart1_status_isr, // 63 UART1 status
    uart1_error_isr,  // 64 UART1 error
    uart2_status_isr, // 65 UART2 status
    uart2_error_isr,  // 66 UART2 error

    unused_isr, // 67 --
    unused_isr, // 68 --
    unused_isr, // 69 --
    unused_isr, // 70 --
    unused_isr, // 71 --
    unused_isr, // 72 --

    // ADC
    adc0_isr, // 73 ADC0
    adc1_isr, // 74 ADC1

    // CMP
    cmp0_isr, // 75 CMP0
    cmp1_isr, // 76 CMP1
    cmp2_isr, // 77 CMP2

    // FTM
    ftm0_isr, // 78 FTM0
    ftm1_isr, // 79 FTM1
    ftm2_isr, // 80 FTM2

    // CMT
    cmt_isr, // 81 CMT

    // RTC
    rtc_alarm_isr,   // 82 RTC Alarm interrupt
    rtc_seconds_isr, // 83 RTC Seconds interrupt

    // PIT
    pit0_isr, // 84 PIT Channel 0
    pit1_isr, // 85 PIT Channel 1
    pit2_isr, // 86 PIT Channel 2
    pit3_isr, // 87 PIT Channel 3

    // PDB
    pdb_isr, // 88 PDB Programmable Delay Block

    // USB OTG
    usb_isr, // 89 USB OTG

    // USBDCD
    usb_charge_isr, // 90 USB Charger Detect

    unused_isr, // 91 --
    unused_isr, // 92 --
    unused_isr, // 93 --
    unused_isr, // 94 --
    unused_isr, // 95 --
    unused_isr, // 96 --

    // DAC
    dac0_isr, // 97 DAC0

    unused_isr, // 98 --

    // TSI
    tsi0_isr, // 99 TSI0

    // MCG
    mcg_isr, // 100 MCG

    // LPTMR
    lptmr_isr, // 101 Low Power Timer

    unused_isr, // 102 --

    // PORT
    porta_isr, // 103 Pin detect (Port A)
    portb_isr, // 104 Pin detect (Port B)
    portc_isr, // 105 Pin detect (Port C)
    portd_isr, // 106 Pin detect (Port D)
    porte_isr, // 107 Pin detect (Port E)

    unused_isr, // 108 --
    unused_isr, // 109 --

    software_isr  // 110 Software interrupt
};

__attribute__ ((section(".startup"), optimize("-Os")))
void ResetHandler(void)
{
    ////////////////////////////////////////////////////////////////////////////
    // Watchdog requires that an unlock sequence be initiated within 256 bus
    // clocks after reset - this is the Watchdog configuration time (WCT).
    // After initiation the sequence must be completed within 20 bus clock
    // cycles.  Then configuration of the WDOG must complete within the WCT.
    // Failing any of these, the system will reset.  Additionally, a wait of
    // at least one bus clock after the unlock sequence is required before
    // configuration.

#define WDOG_UNLOCK  (*(volatile uint16_t *)0x4005200E)
#define WDOG_UNLOCK_SEQ1  ((uint16_t)0xC520)
#define WDOG_UNLOCK_SEQ2  ((uint16_t)0xD928)
#define WDOG_STCTRLH  (*(volatile uint16_t *)0x40052000)
#define WDOG_STCTRLH_ALLOWUPDATE  ((uint16_t)0x0010)

    WDOG_UNLOCK = WDOG_UNLOCK_SEQ1;
    WDOG_UNLOCK = WDOG_UNLOCK_SEQ2;
    // A couple of NOPs to make sure a bus cycle passes before configuration.
    __asm__ volatile ("nop");
    __asm__ volatile ("nop");
    // Just configure to allow update using the refresh sequence.
    WDOG_STCTRLH = WDOG_STCTRLH_ALLOWUPDATE;
    ////////////////////////////////////////////////////////////////////////////

#define PMC_REGSC         (*(volatile uint8_t  *)0x4007D002)
#define PMC_REGSC_ACKISO  ((uint8_t)0x08)

    // Release I/O pins hold, if we woke up from VLLS mode
    if (PMC_REGSC & PMC_REGSC_ACKISO)
        PMC_REGSC |= PMC_REGSC_ACKISO;

#define SMC_PMPROT       (*(volatile uint8_t  *)0x4007E000)
#define SMC_PMPROT_AVLP  ((uint8_t)0x20)
#define SMC_PMPROT_ALLS  ((uint8_t)0x08)
#define SMC_PMPROT_AVLLS ((uint8_t)0x02)

    // Since this is a write once register, make it visible to all F_CPU's
    // so we can into other sleep modes in the future at any speed.
    SMC_PMPROT = SMC_PMPROT_AVLP | SMC_PMPROT_ALLS | SMC_PMPROT_AVLLS;

    ////////////////////////////////////////////////////////////////////////////
    // FLASH -> RAM

    // Copy .fastrun and .data sections from FLASH to RAM
    for (int i = 0; i < (&_edata - &_sdata); i++)
        (&_sdata)[i] = (&_etext)[i];

    // Initialize uninitialized data to zero
    for (int i = 0; i < (&_ebss - &_sbss); i++)
        (&_sbss)[i] = 0;

    // Copy vector table that's in FLASH to RAM
    for (int i = 0; i < NVIC_NUM_INTERRUPTS + 16; i++)
        _VectorsRam[i] = _VectorsFlash[i];

#define SCB_VTOR  (*(volatile uint32_t *)0xE000ED08)

    // Use vector table that was just copied to RAM
    SCB_VTOR = (uint32_t)_VectorsRam;
    ////////////////////////////////////////////////////////////////////////////

    start_clocks();  // Gets from FEI to PEE mode

    // I don't think this is necessary since PRIMASK is set to zero on reset
    // anyway.  It's just an alias for "CPSIE i" which sets the PRIMASK to
    // zero.
    //__enable_irq();

    __libc_init_array();  // Run the deferred C library initialization

    main();

    while (1);
}

static void start_clocks(void)
{
#define OSC0_CR  (*(volatile uint8_t *)0x40065000)
#define OSC_SC8P     ((uint8_t)0x02)
#define OSC_SC2P     ((uint8_t)0x08)
#define OSC_ERCLKEN  ((uint8_t)0x80)

#define MCG_C1  (*(uint8_t volatile *)0x40064000)
#define MCG_C1_FRDIV(n)  (uint8_t)(((n) & 0x07) << 3)
#define MCG_C1_CLKS(n)   (uint8_t)(((n) & 0x03) << 6)

#define MCG_C2  (*(uint8_t volatile *)0x40064001)
#define MCG_C2_RANGE0(n)  (uint8_t)(((n) & 0x03) << 4)
#define MCG_C2_EREFS      (uint8_t)0x04

#define MCG_C5  (*(uint8_t volatile *)0x40064004)
#define MCG_C5_PRDIV0(n)  (uint8_t)((n) & 0x1F)

#define MCG_C6  (*(uint8_t volatile *)0x40064005)
#define MCG_C6_VDIV0(n)  (uint8_t)((n) & 0x1F)
#define MCG_C6_PLLS      (uint8_t)0x40

#define MCG_S  (*(uint8_t volatile *)0x40064006)
#define MCG_S_OSCINIT0    (uint8_t)0x02
#define MCG_S_CLKST(n)    (uint8_t)(((n) & 0x03) << 2)
#define MCG_S_CLKST_MASK  (uint8_t)0x0C
#define MCG_S_IREFST      (uint8_t)0x10
#define MCG_S_PLLST       (uint8_t)0x20
#define MCG_S_LOCK0       (uint8_t)0x40

    ////////////////////////////////////////////////////////////////////////////
    // Hardware always starts in FEI mode //////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    //  C1[CLKS] bits are written to 00
    //  C1[IREFS] bit is written to 1
    //  C6[PLLS] bit is written to 0
    // MCG_SC[FCDIV] defaults to divide by two for internal ref clock
    // I tried changing MSG_SC to divide by 1, it didn't work for me
    ////////////////////////////////////////////////////////////////////////////

    // Enable capacitors for crystal
    OSC0_CR = OSC_SC8P | OSC_SC2P | OSC_ERCLKEN;

    // Enable osc, 8-32 MHz range, low power mode
    MCG_C2 = MCG_C2_RANGE0(2) | MCG_C2_EREFS;

    // Switch to crystal as clock source, FLL input = 16 MHz / 512
    MCG_C1 =  MCG_C1_CLKS(2) | MCG_C1_FRDIV(4);

    // Wait for crystal oscillator to begin
    while ((MCG_S & MCG_S_OSCINIT0) == 0);

    // Wait for FLL to use oscillator
    while ((MCG_S & MCG_S_IREFST) != 0);

    // Wait for MCGOUT to use oscillator
    while ((MCG_S & MCG_S_CLKST_MASK) != MCG_S_CLKST(2));

    ////////////////////////////////////////////////////////////////////////////
    // Now in FBE mode /////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    //  C1[CLKS] bits are written to 10
    //  C1[IREFS] bit is written to 0
    //  C1[FRDIV] must be written to divide xtal to 31.25-39 kHz
    //  C6[PLLS] bit is written to 0
    //  C2[LP] is written to 0
    // If we need faster than the crystal, turn on the PLL
    ////////////////////////////////////////////////////////////////////////////

#if F_CPU == 72000000
    MCG_C5 = MCG_C5_PRDIV0(5);  // config PLL input for 16 MHz Crystal / 6 = 2.667 Hz
#else
    MCG_C5 = MCG_C5_PRDIV0(3);  // config PLL input for 16 MHz Crystal / 4 = 4 MHz
#endif

#if F_CPU == 96000000 || F_CPU == 48000000
    MCG_C6 = MCG_C6_PLLS | MCG_C6_VDIV0(0);  // config PLL for 96 MHz output
#else  // F_CPU == 72000000
    MCG_C6 = MCG_C6_PLLS | MCG_C6_VDIV0(3);  // config PLL for 72 MHz output
#endif

    // wait for PLL to start using xtal as its input
    while (!(MCG_S & MCG_S_PLLST));

    // wait for PLL to lock
    while (!(MCG_S & MCG_S_LOCK0));

    ////////////////////////////////////////////////////////////////////////////
    // Now in PBE mode /////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

#define SIM_CLKDIV1  (*(volatile uint32_t *)0x40048044)
#define SIM_CLKDIV1_OUTDIV1(n)  ((uint32_t)(((n) & 0x0F) << 28))
#define SIM_CLKDIV1_OUTDIV2(n)  ((uint32_t)(((n) & 0x0F) << 24))
#define SIM_CLKDIV1_OUTDIV3(n)  ((uint32_t)(((n) & 0x0F) << 20))
#define SIM_CLKDIV1_OUTDIV4(n)  ((uint32_t)(((n) & 0x0F) << 16))

#define SIM_CLKDIV2  (*(volatile uint32_t *)0x40048048)
#define SIM_CLKDIV2_USBDIV(n)  ((uint32_t)(((n) & 0x07) << 1))
#define SIM_CLKDIV2_USBFRAC    ((uint32_t)0x01)

    // Program the clock dividers
#if F_CPU == 96000000
    // Config divisors: 96 MHz core, 48 MHz bus, 24 MHz flash, USB = 96 / 2
# if F_BUS == 48000000
    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0) | SIM_CLKDIV1_OUTDIV2(1) | SIM_CLKDIV1_OUTDIV4(3);
# else
#  error "F_BUS not supported for 98 MHz"
# endif
    SIM_CLKDIV2 = SIM_CLKDIV2_USBDIV(1);
#elif F_CPU == 72000000
    // config divisors: 72 MHz core, 36 MHz bus, 24 MHz flash, USB = 72 * 2 / 3
# if F_BUS == 36000000
    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0) | SIM_CLKDIV1_OUTDIV2(1) | SIM_CLKDIV1_OUTDIV4(2);
# else
#  error "F_BUS not supported for 72 MHz"
# endif
    SIM_CLKDIV2 = SIM_CLKDIV2_USBDIV(2) | SIM_CLKDIV2_USBFRAC;
#elif F_CPU == 48000000
    // config divisors: 48 MHz core, 48 MHz bus, 24 MHz flash, USB = 96 / 2
    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(1) | SIM_CLKDIV1_OUTDIV2(1) | SIM_CLKDIV1_OUTDIV3(1) |  SIM_CLKDIV1_OUTDIV4(3);
    SIM_CLKDIV2 = SIM_CLKDIV2_USBDIV(1);
#else
# error "F_CPU must be 96, 72 or 48 MHz"
#endif

    // switch to PLL as clock source, FLL input = 16 MHz / 512
    MCG_C1 = MCG_C1_CLKS(0) | MCG_C1_FRDIV(4);
    // wait for PLL clock to be used
    while ((MCG_S & MCG_S_CLKST_MASK) != MCG_S_CLKST(3));

    ////////////////////////////////////////////////////////////////////////////
    // Now in PEE mode /////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
}

char * __brkval = (char *)&_ebss;

#ifndef STACK_MARGIN
# define STACK_MARGIN  4096
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void * _sbrk(int incr)
{
    char * prev, * stack;

    prev = __brkval;
    if (incr != 0)
    {
        __asm__ volatile("mov %0, sp" : "=r" (stack) ::);

        if ((prev + incr) >= (stack - STACK_MARGIN))
        {
            errno = ENOMEM;
            return (void *)-1;
        }

        __brkval = prev + incr;
    }

    return prev;
}

__attribute__((weak)) 
int _read(int file, char * ptr, int len)
{
    return 0;
}

__attribute__((weak)) 
int _close(int fd)
{
    return -1;
}

#include <sys/stat.h>

__attribute__((weak)) 
int _fstat(int fd, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

__attribute__((weak)) 
int _isatty(int fd)
{
    return 1;
}

__attribute__((weak)) 
int _lseek(int fd, long long offset, int whence)
{
    return -1;
}

__attribute__((weak)) 
void _exit(int status)
{
    while (1);
}

__attribute__((weak)) 
void __cxa_pure_virtual(void)
{
    while (1);
}

__attribute__((weak)) 
int __cxa_guard_acquire(char * g) 
{
    return !(*g);
}

__attribute__((weak)) 
void __cxa_guard_release(char * g)
{
    *g = 1;
}

#pragma GCC diagnostic pop

