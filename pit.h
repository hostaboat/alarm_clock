#ifndef _PIT_H_
#define _PIT_H_

#include "armv7m.h"
#include "module.h"
#include "types.h"

#define PIT_MCR_MDIS            (1 << 1)  // Module disable
#define PIT_MCR_FRZ             (1 << 0)
#define PIT_TCTRL_CHN           (1 << 2)  // Chain Mode
#define PIT_TCTRL_TIE           (1 << 1)  // Timer Interrupt Enable
#define PIT_TCTRL_TEN           (1 << 0)  // Timer Enable
#define PIT_TFLG_TIF            (1 << 0) 

extern "C"
{
    void pit0_isr(void);
    void pit1_isr(void);
    void pit2_isr(void);
    void pit3_isr(void);
}

struct PITRegs
{
    uint32_t volatile ldval;
    uint32_t volatile cval;
    uint32_t volatile tctrl;
    uint32_t volatile tflg;
};

enum pit_e : uint8_t
{
    PIT_0,
    PIT_1,
    PIT_2,
    PIT_3,
    PIT_CNT
};

using tcf_t = void (*)(void);  // Timer Callback Function

class Pit : public Module
{
    friend void pit0_isr(void);
    friend void pit1_isr(void);
    friend void pit2_isr(void);
    friend void pit3_isr(void);

    public:
        // Interval is in microseconds
        static Pit * acquire(tcf_t cfunc, uint32_t interval, uint8_t prioriy = NVIC_DEF_PRI);
        static uint32_t getTrigger(uint32_t interval);

        bool enabled(void);

        void release(void);
        void start(void);
        void stop(void);
        bool running(void);
        void pause(void);
        void resume(void);
        void restart(void);

        bool updateCallback(tcf_t cfunc, bool restart = true);
        bool updateInterval(uint32_t interval, bool restart = true);
        void updatePriority(uint8_t priority);

        uint32_t getInterval(void) const { return _interval; }
        uint8_t getPriority(void)  const { return _priority; }
        uint32_t getTimeLeft(void) const;
        uint32_t getTrigger(void)  const { return _trigger; }

        Pit(Pit const &) = delete;
        Pit & operator=(Pit const &) = delete;

    private:
        Pit(void) {}

        void enable(void);
        void disable(void);

        static reg32 _s_mcr;
        static reg32 _s_pit0;
        static reg32 _s_pit1;
        static reg32 _s_pit2;
        static reg32 _s_pit3;
        static PITRegs volatile * const _s_pit_regs[PIT_CNT];
        static Pit _s_timers[PIT_CNT];
        static uint8_t _s_taken;

        template < pit_e PIT > __attribute__((always_inline))
        static void pit_isr(void)
        {
            Pit const & pit = _s_timers[PIT];
            _s_pit_regs[PIT]->tflg = PIT_TFLG_TIF;  // Clear interrupt flag - w1c
            pit._cfunc();
        }

        pit_e _pit = PIT_CNT;
        uint32_t _trigger = 0;
        uint32_t _interval = 0;
        uint8_t _priority = NVIC_DEF_PRI;
        tcf_t _cfunc = nullptr;
};

#endif
