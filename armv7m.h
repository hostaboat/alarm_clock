#ifndef _ARMV7M_H_
#define _ARMV7M_H_

#include "types.h"

////////////////////////////////////////////////////////////////////////////////
// System Control Space (SCS) //////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// System Control Block (SCB) //////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define SCB_SCR_SLEEPDEEP   (1 << 2)
#define SCB_ICSR_PENDSTSET  (1 << 26)

enum except_e
{
    //EXCEPT_NOT           =  0,
    EXCEPT_RESET         =  1,
    EXCEPT_NMI           =  2,
    EXCEPT_HARD_FAULT    =  3,
    EXCEPT_MEM_MANAGE    =  4,
    EXCEPT_BUS_FAULT     =  5,
    EXCEPT_USAGE_FAULT   =  6,
    //EXCEPT_RSVD_7        =  7,
    //EXCEPT_RSVD_8        =  8,
    //EXCEPT_RSVD_9        =  9,
    //EXCEPT_RSVD_10       = 10,
    EXCEPT_SV_CALL       = 11,
    EXCEPT_DEBUG_MONITOR = 12,
    //EXCEPT_RSVD_13       = 13,
    EXCEPT_PEND_SV       = 14,
    EXCEPT_SYS_TICK      = 15,
};

class SCB
{
    public:
        static void setPriority(except_e e, uint8_t priority)
        {
            if (e < EXCEPT_MEM_MANAGE) return;
            _s_shpr_base[e - 4] = priority;
        }
        static int getPriority(except_e e)
        {
            switch (e)
            {
                case EXCEPT_RESET:      return -3;
                case EXCEPT_NMI:        return -2;
                case EXCEPT_HARD_FAULT: return -1;
                default:                return _s_shpr_base[e - 4];
            }
        }
        static bool sysTickIntrPending(void) { return *_s_icsr & SCB_ICSR_PENDSTSET; }
        static void setSleepDeep(void) { *_s_scr |= SCB_SCR_SLEEPDEEP; }
        static void clearSleepDeep(void) { *_s_scr &= ~SCB_SCR_SLEEPDEEP; }
        static int executionPriority(void);

    private:
        static reg32 _s_base;
        static const reg32 _s_cpuid;
        static reg32 _s_icsr;
        static reg32 _s_vtor;
        static reg32 _s_aircr;
        static reg32 _s_scr;
        static reg32 _s_ccr;
        static reg32 _s_shpr1;
        static reg32 _s_shpr2;
        static reg32 _s_shpr3;
        static reg8 _s_shpr_base;
        static reg32 _s_shcsr;
        static reg32 _s_cfsr;
        static reg32 _s_hfsr;
        static reg32 _s_dfsr;
        static reg32 _s_mmfar;
        static reg32 _s_bfar;
        static reg32 _s_afsr;
        static reg32 _s_cpacr;
        static reg32 _s_fp_base;
        static reg32 _s_fpccr;
        static reg32 _s_fpcar;
        static reg32 _s_fpdscr;
        static const reg32 _s_mvfr0;
        static const reg32 _s_mvfr1;
        static const reg32 _s_mvfr2;
};

////////////////////////////////////////////////////////////////////////////////
// The system timer (SysTick) //////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define SYST_CSR_ENABLE     (1 <<  0)
#define SYST_CSR_TICKINT    (1 <<  1)
#define SYST_CSR_CLKSOURCE  (1 <<  2)
#define SYST_CSR_COUNTFLAG  (1 << 16)

#define TICKS_PER_MSEC  (F_CPU / 1000)
#define TICKS_PER_USEC  (F_CPU / 1000000)

extern "C" void systick_isr(void);

class SysTick
{
    friend void systick_isr(void);

    public:
        static void init(void) { init(TICKS_PER_MSEC, 32); }

        static uint32_t currentValue(void) { return *_s_cvr; }
        static uint32_t reloadValue(void) { return *_s_rvr; }
        static uint32_t intervals(void) { return _intervals; }
        static uint32_t msecs(void) { return _intervals; }

    private:
        static void init(uint32_t rv, uint8_t priority)  // rv -> Reload Value
        {
            SCB::setPriority(EXCEPT_SYS_TICK, priority);

            *_s_rvr = rv;
            *_s_cvr = 0;
            *_s_csr = SYST_CSR_ENABLE | SYST_CSR_TICKINT | SYST_CSR_CLKSOURCE;
        }

        static v32 _intervals;

        static reg32 _s_base;
        static reg32 _s_csr;
        static reg32 _s_rvr;
        static reg32 _s_cvr;
        static reg32 _s_calib;
};

////////////////////////////////////////////////////////////////////////////////
// Nested Vectored Interrupt Controller (NVIC) /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
enum irq_e
{
    IRQ_DMA_CH0 = 0,
    IRQ_DMA_CH1 = 1,
    IRQ_DMA_CH2 = 2,
    IRQ_DMA_CH3 = 3,
    IRQ_DMA_CH4 = 4,
    IRQ_DMA_CH5 = 5,
    IRQ_DMA_CH6 = 6,
    IRQ_DMA_CH7 = 7,
    IRQ_DMA_CH8 = 8,
    IRQ_DMA_CH9 = 9,
    IRQ_DMA_CH10 = 10,
    IRQ_DMA_CH11 = 11,
    IRQ_DMA_CH12 = 12,
    IRQ_DMA_CH13 = 13,
    IRQ_DMA_CH14 = 14,
    IRQ_DMA_CH15 = 15,
    IRQ_DMA_ERROR = 16,
    // 17
    IRQ_FTFL_COMPLETE = 18,
    IRQ_FTFL_COLLISION = 19,
    IRQ_LOW_VOLTAGE = 20,
    IRQ_LLWU = 21,
    IRQ_WDOG = 22,
    // 23
    IRQ_I2C0 = 24,
    IRQ_I2C1 = 25,
    IRQ_SPI0 = 26,
    IRQ_SPI1 = 27,
    // 28
    IRQ_CAN_MESSAGE = 29,
    IRQ_CAN_BUS_OFF = 30,
    IRQ_CAN_ERROR = 31,
    IRQ_CAN_TX_WARN = 32,
    IRQ_CAN_RX_WARN = 33,
    IRQ_CAN_WAKEUP = 34,
    IRQ_I2S0_TX = 35,
    IRQ_I2S0_RX = 36,
    // 37..43
    IRQ_UART0_LON = 44,
    IRQ_UART0_STATUS = 45,
    IRQ_UART0_ERROR = 46,
    IRQ_UART1_STATUS = 47,
    IRQ_UART1_ERROR = 48,
    IRQ_UART2_STATUS = 49,
    IRQ_UART2_ERROR = 50,
    // 51..56
    IRQ_ADC0 = 57,
    IRQ_ADC1 = 58,
    IRQ_CMP0 = 59,
    IRQ_CMP1 = 60,
    IRQ_CMP2 = 61,
    IRQ_FTM0 = 62,
    IRQ_FTM1 = 63,
    IRQ_FTM2 = 64,
    IRQ_CMT = 65,
    IRQ_RTC_ALARM = 66,
    IRQ_RTC_SECOND = 67,
    IRQ_PIT_CH0 = 68,
    IRQ_PIT_CH1 = 69,
    IRQ_PIT_CH2 = 70,
    IRQ_PIT_CH3 = 71,
    IRQ_PDB = 72,
    IRQ_USBOTG = 73,
    IRQ_USBDCD = 74,
    // 75..80
    IRQ_DAC0 = 81,
    // 82
    IRQ_TSI = 83,
    IRQ_MCG = 84,
    IRQ_LPTMR = 85,
    // 86
    IRQ_PORTA = 87,
    IRQ_PORTB = 88,
    IRQ_PORTC = 89,
    IRQ_PORTD = 90,
    IRQ_PORTE = 91,
    // 92..93
    IRQ_SOFTWARE = 94,
};

#define NVIC_DEF_PRI  128  // Default priority

class NVIC
{
    public:
        static void init(void)
        {
            for (uint8_t i = 0; i <= IRQ_SOFTWARE; i++)
                _s_ipr[i] = NVIC_DEF_PRI;  // Default interrupts to priority 128
        }

        static void enable(irq_e irq) { set(_s_iser, irq); }
        static void disable(irq_e irq) { set(_s_icer, irq); }
        static bool isEnabled(irq_e irq) { return check(_s_iser, irq); }

        static void setPending(irq_e irq) { set(_s_ispr, irq); }
        static void clearPending(irq_e irq) { set(_s_icpr, irq); }
        static bool isPending(irq_e irq) { return check(_s_ispr, irq); }

        static bool isActive(irq_e irq) { return check(_s_iabr, irq); }

        static void setPriority(irq_e irq, uint8_t priority) { _s_ipr[irq] = priority; }
        static uint8_t getPriority(irq_e irq) { return _s_ipr[irq]; }

    private:
        // ISER, ICER, ISPR, ICPR and IABR register and bit calculations
        // reg_num = irq / 32 (irq >> 5)
        //     bit = irq % 32 (irq & 0x1F)
        // Writes of zero have no effect on ISER, ICER, ISPR, and ICPR so don't
        // have to |= the register with the bit.

        static void set(reg32 reg, irq_e irq) { *(reg + (irq / 32)) = (1 << (irq % 32)); }
        static bool check(reg32 reg, irq_e irq) { return *(reg + (irq / 32)) & (1 << (irq % 32)); }

        static reg32 _s_base;
        static reg32 _s_iser;
        static reg32 _s_icer;
        static reg32 _s_ispr;
        static reg32 _s_icpr;
        static reg32 _s_iabr;
        static reg8  _s_ipr;
};

////////////////////////////////////////////////////////////////////////////////
// Debug ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define DEBUG_DEMCR_TRCENA  (1 << 24)

class Debug
{
    public:
        static void enableDWT(void) { *_s_demcr |= DEBUG_DEMCR_TRCENA; }

    private:
        static reg32 _s_base;
        static reg32 _s_dhcsr;
        static reg32 _s_dcrsr;
        static reg32 _s_dcrdr;
        static reg32 _s_demcr;
};

////////////////////////////////////////////////////////////////////////////////
// Data Watchpoint and Trace unit (DWT) ////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define DWT_CTRL_CYCCNTENA  (1 << 0)

class DWT
{
    public:
        static void enableCycleCount(void) { *_s_ctrl |= DWT_CTRL_CYCCNTENA; }
        static uint32_t cycleCount(void) { return *_s_cyccnt; }
        static void zeroCycleCount(void) { *_s_cyccnt = 0; }

    private:
        static reg32 _s_base;
        static reg32 _s_ctrl;
        static reg32 _s_cyccnt;
        static reg32 _s_cpicnt;
        static reg32 _s_exccnt;
        static reg32 _s_sleepcnt;
        static reg32 _s_lsucnt;
        static reg32 _s_foldcnt;
        static reg32 _s_pcsr;
};

#endif
