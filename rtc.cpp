#include "rtc.h"
#include "module.h"
#include "eeprom.h"
#include "armv7m.h"
#include "utility.h"
#include "types.h"

#define RTC_CR_SWR   (1 <<  0)
#define RTC_CR_WPE   (1 <<  1)
#define RTC_CR_SUP   (1 <<  2)
#define RTC_CR_UM    (1 <<  3)
#define RTC_CR_OSCE  (1 <<  8)
#define RTC_CR_CLKO  (1 <<  9)
#define RTC_CR_SC16P (1 << 10)
#define RTC_CR_SC8P  (1 << 11)
#define RTC_CR_SC4P  (1 << 12)
#define RTC_CR_SC2P  (1 << 13)

#define RTC_SR_TIF (1 << 0)
#define RTC_SR_TOF (1 << 1)
#define RTC_SR_TAF (1 << 2)
#define RTC_SR_TCE (1 << 4)

#define RTC_IER_TIIE (1 << 0)
#define RTC_IER_TOIE (1 << 1)
#define RTC_IER_TAIE (1 << 2)
#define RTC_IER_TSIE (1 << 4)

// More likely for it to be 0 than this
#define RTC_TSR_INVALID  UINT32_MAX

uint16_t Rtc::_s_clock_min_year = 2017;
constexpr uint8_t const Rtc::_s_days_in_month[12];

#ifdef ZELLERS_RULE
constexpr uint8_t const Rtc::_s_zeller_month[12];
#else
constexpr uint8_t const Rtc::_s_month_key_value[12];
constexpr uint8_t const Rtc::_s_year_key_value[4];
#endif

reg32 Rtc::_s_base = (reg32)0x4003D000;
reg32 Rtc::_s_tsr = _s_base;
reg32 Rtc::_s_tpr = _s_base + 1;
reg32 Rtc::_s_tar = _s_base + 2;
reg32 Rtc::_s_tcr = _s_base + 3;
reg32 Rtc::_s_cr  = _s_base + 4;
reg32 Rtc::_s_sr  = _s_base + 5;
reg32 Rtc::_s_lr  = _s_base + 6;
reg32 Rtc::_s_ier = _s_base + 7;
reg32 Rtc::_s_war = _s_base + 8;
reg32 Rtc::_s_rar = _s_base + 9;

reg32 Rtc::_s_vbat = (reg32)0x4003E000;

uint32_t volatile Rtc::_s_isr_seconds = 0;

void rtc_seconds_isr(void)
{
    Rtc::_s_isr_seconds++;
}

//void rtc_alarm_isr(void) {}

Rtc::Rtc(void)
{
    tAlarm alarm;
    tClock clock;
    uint16_t clock_min_year;

#ifdef RTC_INIT
    (void)defaultAlarm(alarm);
    (void)defaultClock(clock);
    (void)defaultClockMinYear(clock_min_year);
#else
    if (!_eeprom.getAlarm(alarm) || !isValidAlarm(alarm))
        (void)defaultAlarm(alarm);

    if (!_eeprom.getClock(clock) || !isValidClock(clock))
        (void)defaultClock(clock);

    if (!_eeprom.getClockMinYear(clock_min_year))
        (void)defaultClockMinYear(clock_min_year);
#endif

    _tsr = RTC_TSR_INVALID;

    _alarm = alarm;

    _clock_dst = clock.dst;
    _clock_type = clock.type;
    _s_clock_min_year = clock_min_year;

    _clock_second = _s_isr_seconds = clock.second;
    _clock_minute = clock.minute;
    _clock_hour = clock.hour;
    _clock_day = clock.day;
    _clock_month = clock.month;
    _clock_year = clock.year;

    enable();
    start();

#ifdef RTC_VBAT
    if (!(*_s_sr & RTC_SR_TIF))
        _s_isr_seconds = *_s_tsr;
    else
#endif
    reset();

    if (_clock_dst)
        dstInit();

    enableIrq();

    (void)update();
}

void Rtc::enableIrq(void)
{
    if (enabledIrq())
        return;

    //NVIC::setPriority(IRQ_RTC_SECOND, 64);
    NVIC::enable(IRQ_RTC_SECOND);
    //NVIC::setPriority(IRQ_RTC_ALARM, 64);
    //NVIC::enable(IRQ_RTC_ALARM);
}

void Rtc::disableIrq(void)
{
    if (!enabledIrq())
        return;

    NVIC::disable(IRQ_RTC_SECOND);
    //NVIC::disable(IRQ_RTC_ALARM);
}

void Rtc::start(void)
{
    if (!Module::enabled(MOD_RTC))
        Module::enable(MOD_RTC);

    if (*_s_cr & RTC_CR_OSCE)
        return;

    // Teensy code uses 16pF and 4pF - 20pF - load though the crystal it says
    // to use has a 12.5pF load.  There is nothing in the schematic that
    // indicates an extra 8pF load is needed.  The crystal is simply attached
    // across XTAL32 and EXTAL32.  Not sure about it so leave as is.
    //*_s_cr = RTC_CR_SC8P | RTC_CR_SC4P;
    *_s_cr = RTC_CR_SC16P | RTC_CR_SC4P;
    *_s_cr |= RTC_CR_OSCE;

    // Not sure what the startup time for the crystal is but I think
    // a millisecond may be good enough.
    delay_msecs(1);
}

bool Rtc::running(void)
{
    return enabled() && (*_s_cr & RTC_CR_OSCE);
}

void Rtc::reset(void)
{
    disableIrq();
    _s_isr_seconds = _clock_second;
    enableIrq();

    *_s_sr = 0;  // TCE must be clear to write to TSR and TPR
    *_s_tpr = 0;
    *_s_tsr = _clock_second;
    //*_s_tar = RTC_TSR_INVALID;  // Not using alarm function
    *_s_ier = RTC_IER_TSIE;  // | RTC_IER_TAIE; // No alarm interrupt
    *_s_sr = RTC_SR_TCE;
}

ru_e Rtc::update(void)
{
    // Don't want to disable the IRQ unless there is a seconds update.
    // An interrupt happening here while checking _s_isr_seconds against
    // _clock_second will get caught up on the next call.
    if (_s_isr_seconds == _clock_second)
        return RU_NO;

    ru_e ru = RU_SEC;

    disableIrq();

    if (_s_isr_seconds >= 60)
    {
        ru = RU_MIN;

        _clock_minute += _s_isr_seconds / 60;
        _s_isr_seconds %= 60;
    }

    _clock_second = _s_isr_seconds;

    enableIrq();

    if (_clock_minute < 60)
        return ru;

    if (_clock_dst)
        _clock_hour += dstUpdate(_clock_minute / 60);

    _clock_hour += _clock_minute / 60;
    _clock_minute %= 60;

    ru = RU_HOUR;

    if (_clock_hour < 24)
        return ru;

    _clock_day += _clock_hour / 24;
    _clock_hour %= 24;

    ru = RU_DAY;

    bool is_leap_year = isLeapYear(_clock_year);
    uint8_t days_in_month = _s_days_in_month[_clock_month - 1];

    while (_clock_day > days_in_month)
    {
        // Check for leap year.  If so, days in month will be +1
        if ((_clock_month == 2) && is_leap_year && (_clock_day >= (uint32_t)(days_in_month + 1)))
        {
            // Date is good so break out
            if (_clock_day == (uint32_t)(days_in_month + 1))
                break;

            _clock_day -= 1;
        }

        _clock_day -= days_in_month;

        if (ru < RU_MON)
            ru = RU_MON;

        if (++_clock_month == 13)
        {
            _clock_month = 1;
            _clock_year++;

            if (ru < RU_YEAR)
                ru = RU_YEAR;

            is_leap_year = isLeapYear(_clock_year);
        }

        days_in_month = _s_days_in_month[_clock_month - 1];
    }

    return ru;
}

#ifdef ZELLERS_RULE
////////////////////////////////////////////////////////////////////////////////
// Zeller's Rule
//
// dow = (d + ((13 * zm) - 1) / 5) + yl2 + (yl2 / 4) + (yf2 / 4) - (yf2 * 2)) % 7
// if (dow < 0) dow += 7
//
// dow = day of week
// d   = day of the month
// zm  = Zeller month number
// yl2 = last two digits of the year
// yf2 = first two digits of the year
//
// Since January and February are considered the last two months of the
// year using Zeller's Rule, subtract 1 from the current year if one of
// these months.
////////////////////////////////////////////////////////////////////////////////
dow_e Rtc::clockDOW(uint8_t day, uint8_t month, uint16_t year)
{
    uint8_t yf2;
    uint8_t yl2;

    if (month <= 2)
        year--;

    yf2 = year / 100;
    yl2 = year % 100;

    int8_t dow =
        (
         day +
         (((13 * _s_zeller_month[month - 1]) - 1) / 5) +
         yl2 +
         (yl2 / 4) +
         (yf2 / 4) -
         (yf2 * 2)
        ) % 7;

    if (dow < 0)
        dow += 7;

    return (dow_e)dow;
}
#else
// Key Value Method
dow_e Rtc::clockDOW(uint8_t day, uint8_t month, uint16_t year)
{
    uint8_t yl2 = year % 100;
    uint8_t dow = (yl2 / 4) + day + _s_month_key_value[month - 1] + yl2;

    if ((month <= 2) && isLeapYear(year))
        dow -= 1;

    while (year >= 2100)
        year -= 400;

    dow = (dow + (_s_year_key_value[(year - 1700) / 100])) % 7;

    // Using Key Value Method, day of week starts on Saturday
    return (dow_e)((dow == 0) ? 6 : dow - 1);
}
#endif  // ZELLERS_RULE

void Rtc::dstInit(void)
{
    uint32_t cycle = 0;
    uint8_t day = (uint8_t)_clock_day;
    uint8_t month = (uint8_t)_clock_month;
    uint16_t year = (uint16_t)_clock_year;

    auto month_days = [](uint8_t month, uint16_t year) -> uint8_t
    {
        uint8_t days = _s_days_in_month[month - 1];
        return ((month == 2) && isLeapYear(year)) ? days+1 : days;
    };

    // Will break out on first or second iteration of outer loop
    while (true)
    {
        dow_e dow = clockDOW(day, month, year);  // Day Of Week

        if ((month == 11) && (day <= 7)
                && ((day <= dow) || ((dow == DOW_SUN) && (_clock_hour < 2))))
        {
            _dst_day = (dow == DOW_SUN) ? day : day + (7 - dow);
            _in_dst = true;
            break;
        }
        else if ((month == 3) && (day <= 14)
                && ((day <= (dow + 7)) || ((dow == DOW_SUN) && (_clock_hour < 2))))
        {
            if (dow == DOW_SUN)
                _dst_day = (day > 7) ? day : day + 7;
            else
                _dst_day = (day > dow) ? day + (7 - dow) : day + (14 - dow);

            _in_dst = false;
            break;
        }

        cycle += month_days(month, year) - (day - 1);  // Include current day

        // Tally up days in months that aren't March or November
        while (true)
        {
            if (++month == 13)
            {
                year++;
                month = 1;
            }

            if ((month == 3) || (month == 11))
                break;

            cycle += month_days(month, year);
        }

        day = 1; // Will be at the first of March or November
    }

    cycle += _dst_day - day;

    // Convert days - cycle - to hours and add two hours. And since the current
    // day is included, subtract the current clock hour.
    _dst_hrs = ((cycle * 24) + 2) - _clock_hour;
    _dst_year = year;
}

int8_t Rtc::dstUpdate(uint32_t hours)
{
    int8_t hour_adjust = 0;

    // The loop accounts for the case where update hasn't been called for
    // a while, e.g. if the device was off for more than one cycle and the
    // RTC was being powered by VBAT.
    while (hours >= _dst_hrs)
    {
        hours -= _dst_hrs;

        _in_dst = !_in_dst;

        uint32_t cycle;

        if (_in_dst)
        {
            // March - November
            cycle = 238;  // Always 238 days

            // Day of November when DST ends will be the number of DST days
            // minus the number of days during DST not including November.
            //                       March        Apr  May  Jun  Jul  Aug  Sep  Oct
            _dst_day = cycle - ((31 - _dst_day) + (30 + 31 + 30 + 31 + 31 + 30 + 31));
        }
        else
        {
            // November - March
            bool is_leap_year = isLeapYear(++_dst_year);

            // The number of days until DST depends on what day in November DST
            // ended and whether or not the year of the next DST is a leap year.
            if ((_dst_day == 1) || ((_dst_day == 2) && is_leap_year))
                cycle = 133;
            else
                cycle = 126;

            //                      November      Dec  Jan  Feb ->
            _dst_day = cycle - ((30 - _dst_day) + (31 + 31 + 28 + (is_leap_year ? 1 : 0)));
        }

        // Don't need to add 2 hours since it's already on the DST day at 2:00 AM
        // But do need to add or subtract the hour adjustment.  DST starts or ends
        // at 2:00 AM, but the clock is set forward or backward by an hour.  If
        // set back have to add an hour and if forward subract an hour.
        _dst_hrs = (cycle * 24) + (_in_dst ? -1 : 1);

        hour_adjust += _in_dst ? 1 : -1;
    }

    _dst_hrs -= hours;

    return hour_adjust;
}

// Tell the clock that the MCU is going to sleep since interrupts will be
// disabled.  Since the RTC_TSR counter will continue incrementing save
// off the current value so a diff can be done when resuming.
void Rtc::sleep(void)
{
    _tsr = *_s_tsr;
}

void Rtc::wake(void)
{
    if (_tsr == RTC_TSR_INVALID)
        return;

    disableIrq();
    _s_isr_seconds += *_s_tsr - _tsr;
    enableIrq();

    _tsr = RTC_TSR_INVALID;
}

bool Rtc::isValidClock(tClock const & clock)
{
    if ((clock.hour >= 24) || (clock.minute >= 60) || (clock.second >= 60))
        return false;

    uint8_t days_in_month = _s_days_in_month[clock.month - 1];
    if ((clock.day == 0)
            || (isLeapYear(clock.year) && (clock.month == 2) && (clock.day > (days_in_month + 1)))
            || (clock.day > days_in_month))
        return false;

    if ((clock.month == 0) || (clock.month > 12) || (clock.year < _s_clock_min_year))
        return false;

    if (clock.type > EE_CLOCK_TYPE_MAX)
        return false;

    return true;
}

bool Rtc::setClock(tClock const & clock)
{
    if (!isValidClock(clock))
        return false;

    (void)_eeprom.setClock(clock);

    _clock_second = clock.second;
    _clock_minute = clock.minute;
    _clock_hour = clock.hour;
    _clock_day = clock.day;
    _clock_month = clock.month;
    _clock_year = clock.year;
    _clock_type = clock.type;
    _clock_dst = clock.dst;

    if (_clock_dst)
        dstInit();

    reset();

    return true;
}

void Rtc::getClock(tClock & clock) const
{
    clock.second = (uint8_t)_clock_second;
    clock.minute = (uint8_t)_clock_minute;
    clock.hour = (uint8_t)_clock_hour;
    clock.day = (uint8_t)_clock_day;
    clock.month = (uint8_t)_clock_month;
    clock.year = (uint16_t)_clock_year;
    clock.type = _clock_type;
    clock.dst = _clock_dst;
}

bool Rtc::defaultClock(tClock & clock)
{
    clock.second = 0;
    clock.minute = 0;
    clock.hour = 0;
    clock.day = 1;
    clock.month = 1;
    clock.year = _s_clock_min_year;
    clock.type = EE_CLOCK_TYPE_12_HOUR; 
    clock.dst = true;

    return _eeprom.setClock(clock);
}

bool Rtc::defaultClockMinYear(uint16_t & clock_min_year)
{
    clock_min_year = _s_clock_min_year;
    return _eeprom.setClockMinYear(clock_min_year);
}

bool Rtc::isValidAlarm(tAlarm const & alarm)
{
    return (alarm.hour < 24) && (alarm.minute < 60) && (alarm.type <= EE_ALARM_TYPE_MAX)
        && (alarm.snooze > 0) && (alarm.snooze < 60) && (alarm.wake < 60) && (alarm.time < 24);
}

bool Rtc::setAlarm(tAlarm const & alarm)
{
    if (!isValidAlarm(alarm))
        return false;

    _alarm = alarm;
    (void)_eeprom.setAlarm(alarm);

    return true;
}

bool Rtc::defaultAlarm(tAlarm & alarm)
{
    alarm.hour = 0;
    alarm.minute = 0;
    alarm.type = EE_ALARM_TYPE_BEEP;
    alarm.snooze = 10;
    alarm.wake = 10;
    alarm.time = 2;
    alarm.enabled = false;

    return _eeprom.setAlarm(alarm);
}
