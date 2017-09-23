#include "tsi.h"
#include "types.h"

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

constexpr tTouch const Tsi::_s_default_touch;

bool volatile Tsi::_s_error = false;
bool volatile Tsi::_s_overrun = false;
bool volatile Tsi::_s_eos = false;
bool volatile Tsi::_s_outrgf = false;

void tsi0_isr(void)
{
    if (*Tsi::_s_gencs & TSI_GENCS_OVRF)
    {
        Tsi::_s_overrun = true;
        *Tsi::_s_gencs |= TSI_GENCS_OVRF;
    }

    if (*Tsi::_s_gencs & TSI_GENCS_EXTERF)
    {
        Tsi::_s_error = true;
        *Tsi::_s_gencs |= TSI_GENCS_EXTERF;
    }

    if (*Tsi::_s_gencs & TSI_GENCS_OUTRGF)
    {
        Tsi::_s_outrgf = true;
        *Tsi::_s_gencs |= TSI_GENCS_OUTRGF;
    }

    if (*Tsi::_s_gencs & TSI_GENCS_EOSF)
    {
        Tsi::_s_eos = true;
        *Tsi::_s_gencs |= TSI_GENCS_EOSF;
    }
}

void Tsi::clear(void)
{
    *_s_gencs |= TSI_GENCS_OVRF | TSI_GENCS_EXTERF | TSI_GENCS_OUTRGF | TSI_GENCS_EOSF;
    _s_overrun = _s_error = _s_outrgf = _s_eos = false;
    NVIC::clearPending(IRQ_TSI);
}

Tsi::Tsi(void)
{
    if (!_eeprom.getTouch(_touch) || !isValid(_touch))
    {
        _touch = _s_default_touch;
        (void)_eeprom.setTouch(_touch);
    }

    Module::enable(MOD_TSI);
    *_s_gencs = 0;

    configure();
}

void Tsi::start(void)
{
    _s_error = _s_overrun = _s_eos = _swts = false;
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

// Returns 0 until scan is finished
uint16_t Tsi::read(uint8_t channel)
{
    // Use variable so that scans are synced with end of scan
    if (!_swts)
    {
        *_s_gencs |= TSI_GENCS_SWTS;
        _swts = true;
    }

    NVIC::disable(IRQ_TSI);
    bool eos = _s_eos;
    if (_s_eos) _s_eos = false;
    NVIC::enable(IRQ_TSI);

    if (!eos)
        return 0;

    _swts = false;

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
    touch = _touch = _s_default_touch;
    configure();
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
    *_s_gencs = ((uint32_t)_touch.nscn << 19) | ((uint32_t)_touch.ps << 16) | _s_gencs_flags;

    start();
}

void Tsi::softTrigger(void)
{
    stop();
    *_s_gencs &= ~TSI_GENCS_STM;
    start();
}

