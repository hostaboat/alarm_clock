#ifndef _UTILITY_H_
#define _UTILITY_H_

#include "types.h"
#include "armv7m.h"

#define MFC(member_func) ((*this).*(member_func))

// Sets the PRIMASK to 1, raising the execution priority to 0
// effectively making the current executing handler uninterruptable
// except by Reset, NMI and HardFault exceptions.
#define __disable_irq() __asm__ volatile ("CPSID i":::"memory");
// ARM states that no memory barrier is needed.
//#define __disable_irq() __asm__ volatile ("CPSID i");

// Sets PRIMASK to 0, thus setting execution priority back to what
// it is configured as.
#define __enable_irq()  __asm__ volatile ("CPSIE i":::"memory");
// ARM states that no memory barrier is needed for Cortex-M when
// __disable_irq() immediately follows.  However  one may want to
// insert an ISB instruction to ensure any pending interrupts are
// taken before code after is executed.  For Cortex-M4 this could
// be up to 2 instructions.
//#define __enable_irq()  __asm__ volatile ("CPSIE i");
//#define __enable_irq()  __asm__ volatile ("CPSIE i\n\t ISB");

inline uint32_t msecs(void)
{
    return SysTick::msecs();
}

inline uint32_t usecs(void)
{
    __disable_irq();

    uint32_t ms = SysTick::msecs();  // Won't increment while interrupts disabled
    uint32_t cv = SysTick::currentValue();  // Read this before reading pending
    bool pending = SCB::sysTickIntrPending();

    __enable_irq();

    // If there is an interrupt pending then CVR reached zero before reading CVR,
    // CVR was zero when reading CVR or CVR reached zero after reading CVR. Only
    // off by a couple of ticks in any case (including the below case where there
    // is no pending interrupt) so just add an extra millisecond.
    if (pending)
        return (ms + 1) * 1000;

    return (ms * 1000) + ((TICKS_PER_MSEC - cv) / TICKS_PER_USEC);
}

inline void delay_msecs(uint32_t ms)
{
    if (ms == 0)
        return;

    uint32_t us = usecs();

    while (ms > 0)
    {
        if ((usecs() - us) >= 1000)
        {
            ms--;
            us += 1000;
        }
    }
}

inline void delay_usecs(uint32_t us)
{
    us *= F_CPU / 2000000;  // 1000000 - 3000000
    if (us == 0)
        return;

    __asm__ volatile
        (
         "L_%=_delay_usecs:\n\t"
         "subs %0, #1\n\t"
         "bne L_%=_delay_usecs\n\t" : "+r" (us)
        );
}

// Taken from FastLED code - see opti.* for license
template < uint16_t WAIT >
class UsMinWait
{
    public:
        UsMinWait(void) {}
        void wait(void) { while (((usecs() & 0xFFFF) - _last_micros) < WAIT); }
        void mark(void) { _last_micros = usecs() & 0xFFFF; }

    private:
        uint16_t _last_micros = 0;
};

class Toggle
{
    public:
        Toggle(void) { reset(); }
        Toggle(uint32_t time) { reset(time); }
        Toggle(uint32_t on_time, uint32_t off_time) { reset(on_time, off_time); }

        virtual bool toggled(void)
        {
            if (_disabled)
                return false;

            uint32_t cm = msecs();

            if ((cm - _last_msec) < _toggle_time)
                return false;

            _on = !_on;
            _toggle_time = _on ? _on_time : _off_time;
            _last_msec = cm;

            return true;
        }

        virtual bool on(void) { return (_disabled ? false : _on); }
        virtual bool off(void) { return (_disabled ? true : !_on); }

        virtual void disable(void) { _disabled = true; }
        virtual void enable(void) { _disabled = false; }

        virtual void reset(void) { reset(_on_time, _off_time); }
        virtual void reset(uint32_t time) { reset(time, time); }

        virtual void reset(uint32_t on_time, uint32_t off_time)
        {
            _on_time = on_time;
            _off_time = off_time;
            _toggle_time = _on_time;
            _last_msec = msecs();
            _on = true;
            _disabled = false;
        }

    protected:
        uint32_t _toggle_time = 1000;  // Arbitrary 1 second
        uint32_t _on_time = _toggle_time;
        uint32_t _off_time = _toggle_time;

    private:
        uint32_t _last_msec = 0;
        bool _on = true;
        bool _disabled = false;
};

#endif

