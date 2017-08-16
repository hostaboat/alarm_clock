#include "lptmr.h"
#include "module.h"
#include "armv7m.h"
#include "types.h"

#define LPTMR_CSR_TEN (1 << 0)
#define LPTMR_CSR_TMS (1 << 1)
#define LPTMR_CSR_TFC (1 << 2)
#define LPTMR_CSR_TPP (1 << 3)
#define LPTMR_CSR_TPS(pci)  (((pci) & 0x03) << 4)
#define LPTMR_CSR_TIE (1 << 6)
#define LPTMR_CSR_TCF (1 << 7)

#define LPTMR_PSR_PCS(pcs)      (((pcs) & 0x03) << 0)
#define LPTMR_PSR_PBYP          (1 << 2)
#define LPTMR_PSR_PRESCALE(psv) (((psv) & 0x0F) << 3)

#define LPTMR_PSR_PCS_MCGIRCLK   LPTMR_PSR_PCS(0) // Internal reference clock
#define LPTMR_PSR_PCS_LPO_1KHZ   LPTMR_PSR_PCS(1) // LPO - 1kHz clock
#define LPTMR_PSR_PCS_ERCLK32K   LPTMR_PSR_PCS(2) // Secondary external reference clock
#define LPTMR_PSR_PCS_OSCERCLK   LPTMR_PSR_PCS(3) // External reference clock

#define LPTMR_CMR_COMPARE(cv)  (uint16_t)(cv)
//#define LPTMR_CNR_COUNTER  Read only

reg32 Lptmr::_s_base = (reg32)0x40040000;
reg32 Lptmr::_s_csr = _s_base;
reg32 Lptmr::_s_psr = _s_base + 1;
reg32 Lptmr::_s_cmr = _s_base + 2;
reg32 Lptmr::_s_cnr = _s_base + 3;
reg32 Lptmr::_s_sim_sopt1 = (reg32)0x40047000;

void Lptmr::enable(void)
{
    if (enabled())
        return;

    Module::enable(MOD_LPTIMER);
    NVIC::enable(IRQ_LPTMR);

    osc32ClockSelect();
}

void Lptmr::disable(void)
{
    if (!enabled())
        return;

    if (running())
        stop();

    Module::disable(MOD_LPTIMER);
    NVIC::disable(IRQ_LPTMR);

    osc32ClockSelect();
}

bool Lptmr::enabled(void)
{
    return Module::enabled(MOD_LPTIMER) && NVIC::isEnabled(IRQ_LPTMR);
}

void Lptmr::start(void)
{
    if (running())
        return;

    if (!enabled())
        enable();

    *_s_csr = 0;
    *_s_psr = LPTMR_PSR_PBYP | LPTMR_PSR_PCS_LPO_1KHZ;
    *_s_cmr = (uint32_t)_msecs;
    *_s_csr = LPTMR_CSR_TEN | LPTMR_CSR_TCF | LPTMR_CSR_TIE;
}

void Lptmr::stop(void)
{
    if (!running())
        return;

    clear();
    *_s_csr = 0;
}

bool Lptmr::running(void)
{
    return enabled() && (*_s_csr & LPTMR_CSR_TEN);
}

void Lptmr::osc32ClockSelect(void)
{
    uint32_t osc = _osc32ksel;
    _osc32ksel = *_s_sim_sopt1 & SIM_SOPT1_OSC_LPO_1KHZ;  // LPO_1KHZ is a mask as well
    *_s_sim_sopt1 &= ~_osc32ksel;
    *_s_sim_sopt1 |= osc;
}

void Lptmr::clear(void)
{
    *_s_csr |= LPTMR_CSR_TCF;
    NVIC::clearPending(IRQ_LPTMR);
}

void lptmr_isr(void)
{
    *Lptmr::_s_csr |= LPTMR_CSR_TCF;
}

