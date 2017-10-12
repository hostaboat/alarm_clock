/*******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 FastLED
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 ******************************************************************************/

// Code refactored and simplified for Teensy3 (MK20DX256VLH7) and Neopixels

#ifndef _OPTI_H_
#define _OPTI_H_

#include "dev.h"
#include "pin.h"
#include "utility.h"
#include "types.h"

//#define LED_COLOR_CORRECTION

struct CHSV;
struct CRGB;

uint8_t sqrt16(uint16_t x);

inline uint8_t scale8(uint16_t i, uint8_t scale)
{
    if (i == 0) return 0;
    return (i * (1 + scale)) >> 8;
}

inline uint8_t scale8_video(uint16_t i, uint8_t scale)
{
    if ((i == 0) || (scale == 0)) return 0;
    return ((i * scale) >> 8) + 1;
}

// This does an unsigned saturating add (actually 4 in a 32-bit register),
// meaning if the value would overflow it is set to the max of the data
// type, in this case, for uint8_t, 255.
inline uint8_t qadd8(uint8_t i, uint8_t j)
{
    __asm__ volatile ("uqadd8 %0, %0, %1" : "+r" (i) : "r" (j));
    return i;
}

inline uint8_t qsub8(uint8_t i, uint8_t j)
{
    int t = i - j;
    if (t < 0) t = 0;
    return t;
}

inline uint8_t rand8(uint8_t lo, uint8_t hi, uint16_t entropy = 0)
{
    static uint16_t seed = 0;

    if (lo == hi)
        return lo;

    if (lo > hi)
    {
        uint8_t tmp = lo;
        lo = hi;
        hi = tmp;
    }

    seed = (seed * 2053) + 13849 + entropy;
    uint8_t r = (uint8_t)(seed & 0xFF) + (uint8_t)(seed >> 8);
    return (r % ((hi - lo) + 1)) + lo;
}

void hsv2rgb_rainbow(CHSV const & hsv, CRGB & rgb);
void rgb2hsv_approximate(CRGB const & rgb, CHSV & hsv);

struct CHSV
{
    union
    {
        struct { uint8_t h; uint8_t s; uint8_t v; };
        uint8_t raw[3];
    };

    enum Hue : uint8_t
    {
        RED = 0,
        ORANGE = 32,
        YELLOW = 64,
        GREEN = 96,
        AQUA = 128,
        BLUE = 160,
        PURPLE = 192,
        PINK = 224,
    };

    CHSV(void) {}
    CHSV(uint8_t h, uint8_t s, uint8_t v) : h(h), s(s), v(v) {}
    CHSV(CHSV const & copy) { h = copy.h; s = copy.s; v = copy.v; }
    CHSV(CRGB const & copy) { rgb2hsv_approximate(copy, *this); }
    CHSV & operator=(CHSV const & rval) { h = rval.h; s = rval.s; v = rval.v; return *this; }
    CHSV & operator=(CRGB const & rval) { rgb2hsv_approximate(rval, *this); return *this; }
    bool operator ==(CHSV const & rhs) const {
        for (uint8_t i = 0; i < sizeof(raw); i++) { if (raw[i] != rhs.raw[i]) return false; } return true; }
    bool operator != (CHSV const & rhs) const { return !(*this == rhs); }
    uint8_t & operator[](uint8_t i) { return raw[i]; }
    const uint8_t & operator[](uint8_t i) const { return raw[i]; }
};

struct CRGB
{
    union
    {
        struct { uint8_t r; uint8_t g; uint8_t b; };
        uint8_t raw[3];
    };

    enum Color : uint32_t
    {
        ALICE_BLUE             = 0xF0F8FF,
        AMETHYST               = 0x9966CC,
        ANTIQUE_WHITE          = 0xFAEBD7,
        AQUA                   = 0x00FFFF,
        AQUAMARINE             = 0x7FFFD4,
        AZURE                  = 0xF0FFFF,
        BEIGE                  = 0xF5F5DC,
        BISQUE                 = 0xFFE4C4,
        BLACK                  = 0x000000,
        BLANCHED_ALMOND        = 0xFFEBCD,
        BLUE                   = 0x0000FF,
        BLUE_VIOLET            = 0x8A2BE2,
        BROWN                  = 0xA52A2A,
        BURLY_WOOD             = 0xDEB887,
        CADET_BLUE             = 0x5F9EA0,
        CHARTREUSE             = 0x7FFF00,
        CHOCOLATE              = 0xD2691E,
        CORAL                  = 0xFF7F50,
        CORNFLOWER_BLUE        = 0x6495ED,
        CORNSILK               = 0xFFF8DC,
        CRIMSON                = 0xDC143C,
        DARK_BLUE              = 0x00008B,
        DARK_CYAN              = 0x008B8B,
        DARK_GOLDENROD         = 0xB8860B,
        DARK_GREEN             = 0x006400,
        DARK_GREY              = 0xA9A9A9,
        DARK_KHAKI             = 0xBDB76B,
        DARK_MAGENTA           = 0x8B008B,
        DARK_OLIVE_GREEN       = 0x556B2F,
        DARK_ORANGE            = 0xFF8C00,
        DARK_ORCHID            = 0x9932CC,
        DARK_RED               = 0x8B0000,
        DARK_SALMON            = 0xE9967A,
        DARK_SEA_GREEN         = 0x8FBC8F,
        DARK_SLATE_BLUE        = 0x483D8B,
        DARK_SLATE_GREY        = 0x2F4F4F,
        DARK_TURQUOISE         = 0x00CED1,
        DARK_VIOLET            = 0x9400D3,
        DEEP_PINK              = 0xFF1493,
        DEEP_SKY_BLUE          = 0x00BFFF,
        DIM_GREY               = 0x696969,
        DODGER_BLUE            = 0x1E90FF,
#ifdef LED_COLOR_CORRECTION
        FAIRY_LIGHT            = 0xFFE42D,
#else
        FAIRY_LIGHT            = 0xFF9D2A,
#endif
        FIRE_BRICK             = 0xB22222,
        FLORAL_WHITE           = 0xFFFAF0,
        FOREST_GREEN           = 0x228B22,
        FUCHSIA                = 0xFF00FF,
        GAINSBORO              = 0xDCDCDC,
        GHOST_WHITE            = 0xF8F8FF,
        GOLD                   = 0xFFD700,
        GOLDENROD              = 0xDAA520,
        GREEN                  = 0x008000,
        GREEN_YELLOW           = 0xADFF2F,
        GREY                   = 0x808080,
        HONEYDEW               = 0xF0FFF0,
        HOT_PINK               = 0xFF69B4,
        INDIAN_RED             = 0xCD5C5C,
        INDIGO                 = 0x4B0082,
        IVORY                  = 0xFFFFF0,
        KHAKI                  = 0xF0E68C,
        LAVENDER               = 0xE6E6FA,
        LAVENDER_BLUSH         = 0xFFF0F5,
        LAWN_GREEN             = 0x7CFC00,
        LEMON_CHIFFON          = 0xFFFACD,
        LIGHT_BLUE             = 0xADD8E6,
        LIGHT_CORAL            = 0xF08080,
        LIGHT_CYAN             = 0xE0FFFF,
        LIGHT_GOLDENROD_YELLOW = 0xFAFAD2,
        LIGHT_GREEN            = 0x90EE90,
        LIGHT_GREY             = 0xD3D3D3,
        LIGHT_PINK             = 0xFFB6C1,
        LIGHT_SALMON           = 0xFFA07A,
        LIGHT_SEA_GREEN        = 0x20B2AA,
        LIGHT_SKY_BLUE         = 0x87CEFA,
        LIGHT_SLATE_GREY       = 0x778899,
        LIGHT_STEEL_BLUE       = 0xB0C4DE,
        LIGHT_YELLOW           = 0xFFFFE0,
        LIME                   = 0x00FF00,
        LIME_GREEN             = 0x32CD32,
        LINEN                  = 0xFAF0E6,
        MAROON                 = 0x800000,
        MEDIUM_AQUAMARINE      = 0x66CDAA,
        MEDIUM_BLUE            = 0x0000CD,
        MEDIUM_ORCHID          = 0xBA55D3,
        MEDIUM_PURPLE          = 0x9370DB,
        MEDIUM_SEA_GREEN       = 0x3CB371,
        MEDIUM_SLATE_BLUE      = 0x7B68EE,
        MEDIUM_SPRING_GREEN    = 0x00FA9A,
        MEDIUM_TURQUOISE       = 0x48D1CC,
        MEDIUM_VIOLET_RED      = 0xC71585,
        MIDNIGHT_BLUE          = 0x191970,
        MINT_CREAM             = 0xF5FFFA,
        MISTY_ROSE             = 0xFFE4E1,
        MOCCASIN               = 0xFFE4B5,
        NAVAJO_WHITE           = 0xFFDEAD,
        NAVY                   = 0x000080,
        OLD_LACE               = 0xFDF5E6,
        OLIVE                  = 0x808000,
        OLIVE_DRAB             = 0x6B8E23,
        ORANGE                 = 0xFFA500,
        ORANGE_RED             = 0xFF4500,
        ORCHID                 = 0xDA70D6,
        PALE_GOLDENROD         = 0xEEE8AA,
        PALE_GREEN             = 0x98FB98,
        PALE_TURQUOISE         = 0xAFEEEE,
        PALE_VIOLET_RED        = 0xDB7093,
        PAPAYA_WHIP            = 0xFFEFD5,
        PEACH_PUFF             = 0xFFDAB9,
        PERU                   = 0xCD853F,
        PINK                   = 0xFFC0CB,
        PLAID                  = 0xCC5533,
        PLUM                   = 0xDDA0DD,
        POWDER_BLUE            = 0xB0E0E6,
        PURPLE                 = 0x800080,
        RED                    = 0xFF0000,
        ROSY_BROWN             = 0xBC8F8F,
        ROYAL_BLUE             = 0x4169E1,
        SADDLE_BROWN           = 0x8B4513,
        SALMON                 = 0xFA8072,
        SANDY_BROWN            = 0xF4A460,
        SEASHELL               = 0xFFF5EE,
        SEA_GREEN              = 0x2E8B57,
        SIENNA                 = 0xA0522D,
        SILVER                 = 0xC0C0C0,
        SKY_BLUE               = 0x87CEEB,
        SLATE_BLUE             = 0x6A5ACD,
        SLATE_GREY             = 0x708090,
        SNOW                   = 0xFFFAFA,
        SPRING_GREEN           = 0x00FF7F,
        STEEL_BLUE             = 0x4682B4,
        TAN                    = 0xD2B48C,
        TEAL                   = 0x008080,
        THISTLE                = 0xD8BFD8,
        TOMATO                 = 0xFF6347,
        TURQUOISE              = 0x40E0D0,
        VIOLET                 = 0xEE82EE,
        WHEAT                  = 0xF5DEB3,
        WHITE                  = 0xFFFFFF,
        WHITE_SMOKE            = 0xF5F5F5,
        YELLOW                 = 0xFFFF00,
        YELLOW_GREEN           = 0x9ACD32,
    };

    CRGB(void) {}
    CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
    CRGB(uint32_t colorcode)
        : r((colorcode >> 16) & 0xFF), g((colorcode >> 8) & 0xFF), b((colorcode >> 0) & 0xFF) {}
    CRGB(CHSV const & copy) { hsv2rgb_rainbow(copy, *this); }
    CRGB(CRGB const & copy) { r = copy.r; g = copy.g; b = copy.b; }
    CRGB & operator=(CRGB const & rval) { r = rval.r; g = rval.g; b = rval.b; return *this; }
    CRGB & operator=(uint32_t colorcode)
    {
        r = (colorcode >> 16) & 0xFF;
        g = (colorcode >>  8) & 0xFF;
        b = (colorcode >>  0) & 0xFF;
        return *this;
    }
    CRGB & operator=(CHSV const & rval) { hsv2rgb_rainbow(rval, *this); return *this; }
    uint8_t & operator[](uint8_t i) { return raw[i]; }
    const uint8_t & operator[](uint8_t i) const { return raw[i]; }
    bool operator == (CRGB const & rhs) const {
        for (uint8_t i = 0; i < sizeof(raw); i++) { if (raw[i] != rhs.raw[i]) return false; } return true; }
    bool operator != (CRGB const & rhs) const { return !(*this == rhs); }
    bool operator == (CHSV const & hsv) const { CRGB rgb; hsv2rgb_rainbow(hsv, rgb); return *this == rgb; }
    bool operator != (CHSV const & hsv) const { return !(*this == hsv); }
    bool operator == (uint32_t colorcode) const { CRGB c(colorcode); return *this == c; }
    bool operator != (uint32_t colorcode) const { return !(*this == colorcode); }
    bool isBlack(void) { return (r == 0) && (g == 0) && (b == 0); }
    uint32_t colorCode(void) const { return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b; }
};

struct Palette
{
    enum pal_e
    {
        PAL_PARTY_COLORS,
        PAL_PURPLE_SWIRL,
        PAL_LAVA_COLORS,
        PAL_OCEAN_COLORS,
        PAL_RAINBOW_COLORS,
        PAL_FOREST_COLORS,
        PAL_RWB_SWIRL,
        PAL_CNT
    };

    static CRGB colorFromPalette(pal_e pal, uint8_t index, uint8_t brightness);

    private:
        static CRGB const _s_party_colors[16];
        static CRGB const _s_purple_swirl[16];
        static CRGB const _s_lava_colors[16];
        static CRGB const _s_ocean_colors[16];
        static CRGB const _s_rainbow_colors[16];
        static CRGB const _s_forest_colors[16];
        static CRGB const _s_rwb_swirl[16];

        static CRGB const * const _s_palettes[PAL_CNT];
};

template < pin_t PIN, uint8_t N >
class Leds : public DevPin < PinOut, PIN >
{
    public:
        static Leds & acquire(void) { static Leds leds; return leds; }

        void show(void) { _show(_leds, _brightness); }
        void show(uint8_t brightness) { _show(_leds, brightness); }

        void show(CRGB const * rgb) { _show(rgb, _brightness); }
        void show(CRGB const * rgb, uint8_t brightness) { _show(rgb, brightness); }

        void showColor(void) { _showColor(_color, _brightness); }
        void showColor(uint8_t brightness) { _showColor(_color, brightness); }

        void showColor(CRGB const & rgb) { _showColor(rgb, _brightness); }
        void showColor(CRGB const & rgb, uint8_t brightness) { _showColor(rgb, brightness); }

        void showColor(CRGB::Color const & c) { _showColor((CRGB const &)c, _brightness); }
        void showColor(CRGB::Color const & c, uint8_t brightness) { _showColor((CRGB const &)c, brightness); }

        void startPalette(Palette::pal_e pal);
        void updatePalette(void);

        void set(CRGB const * rgb) { for (uint8_t i = 0; i < N; i++) _leds[i] = rgb[i]; }
        void update(CRGB const * rgb) { set(rgb); show(); }
        void setColor(CRGB const & rgb) { _color = rgb; for (uint8_t i = 0; i < N; i++) _leds[i] = _color; }
        void updateColor(CRGB const & rgb) { setColor(rgb); show(); }
        Leds & operator=(CRGB const & rval) { for (uint8_t i = 0; i < N; i++) _leds[i] = rval; return *this; }

        void setBrightness(uint8_t brightness) { _brightness = brightness; }
        void updateBrightness(uint8_t brightness) { setBrightness(brightness); show(); }
        uint8_t brightness(void) const { return _brightness; }

        void blank(void) { showColor(CRGB::BLACK, 0); }
        void clear(void) { for (uint8_t i = 0; i < N; i++) _leds[i] = 0; }
        bool isClear(void) { for (uint8_t i = 0; i < N; i++) if (!_leds[i].isBlack()) return false; return true; }

    private:
        Leds(void) { Debug::enableDWT(); DWT::enableCycleCount(); clear(); this->_pin.clear(); }

        static constexpr uint8_t const _s_trst = 50; 
        static constexpr uint32_t const _s_clks_per_us = (F_CPU / 1000000);
        template < uint32_t NS > static constexpr uint32_t ns(void)
        { return ((NS * _s_clks_per_us) + 999) / 1000; }
        static constexpr uint32_t const _s_t1 = ns < 250 > ();
        static constexpr uint32_t const _s_t2 = ns < 625 > ();
        static constexpr uint32_t const _s_t3 = ns < 375 > ();
        static constexpr uint32_t const _s_wait_0 = _s_t1 + _s_t2 + _s_t3;
        static constexpr uint32_t const _s_wait_1 = _s_t3 + (2 * (F_CPU / 24000000));
        static constexpr uint32_t const _s_wait_2 = _s_t2 + _s_t3 + (2 * (F_CPU / 24000000));
        static constexpr uint32_t _s_min_micros = 1000000 / 400;  // max refresh rate

        uint8_t _brightness = 255;
        bool _dither = true;
        uint16_t _fps = 0;  // Tracking for current FPS value
        CRGB _scale;
        CRGB _leds[N];
        CRGB _color;
        uint8_t const * _data = nullptr;
        uint8_t _advance = 0;
        uint8_t _d[3] = {};
        uint8_t _e[3] = {};
        UsMinWait < _s_trst > _us_wait;

        Palette::pal_e _palette = Palette::PAL_PARTY_COLORS;
        uint8_t _pindex = 0;
        uint8_t _pinc = 0;

        void countFPS(uint8_t num_frames = 25);
        void ditherInit(void);
        void binaryDitherInit(void);
        void adjustScale(uint8_t brightness);

        //enum order_e { RGB = 0012, RBG = 0021, GRB = 0102, GBR = 0120, BRG = 0201, BGR = 0210 };

        // For GRB the second, then the first then the last of the CRGB - 1 0 2
        static constexpr uint8_t const _s_rgb_order = 0x42; // GRB 0100 0010
        template < uint8_t X > constexpr uint8_t RB(void) { return (_s_rgb_order >> (3 * (2 - X))) & 0x03; }

        template < uint8_t SLOT > uint8_t loadByte(void) { return _data[RB<SLOT>()]; }
        template < uint8_t SLOT > uint8_t dither(uint8_t b) { return b ? qadd8(b, _d[RB<SLOT>()]) : 0; }
        template < uint8_t SLOT > uint8_t scale(uint8_t b) { return scale8_video(b, _scale.raw[RB<SLOT>()]); }

        void _show(CRGB const * rgb, uint8_t brightness) { _advance = 3; showLeds(rgb, brightness); }
        void _showColor(CRGB const & rgb, uint8_t brightness) { _advance = 0; showLeds(&rgb, brightness); }
        void updatePalette(uint8_t index);
        void showLeds(CRGB const * rgb, uint8_t brightness);
        void showPixels(void);
};

template < pin_t PIN, uint8_t N >
void Leds < PIN, N >::countFPS(uint8_t num_frames)
{
    static uint8_t br = 0;
    static uint32_t lastframe = msecs();

    if (br++ >= num_frames)
    {
        uint32_t now = msecs();
        now -= lastframe;
        _fps = (br * 1000) / now;
        br = 0;
        lastframe = now;
    }
}

template < pin_t PIN, uint8_t N >
void Leds < PIN, N >::ditherInit(void)
{
    if (_dither)
        binaryDitherInit();
    else
        _d[0] = _d[1] = _d[2] = _e[0] = _e[1] = _e[2] = 0;
}

template < pin_t PIN, uint8_t N >
void Leds < PIN, N >::binaryDitherInit(void)
{
    // Set 'virtual bits' of dithering to the highest level that is not likely to
    // cause excessive flickering at low brightness levels + low update rates.
    // These pre-set values are a little ambitious, since a 400Hz update rate for
    // WS2811-family LEDs is only possible with 85 pixels or fewer.
    // Once we have a 'number of milliseconds since last update' value available
    // here, we can quickly calculate the correct number of 'virtual bits' on the
    // fly with a couple of 'if' statements -- no division required.  At this
    // point, the division is done at compile time, so there's no runtime cost,
    // but the values are still hard-coded.
#define MAX_LIKELY_UPDATE_RATE_HZ     400
#define MIN_ACCEPTABLE_DITHER_RATE_HZ  50
#define UPDATES_PER_FULL_DITHER_CYCLE (MAX_LIKELY_UPDATE_RATE_HZ / MIN_ACCEPTABLE_DITHER_RATE_HZ)
#define RECOMMENDED_VIRTUAL_BITS ((UPDATES_PER_FULL_DITHER_CYCLE > 1) + \
        (UPDATES_PER_FULL_DITHER_CYCLE > 2) + \
        (UPDATES_PER_FULL_DITHER_CYCLE > 4) + \
        (UPDATES_PER_FULL_DITHER_CYCLE > 8) + \
        (UPDATES_PER_FULL_DITHER_CYCLE > 16) + \
        (UPDATES_PER_FULL_DITHER_CYCLE > 32) + \
        (UPDATES_PER_FULL_DITHER_CYCLE > 64) + \
        (UPDATES_PER_FULL_DITHER_CYCLE > 128))
#define VIRTUAL_BITS RECOMMENDED_VIRTUAL_BITS

    // R is the dither signal 'counter'.
    static uint8_t R = 0;
    R++;

    // R is wrapped around at 2^ditherBits,
    // so if ditherBits is 2, R will cycle through (0,1,2,3)
    uint8_t ditherBits = VIRTUAL_BITS;
    R &= (1 << ditherBits) - 1;

    // Q is the "unscaled dither signal" itself.
    // It's initialized to the reversed bits of R.
    // If 'ditherBits' is 2, Q here will cycle through (0,128,64,192)
    uint8_t Q = 0;

    // Reverse bits in a byte
    if (R & 0x01) { Q |= 0x80; }
    if (R & 0x02) { Q |= 0x40; }
    if (R & 0x04) { Q |= 0x20; }
    if (R & 0x08) { Q |= 0x10; }
    if (R & 0x10) { Q |= 0x08; }
    if (R & 0x20) { Q |= 0x04; }
    if (R & 0x40) { Q |= 0x02; }
    if (R & 0x80) { Q |= 0x01; }

    // Now we adjust Q to fall in the center of each range,
    // instead of at the start of the range.
    // If ditherBits is 2, Q will be (0, 128, 64, 192) at first,
    // and this adjustment makes it (31, 159, 95, 223).
    if (ditherBits < 8)
        Q += 1 << (7 - ditherBits);

    // D and E form the "scaled dither signal"
    // which is added to pixel values to affect the
    // actual dithering.

    // Setup the initial D and E values
    for (int i = 0; i < 3; i++)
    {
        uint8_t s = _scale.raw[i];
        _e[i] = s ? (256 / s) + 1 : 0;
        _d[i] = scale8(Q, _e[i]);
        if(_d[i]) _d[i]--;
        if(_e[i]) _e[i]--;
    }
}

template < pin_t PIN, uint8_t N >
void Leds < PIN, N >::adjustScale(uint8_t brightness)
{
    static constexpr uint32_t const cc = 0xFFB0F0; // Color correction
    static constexpr uint32_t const ct = 0xFFFFFF; // Color temperature

    if (brightness == 0)
    {
        _scale[0] = _scale[1] = _scale[2] = 0;
        return;
    }

    _scale[0] = (((((cc >>  0) & 0xFF) + 1) * (((ct >>  0) & 0xFF) + 1) * brightness) / 0x10000) & 0xFF;
    _scale[1] = (((((cc >>  8) & 0xFF) + 1) * (((ct >>  8) & 0xFF) + 1) * brightness) / 0x10000) & 0xFF;
    _scale[2] = (((((cc >> 16) & 0xFF) + 1) * (((ct >> 16) & 0xFF) + 1) * brightness) / 0x10000) & 0xFF;
}

template < pin_t PIN, uint8_t N >
void Leds < PIN, N >::startPalette(Palette::pal_e palette)
{
    _palette = palette;
    _pinc = rand8(3, 5);
    updatePalette(0);
}

template < pin_t PIN, uint8_t N >
void Leds < PIN, N >::updatePalette(void)
{
    updatePalette(_pindex);
}

template < pin_t PIN, uint8_t N >
void Leds < PIN, N >::updatePalette(uint8_t index)
{
    static uint32_t update = 0;

    if ((msecs() - update) < 22)
        return;

    update = msecs();

    _pindex = index;
    for (uint8_t i = 0; i < N; i++)
    {
        _leds[i] = Palette::colorFromPalette(_palette, index, _brightness);
        index += _pinc;
    }
    _pindex++;

    showLeds(_leds, _brightness);
}

template < pin_t PIN, uint8_t N >
void Leds < PIN, N >::showLeds(CRGB const * rgb, uint8_t brightness)
{
    static uint32_t last_show = 0;

#ifndef LED_COLOR_CORRECTION
    _scale[0] = _scale[1] = _scale[2] = brightness;
#else
    adjustScale(brightness);
#endif

    _data = (uint8_t const *)rgb;

    // Guard against showing too rapidly
    while ((usecs() - last_show) < _s_min_micros);

    last_show = usecs();

    bool d = _dither;

    if (_fps < 100)
        _dither = false;

    ditherInit();

    _us_wait.wait();
    showPixels();
    _us_wait.mark();

    _dither = d;

    countFPS();
}

template < pin_t PIN, uint8_t N >
void Leds < PIN, N >::showPixels(void)
{
    DWT::zeroCycleCount();

    this->_pin.clear();

    _d[RB<0>()] = _e[RB<0>()] - _d[RB<0>()];

    uint32_t next_mark = DWT::cycleCount() + _s_wait_0;

    auto write_bits = [&](uint8_t b) -> void
    {
        for (uint8_t bit = 0x80; bit != 0; bit >>= 1)
        {
            while (DWT::cycleCount() < next_mark);
            next_mark = DWT::cycleCount() + _s_wait_0;

            this->_pin.set();

            if (b & bit) while ((next_mark - DWT::cycleCount()) > _s_wait_1);
            else while ((next_mark - DWT::cycleCount()) > _s_wait_2);

            this->_pin.clear();
        }
    };

    __disable_irq();

    for (uint8_t i = 0; i < N; i++)
    {
        _d[0] = _e[0] - _d[0];
        _d[1] = _e[1] - _d[1];
        _d[2] = _e[2] - _d[2];

        write_bits(scale<0>(dither<0>(loadByte<0>())));
        write_bits(scale<1>(dither<1>(loadByte<1>())));
        write_bits(scale<2>(dither<2>(loadByte<2>())));

        _data += _advance;
    }

    __enable_irq();
}

using TLeds = Leds < PIN_NEO, 16 >;

#endif
