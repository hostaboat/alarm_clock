#include "tsi.h"
#include "types.h"

#define TSI_GENCS_STPE     (1 << 0)
#define TSI_GENCS_STM      (1 << 1)
#define TSI_GENCS_ESOR     (1 << 4)
#define TSI_GENCS_ERIE     (1 << 5)
#define TSI_GENCS_TSIIE    (1 << 6)
#define TSI_GENCS_TSIEN    (1 << 7)
#define TSI_GENCS_SWTS     (1 << 8)
#define TSI_GENCS_SCNIP    (1 << 9)
#define TSI_GENCS_OVRF     (1 << 12)
#define TSI_GENCS_EXTERF   (1 << 13)
#define TSI_GENCS_OUTRGF   (1 << 14)
#define TSI_GENCS_EOSF     (1 << 15)
//#define TSI_GENCS_PS
//#define TSI_GENCS_NSCN
//#define TSI_GENCS_LPSCNITV
#define TSI_GENCS_LPCLKS   (1 << 28)

constexpr tTouch const Tsi::_s_default_touch;
bool Tsi::_s_error = false;
bool Tsi::_s_overrun = false;

reg32 Tsi::_s_conf_base = (reg32)0x40045000;
reg32 Tsi::_s_gencs  = _s_conf_base;
reg32 Tsi::_s_scanc  = _s_conf_base +  1;
reg32 Tsi::_s_pen    = _s_conf_base +  2;
reg32 Tsi::_s_wucntr = _s_conf_base +  3;
reg32 Tsi::_s_cntr_base  = (reg32)0x40045100;
reg16 Tsi::_s_cntr = (reg16)_s_cntr_base;
//reg32 Tsi::_s_cntr1  = _s_cntr_base;
//reg32 Tsi::_s_cntr3  = _s_cntr_base + 1;
//reg32 Tsi::_s_cntr5  = _s_cntr_base + 2;
//reg32 Tsi::_s_cntr7  = _s_cntr_base + 3;
//reg32 Tsi::_s_cntr9  = _s_cntr_base + 4;
//reg32 Tsi::_s_cntr11 = _s_cntr_base + 5;
//reg32 Tsi::_s_cntr13 = _s_cntr_base + 6;
//reg32 Tsi::_s_cntr15 = _s_cntr_base + 7;
reg32 Tsi::_s_threshold = _s_cntr_base + 8;

void tsi0_isr(void)
{
    if (*Tsi::_s_gencs & TSI_GENCS_EXTERF)
    {
        Tsi::_s_error = true;
        *Tsi::_s_gencs |= TSI_GENCS_EXTERF;
    }

    if (*Tsi::_s_gencs & TSI_GENCS_OVRF)
    {
        Tsi::_s_overrun = true;
        *Tsi::_s_gencs |= TSI_GENCS_OVRF;
    }
}

Tsi::Tsi(void)
{
    Module::enable(MOD_TSI);

    *_s_gencs = 0;

    if (!_eeprom.getTouch(_touch) || !isValid(_touch))
    {
        _touch = _s_default_touch;
        (void)_eeprom.setTouch(_touch);
    }

    configure();
}

void Tsi::start(void)
{
    *_s_gencs |= TSI_GENCS_TSIEN;
    NVIC::enable(IRQ_TSI);
}

void Tsi::stop(void)
{
    NVIC::disable(IRQ_TSI);
    while (scanning());
    *_s_gencs &= ~TSI_GENCS_TSIEN;
}

bool Tsi::running(void)
{
    return *_s_gencs & TSI_GENCS_TSIEN;
}

bool Tsi::scanning(void)
{
    return *_s_gencs & TSI_GENCS_SCNIP;
}

bool Tsi::eos(void)
{
    return *_s_gencs & TSI_GENCS_EOSF;
}

bool Tsi::touched(uint8_t channel)
{
    return read(channel) >= _touch.threshold;
}

uint16_t Tsi::read(uint8_t channel)
{
    *_s_gencs |= TSI_GENCS_SWTS;
    while (scanning());
    while (!eos());
    *_s_gencs |= TSI_GENCS_EOSF;
    return _s_cntr[channel];
}

bool Tsi::isValid(tTouch const & touch)
{
    return
        (touch.nscn <= _s_max_touch.nscn) &&
        (touch.ps <= _s_max_touch.ps) &&
        (touch.refchrg <= _s_max_touch.refchrg) &&
        (touch.extchrg <= _s_max_touch.extchrg) &&
        (touch.threshold <= _s_max_touch.threshold);
}

bool Tsi::set(tTouch const & touch)
{
    if (!isValid(touch))
        return false;

    _touch = touch;

    (void)_eeprom.setTouch(_touch);
    configure();

    return true;
}

void Tsi::get(tTouch & touch) const
{
    touch = _touch;
}

void Tsi::defaults(tTouch & touch)
{
    touch = _s_default_touch;
}

bool Tsi::configure(uint8_t nscn, uint8_t ps, uint8_t refchrg, uint8_t extchrg)
{
    tTouch t;
    t.nscn = nscn;
    t.ps = ps;
    t.refchrg = refchrg;
    t.extchrg = extchrg;
    t.threshold = _touch.threshold;

    if (!isValid(t))
        return false;

    _touch = t;
    configure();

    return true;
}

void Tsi::configure(void)
{
    stop();

    // REFCHRG, EXTCHRG
    *_s_scanc = ((uint32_t)_touch.refchrg << 24) | ((uint32_t)_touch.extchrg << 16);

    // NSCAN, PS, STM (Scan Trigger Mode) - set for periodic scan
    // Doesn't make sense to have it scanning continuously
    // and I couldn't get it to work, so use soft trigger.
    //*_s_gencs = ((uint32_t)_touch.nscn << 19) | ((uint32_t)_touch.ps << 16) | TSI_GENCS_STM;
    //*_s_gencs = ((uint32_t)_touch.nscn << 19) | ((uint32_t)_touch.ps << 16);
    // Add Error interrupt mainly for short circuit detection of electrode
    *_s_gencs = ((uint32_t)_touch.nscn << 19) | ((uint32_t)_touch.ps << 16) | TSI_GENCS_ERIE;

    start();
}

void Tsi::softTrigger(void)
{
    stop();
    *_s_gencs &= ~TSI_GENCS_STM;
    start();
}

