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

struct CHSV;
struct CRGB;

void hsv2rgb_rainbow(CHSV const & hsv, CRGB & rgb);

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

// This does an unsigned saturating add (actually 4 in a 32-bit register,
// meaning if the value would overflow it is set to the max of the data
// type, in this case, for uint8_t, 255.
inline uint8_t qadd8(uint8_t i, uint8_t j)
{
    __asm__ volatile ("uqadd8 %0, %0, %1" : "+r" (i) : "r" (j));
    return i;
}

struct CHSV
{
    union
    {
        struct { uint8_t h; uint8_t s; uint8_t v; };
        uint8_t raw[3];
    };

    CHSV(void) {}
    CHSV(uint8_t h, uint8_t s, uint8_t v) : h(h), s(s), v(v) {}
    CHSV(CHSV const & copy) { h = copy.h; s = copy.s; v = copy.v; }
    CHSV & operator=(CHSV const & rval) { h = rval.h; s = rval.s; v = rval.v; return *this; }
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
        RED   = 0xFF0000,
        GREEN = 0x00FF00,
        BLUE  = 0x0000FF,
        WHITE = 0xFFFFFF,
        BLACK = 0x000000,
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
    bool isBlack(void) { return (r == 0) && (g == 0) && (b == 0); }
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

        void countFPS(uint8_t num_frames = 25);
        void ditherInit(void);
        void binaryDitherInit(void);

        //enum order_e { RGB = 0012, RBG = 0021, GRB = 0102, GBR = 0120, BRG = 0201, BGR = 0210 };

        // For GRB the second, then the first then the last of the CRGB - 1 0 2
        static constexpr uint8_t const _s_rgb_order = 0x42; // GRB 0100 0010
        template < uint8_t X > uint8_t RB(void) { return (_s_rgb_order >> (3 * (2 - X))) & 0x03; }

        template < uint8_t SLOT > uint8_t loadByte(void) { return _data[RB<SLOT>()]; }
        template < uint8_t SLOT > uint8_t dither(uint8_t b) { return b ? qadd8(b, _d[RB<SLOT>()]) : 0; }
        template < uint8_t SLOT > uint8_t scale(uint8_t b) { return scale8(b, _scale.raw[RB<SLOT>()]); }

        void _show(CRGB const * rgb, uint8_t brightness) { _advance = 3; showLeds(rgb, brightness); }
        void _showColor(CRGB const & rgb, uint8_t brightness) { _advance = 0; showLeds(&rgb, brightness); }
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
    // Set 'virtual bits' of dithering to the highest level
    // that is not likely to cause excessive flickering at
    // low brightness levels + low update rates.
    // These pre-set values are a little ambitious, since
    // a 400Hz update rate for WS2811-family LEDs is only
    // possible with 85 pixels or fewer.
    // Once we have a 'number of milliseconds since last update'
    // value available here, we can quickly calculate the correct
    // number of 'virtual bits' on the fly with a couple of 'if'
    // statements -- no division required.  At this point,
    // the division is done at compile time, so there's no runtime
    // cost, but the values are still hard-coded.
#define MAX_LIKELY_UPDATE_RATE_HZ     400
#define MIN_ACCEPTABLE_DITHER_RATE_HZ  50
#define UPDATES_PER_FULL_DITHER_CYCLE (MAX_LIKELY_UPDATE_RATE_HZ / MIN_ACCEPTABLE_DITHER_RATE_HZ)
#define RECOMMENDED_VIRTUAL_BITS ((UPDATES_PER_FULL_DITHER_CYCLE>1) + \
        (UPDATES_PER_FULL_DITHER_CYCLE>2) + \
        (UPDATES_PER_FULL_DITHER_CYCLE>4) + \
        (UPDATES_PER_FULL_DITHER_CYCLE>8) + \
        (UPDATES_PER_FULL_DITHER_CYCLE>16) + \
        (UPDATES_PER_FULL_DITHER_CYCLE>32) + \
        (UPDATES_PER_FULL_DITHER_CYCLE>64) + \
        (UPDATES_PER_FULL_DITHER_CYCLE>128) )
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
void Leds < PIN, N >::showLeds(CRGB const * rgb, uint8_t brightness)
{
    static uint32_t last_show = 0;

    _scale[0] = _scale[1] = _scale[2] = brightness;
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
//using TLeds = Leds < PIN_NEO, 8 >;

#endif
