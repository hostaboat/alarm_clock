#ifndef _TSI_H_
#define _TSI_H_

#include "module.h"
#include "pin.h"
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

class Tsi : public Module
{
    public:
        class Channel
        {
            friend Tsi;

            public:
                uint16_t read(void)
                {
                    //while (_tsi.scanning());
                    return _tsi.read(_channel);
                }

                Channel(Channel const &) = delete;
                Channel & operator=(Channel const &) = delete;

            private:
                Channel(uint8_t channel, Tsi & tsi) : _channel(channel), _tsi(tsi) {}
                uint8_t _channel;
                Tsi & _tsi;
        };

        template < pin_t PIN >
        static Channel & acquire(void)
        {
            static_assert((PIN < sizeof(_s_pin_to_channel))
                    && (_s_pin_to_channel[PIN] != 255), "Touch : Invalid Pin");

            Tsi & tsi = acquire();

            static Channel ch(_s_pin_to_channel[PIN], tsi);

            tsi.stop();
            *tsi._s_pen |= (1 << _s_pin_to_channel[PIN]);
            tsi.start();

            return ch;
        }

        void start(void) { *_s_gencs |= TSI_GENCS_TSIEN; }
        void stop(void) { while (scanning()); *_s_gencs &= ~TSI_GENCS_TSIEN; }
        bool running(void) { return *_s_gencs & TSI_GENCS_TSIEN; }

        bool scanning(void) { return *_s_gencs & TSI_GENCS_SCNIP; }
        uint16_t read(uint8_t channel)
        {
            *_s_gencs |= TSI_GENCS_SWTS;
            while (scanning());
            return _s_cntr[channel];
        }

        Tsi(Tsi const &) = delete;
        Tsi & operator=(Tsi const &) = delete;

    private:
        static Tsi & acquire(void) { static Tsi tsi; return tsi; }

        Tsi(void)
        {
            Module::enable(MOD_TSI);

            *_s_gencs = 0;
            configure();
            start();
        }

        void configure(void)
        {
            // REFCHRG = 8uA, EXTCHRG = 6uA
            *_s_scanc = (0x03 << 24) | (0x02 << 16);

            // NSCAN = 10 times per electrode
            // PS = 4 (Electrode Oscillator Frequency divided by N) 
            // STM (Scan Trigger Mode) - periodic scan
            //*_s_gencs = (0x09 << 19) | (0x02 << 16) | TSI_GENCS_STM;
            // Doesn't make sense to have it scanning continuously so use soft trigger
            *_s_gencs = (0x09 << 19) | (0x02 << 16);
        }

        void softTrigger(void)
        {
            stop();
            *_s_gencs &= ~TSI_GENCS_STM;
            start();
        }

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

// These settings give approx 0.02 pF sensitivity and 1200 pF range
// Lower current, higher number of scans, and higher prescaler
// increase sensitivity, but the trade-off is longer measurement
// time and decreased range.
#define CURRENT   2 // 0 to 15 - current to use, value is 2*(current+1)
#define NSCAN     9 // number of times to scan, 0 to 31, value is nscan+1
#define PRESCALE  2 // prescaler, 0 to 7 - value is 2^(prescaler+1)

// output is approx pF * 50
// time to measure 33 pF is approx 0.25 ms
// time to measure 1000 pF is approx 4.5 ms

#endif
