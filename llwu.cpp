#include "llwu.h"
#include "armv7m.h"
#include "types.h"
#include "pin.h"
#include "lptmr.h"

#define MCG_C1_CLKS(val) (((val) & 0x03) << 6)
#define MCG_C2_LP        (1 << 1)
#define MCG_C6_CME0      (1 << 5)
#define MCG_S_CLKST_MASK (0x03 << 2)
#define MCG_S_CLKST(val) (((val) & 0x03) << 2)
#define MCG_S_IREFST     (1 << 4)
#define MCG_S_PLLST      (1 << 5)
#define MCG_S_LOCK0      (1 << 6)

wus_e volatile Llwu::_s_wakeup_source = WUS_NONE;
wums_e volatile Llwu::_s_wakeup_module = WUMS_NOT;
int8_t volatile Llwu::_s_wakeup_pin = -1;
Lptmr * Llwu::_s_lptmr = nullptr;
constexpr int8_t const Llwu::_s_wups_to_pin[16];

reg8 Llwu::_s_base = (reg8)0x4007C000;
reg8 Llwu::_s_pe1 = _s_base;
reg8 Llwu::_s_pe2 = _s_base + 1;
reg8 Llwu::_s_pe3 = _s_base + 2;
reg8 Llwu::_s_pe4 = _s_base + 3;
reg8 Llwu::_s_me  = _s_base + 4;
reg8 Llwu::_s_f1  = _s_base + 5;
reg8 Llwu::_s_f2  = _s_base + 6;
reg8 Llwu::_s_f3  = _s_base + 7;
//reg8 Llwu::_s_filt1 = _s_base + 8;
//reg8 Llwu::_s_filt2 = _s_base + 9;
//reg8 Llwu::_s_rst = _s_base + 10;

reg8 Llwu::_s_smc_base = (reg8)0x4007E000;
//reg8 Llwu::_s_smc_pmprot   = _s_smc_base;
reg8 Llwu::_s_smc_pmctrl   = _s_smc_base + 1;
//reg8 Llwu::_s_smc_vllsctrl = _s_smc_base + 2;
//reg8 Llwu::_s_smc_pmstat   = _s_smc_base + 3;

reg8 Llwu::_s_mcg_base = (reg8)0x40064000;
reg8 Llwu::_s_mcg_c1 = _s_mcg_base;
reg8 Llwu::_s_mcg_c2 = _s_mcg_base + 1;
//reg8 Llwu::_s_mcg_c3 = _s_mcg_base + 2;
//reg8 Llwu::_s_mcg_c4 = _s_mcg_base + 3;
//reg8 Llwu::_s_mcg_c5 = _s_mcg_base + 4;
reg8 Llwu::_s_mcg_c6 = _s_mcg_base + 5;
reg8 Llwu::_s_mcg_s  = _s_mcg_base + 6;
//reg8 Llwu::_s_mcg_sc = _s_mcg_base + 8;
//reg8 Llwu::_s_mcg_atcvh = _s_mcg_base + 10;
//reg8 Llwu::_s_mcg_atcvl = _s_mcg_base + 11;
//reg8 Llwu::_s_mcg_c7 = _s_mcg_base + 12;
//reg8 Llwu::_s_mcg_c8 = _s_mcg_base + 13;

void Llwu::enable(void)
{
    if (enabled())
        return;

    NVIC::setPriority(IRQ_LLWU, 64);
    NVIC::enable(IRQ_LLWU);
    SCB::setSleepDeep();
    stopMode();
}

void Llwu::disable(void)
{
    if (!enabled())
        return;

    if (_s_lptmr != nullptr)
        timerDisable();

    stopMode();
    SCB::clearSleepDeep();
    NVIC::disable(IRQ_LLWU);
    NVIC::setPriority(IRQ_LLWU, 128);

    *_s_pe1 = *_s_pe2 = *_s_pe3 = *_s_pe4 = 0;
    *_s_me = 0;
}

bool Llwu::enabled(void)
{
    return NVIC::isEnabled(IRQ_LLWU);
}

void Llwu::stopMode(void)
{
    uint8_t sm = _stop_mode;
    _stop_mode = *_s_smc_pmctrl & SMC_PMCTRL_STOPM_MASK;
    *_s_smc_pmctrl &= ~_stop_mode;
    *_s_smc_pmctrl |= sm;
}

bool Llwu::timerEnable(uint16_t msecs)
{
    if (_s_lptmr != nullptr)
        return false;

    _s_lptmr = &Lptmr::acquire();
    _s_lptmr->enable();
    _s_lptmr->setTime(msecs);

    *_s_me |= (1 << WUMS_LPTMR);

    return true;
}

bool Llwu::timerDisable(void)
{
    if (_s_lptmr == nullptr)
        return false;

    *_s_me &= ~(1 << WUMS_LPTMR);

    _s_lptmr->disable();
    _s_lptmr = nullptr;

    return true;
}

wus_e Llwu::sleep(void)
{
    _s_wakeup_source = WUS_NONE;
    _s_wakeup_module = WUMS_NOT;
    _s_wakeup_pin = -1;

    if (!enabled())
        return WUS_ERROR;

    // Return if there is nothing that can wake up the MPU
    if ((*_s_pe1 == 0) && (*_s_pe2 == 0)
            && (*_s_pe3 == 0) && (*_s_pe4 == 0) && (*_s_me == 0))
        return WUS_ERROR;

    if (*_s_me & (1 << WUMS_LPTMR))
        _s_lptmr->start();

    // XXX This may not be necessary since they're cleared in wakeup_isr
    *_s_f1 = *_s_f2 = 0xFF;

    // Freescale documentation says to do this:
    // http://cache.nxp.com/files/32bit/doc/app_note/AN4503.pdf
    // Disables the loss of clock monitoring circuit for the OSC0
    // in case it is configured to generate an interrupt or reset
    // since clocks will stop when the wfi instruction is executed.
    *_s_mcg_c6 &= ~MCG_C6_CME0;

    // Wait For Interrupt instruction
    // Puts the MCU into DEEPSLEEP / LLS mode
    __asm__ volatile ("wfi");

    *_s_mcg_c6 |= MCG_C6_CME0;

    if (*_s_me & (1 << WUMS_LPTMR))
        _s_lptmr->stop();

    return _s_wakeup_source;
}

void wakeup_isr(void)
{
    uint16_t wufp = (uint16_t)*Llwu::_s_f2 << 8 | *Llwu::_s_f1;
    uint8_t wufm = *Llwu::_s_f3;

    if (wufm != 0)
    {
        if (wufm & (1 << WUMS_LPTMR))
        {
            // If there is a pending interrupt with the LPTMR then clear the flag
            // and pending interrupt in that order.  If the order is reversed then
            // after clearing the pending interrupt, another interrupt is immediately
            // generated since the LPTMR_CSR_TCF flag hasn't been cleared yet.
            Llwu::_s_lptmr->clear();
            Llwu::_s_wakeup_module = WUMS_LPTMR;
        }

        Llwu::_s_wakeup_source = WUS_MODULE;
    }
    else if (wufp != 0)
    {
        Llwu::_s_wakeup_source = WUS_PIN;
        Llwu::_s_wakeup_pin = Llwu::_s_wups_to_pin[__builtin_ctz(wufp)];

        *Llwu::_s_f1 = *Llwu::_s_f2 = 0xFF;  // w1c

        // Clearing PORT/Pin interrupts isn't necessary.  Only this ISR is called and
        // not the ones configured in the PORT_PCR registers.
    }

    // Should wake up into PBE mode - need to go back into PEE mode
    // Code taken from Freescale: K20 72MHz Bare Metal Examples
    uint8_t status = *Llwu::_s_mcg_s;
    if (((status & MCG_S_CLKST_MASK) == MCG_S_CLKST(0x02))  // check CLKS mux has selcted external reference
            && (!(status & MCG_S_IREFST))  // check FLL ref is external ref clk
            && (status & MCG_S_PLLST)      // check PLLS mux has selected PLL
            && (!(*Llwu::_s_mcg_c2 & MCG_C2_LP)))   // check Low Power bit not set
    {
        while (!(*Llwu::_s_mcg_s & MCG_S_LOCK0));
        *Llwu::_s_mcg_c1 &= ~MCG_C1_CLKS(0x03);
        while ((*Llwu::_s_mcg_s & MCG_S_CLKST_MASK) != MCG_S_CLKST(0x03));
    }
}

