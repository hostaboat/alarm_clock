#ifndef _TSI_H_
#define _TSI_H_

#include "module.h"
#include "pin.h"
#include "eeprom.h"
#include "utility.h"
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

using tTouch = eTouch;

extern "C" void tsi0_isr(void);

class Tsi : public Module
{
    friend void tsi0_isr(void);

    public:
        class Channel
        {
            friend class Tsi;

            public:
                bool touched(void) { return _tsi.touched(_channel); }
                uint16_t read(void) { return _tsi.read(_channel); }

                Channel(Channel const &) = delete;
                Channel & operator=(Channel const &) = delete;

            private:
                Channel(uint8_t channel, Tsi & tsi) : _channel(channel), _tsi(tsi) {}
                uint8_t _channel;
                Tsi & _tsi;
        };

        template < pin_t PIN >
        Channel & acquire(void)
        {
            static_assert((PIN < sizeof(_s_pin_to_channel))
                    && (_s_pin_to_channel[PIN] != 255), "Touch : Invalid Pin");

            static Channel ch(_s_pin_to_channel[PIN], *this);

            if (!(*_s_pen & (1 << _s_pin_to_channel[PIN])))
            {
                stop();
                *_s_pen |= (1 << _s_pin_to_channel[PIN]);
                start();
            }

            return ch;
        }

        // Enable Low Power Scan Pin
        template < pin_t PIN >
        void enableLPSP(void)
        {
            static_assert((PIN < sizeof(_s_pin_to_channel))
                    && (_s_pin_to_channel[PIN] != 255), "Touch : Invalid Pin");

            stop();
            *_s_gencs &= ~TSI_GENCS_ESOR;
            *_s_threshold = (uint32_t)_touch.threshold;
            *_s_pen |= (uint32_t)_s_pin_to_channel[PIN] << 16;
            start();
        }

        void disableLPSP(void)
        {
            stop();
            *_s_gencs |= TSI_GENCS_ESOR;
            *_s_pen &= ~(0x0F << 16);
            start();
        }

        void clearIntr(void);

        static Tsi & acquire(void) { static Tsi tsi; return tsi; }

        bool valid(void) { return !_s_error && !_s_overrun; }

        void start(void);
        void stop(void);
        bool running(void);

        static bool isValid(tTouch const & touch);

        bool setConfig(tTouch const & config);
        void getConfig(tTouch & config) const;
        void defaults(tTouch & config);
        bool configure(uint8_t nscn, uint8_t ps, uint8_t refchrg, uint8_t extchrg);

        uint16_t threshold(void) const { return _touch.threshold; }

        // Max values - these are the values written to and retrieved
        // from the TSI registers.
        static uint8_t maxNscn(void) { return _s_max_touch.nscn; }
        static uint8_t maxPs(void) { return _s_max_touch.ps; }
        static uint8_t maxRefChrg(void) { return _s_max_touch.refchrg; }
        static uint8_t maxExtChrg(void) { return _s_max_touch.extchrg; }
        static uint16_t maxThreshold(void) { return _s_max_touch.threshold; }

        // These are the actual values that correspond to what is written to
        // or read from the registers.
        static uint8_t valNscn(uint8_t nscn) { return nscn + 1; }
        static uint8_t valPs(uint8_t ps) { return 1 << ps; }
        static uint8_t valRefChrg(uint8_t refchrg) { return (refchrg + 1) * 2; }
        static uint8_t valExtChrg(uint8_t extchrg) { return (extchrg + 1) * 2; }

        Tsi(Tsi const &) = delete;
        Tsi & operator=(Tsi const &) = delete;

    private:
        Tsi(void);

        bool scanning(void);
        bool eos(void); // End Of Scan

        bool touched(uint8_t channel);
        uint16_t read(uint8_t channel);
        void configure(void);
        void softTrigger(void);

        static bool volatile _s_error;
        static bool volatile _s_overrun;
        static bool volatile _s_eos;
        static bool volatile _s_outrgf;

        // Software triggered scan
        bool _swts = false;

        // NSCN = 10 times per electrode
        // PS = 4 (Electrode Oscillator Frequency divided by N)
        // REFCHRG = 16uA
        // EXTCHRG = 32uA
        // Threshold = 352
        // These settings give approx 0.05pF sensitivity and about a 17.5pF electrode
        // capacitance, 1.8MHz electrode frequency and 20us sample time when touched.
        static constexpr tTouch const _s_default_touch{ 0x09, 0x02, 0x07, 0x0F, 0x0160 };

        // NSCN = 32 times per electrode (0 - 31)
        // PS = 128 (0 - 7)
        // REFCHRG = 32uA (0 - 15)
        // EXTCHRG = 32uA (0 - 15)
        // Threshold = 65535
        static constexpr tTouch const _s_max_touch{ 0x1F, 0x07, 0x0F, 0x0F, 65535 };

        // Add Error interrupt mainly for short circuit detection of electrode and
        // Touch Sensing interrupt for end of scan since depending on configuration,
        // e.g. with high NSCN or PS, the scan may take too long to wait for.
        static constexpr uint32_t const _s_gencs_flags =
            TSI_GENCS_ERIE | TSI_GENCS_TSIIE |  // Interrupt flags
            TSI_GENCS_ESOR |  // End of Scan Interrupt
            TSI_GENCS_STPE |  // Continue running in low-power modes
            TSI_GENCS_EOSF | TSI_GENCS_OUTRGF | TSI_GENCS_EXTERF | TSI_GENCS_OVRF; // Clear w1c flags

        tTouch _touch{_s_default_touch};

        Eeprom & _eeprom = Eeprom::acquire();

        static constexpr uint8_t const _s_pin_to_channel[34] =
        {
         //   0    1    2    3    4    5    6    7    8    9
              9,  10, 255, 255, 255, 255, 255, 255, 255, 255,
            255, 255, 255, 255, 255,  13,   0,   6,   8,   7,
            255, 255,  14,  15, 255,  12, 255, 255, 255, 255,
            255, 255,  11,   5
        };

        static reg32 _s_conf_base;
        static reg32 _s_gencs;
        static reg32 _s_scanc;
        static reg32 _s_pen;
        static reg32 _s_wucntr;
        static reg32 _s_cntr_base;
        static reg16 _s_cntr;
        //static reg32 _s_cntr1;
        //static reg32 _s_cntr3;
        //static reg32 _s_cntr5;
        //static reg32 _s_cntr7;
        //static reg32 _s_cntr9;
        //static reg32 _s_cntr11;
        //static reg32 _s_cntr13;
        //static reg32 _s_cntr15;
        static reg32 _s_threshold;
};

////////////////////////////////////////////////////////////////////////////////
// TSI Calculations
//
// F_elec = I_elec / (2 * C_elec * V_delta)
//
// F_elec = electrode oscillator frequency
// I_elec = electrode oscillator constant current
// C_elec = electrode capacitance
// V_delta = hysteresis delta voltage
//
// T_cap_samp = (PS * NSCN) / F_elec
//            = (2 * PS * NSCN * C_elec * V_delta) / I_elec
//
// T_cap_samp = pin capacitance sampling time
// PS = electrode oscillator prescaler - GENCS[PS]
// NSCN = number of consecutive electrode scans - GENCS[NSCN]
//
// F_ref = I_ref / (2 * C_ref * V_delta)
//
// F_ref = reference oscillator frequency
// I_ref = reference oscillator constant current
// C_ref = internal reference capacitor
// V_delta = hysteresis delta volatage
//
// TSI_CHn_CNT = T_cap_samp * F_ref
//             = ((I_ref * PS * NSCN) / (C_ref * I_elec)) * C_elec
//
// Below taken from:
//  K20P64M72SF1 - Rev. 3, 11/2012
//  K20 Sub-Family Data Sheet
//
// Values other than C_elec assume C_elec = 20pF
//
//              Min.    Typ.    Max.    Unit
// C_elec         1      20     500       pF
// F_ref_max    ----      8      15      MHz
// F_elec_max   ----      1     1.8      MHz
// C_ref        ----      1     ----      pF
// V_delta      ----    500     ----      mV
// I_ref
//   2uA        ----      2       3       uA
//  32uA        ----     36      50       uA
// I_elec
//   2uA        ----      2       3       uA
//  32uA        ----     36      50       uA
//
// Seems odd that for 32uA the typical would be 36uA
//
// Sensitivity is capacitance change per one count of TSI_CHn_CNT.
//  Sensitivity = (C_ref * I_elec) / (I_ref * PS * NSCN)
// Lower value == higher sensitivity
// Capacitance of electrode can be measured by:
//  C_elec = TSI_CHn_CNT * Sensitivity
// Maximum capacitance of electrode is where TSI_CHn_CNT is at its maximum = 65535
//  C_elec[MAX] = 65535 * Sensitivity
// Higher sensitivity (i.e. lower value) -> lower capacitance range
// Beyond this, I'm not sure whether or not the value will wrap or stay at 65535.
// Either way, undesirable behaviour.
////////////////////////////////////////////////////////////////////////////////

#endif
