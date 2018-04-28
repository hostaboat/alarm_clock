#ifndef _EEPROM_H_
#define _EEPROM_H_

#include "module.h"
#include "types.h"

enum eei_e : uint8_t
{
    // 1-4
    EEI_ALARM_HOUR_MINUTE,  // Hour & Minute
    EEI_ALARM_SNOOZE_WAKE,  // Snooze & Wake times
    EEI_ALARM_TOTAL_TIME,   // Total Alarm time
    EEI_ALARM_TYPE_STATE,   // Alarm Type and State

    // 5-9
    EEI_CLOCK_SECOND,       // Second
    EEI_CLOCK_HOUR_MINUTE,  // Hour & Minute
    EEI_CLOCK_MONTH_DAY,    // Month & Day
    EEI_CLOCK_YEAR,         // Year
    EEI_CLOCK_TYPE_DST,     // Type & DST

    // 10-11
    EEI_TRACK_LOW,
    EEI_TRACK_HIGH,

    // 12-14
    EEI_TOUCH_NSCN_PS,          // NSCN & PS
    EEI_TOUCH_REFCHRG_EXTCHRG,  // REFCHRG & EXTCHRG
    EEI_TOUCH_THRESHOLD,

    // 15-21
    EEI_POWER_NAP_SECS_HIGH,   // Nap Time high 16 bits
    EEI_POWER_NAP_SECS_LOW,    // Nap Time low 16 bits
    EEI_POWER_STOP_SECS_HIGH,  // Player Stop Time high 16 bits
    EEI_POWER_STOP_SECS_LOW,   // Player Stop Time low 16 bits
    EEI_POWER_SLEEP_SECS_HIGH, // Sleep Time high 16 bits
    EEI_POWER_SLEEP_SECS_LOW,  // Sleep Time low 16 bits
    EEI_POWER_TOUCH_SECS,      // Touch Time to sleep

    // 22-27
    EEI_NLC_01_LOW,  // Night Light Color
    EEI_NLC_01_HIGH,
    EEI_NLC_02_LOW,
    EEI_NLC_02_HIGH,
    EEI_NLC_03_LOW,
    EEI_NLC_03_HIGH,

    // Num entries : 27
// Used to determine the EEPROM size
#define EEI_CNT  27
};

#define EE_ALARM_TYPE_BEEP   0x00
#define EE_ALARM_TYPE_MUSIC  0x01
#define EE_ALARM_TYPE_MAX    EE_ALARM_TYPE_MUSIC

#define EE_ALARM_STATE_DISABLED  0x00
#define EE_ALARM_STATE_ENABLED   0x01
#define EE_ALARM_STATE_MAX       EE_ALARM_STATE_ENABLED

#define EE_CLOCK_TYPE_12_HOUR  0x00
#define EE_CLOCK_TYPE_24_HOUR  0x01
#define EE_CLOCK_TYPE_MAX      EE_CLOCK_TYPE_24_HOUR

#define EE_CLOCK_DST_DISABLED  0x00
#define EE_CLOCK_DST_ENABLED   0x01
#define EE_CLOCK_DST_MAX       EE_CLOCK_DST_ENABLED

// Night Light Colors Count
#define EE_NLC_CNT  3

struct eAlarm
{
    uint8_t hour;
    uint8_t minute;
    uint8_t type;
    uint8_t snooze;
    uint8_t wake;
    uint8_t time;
    bool enabled;
};

struct eClock
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t type;
    bool dst;  // Daylight Saving Time
};

struct eTouch
{
    uint8_t nscn;
    uint8_t ps;
    uint8_t refchrg;
    uint8_t extchrg;
    uint16_t threshold;
};

struct ePower
{
    uint32_t nap_secs;
    uint32_t stop_secs; // Player stop time
    uint32_t sleep_secs;
    uint8_t touch_secs;
};

class Eeprom : public Module
{
    public:
        static Eeprom & acquire(void) { static Eeprom eeprom; return eeprom; }

        bool valid(void) { return _max_index != -1; }

        bool write(eei_e index, uint16_t value);
        bool read(eei_e index, uint16_t & value) const;

        bool setAlarm(eAlarm const & alarm);
        bool getAlarm(eAlarm & alarm) const;

        bool setClock(eClock const & clock);
        bool getClock(eClock & clock) const;

        bool setTrack(int track);
        bool getTrack(int & track) const;

        bool setTouch(eTouch const & touch);
        bool getTouch(eTouch & touch) const;

        bool setPower(ePower const & power);
        bool getPower(ePower & power) const;

        bool setNLC(uint8_t index, uint32_t color_code);
        bool getNLC(uint8_t index, uint32_t & color_code) const;

        Eeprom(Eeprom const &) = delete;
        Eeprom & operator=(Eeprom const &) = delete;

    private:
        Eeprom(void);

        void setMaxIndex(void);

        bool ready(eei_e index) const;

        reg16 _eeprom = (reg16)0x14000000;
        int16_t _max_index = 0;

        static constexpr int const _s_ready_times = 96000;

        static reg8 _s_base;
        static reg8 _s_fstat;
        static reg8 _s_fcnfg;
        //static reg8 _s_fsec;
        //static reg8 _s_fopt;
        //static reg8 _s_fccob3;
        //static reg8 _s_fccob2;
        //static reg8 _s_fccob1;
        static reg8 _s_fccob0;
        //static reg8 _s_fccob7;
        //static reg8 _s_fccob6;
        static reg8 _s_fccob5;
        static reg8 _s_fccob4;
        //static reg8 _s_fccobb;
        //static reg8 _s_fccoba;
        //static reg8 _s_fccob9;
        //static reg8 _s_fccob8;
        //static reg8 _s_fprot3;
        //static reg8 _s_fprot2;
        //static reg8 _s_fprot1;
        //static reg8 _s_fprot0;
        //static reg8 _s_feprot;
        //static reg8 _s_fdprot;

        static reg32 _s_sim_fcfg1;
};

#endif

