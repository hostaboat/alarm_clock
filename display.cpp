#include "display.h"
#include "types.h"

constexpr uint8_t const Seg7Display::_s_number_map[];
constexpr uint8_t const Seg7Display::_s_char_map[];

uint16_t Seg7Display::validateValue(uint16_t n, uint16_t min, uint16_t max)
{
    return ((n < min) ? min : ((n > max) ? max : n));
}

#define DIGIT_FLAGS(X, flags)  ((flags) & (DF_BL ## X | DF_DP ## X))
df_t Seg7Display::digitFlags(uint8_t X, df_t flags)
{
    if (X == 0)
        return DIGIT_FLAGS(0, flags);
    else if (X == 1)
        return DIGIT_FLAGS(1, flags);
    else if (X == 2)
        return DIGIT_FLAGS(2, flags);
    else if (X == 3)
        return DIGIT_FLAGS(3, flags);

    return DF_NONE;
}

void Seg7Display::setDPFlag(uint8_t X, df_t & flags)
{
    if (X == 0)
        flags |= DF_DP0;
    else if (X == 1)
        flags |= DF_DP1;
    else if (X == 2)
        flags |= DF_DP2;
    else if (X == 3)
        flags |= DF_DP3;
}

// Mapped position in display array
uint8_t Seg7Display::mPos(uint8_t X)
{
    return (X < 2) ? X : (uint8_t)(validateValue(X, 0, 3) + 1);
}

// Maps digit to display value
void Seg7Display::setDigit(uint8_t mX, uint8_t X, df_t flags)
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
void Seg7Display::setNum(uint8_t dX, uint8_t X, df_t flags)
{
    dX = _s_number_map[(uint8_t)validateValue(dX, 0, sizeof(_s_number_map) - 1)];
    setDigit(dX, X, flags);
}

void Seg7Display::showNum(uint8_t dX, uint8_t X, df_t flags)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        if (i == X) setNum(dX, X, flags);
        else setNum(0, i, DF_BL);
    }

    show();
}

void Seg7Display::setChar(uint8_t cX, uint8_t X, df_t flags)
{
    cX = _s_char_map[(uint8_t)validateValue(cX, 0, sizeof(_s_char_map) - 1)];
    setDigit(cX, X, flags);
}

void Seg7Display::showChar(uint8_t cX, uint8_t X, df_t flags)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        if (i == X) setChar(cX, X, flags);
        else setChar(0, i, DF_BL);
    }

    show();
}

void Seg7Display::setColon(df_t flags)
{
    _positions[2] = (flags & DF_COLON) ? _colon : 0x00;
}

void Seg7Display::numberTypeValues(uint16_t & max, uint16_t & num, uint16_t & den, df_t flags)
{
    if (flags & DF_OCT)
    {
        max = 4093; num = 512; den = 8;
    }
    else if (flags & DF_HEX)
    {
        max = 65535; num = 4096; den = 16;
    }
    else
    {
        max = 9999; num = 1000; den = 10;
    }
}

void Seg7Display::setBlank(void)
{
    for (int i = 0; i < 5; i++)
        _positions[i] = 0x00;
}

void Seg7Display::blank(void)
{
    // Doesn't turn it off or put it in standby but just blanks it
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

    setChar(d0, 0, flags);
    setChar(d1, 1, flags);
    setChar(d2, 2, flags);
    setChar(d3, 3, flags);
    setColon(flags);
}

void Seg7Display::showDigits(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, df_t flags)
{
    setDigits(d0, d1, d2, d3, flags);
    show();
}

void Seg7Display::setInteger(int32_t integer, df_t flags)
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

    uint16_t max, num, den;
    numberTypeValues(max, num, den, flags);

    n = validateValue(n, 0, max);

    bool zeros = flags & DF_LZ;

    for (uint8_t i = 0; i < 4; i++)
    {
        // If last digit is 0 or current digit is not zero, start
        // displaying zeros
        if (((i == 3) && (n == 0)) || (n >= num))
            zeros = true;

        setNum((uint8_t)(n / num), i, (zeros ? flags : (flags | DF_BL_MASK)));

        n %= num;
        num /= den;
    }

    setColon(DF_NONE);
}

void Seg7Display::showInteger(int32_t integer, df_t flags)
{
    setInteger(integer, flags);
    show();
}

void Seg7Display::setTime(uint8_t hour_min, uint8_t min_sec, df_t flags)
{
    setNum(hour_min / 10, 0, flags);
    setNum(hour_min % 10, 1, flags);
    setNum(min_sec / 10, 2, flags);
    setNum(min_sec % 10, 3, flags);
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
        setDigit(0, 0, flags);
    else
        setNum(month / 10, 0, flags);
    setNum(month % 10, 1, flags);
    setNum(day / 10, 2, flags);
    setNum(day % 10, 3, flags);
    setColon(DF_NONE);
}

void Seg7Display::showDate(uint8_t month, uint8_t day, df_t flags)
{
    setDate(month, day, flags);
    show();
}

#ifndef USE_STR_STATE_FUNC
void Seg7Display::setString(char const * str, df_t flags)
{
    if (((flags & DF_BL) == DF_BL) || (str[0] == 0))
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

    static uint8_t buf[4] = { 0, 0, 0, 0 };
    df_t colon = DF_NONE;
    uint8_t j = 0, end = 4, bc = 2;

    for (uint8_t i = 0; (input_type(str[i]) != tNULL) && (j < end); i++)
    {
        ct_e ct = input_type(str[i]), nct = input_type(str[i+1]);
        uint8_t c = str[i];
        
        if ((ct == tCHAR) && (nct == tDOT))
        {
            buf[j++] = c | 0x80;
            i++;  // Move past decimal
        }
        else if (ct == tCHAR)
        {
            buf[j++] = c;
        }
        else if (ct == tDOT)
        {
            buf[j++] = 0x80;
        }
        else if (ct == tCOLON)
        {
            if ((j > 2) || (colon != DF_NONE))
                break;

            bc = j;
            end = j + 2;
            colon = DF_COLON;
        }
    }

    // If there's a colon keep characters next to it on both sides
    uint8_t pos = 0;

    // Possible blanks before colon
    for (; pos < (2 - bc); pos++)
        setChar(0x00, pos, flags);

    // Characters plus decimal
    for (uint8_t i = 0; i < j; i++, pos++)
    {
        if (buf[i] & 0x80)
            setDPFlag(pos, flags);

        setChar(buf[i] & 0x7F, pos, flags);
    }

    // Possible blanks after colon
    for (; pos < 4; pos++)
        setChar(0x00, pos, flags);

    setColon(colon);
}
#else
void Seg7Display::setString(char const * str, df_t flags)
{
    if (((flags & DF_BL) == DF_BL) || (str[0] == 0))
    {
        setBlank();
        return;
    }

    enum { tCHAR, tDOT, tCOLON, tNULL };
    enum { INPUT, NEXT, FAIL };
    enum { E = 15 };  // End state

    static uint8_t const states[E][FAIL+1] = {
    //     INPUT  NEXT  FAIL
        {  tCHAR,    1,    8 },  //  0  CHAR 0 or  DOT 0
        {  tCHAR,    3,    8 },  //  1   DOT 0 or CHAR 1
        {  tCHAR,    3,    9 },  //  2  CHAR 1 or  DOT 1
        {  tCHAR,    6,    9 },  //  3   DOT 1 or CHAR 2
        {  tCHAR,    6,   10 },  //  4   COLON or CHAR 2 or DOT 2
        {  tCHAR,    6,   11 },  //  5  CHAR 2 or  DOT 2
        {  tCHAR,   12,   11 },  //  6   DOT 2 or CHAR 3
        {  tCHAR,   12,   12 },  //  7  CHAR 3 or  DOT 3
        {   tDOT,    2,   13 },  //  8   DOT 0 or COLON
        {   tDOT,    4,   13 },  //  9   DOT 1 or COLON
        {   tDOT,    7,   13 },  // 10   COLON or  DOT 2
        {   tDOT,    7,   14 },  // 11   DOT 2 or NULL
        {   tDOT,   14,   14 },  // 12   DOT 3 or NULL
        { tCOLON,    5,   14 },  // 13   COLON or NULL
        {  tNULL,    E,    E },  // 14        NULL
    };

    // Should maybe use character map to validate character can be displayed.
    auto input_type = [](char c)
    {
        if (c == 0) return tNULL;
        else if (c == '.') return tDOT;
        else if (c == ':') return tCOLON;
        else return tCHAR;
    };

    auto input_cmp = [&](char c, uint8_t cs)
    {
        return input_type(c) == states[cs][INPUT];
    };

    auto advance_state = [&](char c, uint8_t & cs)
    {
        while ((cs != E) && !input_cmp(c, cs))
            cs = states[cs][FAIL];
        return (cs != E);
    };

    auto next_state = [&](uint8_t & cs)
    {
        return ((cs = states[cs][NEXT]) != E);
    };

    auto validate_string = [&](char const * s)
    {
        uint8_t cs = 0, i = 0;
        while (advance_state(s[i], cs) && next_state(cs))
            i++;
        return i;
    };

    uint8_t end = validate_string(str);

    uint8_t buf[4] = { 0, 0, 0, 0 };
    uint8_t lc = 0;
    df_t colon = DF_NONE;

    for (int8_t i = end - 1, j = 3; i >= 0; i--)
    {
        char c = str[i];

        if (input_type(c) == tDOT)
            buf[(input_type(lc) == tDOT) ? --j : j] = 0x80;
        else if (input_type(c) == tCOLON)
            colon = DF_COLON;
        else
            buf[j--] |= (uint8_t)c;

        lc = c;
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        if (buf[i] & 0x80)
            setDPFlag(i, flags);

        setChar(buf[i] & 0x7F, i, flags);
    }

    setColon(colon);
}
#endif

void Seg7Display::showString(char const * str, df_t flags)
{
    setString(str, flags);
    show();
}

void Seg7Display::showDashes(df_t flags)
{
    if (flags & DF_BL)
    {
        blank();
        return;
    }

#ifdef USE_STR_FUNC
    setString("----");
#else
    setChar('-', 0);
    setChar('-', 1);
    setChar('-', 2);
    setChar('-', 3);
    setColon(DF_NONE);
#endif

    show();
}

void Seg7Display::showError(df_t flags)
{
    if (flags & DF_BL)
    {
        blank();
        return;
    }

#ifdef USE_STR_FUNC
    setString("Err");
#else
    setChar( 0 , 0);
    setChar('E', 1);
    setChar('r', 2);
    setChar('r', 3);
    setColon(DF_NONE);
#endif

    show();
}

void Seg7Display::showDone(df_t flags)
{
    if (flags & DF_BL)
    {
        blank();
        return;
    }

#ifdef USE_STR_FUNC
    setString("donE");
#else
    setChar('d', 0);
    setChar('o', 1);
    setChar('n', 2);
    setChar('E', 3);
    setColon(DF_NONE);
#endif

    show();
}

void Seg7Display::showLowBattery(df_t flags)
{
    if (flags & DF_BL)
    {
        blank();
        return;
    }

#ifdef USE_STR_FUNC
    setString("Lo.bA.");
#else
    setChar('L', 0);
    setChar('o', 1, DF_DP1);
    setChar('b', 2);
    setChar('A', 3, DF_DP3);
    setColon(DF_NONE);
#endif

    show();
}

void Seg7Display::showOff(df_t flags)
{
    if (flags & DF_BL)
    {
        blank();
        return;
    }

#ifdef USE_STR_FUNC
    setString("OFF");
#else
    setChar( 0 , 0);
    setChar('O', 1);
    setChar('F', 2);
    setChar('F', 3);
    setColon(DF_NONE);
#endif

    show();
}

void Seg7Display::showBeep(df_t flags)
{
    if (flags & DF_BL)
    {
        blank();
        return;
    }

#ifdef USE_STR_FUNC
    setString("bEEP");
#else
    setChar('b', 0);
    setChar('E', 1);
    setChar('E', 2);
    setChar('P', 3);
    setColon(DF_NONE);
#endif

    show();
}

void Seg7Display::showPlay(df_t flags)
{
    if (flags & DF_BL)
    {
        blank();
        return;
    }

#ifdef USE_STR_FUNC
    setString("PLAY");
#else
    setChar('P', 0);
    setChar('L', 1);
    setChar('A', 2);
    setChar('Y', 3);
    setColon(DF_NONE);
#endif

    show();
}

void Seg7Display::show12Hour(df_t flags)
{
    if (flags & DF_BL)
    {
        blank();
        return;
    }

#ifdef USE_STR_FUNC
    setString("12 H");
#else
    setChar('1', 0);
    setChar('2', 1);
    setChar(' ', 2);
    setChar('H', 3);
    setColon(DF_NONE);
#endif

    show();
}

void Seg7Display::show24Hour(df_t flags)
{
    if (flags & DF_BL)
    {
        blank();
        return;
    }

#ifdef USE_STR_FUNC
    setString("24 H");
#else
    setChar('2', 0);
    setChar('4', 1);
    setChar(' ', 2);
    setChar('H', 3);
    setColon(DF_NONE);
#endif

    show();
}

void Seg7Display::showTouch(df_t flags)
{
    if (flags & DF_BL)
    {
        blank();
        return;
    }

#ifdef USE_STR_FUNC
    setString("tucH");
#else
    setChar('t', 0);
    setChar('u', 1);
    setChar('c', 2);
    setChar('H', 3);
    setColon(DF_NONE);
#endif

    show();
}

