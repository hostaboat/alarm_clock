#ifndef _RTC_H_
#define _RTC_H_

#include "module.h"
#include "eeprom.h"
#include "armv7m.h"
#include "types.h"

using tClock = eClock;
using tAlarm = eAlarm;

extern "C" void rtc_seconds_isr(void);
extern "C" void rtc_alarm_isr(void);

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

enum dow_e  // Day Of Week
{
    DOW_SUN,
    DOW_MON,
    DOW_TUE,
    DOW_WED,
    DOW_THU,
    DOW_FRI,
    DOW_SAT,
};

class Rtc : public Module
{
    friend void rtc_seconds_isr(void);
    friend void rtc_alarm_isr(void);

    public:
        static Rtc & acquire(void) { static Rtc rtc; return rtc; }

        void enable(void);
        bool enabled(void);
        void disable(void);

        void start(void);
        void stop(void) { if (!running()) return; clearIntr(); *_s_cr = 0; }
        bool running(void);

        ////////////////////////////////////////////////////////////////////////
        // Clock ///////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        static bool isLeapYear(uint16_t year)
        { return ((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0)); }

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
        static dow_e clockDOW(uint8_t day, uint8_t month, uint16_t year);  // Day Of Week

        ru_e update(void);  // Updates the clock utilizing the RTC on the Teensy

        // Tell the clock that the MCU is going to sleep since interrupts will be
        // disabled.  Since the RTC_TSR counter will continue incrementing sleep
        // saves off the current value so a diff can be done with RTC_TSR when
        // waking/resuming.  Call sleep() before putting the MCU to sleep and
        // wake() after MCU wakes up.
        void sleep(void);
        uint32_t wake(void);

        ////////////////////////////////////////////////////////////////////////
        // Alarm ///////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        bool setAlarm(tAlarm const & alarm);
        void getAlarm(tAlarm & alarm) const { alarm = _alarm; }
        uint8_t alarmHour(void) const { return _alarm.hour; }
        uint8_t alarmMinute(void) const { return _alarm.minute; }
        uint8_t alarmType(void) const { return _alarm.type; }
        uint8_t alarmSnooze(void) const { return _alarm.snooze; }
        uint8_t alarmWake(void) const { return _alarm.wake; }
        uint8_t alarmTime(void) const { return _alarm.time; }
        bool alarmIsMusic(void) const { return _alarm.type == alarmMusic(); }
        bool alarmIsBeep(void) const { return _alarm.type == alarmBeep(); }
        bool alarmEnabled(void) const { return _alarm.enabled; }
        bool alarmDisabled(void) const { return !alarmEnabled(); }

        static bool isValidAlarm(tAlarm const & alarm);
        static uint8_t alarmMusic(void) { return EE_ALARM_TYPE_MUSIC; }
        static uint8_t alarmBeep(void) { return EE_ALARM_TYPE_BEEP; }
        static bool alarmIsMusic(uint8_t type) { return type == alarmMusic(); }
        static bool alarmIsBeep(uint8_t type) { return type == alarmBeep(); }

        void alarmStart(void);
        bool alarmIsSet(void);
        bool alarmInProgress(void);
        bool alarmIsAwake(void);
        bool alarmSnooze(void);
        void alarmStop(void);

        // For Llwu to call if alarm was wakeup source
        void clearIntr(void);

    private:
        Rtc(void);

        void reset(void);

        bool defaultClock(tClock & clock);
        bool defaultAlarm(tAlarm & alarm);

        // Daylight Saving Time routines
        void dstInit(void);
        int8_t dstUpdate(uint32_t hours);

        static uint32_t volatile _s_isr_seconds;
        static uint32_t volatile _s_alarm_start;
        static constexpr uint16_t const _s_clock_default_year = 2017;

        uint32_t _clock_second = 0;
        uint32_t _clock_minute = 0;
        uint32_t _clock_hour = 0;
        uint32_t _clock_day = 0;
        uint32_t _clock_month = 0;
        uint32_t _clock_year = 0;

        uint8_t _clock_type = EE_CLOCK_TYPE_12_HOUR;
        bool _clock_dst = false;

        tAlarm _alarm{};

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

        // For calculating day of week
#ifdef ZELLERS_RULE
        // Using Zeller's Rule
        static constexpr uint8_t const _s_zeller_month[12] = { 11, 12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
#else
        // Using Key Value Method
        static constexpr uint8_t const _s_month_key_value[12] = { 1, 4, 4, 0, 2, 5, 0, 3, 6, 1, 4, 6 };
        static constexpr uint8_t const _s_year_key_value[4] = { 4 /*1700s*/, 2 /*1800s*/, 0 /*1900s*/, 6 /*2000s*/ };
#endif

        bool _in_dst;
        uint8_t _dst_day;
        uint16_t _dst_year;
        uint32_t _dst_hrs;

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

        // 32 byte register that is accessible during all power modes
        // and is powered by VBAT.
        static reg32 _s_vbat;
};

#endif
