#include "eeprom.h"
#include "utility.h"
#include "types.h"

// XXX Make sure to set this to what is wanted on *FIRST* use as there
// is no easy way to change it after FlexRAM has been initialized as EEPROM.
// The smaller the value, the higher the write endurance.
#define EEPROM_SIZE  2048

#define EEPROM_MAX  2048
#define EEPARTITION 0x03  // all 32K dataflash for EEPROM, none for Data
#define EEESPLIT    0x30  // must be 0x30 on these chips

#define EE_SIZE_2048   0x03
#define EE_SIZE_1024   0x04
#define EE_SIZE_512    0x05
#define EE_SIZE_256    0x06
#define EE_SIZE_128    0x07
#define EE_SIZE_64     0x08
#define EE_SIZE_32     0x09
#define EE_SIZE_0      0x0F

#define EE_SIZE(n)  EE_SIZE_ ## n

#if EEPROM_SIZE == 2048
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(2048))
#elif EEPROM_SIZE == 1024
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(1024))
#elif EEPROM_SIZE == 512
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(512))
#elif EEPROM_SIZE == 256
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(256))
#elif EEPROM_SIZE == 128
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(128))
#elif EEPROM_SIZE == 64
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(64))
#elif EEPROM_SIZE == 32
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(32))
#else
# error "Invalid EEPROM_SIZE"
#endif

// 32 bit
#define EE_MAX_INDEX(eeprom_size)   (uint16_t)(((eeprom_size) / 4) - 1)

#define FTFL_FCNFG_EEERDY  (1 << 0)
#define FTFL_FCNFG_RAMRDY  (1 << 1)

reg8 Eeprom::_s_base = (reg8)0x40020000;
reg8 Eeprom::_s_fstat = _s_base;
reg8 Eeprom::_s_fcnfg = _s_base + 0x01;
//reg8 Eeprom::_s_fsec = _s_base + 0x02;
//reg8 Eeprom::_s_fopt = _s_base + 0x03;
//reg8 Eeprom::_s_fccob3 = _s_base + 0x04;
//reg8 Eeprom::_s_fccob2 = _s_base + 0x05;
//reg8 Eeprom::_s_fccob1 = _s_base + 0x06;
reg8 Eeprom::_s_fccob0 = _s_base + 0x07;
//reg8 Eeprom::_s_fccob7 = _s_base + 0x08;
//reg8 Eeprom::_s_fccob6 = _s_base + 0x09;
reg8 Eeprom::_s_fccob5 = _s_base + 0x0A;
reg8 Eeprom::_s_fccob4 = _s_base + 0x0B;
//reg8 Eeprom::_s_fccobb = _s_base + 0x0C;
//reg8 Eeprom::_s_fccoba = _s_base + 0x0D;
//reg8 Eeprom::_s_fccob9 = _s_base + 0x0E;
//reg8 Eeprom::_s_fccob8 = _s_base + 0x0F;
//reg8 Eeprom::_s_fprot3 = _s_base + 0x10;
//reg8 Eeprom::_s_fprot2 = _s_base + 0x11;
//reg8 Eeprom::_s_fprot1 = _s_base + 0x12;
//reg8 Eeprom::_s_fprot0 = _s_base + 0x13;
//reg8 Eeprom::_s_feprot = _s_base + 0x16;
//reg8 Eeprom::_s_fdprot = _s_base + 0x17;
reg32 Eeprom::_s_sim_fcfg1 = (reg32)0x4004804C;

// This is basically a carbon copy taken from the teensy3 core file eeprom.c
Eeprom::Eeprom(void)
{
    Module::enable(MOD_FTFL);

    // This will only execute if FlexRAM has not been configured as EEPROM.
    // It's a one time deal so be sure to set FLEX_RAM_SIZE accordingly the
    // first time this is used.
    if (*_s_fcnfg & FTFL_FCNFG_RAMRDY)
    {
        uint16_t do_flash_cmd[] =
        {
            0xf06f, 0x037f,   // 0x00: mvn.w  r3, #127      ; 0x7F
            0x7003,           // 0x04: strb   r3, [r0, #0]
            0x7803,           // 0x06: ldrb   r3, [r0, #0]
            0xf013, 0x0f80,   // 0x08: tst.w  r3, #128      ; 0x80
            0xd0fb,           // 0x0C: beq.n  6 <do_flash_cmd+0x6>
            0x4770            // 0x0E: bx     lr
        };

        // FlexRAM is configured as traditional RAM and needs to be
        // configured for EEPROM usage.
        *_s_fccob0 = 0x80;           // PGMPART = Program Partition Command
        *_s_fccob4 = EE_SIZE_CODE;   // EEPROM Size Code
        *_s_fccob5 = 0x03;           // 0K for Dataflash, 32K for EEPROM backup

        __disable_irq();

        // do_flash_cmd() must execute from RAM.  Luckily the C syntax is simple...
        // I think the "| 1" is setting EPSR.T indicating execution in Thumb state.
        // If it's zero on return a fault happens when the next instruction executes.
        // ARMv7-M Architecture Reference Manual - B1.3.3 Execution state
        (*((void (*)(uint8_t volatile *))((uint32_t)do_flash_cmd | 1)))(_s_fstat);

        __enable_irq();

        uint8_t status = *_s_fstat;
        if (status & 0x70)
        {
            *_s_fstat = (status & 0x70);
            _max_index = -1;
            return;
        }
    }

    switch ((*_s_sim_fcfg1 & 0x000F0000) >> 16) // EESIZE
    {
        case EE_SIZE(2048): _max_index = EE_MAX_INDEX(2048); break;
        case EE_SIZE(1024): _max_index = EE_MAX_INDEX(1024); break;
        case EE_SIZE(512):  _max_index = EE_MAX_INDEX(512); break;
        case EE_SIZE(256):  _max_index = EE_MAX_INDEX(256); break;
        case EE_SIZE(128):  _max_index = EE_MAX_INDEX(128); break;
        case EE_SIZE(64):   _max_index = EE_MAX_INDEX(64); break;
        case EE_SIZE(32):   _max_index = EE_MAX_INDEX(32); break;
        case EE_SIZE(0):    _max_index = -1; break;
    }
}

bool Eeprom::ready(eei_e index) const
{
    if ((int16_t)index > _max_index)
        return false;

    int i = 0;
    for (; i < _s_ready_times; i++)
    {
        if (*_s_fcnfg & FTFL_FCNFG_EEERDY)
            break;
    }

    return i < _s_ready_times;
}

bool Eeprom::write(eei_e index, uint32_t value)
{
    if (!ready(index))
        return false;

    if (_eeprom[index] != value)
        _eeprom[index] = value;

    return true;
}

bool Eeprom::setAlarm(eAlarm const & alarm)
{
    uint8_t s = alarm.enabled ? EE_ALARM_STATE_ENABLED : EE_ALARM_STATE_DISABLED;
    return (write(EEI_ALARM_HOUR, alarm.hour)
            && write(EEI_ALARM_MINUTE, alarm.minute)
            && write(EEI_ALARM_TYPE, alarm.type)
            && write(EEI_ALARM_SNOOZE, alarm.snooze)
            && write(EEI_ALARM_WAKE, alarm.wake)
            && write(EEI_ALARM_TIME, alarm.time)
            && write(EEI_ALARM_STATE, s));
}

bool Eeprom::getAlarm(eAlarm & alarm) const
{
    uint8_t s;
    if (!read < uint8_t > (EEI_ALARM_HOUR, alarm.hour)
            || !read < uint8_t > (EEI_ALARM_MINUTE, alarm.minute)
            || !read < uint8_t > (EEI_ALARM_TYPE, alarm.type)
            || !read < uint8_t > (EEI_ALARM_SNOOZE, alarm.snooze)
            || !read < uint8_t > (EEI_ALARM_WAKE, alarm.wake)
            || !read < uint8_t > (EEI_ALARM_TIME, alarm.time)
            || !read < uint8_t > (EEI_ALARM_STATE, s))
        return false;
    alarm.enabled = (s == EE_ALARM_STATE_ENABLED);
    return true;
}

bool Eeprom::setClock(eClock const & clock)
{
    uint8_t dst = clock.dst ? EE_CLOCK_DST_ENABLED : EE_CLOCK_DST_DISABLED;
    return (write(EEI_CLOCK_SECOND, clock.second)
            && write(EEI_CLOCK_MINUTE, clock.minute)
            && write(EEI_CLOCK_HOUR, clock.hour)
            && write(EEI_CLOCK_DAY, clock.day)
            && write(EEI_CLOCK_MONTH, clock.month)
            && write(EEI_CLOCK_YEAR, clock.year)
            && write(EEI_CLOCK_TYPE, clock.type)
            && write(EEI_CLOCK_DST, dst));
}

bool Eeprom::getClock(eClock & clock) const
{
    uint8_t dst;
    if (!read < uint8_t > (EEI_CLOCK_SECOND, clock.second)
            || !read < uint8_t > (EEI_CLOCK_MINUTE, clock.minute)
            || !read < uint8_t > (EEI_CLOCK_HOUR, clock.hour)
            || !read < uint8_t > (EEI_CLOCK_DAY, clock.day)
            || !read < uint8_t > (EEI_CLOCK_MONTH, clock.month)
            || !read < uint16_t > (EEI_CLOCK_YEAR, clock.year)
            || !read < uint8_t > (EEI_CLOCK_TYPE, clock.type)
            || !read < uint8_t > (EEI_CLOCK_DST, dst))
        return false;
    clock.dst = (dst == EE_CLOCK_DST_ENABLED);
    return true;
}

bool Eeprom::setClockMinYear(uint16_t year)
{
    return write(EEI_CLOCK_MIN_YEAR, year);
}

bool Eeprom::getClockMinYear(uint16_t & year) const
{
    return read < uint16_t > (EEI_CLOCK_MIN_YEAR, year);
}

bool Eeprom::setTimer(eTimer const & timer)
{
    return (write(EEI_TIMER_SECONDS, timer.seconds)
            && write(EEI_TIMER_MINUTES, timer.minutes));
}

bool Eeprom::getTimer(eTimer & timer) const
{
    return (read < uint8_t > (EEI_TIMER_SECONDS, timer.seconds)
            && read < uint8_t > (EEI_TIMER_MINUTES, timer.minutes));
}

bool Eeprom::setFileIndex(uint16_t file_index)
{
    return write(EEI_FILE_INDEX, file_index);
}

bool Eeprom::getFileIndex(uint16_t & file_index) const
{
    return read < uint16_t > (EEI_FILE_INDEX, file_index);
}

bool Eeprom::setTouch(eTouch const & touch)
{
    return write(EEI_TOUCH_NSCN, touch.nscn)
        && write(EEI_TOUCH_PS, touch.ps)
        && write(EEI_TOUCH_REFCHRG, touch.refchrg)
        && write(EEI_TOUCH_EXTCHRG, touch.extchrg)
        && write(EEI_TOUCH_THRESHOLD, touch.threshold);
}

bool Eeprom::getTouch(eTouch & touch) const
{
    return read < uint8_t > (EEI_TOUCH_NSCN, touch.nscn)
        && read < uint8_t > (EEI_TOUCH_PS, touch.ps)
        && read < uint8_t > (EEI_TOUCH_REFCHRG, touch.refchrg)
        && read < uint8_t > (EEI_TOUCH_EXTCHRG, touch.extchrg)
        && read < uint16_t > (EEI_TOUCH_THRESHOLD, touch.threshold);
}

bool Eeprom::setLedsColor(uint32_t color_code)
{
    return write(EEI_LEDS_COLOR, color_code);
}

bool Eeprom::getLedsColor(uint32_t & color_code) const
{
    return read < uint32_t > (EEI_LEDS_COLOR, color_code);
}
