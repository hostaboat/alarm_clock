#ifndef _LLWU_H_
#define _LLWU_H_

#include "pin.h"
#include "armv7m.h"
#include "lptmr.h"
#include "types.h"

// WakeUp Pin Enable detection
enum wupe_e { WUPE_NONE, WUPE_RISING, WUPE_FALLING, WUPE_CHANGE };

// WakeUp Source
enum wus_e { WUS_ERROR = -1, WUS_NONE = 0, WUS_PIN, WUS_MODULE };

// WakeUp Pin Source
enum wups_e
{
    WUPS_NO = -1,
    WUPS_0  =  0,
    WUPS_1,
    WUPS_2,
    WUPS_3,
    WUPS_4,
    WUPS_5,
    WUPS_6,
    WUPS_7,
    WUPS_8,
    WUPS_9,
    WUPS_10,
    WUPS_11,
    WUPS_12,
    WUPS_13,
    WUPS_14,
    WUPS_15
};

// WakeUp Module Source
enum wums_e
{
    WUMS_LPTMR,
    WUMS_CMP0,
    WUMS_CMP1,
    WUMS_CMP2,
    WUMS_TSI,
    WUMS_RTC_ALARM,
    WUMS_RSVD,
    WUMS_RTC_SECONDS,
    WUMS_NOT
};

extern "C" void wakeup_isr(void);

class Llwu 
{
    friend void wakeup_isr(void);

    public:
        static Llwu & acquire(void) { static Llwu llwu; return llwu; }

        void enable(void);
        void disable(void);
        bool enabled(void);

        wus_e sleep(void);

        wus_e wakeupSource(void) const { return _s_wakeup_source; }
        wums_e wakeupModule(void) const { return _s_wakeup_module; }
        int8_t wakeupPin(void) const { return _s_wakeup_pin; }

        template < pin_t PIN >
        bool pinsEnable(PinInputX < PIN > const & pin, irqc_e irqc) { return pinEnable(pin, irqc); }

        template < pin_t PIN, class... PINs >
        bool pinsEnable(PinInputX < PIN > const & pin, irqc_e irqc, PINs const &... pins)
        {
            if (!pinEnable(pin, irqc)) return false;
            return pinsEnable(pins...);
        }

        template < pin_t PIN >
        bool pinEnable(PinInputX < PIN > const & pin, irqc_e irqc)
        {
            if (!enabled() || (_pin_to_wups[PIN] == WUPS_NO) || (irqc == IRQC_DISABLED))
                return false;

            uint8_t rnum = _pin_to_wups[PIN] / 4;
            uint8_t rshift = (_pin_to_wups[PIN] % 4) * 2;
            _s_pe1[rnum] |= _irqc_to_wupe[irqc] << rshift;

            return true;
        }

        template < pin_t PIN >
        void pinsDisable(PinInputX < PIN > const & pin) { return pinDisable(pin); }

        template < pin_t PIN, class... PINs >
        void pinsDisable(PinInputX < PIN > const & pin, PINs const &... pins)
        {
            pinDisable(pin);
            return pinsDisable(pins...);
        }

        template < pin_t PIN >
        void pinDisable(PinInputX < PIN > const & pin)
        {
            if (!enabled() || (_pin_to_wups[PIN] == WUPS_NO))
                return;

            uint8_t rnum = _pin_to_wups[PIN] / 4;
            uint8_t rshift = (_pin_to_wups[PIN] % 4) * 2;
            _s_pe1[rnum] &= ~(WUPE_CHANGE << rshift);
        }

        bool timerEnable(uint16_t msecs);
        bool timerDisable(void);

        Llwu(const Llwu &) = delete;
        Llwu & operator=(const Llwu &) = delete;

    private:
        Llwu(void) {}

        void stopMode(void);
#define SMC_PMCTRL_STOPM_MASK    (uint8_t)0x07
#define SMC_PMCTRL_STOPM(val)    (uint8_t)((val) & 0x07)
#define SMC_PMCTRL_STOPM_LLS   SMC_PMCTRL_STOPM(0x03)
        uint8_t _stop_mode = SMC_PMCTRL_STOPM_LLS;

        static wus_e volatile _s_wakeup_source;
        static wums_e volatile _s_wakeup_module;
        static int8_t volatile _s_wakeup_pin;
        static Lptmr * _s_lptmr;

        static reg8 _s_base;
        static reg8 _s_pe1;
        static reg8 _s_pe2;
        static reg8 _s_pe3;
        static reg8 _s_pe4;
        static reg8 _s_me;
        static reg8 _s_f1;
        static reg8 _s_f2;
        static reg8 _s_f3;
        //static reg8 _s_filt1;
        //static reg8 _s_filt2;
        //static reg8 _s_rst;

        static reg8 _s_smc_base;
        //static reg8 _s_smc_pmprot;
        static reg8 _s_smc_pmctrl;
        //static reg8 _s_smc_vllsctrl;
        //static reg8 _s_smc_pmstat;

        static reg8 _s_mcg_base;
        static reg8 _s_mcg_c1;
        static reg8 _s_mcg_c2;
        //static reg8 _s_mcg_c3;
        //static reg8 _s_mcg_c4;
        //static reg8 _s_mcg_c5;
        static reg8 _s_mcg_c6;
        static reg8 _s_mcg_s;
        //static reg8 _s_mcg_sc;
        //static reg8 _s_mcg_atcvh;
        //static reg8 _s_mcg_atcvl;
        //static reg8 _s_mcg_c7;
        //static reg8 _s_mcg_c8;

        wups_e const _pin_to_wups[PIN_CNT] =
        {
            WUPS_NO, WUPS_NO, WUPS_12, WUPS_NO, WUPS_4,  WUPS_NO, WUPS_14, WUPS_13,
            WUPS_NO, WUPS_7,  WUPS_8,  WUPS_10, WUPS_NO, WUPS_9,  WUPS_NO, WUPS_NO,
            WUPS_5,  WUPS_NO, WUPS_NO, WUPS_NO, WUPS_NO, WUPS_15, WUPS_6,  WUPS_NO,
        };

        static constexpr int8_t const _s_wups_to_pin[16] =
        {
            -1, -1, -1, -1, 4, 16, 22, 9, 10, 13, 11, -1, -1, 7, 6, 21,
        };

        wupe_e const _irqc_to_wupe[IRQC_MASK+1] =
        {
            WUPE_NONE,  // IRQC DISABLED
            WUPE_RISING, WUPE_FALLING, WUPE_CHANGE,  // Not sure about these since they are DMA
            WUPE_NONE, WUPE_NONE, WUPE_NONE, WUPE_NONE,  // IRQC RES - These are reserved IRQC
            WUPE_NONE,  // IRQC LOW - Can't enable for just low
            WUPE_RISING, WUPE_FALLING, WUPE_CHANGE,  // IRQC RISING, FALLING, CHANGE
            WUPE_NONE,  // IRQC HIGH - Can't enable for just high
            WUPE_NONE, WUPE_NONE, WUPE_NONE,  // Invalid values
        };
};

#endif
