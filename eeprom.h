#ifndef _EEPROM_H_
#define _EEPROM_H_

#include "module.h"
#include "types.h"

enum eei_e : uint8_t
{
    EEI_ALARM_HOUR,
    EEI_ALARM_MINUTE,
    EEI_ALARM_TYPE,
    EEI_ALARM_SNOOZE,
    EEI_ALARM_WAKE,
    EEI_ALARM_TIME,
    EEI_ALARM_STATE,
    EEI_CLOCK_SECOND,
    EEI_CLOCK_MINUTE,
    EEI_CLOCK_HOUR,
    EEI_CLOCK_DAY,
    EEI_CLOCK_MONTH,
    EEI_CLOCK_YEAR,
    EEI_CLOCK_TYPE,
    EEI_CLOCK_DST,
    EEI_CLOCK_MIN_YEAR,
    EEI_TIMER_SECONDS,
    EEI_TIMER_MINUTES,
    EEI_FILE_INDEX,
    EEI_TOUCH_THRESHOLD,
    EEI_LEDS_COLOR,
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

struct eTimer
{
    uint8_t minutes;
    uint8_t seconds;
};

class Eeprom : public Module
{
    public:
        static Eeprom & acquire(void) { static Eeprom eeprom; return eeprom; }

        bool valid(void) { return _max_index != -1; }

        template < typename T >
        bool read(eei_e index, T & value) const
        {
            if (!ready(index)) return false;
            value = static_cast < T > (_eeprom[index]);
            return true;
        }

        bool write(eei_e index, uint32_t value);

        bool setAlarm(eAlarm const & alarm);
        bool getAlarm(eAlarm & alarm) const;

        bool setClock(eClock const & clock);
        bool getClock(eClock & clock) const;

        bool setClockMinYear(uint16_t year);
        bool getClockMinYear(uint16_t & year) const;

        bool setTimer(eTimer const & timer);
        bool getTimer(eTimer & timer) const;

        bool setFileIndex(uint16_t file_index);
        bool getFileIndex(uint16_t & file_index) const;

        bool setTouchThreshold(uint16_t touch_threshold);
        bool getTouchThreshold(uint16_t & touch_threshold) const;

        bool setLedsColor(uint32_t color_code);
        bool getLedsColor(uint32_t & color_code) const;

        Eeprom(Eeprom const &) = delete;
        Eeprom & operator=(Eeprom const &) = delete;

    private:
        Eeprom(void);

        void setMaxIndex(void);

        bool ready(eei_e index) const;

        reg32 _eeprom = (reg32)0x14000000;
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

