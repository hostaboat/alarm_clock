#include "pit.h"
#include "armv7m.h"
#include "utility.h"
#include "types.h"

////////////////////////////////////////////////////////////////////////////////
// Static Members //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
reg32 Pit::_s_mcr = (reg32)0x40037000;
reg32 Pit::_s_pit0 = (reg32)0x40037100;
reg32 Pit::_s_pit1 = (reg32)0x40037110;
reg32 Pit::_s_pit2 = (reg32)0x40037120;
reg32 Pit::_s_pit3 = (reg32)0x40037130;

PITRegs volatile * const Pit::_s_pit_regs[PIT_CNT] = 
{
    reinterpret_cast < PITRegs volatile * const > (_s_pit0),
    reinterpret_cast < PITRegs volatile * const > (_s_pit1),
    reinterpret_cast < PITRegs volatile * const > (_s_pit2),
    reinterpret_cast < PITRegs volatile * const > (_s_pit3),
};

Pit Pit::_s_timers[PIT_CNT];
uint8_t Pit::_s_taken = 0;

////////////////////////////////////////////////////////////////////////////////
// Static Methods //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
Pit * Pit::acquire(tcf_t cfunc, uint32_t interval, uint8_t priority)
{
    if (cfunc == nullptr)
        return nullptr;

    uint32_t trigger = getTrigger(interval);
    if (trigger == 0)
        return nullptr;

    uint8_t pit;
    for (pit = PIT_0; pit < PIT_CNT; pit++)
    {
        if (_s_timers[pit]._pit == PIT_CNT)
            break;
    }

    if (pit == PIT_CNT)
        return nullptr;

    if (0 == _s_taken++)
    {
        Module::enable(MOD_PIT);
        *_s_mcr = 0;
    }

    Pit & pt = _s_timers[pit];

    pt._pit = (pit_e)pit;
    pt._interval = interval;
    pt._trigger = trigger;
    pt._priority = priority;
    pt._cfunc = cfunc;
    pt.enable();

    return &pt;
}

uint32_t Pit::getTrigger(uint32_t interval)
{
    if (interval == 0)
        return 0;

    // Per the Freescale Reference Manual:
    // LDVAL trigger = (period / clock period) - 1
    //               = (period * clock frequency) - 1
    // period = interval / 1000000  (in seconds)
    // clock frequency = F_BUS
    // LDVAL trigger = ((interval / 1000000) * F_BUS) - 1
    //               = (interval * (F_BUS / 1000000)) - 1
    // To avoid accidental overflow and fractions, divide F_BUS by 1000000 first
    // since the result will be a small integer.
    // F_BUS is assumed to be >= and divisible by 1000000 which for a Teensy3 will always be true.
    uint32_t trigger = (interval * (F_BUS / 1000000)) - 1;

    // Microseconds too large causing overflow.
    if (trigger <= interval)
        trigger = 0;

    return trigger;
}

////////////////////////////////////////////////////////////////////////////////
// Public Methods //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void Pit::release(void)
{
    disable();

    _pit = PIT_CNT;
    _trigger = 0;
    _interval = 0;
    _priority = NVIC_DEF_PRI;
    _cfunc = nullptr;

    if (--_s_taken == 0)
    {
        *_s_mcr = PIT_MCR_MDIS;
        Module::disable(MOD_PIT);
    }
}

void Pit::enable(void)
{
    NVIC::setPriority((irq_e)(IRQ_PIT_CH0 + _pit), _priority);
    NVIC::enable((irq_e)(IRQ_PIT_CH0 + _pit));
}

void Pit::disable(void)
{
    stop();

    NVIC::disable((irq_e)(IRQ_PIT_CH0 + _pit));
    NVIC::setPriority((irq_e)(IRQ_PIT_CH0 + _pit), _priority);
}

bool Pit::enabled(void)
{
    return Module::enabled(MOD_PIT) && NVIC::isEnabled((irq_e)(IRQ_PIT_CH0 + _pit));
}

void Pit::start(void)
{
    _s_pit_regs[_pit]->ldval = _trigger;
    resume();
}

bool Pit::running(void)
{
    return _s_pit_regs[_pit]->tctrl & PIT_TCTRL_TEN;
}

void Pit::pause(void)
{
    _s_pit_regs[_pit]->tctrl = 0;
}

void Pit::resume(void)
{
    _s_pit_regs[_pit]->tctrl = PIT_TCTRL_TIE;
    _s_pit_regs[_pit]->tctrl |= PIT_TCTRL_TEN;
}

void Pit::restart(void)
{
    pause();
    _s_pit_regs[_pit]->ldval = _trigger;
    resume();
}

void Pit::stop(void)
{
    pause();
}

bool Pit::updateCallback(tcf_t cfunc, bool restart)
{
    if (cfunc == nullptr)
        return false;

    NVIC::disable((irq_e)(IRQ_PIT_CH0 + _pit));
    _cfunc = cfunc;
    NVIC::enable((irq_e)(IRQ_PIT_CH0 + _pit));

    if (restart)
        this->restart();

    return true;
}

bool Pit::updateInterval(uint32_t interval, bool restart)
{
    uint32_t trigger = getTrigger(interval);

    if (trigger == 0)
        return false;

    _trigger = trigger;

    if (restart)
        this->restart();
    else
        _s_pit_regs[_pit]->ldval = _trigger;

    return true;
}

void Pit::updatePriority(uint8_t priority)
{
    _priority = priority;
    NVIC::setPriority((irq_e)(IRQ_PIT_CH0 + _pit), priority);
}

uint32_t Pit::getTimeLeft(void) const
{
    uint32_t cval = _s_pit_regs[_pit]->cval;
    return (uint32_t)((cval + 1) * (float)(1000000 / F_BUS));
}

////////////////////////////////////////////////////////////////////////////////
// PIT Interrupt Service Routines //////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void pit0_isr(void)
{
    Pit::pit_isr < PIT_0 > ();
}

void pit1_isr(void)
{
    Pit::pit_isr < PIT_1 > ();
}

void pit2_isr(void)
{
    Pit::pit_isr < PIT_2 > ();
}

void pit3_isr(void)
{
    Pit::pit_isr < PIT_3 > ();
}

