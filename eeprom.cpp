#include "eeprom.h"
#include "utility.h"
#include "types.h"

// XXX Make sure to set this to what is wanted on *FIRST* use as there
// is no easy way to change it after FlexRAM has been initialized as EEPROM.
// The smaller the value, the higher the write endurance.
#ifndef EEPROM_SIZE
# define EEPROM_SIZE  (EEI_CNT * 2)
#endif

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

#if EEPROM_SIZE > 2048
# error "Not enough EEPROM"
#elif EEPROM_SIZE > 1024
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(2048))
#elif EEPROM_SIZE > 512
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(1024))
#elif EEPROM_SIZE > 256
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(512))
#elif EEPROM_SIZE > 128
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(256))
#elif EEPROM_SIZE > 64
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(128))
#elif EEPROM_SIZE > 32
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(64))
#else
# define EE_SIZE_CODE  (EEESPLIT | EE_SIZE(32))
#endif

// Datasheet says that write efficiency is the same for both 16 and 32 bit writes
// * 0.25 for 8-bit writes to FlexRAM
// * 0.50 for 16-bit or 32-bit writes to FlexRAM
// Use 16 bit to maximize both space and write efficiency
#define EE_MAX_INDEX(eeprom_size)   (uint16_t)(((eeprom_size) / 2) - 1)

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
        *_s_fccob5 = EEPARTITION;    // 0K for Dataflash, 32K for EEPROM backup

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

bool Eeprom::write(eei_e index, uint16_t value)
{
    if (!ready(index))
        return false;

    if (_eeprom[index] != value)
        _eeprom[index] = value;

    return true;
}

bool Eeprom::read(eei_e index, uint16_t & value) const
{
    if (!ready(index))
        return false;

    value = _eeprom[index];

    return true;
}

bool Eeprom::setAlarm(eAlarm const & alarm)
{
    uint16_t hm = ((uint16_t)alarm.hour << 8) | alarm.minute;
    uint16_t sw = ((uint16_t)alarm.snooze << 8) | alarm.wake;
    uint16_t at = alarm.time;
    uint16_t ts = ((uint16_t)alarm.type << 8)
        | (alarm.enabled ? EE_ALARM_STATE_ENABLED : EE_ALARM_STATE_DISABLED);

    return (write(EEI_ALARM_HOUR_MINUTE, hm)
            && write(EEI_ALARM_SNOOZE_WAKE, sw)
            && write(EEI_ALARM_TOTAL_TIME, at)
            && write(EEI_ALARM_TYPE_STATE, ts));
}

bool Eeprom::getAlarm(eAlarm & alarm) const
{
    uint16_t hm, sw, at, ts;
    if (!read(EEI_ALARM_HOUR_MINUTE, hm)
            || !read(EEI_ALARM_SNOOZE_WAKE, sw)
            || !read(EEI_ALARM_TOTAL_TIME, at)
            || !read(EEI_ALARM_TYPE_STATE, ts))
        return false;

    alarm.hour = (uint8_t)(hm >> 8);
    alarm.minute = (uint8_t)(hm & 0xFF);
    alarm.snooze = (uint8_t)(sw >> 8);
    alarm.wake = (uint8_t)(sw & 0xFF);
    alarm.time = (uint8_t)(at & 0xFF);
    alarm.type = (uint8_t)(ts >> 8);
    alarm.enabled = (uint8_t)(ts & 0xFF) == EE_ALARM_STATE_ENABLED;

    return true;
}

bool Eeprom::setClock(eClock const & clock)
{
    uint16_t s = clock.second;
    uint16_t hm = ((uint16_t)clock.hour << 8) | clock.minute;
    uint16_t md = ((uint16_t)clock.month << 8) | clock.day;
    uint16_t y = clock.year;
    uint16_t td = ((uint16_t)clock.type << 8)
        | (clock.dst ? EE_CLOCK_DST_ENABLED : EE_CLOCK_DST_DISABLED);

    return (write(EEI_CLOCK_SECOND, s)
            && write(EEI_CLOCK_HOUR_MINUTE, hm)
            && write(EEI_CLOCK_MONTH_DAY, md)
            && write(EEI_CLOCK_YEAR, y)
            && write(EEI_CLOCK_TYPE_DST, td));
}

bool Eeprom::getClock(eClock & clock) const
{
    uint16_t s, hm, md, y, td;
    if (!read(EEI_CLOCK_SECOND, s)
            || !read(EEI_CLOCK_HOUR_MINUTE, hm)
            || !read(EEI_CLOCK_MONTH_DAY, md)
            || !read(EEI_CLOCK_YEAR, y)
            || !read(EEI_CLOCK_TYPE_DST, td))
        return false;

    clock.second = (uint8_t)(s & 0xFF);
    clock.minute = (uint8_t)(hm & 0xFF);
    clock.hour = (uint8_t)(hm >> 8);
    clock.day = (uint8_t)(md & 0xFF);
    clock.month = (uint8_t)(md >> 8);
    clock.year = y;
    clock.type = (uint8_t)(td >> 8);
    clock.dst = (uint8_t)(td & 0xFF) == EE_CLOCK_DST_ENABLED;
    return true;
}

bool Eeprom::setClockMinYear(uint16_t year)
{
    return write(EEI_CLOCK_MIN_YEAR, year);
}

bool Eeprom::getClockMinYear(uint16_t & year) const
{
    return read(EEI_CLOCK_MIN_YEAR, year);
}

bool Eeprom::setTrack(uint16_t track)
{
    return write(EEI_TRACK, track);
}

bool Eeprom::getTrack(uint16_t & track) const
{
    return read(EEI_TRACK, track);
}

bool Eeprom::setTouch(eTouch const & touch)
{
    uint16_t np = ((uint16_t)touch.nscn << 8) | touch.ps;
    uint16_t re = ((uint16_t)touch.refchrg << 8) | touch.extchrg;
    uint16_t t = touch.threshold;

    return write(EEI_TOUCH_NSCN_PS, np)
        && write(EEI_TOUCH_REFCHRG_EXTCHRG, re)
        && write(EEI_TOUCH_THRESHOLD, t);
}

bool Eeprom::getTouch(eTouch & touch) const
{
    uint16_t np, re, t;
    if (!read(EEI_TOUCH_NSCN_PS, np)
            || !read(EEI_TOUCH_REFCHRG_EXTCHRG, re)
            || !read(EEI_TOUCH_THRESHOLD, t))
        return false;

    touch.nscn = (uint8_t)(np >> 8);
    touch.ps = (uint8_t)(np & 0xFF);
    touch.refchrg = (uint8_t)(re >> 8);
    touch.extchrg = (uint8_t)(re & 0xFF);
    touch.threshold = t;

    return true;
}

bool Eeprom::setLedsColor(uint32_t color_code)
{
    return write(EEI_LEDS_COLOR_LOW, color_code & 0xFFFF)
        && write(EEI_LEDS_COLOR_HIGH, color_code >> 16);
}

bool Eeprom::getLedsColor(uint32_t & color_code) const
{
    uint16_t ccl, cch;
    if (!read(EEI_LEDS_COLOR_LOW, ccl)
            || !read(EEI_LEDS_COLOR_HIGH, cch))
        return false;

    color_code = ((uint32_t)cch << 16) | (uint32_t)ccl;

    return true;
}

bool Eeprom::setPower(ePower const & power)
{
    uint16_t nsh = (uint16_t)(power.nap_secs >> 16);
    uint16_t nsl = (uint16_t)(power.nap_secs & 0xFFFF);
    uint16_t psh = (uint16_t)(power.stop_secs >> 16);
    uint16_t psl = (uint16_t)(power.stop_secs & 0xFFFF);
    uint16_t ssh = (uint16_t)(power.sleep_secs >> 16);
    uint16_t ssl = (uint16_t)(power.sleep_secs & 0xFFFF);
    uint16_t ts = power.touch_secs;

    return (write(EEI_POWER_NAP_SECS_HIGH, nsh)
            && write(EEI_POWER_NAP_SECS_LOW, nsl)
            && write(EEI_POWER_STOP_SECS_HIGH, psh)
            && write(EEI_POWER_STOP_SECS_LOW, psl)
            && write(EEI_POWER_SLEEP_SECS_HIGH, ssh)
            && write(EEI_POWER_SLEEP_SECS_LOW, ssl)
            && write(EEI_POWER_TOUCH_SECS, ts));
}

bool Eeprom::getPower(ePower & power) const
{
    uint16_t nsh, nsl, psh, psl, ssh, ssl, ts;
    if (!read(EEI_POWER_NAP_SECS_HIGH, nsh)
            || !read(EEI_POWER_NAP_SECS_LOW, nsl)
            || !read(EEI_POWER_STOP_SECS_HIGH, psh)
            || !read(EEI_POWER_STOP_SECS_LOW, psl)
            || !read(EEI_POWER_SLEEP_SECS_HIGH, ssh)
            || !read(EEI_POWER_SLEEP_SECS_LOW, ssl)
            || !read(EEI_POWER_TOUCH_SECS, ts))
        return false;

    power.sleep_secs = ((uint32_t)ssh << 16) | (uint32_t)ssl;
    power.nap_secs = ((uint32_t)nsh << 16) | (uint32_t)nsl;
    power.stop_secs = ((uint32_t)psh << 16) | (uint32_t)psl;
    power.touch_secs = (uint8_t)ts;

    return true;
}
