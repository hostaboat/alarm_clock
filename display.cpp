#include "display.h"
#include "types.h"

constexpr uint8_t const Seg7Display::_s_number_map[];
constexpr uint8_t const Seg7Display::_s_char_map[];

uint16_t Seg7Display::validateValue(uint16_t n, uint16_t min, uint16_t max)
{
    return ((n < min) ? min : ((n > max) ? max : n));
}

#define DIGIT_FLAGS(X, flags)  ((flags) & (DF_BL ## X | DF_DP ## X))
df_t Seg7Display::digitFlags(dp_e X, df_t flags)
{
    if (X == DP_0)
        return DIGIT_FLAGS(0, flags);
    else if (X == DP_1)
        return DIGIT_FLAGS(1, flags);
    else if (X == DP_2)
        return DIGIT_FLAGS(2, flags);
    else if (X == DP_3)
        return DIGIT_FLAGS(3, flags);

    return DF_NONE;
}

void Seg7Display::setDPFlag(dp_e X, df_t & flags)
{
    if (X == DP_0)
        flags |= DF_DP0;
    else if (X == DP_1)
        flags |= DF_DP1;
    else if (X == DP_2)
        flags |= DF_DP2;
    else if (X == DP_3)
        flags |= DF_DP3;
}

// Mapped position in display array
uint8_t Seg7Display::mPos(dp_e X)
{
    return (X < DP_2) ? X : X + 1;
}

// Maps digit to display value
void Seg7Display::setDigit(uint8_t mX, dp_e X, df_t flags)
{
    if (flags != DF_NONE)
    {
        flags = digitFlags(X, flags);

        if (flags & DF_BL_MASK)
            mX = 0x00;

        if (flags & DF_DP_MASK)
            mX |= _decimal;
    }

    _positions[mPos(X)] = mX;
}

// Maps number to display value and sets to position in display array
void Seg7Display::setNum(uint8_t dX, dp_e X, df_t flags)
{
    dX = _s_number_map[(uint8_t)validateValue(dX, 0, sizeof(_s_number_map) - 1)];
    setDigit(dX, X, flags);
}

void Seg7Display::setChar(uint8_t cX, dp_e X, df_t flags)
{
    cX = _s_char_map[(uint8_t)validateValue(cX, 0, sizeof(_s_char_map) - 1)];
    setDigit(cX, X, flags);
}

void Seg7Display::setColon(df_t flags)
{
    _positions[2] = (flags & DF_COLON) ? _colon : 0x00;
}

void Seg7Display::numberTypeValues(uint32_t & max, uint32_t & num, uint32_t & den, nd_e nd, df_t flags)
{
    if (flags & DF_OCT)
    {
        den = 8;
        max = 1 << (nd * 3);
        num = max >> 3;
        max -= 1;
    }
    else if (flags & DF_HEX)
    {
        den = 16;
        max = 1 << (nd * 4);
        num = max >> 4;
        max -= 1;
    }
    else
    {
        switch (nd)
        {
            case ND_1: max =    10; break;
            case ND_2: max =   100; break;
            case ND_3: max =  1000; break;
            case ND_4: max = 10000; break;
        }

        den = 10;
        num = max / den;
        max -= 1;
    }
}

void Seg7Display::setBlank(void)
{
    for (int i = 0; i < 5; i++)
        _positions[i] = 0x00;
}

void Seg7Display::blank(void)
{
    setBlank();
    show();
}

// Reads from left to right on display
void Seg7Display::setDigits(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, df_t flags)
{
    if (((flags & DF_BL) == DF_BL) && !(flags & DF_COLON))
    {
        setBlank();
        return;
    }

    setChar(d0, DP_0, flags);
    setChar(d1, DP_1, flags);
    setChar(d2, DP_2, flags);
    setChar(d3, DP_3, flags);
    setColon(flags);
}

void Seg7Display::showDigits(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, df_t flags)
{
    setDigits(d0, d1, d2, d3, flags);
    show();
}

void Seg7Display::setInteger(int32_t integer, dp_e dp, nd_e nd, df_t flags)
{
    if ((flags & DF_BL) == DF_BL)
    {
        setBlank();
        return;
    }

    uint16_t n;
    if (integer < 0)
    {
        n = (uint16_t)(integer * -1);
        flags |= DF_NEG;
    }
    else
    {
        n = (uint16_t)integer;
    }


    nd_e ntv_nd = (dp + nd) > ND_4 ? (nd_e)(nd - dp) : nd;
    uint32_t max, num, den;
    numberTypeValues(max, num, den, ntv_nd, flags);

    n = validateValue(n, 0, max);

    bool zeros = flags & DF_LZ;
    uint8_t end = (dp + nd) > 4 ? 4 : dp + nd;

    for (uint8_t i = dp; i < end; i++)
    {
        // If last digit is 0 or current digit is not zero, start
        // displaying zeros
        if (((i == 3) && (n == 0)) || (n >= num))
            zeros = true;

        setNum((uint8_t)(n / num), (dp_e)i, (zeros ? flags : (flags | DF_BL_MASK)));

        n %= num;
        num /= den;
    }

    setColon(DF_NONE);
}

void Seg7Display::showInteger(int32_t integer, dp_e dp, nd_e nd, df_t flags)
{
    setInteger(integer, dp, nd, flags);
    show();
}

void Seg7Display::setInteger(int32_t integer, df_t flags)
{
    setInteger(integer, DP_0, ND_4, flags);
}

void Seg7Display::showInteger(int32_t integer, df_t flags)
{
    setInteger(integer, flags);
    show();
}

void Seg7Display::setTime(uint8_t hour_min, uint8_t min_sec, df_t flags)
{
    setNum(hour_min / 10, DP_0, flags);
    setNum(hour_min % 10, DP_1, flags);
    setNum(min_sec / 10, DP_2, flags);
    setNum(min_sec % 10, DP_3, flags);
    setColon(flags | DF_COLON);
}

void Seg7Display::showTime(uint8_t hour_min, uint8_t min_sec, df_t flags)
{
    setTime(hour_min, min_sec, flags);
    show();
}

void Seg7Display::setTimer(uint8_t hour_min, uint8_t min_sec, df_t flags)
{
    // Blanks out both digits before colon if zero and just first if 1-9
    if ((hour_min < 10) && (flags & DF_NO_LZ))
        flags |= ((hour_min == 0) ? DF_BL_BC : DF_BL0);

    setTime(hour_min, min_sec, flags);
}

void Seg7Display::showTimer(uint8_t hour_min, uint8_t min_sec, df_t flags)
{
    setTimer(hour_min, min_sec, flags);
    show();
}

void Seg7Display::setClock(uint8_t hour, uint8_t minute, df_t flags)
{
    if (flags & DF_12H)
    {
        // If PM, show decimal point at end
        if (hour > 11)
            flags |= DF_PM;

        // No leading zeros
        flags |= DF_NO_LZ;

        hour = (hour == 0) ? 12 : ((hour > 12) ? (hour - 12) : hour);
    }

    // Blanks out first digit if zero
    if ((hour < 10) && (flags & DF_NO_LZ))
        flags |= DF_BL0;

    setTime(hour, minute, flags);
}

void Seg7Display::showClock(uint8_t hour, uint8_t minute, df_t flags)
{
    setClock(hour, minute, flags);
    show();
}

void Seg7Display::setClock12(uint8_t hour, uint8_t minute, df_t flags)
{
    setClock(hour, minute, flags | DF_12H);
}

void Seg7Display::showClock12(uint8_t hour, uint8_t minute, df_t flags)
{
    setClock12(hour, minute, flags);
    show();
}

void Seg7Display::setClock24(uint8_t hour, uint8_t minute, df_t flags)
{
    setClock(hour, minute, flags | DF_24H);
}

void Seg7Display::showClock24(uint8_t hour, uint8_t minute, df_t flags)
{
    setClock24(hour, minute, flags);
    show();
}

void Seg7Display::setDate(uint8_t month, uint8_t day, df_t flags)
{
    flags |= DF_DP1;  // Add decimal to delineate

    if (month < 10)
        setDigit(0, DP_0, flags);
    else
        setNum(month / 10, DP_0, flags);
    setNum(month % 10, DP_1, flags);
    setNum(day / 10, DP_2, flags);
    setNum(day % 10, DP_3, flags);
    setColon(DF_NONE);
}

void Seg7Display::showDate(uint8_t month, uint8_t day, df_t flags)
{
    setDate(month, day, flags);
    show();
}

void Seg7Display::setOption(char const * opt, uint8_t val, df_t flags)
{
    setString(opt, DP_0, ND_2, flags);
    setInteger(val, DP_2, ND_2, flags);
    setColon(DF_COLON);
}

void Seg7Display::showOption(char const * opt, uint8_t val, df_t flags)
{
    setOption(opt, val, flags);
    show();
}

void Seg7Display::setOption(char const * opt, char val, df_t flags)
{
    setString(opt, DP_0, ND_2, flags);
    setDigit(0, DP_2, flags);
    setChar(val, DP_3, flags);
    setColon(DF_COLON);
}

void Seg7Display::showOption(char const * opt, char val, df_t flags)
{
    setOption(opt, val, flags);
    show();
}

void Seg7Display::setString(char const * str, dp_e dp, nd_e nd, df_t flags)
{
    if (((flags & DF_BL) == DF_BL) || (str == nullptr) || (str[0] == 0))
    {
        setBlank();
        return;
    }

    enum ct_e : uint8_t { tCHAR, tDOT, tCOLON, tNULL };

    // Should maybe use character map to validate character can be displayed.
    auto input_type = [](char c) -> ct_e
    {
        if (c == 0) return tNULL;
        else if (c == '.') return tDOT;
        else if (c == ':') return tCOLON;
        else return tCHAR;
    };

    uint8_t buf[4] = { 0, 0, 0, 0 };
    df_t colon = DF_NONE;
    uint8_t j = dp;
    uint8_t bc = 2;
    uint8_t end = (dp + nd) > 4 ? 4 : dp + nd;

    for (uint8_t i = 0; (input_type(str[i]) != tNULL) && (j < end); i++)
    {
        ct_e ct = input_type(str[i]), nct = input_type(str[i+1]);

        if (ct == tCHAR)
        {
            buf[j] = str[i];
            if (nct != tDOT) j++;
        }
        else if (ct == tDOT)
        {
            buf[j++] |= 0x80;
        }
        else if (ct == tCOLON)
        {
            if ((j > 2) || (colon != DF_NONE))
                break;

            bc = j;

            if ((j + 2) < end)
                end = j + 2;

            colon = DF_COLON;
        }
    }

    uint8_t pos = dp;

    // Possible blanks before colon
    for (; pos < (2 - bc); pos++)
        setChar(0x00, (dp_e)pos, flags);

    // Characters plus decimal
    for (uint8_t i = 0; i < j; i++, pos++)
    {
        if (buf[i] & 0x80)
            setDPFlag((dp_e)pos, flags);

        setChar(buf[i] & 0x7F, (dp_e)pos, flags);
    }

    // Possible blanks after colon
    for (; pos < end; pos++)
        setChar(0x00, (dp_e)pos, flags);

    setColon(colon);
}

void Seg7Display::showString(char const * str, dp_e dp, nd_e nd, df_t flags)
{
    setString(str, dp, nd, flags);
    show();
}

void Seg7Display::setString(char const * str, df_t flags)
{
    setString(str, DP_0, ND_4, flags);
}

void Seg7Display::showString(char const * str, df_t flags)
{
    setString(str, flags);
    show();
}

void Seg7Display::setBars(uint8_t n, bool reverse, df_t flags)
{
    static constexpr uint8_t const fbars[3] = { 0x08, 0x48, 0x49 };
    static constexpr uint8_t const rbars[3] = { 0x01, 0x41, 0x49 };
    uint8_t const * const bars = reverse ? rbars : fbars;

    n = validateValue(n, 1, 3);

    for (uint8_t i = DP_0; i <= DP_3; i++)
        setDigit(bars[n - 1], (dp_e)i, flags);

    setColon(DF_NONE);
}

void Seg7Display::showBars(uint8_t n, bool reverse, df_t flags)
{
    setBars(n, reverse, flags);
    show();
}

void Seg7Display::setDashes(nd_e nd, bool reverse, df_t flags)
{
    uint8_t inc = reverse ? -1 : 1;
    dp_e dp = reverse ? DP_3 : DP_0;
    uint8_t n = 0;

    while (n != ND_4)
    {
        if (n < nd)
            setDigit(0x40, dp, flags);
        else
            setDigit(0x00, dp, flags);

        dp = (dp_e)(dp + inc);
        n++;
    }

    setColon(DF_NONE);
}

void Seg7Display::showDashes(nd_e nd, bool reverse, df_t flags)
{
    setDashes(nd, reverse, flags);
    show();
}

void Seg7Display::setUnderscores(nd_e nd, bool reverse, df_t flags)
{
    uint8_t inc = reverse ? -1 : 1;
    dp_e dp = reverse ? DP_3 : DP_0;
    uint8_t n = 0;

    while (n != ND_4)
    {
        if (n < nd)
            setDigit(0x08, dp, flags);
        else
            setDigit(0x00, dp, flags);

        dp = (dp_e)(dp + inc);
        n++;
    }

    setColon(DF_NONE);
}

void Seg7Display::showUnderscores(nd_e nd, bool reverse, df_t flags)
{
    setUnderscores(nd, reverse, flags);
    show();
}

void Seg7Display::setColon(void)
{
    setColon(DF_COLON);
}

void Seg7Display::showColon(void)
{
    setColon();
    show();
}

void Seg7Display::showDashes(df_t flags)
{
    if (flags & DF_BL) blank();
    else showDashes(ND_4, false, flags);
}

void Seg7Display::showUnderscores(df_t flags)
{
    if (flags & DF_BL) blank();
    else showUnderscores(ND_4, false, flags);
}

void Seg7Display::showError(df_t flags)
{
    if (flags & DF_BL) blank();
    else showString("ERR");
}

void Seg7Display::showDone(df_t flags)
{
    if (flags & DF_BL) blank();
    else showString("donE");
}

void Seg7Display::showOff(df_t flags)
{
    if (flags & DF_BL) blank();
    else showString("OFF");
}

void Seg7Display::showBeep(df_t flags)
{
    if (flags & DF_BL) blank();
    else showString("bEEP");
}

void Seg7Display::showPlay(df_t flags)
{
    if (flags & DF_BL) blank();
    else showString("PLAY");
}

void Seg7Display::show12Hour(df_t flags)
{
    if (flags & DF_BL) blank();
    else showString("12 H");
}

void Seg7Display::show24Hour(df_t flags)
{
    if (flags & DF_BL) blank();
    else showString("24 H");
}

