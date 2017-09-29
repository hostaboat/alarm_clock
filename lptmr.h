#ifndef _LPTMR_H_
#define _LPTMR_H_

#include "module.h"
#include "types.h"

extern "C" void lptmr_isr(void);

// Uses 1kHz clock and interrupt
class Lptmr : public Module
{
    friend void lptmr_isr(void);

    public:
        static Lptmr & acquire(void) { static Lptmr lptmr; return lptmr; }

        void enable(void);
        void disable(void);
        bool enabled(void);

        void start(void);
        void stop(void);
        bool running(void);

        void setTime(uint16_t msecs);
        void clearIntr(void);

        Lptmr(const Lptmr &) = delete;
        Lptmr & operator=(const Lptmr &) = delete;

    private:
        Lptmr(uint16_t msecs = 0) : _msecs(msecs) {}

        void osc32ClockSelect(void);

        uint16_t _msecs = 0;
#define SIM_SOPT1_OSC_LPO_1KHZ  (0x03 << 18)
        uint32_t _osc32ksel = (uint32_t)SIM_SOPT1_OSC_LPO_1KHZ;

        static reg32 _s_base;
        static reg32 _s_csr;
        static reg32 _s_psr;
        static reg32 _s_cmr;
        static reg32 _s_cnr;

        static reg32 _s_sim_sopt1;
};

/*******************************************************************************
 * LPTMR0_PSR[PCS]    Prescaler/glitch filter    Chip Clock
 *                         clock number
 *       00                      0               MCGIRCLK - internal reference clock (not available in VLPS/LLS/VLLS
 *       01                      1               LPO - 1 kHz clock
 *       10                      2               ERCLK32K - secondary external reference clock
 *       11                      3               OSCERCLK - external reference clock
 *
 * LPTMR0_CSR[TPS]   Pulse counter input number  Chip Input
 *       00                      0               CMP0 output
 *       01                      1               LPTMR_ALT1 pin
 *       10                      2               LPTMR_ALT2 pin
 *       11                      3               Reserved
 *
 *  Bus Interface Clock     Internal Clocks
 *     Flash clock          LPO, OSCERCLK, MCGIRCLK, ERCLK32K
 *
 *     SIM_SOPT1  18-19  32K oscillator clock select
 *     Selects the 32 kHz clock source (ERCLK32K) for TSI and LPTMR. This bit is reset only for POR/LVD
 *     00  System oscillator (OSC32KCLK)
 *     01  Reserved
 *     10  RTC 32.768 kHz oscillator
 *     11  LPO 1 kHz
 *
 * Write CSR first with timer disabled
 * Then PSR and CMR
 * Then CSR[TIE] as last step - is this before, during or after setting CSR[TEN]?
 ******************************************************************************/

#endif
