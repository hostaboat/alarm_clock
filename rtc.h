#ifndef _RTC_H_
#define _RTC_H_

#include "module.h"
#include "eeprom.h"
#include "armv7m.h"
#include "types.h"

using tClock = eClock;
using tAlarm = eAlarm;
using tTimer = eTimer;

extern "C" void rtc_seconds_isr(void);
//extern "C" void rtc_alarm_isr(void);

// RTC Update
// Returned when update() is called indicating what was updated
// With any value returned, values less than it were also updated
enum ru_e : uint8_t
{
    RU_NO,
    RU_SEC,
    RU_MIN,
    RU_HOUR,
    RU_DAY,
    RU_MON,
    RU_YEAR,
};

class Rtc : public Module
{
    friend void rtc_seconds_isr(void);
    //friend void rtc_alarm_isr(void);

    public:
        static Rtc & acquire(void) { static Rtc rtc; return rtc; }

        void enable(void) { if (Module::enabled(MOD_RTC)) return; Module::enable(MOD_RTC); }
        bool enabled(void) { return Module::enabled(MOD_RTC) && enabledIrq(); }
        void disable(void) { if (!enabled()) return; stop(); disableIrq(); Module::disable(MOD_RTC); }

        void start(void);
        void stop(void) { if (!running()) return; *_s_cr = 0; }
        bool running(void);

        // Tell the clock that the MCU is going to sleep since interrupts will be
        // disabled.  Since the RTC_TSR counter will continue incrementing sleep
        // saves off the current value so a diff can be done with RTC_TSR when
        // waking/resuming.  Call sleep() before putting the MCU to sleep and
        // wake() after MCU wakes up.
        void sleep(void);
        void wake(void);

        // Updates the clock utilizing the RTC on the Teensy
        ru_e update(void);

        static bool isLeapYear(uint16_t year)
        { return ((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0)); }

        ////////////////////////////////////////////////////////////////////////
        // Clock ///////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        bool setClock(tClock const & clock);
        void getClock(tClock & clock) const;
        uint8_t clockSecond(void) const { return (uint8_t)_clock_second; }
        uint8_t clockMinute(void) const { return (uint8_t)_clock_minute; }
        uint8_t clockHour(void) const { return (uint8_t)_clock_hour; }
        uint8_t clockDay(void) const { return (uint8_t)_clock_day; }
        uint8_t clockMonth(void) const { return (uint8_t)_clock_month; }
        uint16_t clockYear(void) const { return (uint16_t)_clock_year; }
        uint8_t clockType(void) const { return _clock_type; }
        bool clockIs12H(void) const { return _clock_type == clock12H(); }
        bool clockIs24H(void) const { return _clock_type == clock24H(); }
        uint16_t clockMinYear(void) const { return _s_clock_min_year; }
        uint16_t clockMaxYear(void) const { return _s_clock_min_year + _s_clock_max_years; }

        static bool isValidClock(tClock const & clock);
        static uint8_t clock12H(void) { return EE_CLOCK_TYPE_12_HOUR; }
        static uint8_t clock24H(void) { return EE_CLOCK_TYPE_24_HOUR; }
        static bool clockIs12H(uint8_t type) { return type == clock12H(); }
        static bool clockIs24H(uint8_t type) { return type == clock24H(); }
        static uint8_t clockMaxDay(uint8_t month, uint16_t year)
        {
            return ((month == 2) && isLeapYear(year))
                ? _s_days_in_month[month - 1] + 1
                : _s_days_in_month[month - 1];
        }

        ////////////////////////////////////////////////////////////////////////
        // Alarm ///////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        bool setAlarm(tAlarm const & alarm);
        void getAlarm(tAlarm & alarm) const { alarm = _alarm; }
        uint8_t alarmHour(void) const { return _alarm.hour; }
        uint8_t alarmMinute(void) const { return _alarm.minute; }
        uint8_t alarmType(void) const { return _alarm.type; }
        bool alarmIsMusic(void) const { return _alarm.type == alarmMusic(); }
        bool alarmIsBeep(void) const { return _alarm.type == alarmBeep(); }
        bool alarmEnabled(void) const { return _alarm.enabled; }
        bool alarmDisabled(void) const { return !alarmEnabled(); }

        static bool isValidAlarm(tAlarm const & alarm);
        static uint8_t alarmMusic(void) { return EE_ALARM_TYPE_MUSIC; }
        static uint8_t alarmBeep(void) { return EE_ALARM_TYPE_BEEP; }
        static bool alarmIsMusic(uint8_t type) { return type == alarmMusic(); }
        static bool alarmIsBeep(uint8_t type) { return type == alarmBeep(); }

        ////////////////////////////////////////////////////////////////////////
        // Timer ///////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        bool setTimer(tTimer const & timer);
        void getTimer(tTimer & timer) const { timer = _timer; }
        uint8_t timerMinutes(void) const { return _timer.minutes; }
        uint8_t timerSeconds(void) const { return _timer.seconds; }

        static bool isValidTimer(tTimer const & timer);

    private:
        Rtc(void);

        void setClock(void);
        void reset(void);

        void enableIrq(void);
        void disableIrq(void);
        bool enabledIrq(void) { return NVIC::isEnabled(IRQ_RTC_SECOND); /* && NVIC::isEnabled(IRQ_RTC_ALARM); */ }

        bool defaultClock(tClock & clock);
        bool defaultClockMinYear(uint16_t & clock_min_year);
        bool defaultAlarm(tAlarm & alarm);
        bool defaultTimer(tTimer & timer);

        static uint32_t volatile _s_rtc_seconds;
        static uint16_t _s_clock_min_year;
        static constexpr uint16_t _s_clock_max_years = 300;

        uint32_t _clock_second = 0;
        uint32_t _clock_minute = 0;
        uint32_t _clock_hour = 0;
        uint32_t _clock_day = 0;
        uint32_t _clock_month = 0;
        uint32_t _clock_year = 0;

        uint8_t _clock_type = EE_CLOCK_TYPE_12_HOUR;

        tAlarm _alarm{};
        tTimer _timer{};

        Eeprom & _eeprom = Eeprom::acquire();

        // Stores contents of Time Seconds Register before MCU goes to sleep
        uint32_t _tsr;

        static constexpr uint8_t const _s_days_in_month[12] =
        {
            31,  // January
            28,  // February
            31,  // March
            30,  // April
            31,  // May
            30,  // June
            31,  // July
            31,  // August
            30,  // September
            31,  // October
            30,  // November
            31   // December
        };

        static reg32 _s_base;
        static reg32 _s_tsr;
        static reg32 _s_tpr;
        static reg32 _s_tar;
        static reg32 _s_tcr;
        static reg32 _s_cr;
        static reg32 _s_sr;
        static reg32 _s_lr;
        static reg32 _s_ier;
        static reg32 _s_war;
        static reg32 _s_rar;
};

#endif
