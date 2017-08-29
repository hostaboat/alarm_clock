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

#include "opti.h"
#include "types.h"

// Sometimes the compiler will do clever things to reduce
// code size that result in a net slowdown, if it thinks that
// a variable is not used in a certain location.
// This macro does its best to convince the compiler that
// the variable is used in this location, to help control
// code motion and de-duplication that would result in a slowdown.
#define FORCE_REFERENCE(var)  __asm__ volatile ( "" : : "r" (var) )

#define K255 255
#define K171 171
#define K170 170
#define K85  85

void hsv2rgb_rainbow(CHSV const & hsv, CRGB & rgb)
{
    static constexpr uint8_t const Y1 = 0;  // Moderate Yellow boost
    static constexpr uint8_t const Y2 = 1;  // Strong Yellow boost
    static constexpr uint8_t const G2 = 0;
    static constexpr uint8_t const Gscale = 0;

    uint8_t hue = hsv.h;
    uint8_t sat = hsv.s;
    uint8_t val = hsv.v;

    uint8_t offset8 = (hue & 0x1F) << 3; // 0..31 * 8

    uint8_t third = scale8(offset8, (256 / 3)); // max = 85

    uint8_t r, g, b;

    if (!(hue & 0x80))
    {
        // 0XX
        if (!(hue & 0x40))
        {
            // 00X
            //section 0-1
            if (!(hue & 0x20))
            {
                // 000
                //case 0: // R -> O
                r = K255 - third;
                g = third;
                b = 0;
                FORCE_REFERENCE(b);
            }
            else
            {
                // 001
                //case 1: // O -> Y
                if (Y1)
                {
                    r = K171;
                    g = K85 + third ;
                    b = 0;
                    FORCE_REFERENCE(b);
                }

                if (Y2)
                {
                    r = K170 + third;
                    //uint8_t twothirds = (third << 1);
                    uint8_t twothirds = scale8(offset8, ((256 * 2) / 3)); // max=170
                    g = K85 + twothirds;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
            }
        }
        else
        {
            //01X
            // section 2-3
            if (!(hue & 0x20))
            {
                // 010
                //case 2: // Y -> G
                if (Y1)
                {
                    //uint8_t twothirds = (third << 1);
                    uint8_t twothirds = scale8(offset8, ((256 * 2) / 3)); // max=170
                    r = K171 - twothirds;
                    g = K170 + third;
                    b = 0;
                    FORCE_REFERENCE(b);
                }

                if (Y2)
                {
                    r = K255 - offset8;
                    g = K255;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
            }
            else
            {
                // 011
                // case 3: // G -> A
                r = 0;
                FORCE_REFERENCE(r);
                g = K255 - third;
                b = third;
            }
        }
    }
    else  // (hue & 0x80) == true
    {
        // section 4-7
        // 1XX
        if (!(hue & 0x40))
        {
            // 10X
            if (!(hue & 0x20))
            {
                // 100
                //case 4: // A -> B
                r = 0;
                FORCE_REFERENCE(r);
                //uint8_t twothirds = (third << 1);
                uint8_t twothirds = scale8(offset8, ((256 * 2) / 3)); // max=170
                g = K171 - twothirds; //K170?
                b = K85  + twothirds;
                
            }
            else
            {
                // 101
                //case 5: // B -> P
                r = third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K255 - third;
                
            }
        }
        else  // (hue & 0x40) == true
        {
            if (!(hue & 0x20))
            {
                // 110
                //case 6: // P -- K
                r = K85 + third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K171 - third;
                
            }
            else
            {
                // 111
                //case 7: // K -> R
                r = K170 + third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K85 - third;
                
            }
        }
    }
    
    // This is one of the good places to scale the green down,
    // although the client can scale green down as well.
    if (G2) g = g >> 1;
    if (Gscale) g = scale8_video(g, Gscale);
    
    // Scale down colors if we're desaturated at all
    // and add the brightness_floor to r, g, and b.
    if (sat != 255)
    {
        if (sat == 0)
        {
            r = 255; b = 255; g = 255;
        }
        else
        {
            //nscale8x3_video( r, g, b, sat);
            if (r) r = scale8(r, sat);
            if (g) g = scale8(g, sat);
            if (b) b = scale8(b, sat);
            
            uint8_t desat = 255 - sat;
            desat = scale8(desat, desat);
            
            uint8_t brightness_floor = desat;
            r += brightness_floor;
            g += brightness_floor;
            b += brightness_floor;
        }
    }
    
    // Now scale everything down if we're at value < 255.
    if (val != 255)
    {
        val = scale8_video(val, val);
        if (val == 0)
        {
            r = g = b = 0;
        }
        else
        {
            // nscale8x3_video( r, g, b, val);
            if (r) r = scale8(r, val);
            if (g) g = scale8(g, val);
            if (b) b = scale8(b, val);
        }
    }
    
    rgb.r = r;
    rgb.g = g;
    rgb.b = b;
}

CRGB const Palette::_s_party_colors[16] =
{
    0x5500AB, 0x84007C, 0xB5004B, 0xE5001B,
    0xE81700, 0xB84700, 0xAB7700, 0xABAB00,
    0xAB5500, 0xDD2200, 0xF2000E, 0xC2003E,
    0x8F0071, 0x5F00A1, 0x2F00D0, 0x0007F9
};

CRGB const Palette::_s_purple_swirl[16] =
{
    CRGB::FUCHSIA,
    CRGB::MEDIUM_PURPLE,
    CRGB::MEDIUM_PURPLE,
    CRGB::LAVENDER,

    CRGB::LAVENDER,
    CRGB::PURPLE,
    CRGB::DARK_MAGENTA,
    CRGB::DARK_MAGENTA,

    CRGB::DARK_MAGENTA,
    CRGB::DARK_ORCHID,
    CRGB::WHITE,
    CRGB::DARK_VIOLET,

    CRGB::DARK_VIOLET,
    CRGB::WHITE,
    CRGB::FUCHSIA,
    CRGB::LAVENDER_BLUSH,
};

CRGB const Palette::_s_lava_colors[16] =
{
    CRGB::BLACK,
    CRGB::MAROON,
    CRGB::BLACK,
    CRGB::MAROON,

    CRGB::DARK_RED,
    CRGB::MAROON,
    CRGB::DARK_RED,
    CRGB::MAROON,  // Added to prevent possible one byte read past end of buffer

    CRGB::DARK_RED,
    CRGB::DARK_RED,
    CRGB::RED,
    CRGB::ORANGE,

    CRGB::WHITE,
    CRGB::ORANGE,
    CRGB::RED,
    CRGB::DARK_RED
};

CRGB const Palette::_s_ocean_colors[16] =
{
    CRGB::MIDNIGHT_BLUE,
    CRGB::DARK_BLUE,
    CRGB::MIDNIGHT_BLUE,
    CRGB::NAVY,

    CRGB::DARK_BLUE,
    CRGB::MEDIUM_BLUE,
    CRGB::SEA_GREEN,
    CRGB::TEAL,

    CRGB::CADET_BLUE,
    CRGB::BLUE,
    CRGB::DARK_CYAN,
    CRGB::CORNFLOWER_BLUE,

    CRGB::AQUAMARINE,
    CRGB::SEA_GREEN,
    CRGB::AQUA,
    CRGB::LIGHT_SKY_BLUE
};

CRGB const Palette::_s_rainbow_colors[16] =
{
    0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00,
    0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
    0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5,
    0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B
};

CRGB const Palette::_s_forest_colors[16] =
{
    CRGB::DARK_GREEN,
    CRGB::DARK_GREEN,
    CRGB::DARK_OLIVE_GREEN,
    CRGB::DARK_GREEN,

    CRGB::GREEN,
    CRGB::FOREST_GREEN,
    CRGB::OLIVE_DRAB,
    CRGB::GREEN,

    CRGB::SEA_GREEN,
    CRGB::MEDIUM_AQUAMARINE,
    CRGB::LIME_GREEN,
    CRGB::YELLOW_GREEN,

    CRGB::LIGHT_GREEN,
    CRGB::LAWN_GREEN,
    CRGB::MEDIUM_AQUAMARINE,
    CRGB::FOREST_GREEN
};

CRGB const Palette::_s_rwb_swirl[16] =
{
    CRGB::RED,
    CRGB::WHITE,
    CRGB::BLUE,
    CRGB::BLUE,

    CRGB::RED,
    CRGB::WHITE,
    CRGB::WHITE,
    CRGB::BLUE,

    CRGB::BLUE,
    CRGB::RED,
    CRGB::RED,
    CRGB::BLUE,

    CRGB::WHITE,
    CRGB::BLUE,
    CRGB::BLUE,
    CRGB::RED,
};

CRGB const * const Palette::_s_palettes[PAL_CNT] =
{
    _s_party_colors,
    _s_purple_swirl,
    _s_lava_colors,
    _s_ocean_colors,
    _s_rainbow_colors,
    _s_forest_colors,
    _s_rwb_swirl,
};

CRGB Palette::colorFromPalette(pal_e pal, uint8_t index, uint8_t brightness)
{
    if ((brightness == 0) || (pal == PAL_CNT))
        return CRGB(0,0,0);

    CRGB const * const palette = _s_palettes[pal];
    uint8_t hi4 = index >> 4;
    uint8_t lo4 = index & 0x0F;
    bool blend = lo4 != 0;
    uint8_t r1 = palette[hi4].r;
    uint8_t g1 = palette[hi4].g;
    uint8_t b1 = palette[hi4].b;

    if (blend)
    {
        CRGB const & entry = palette[(hi4 + 1) % 16];

        uint8_t f2 = lo4 << 4;
        uint8_t f1 = 255 - f2;

        //    rgb1.nscale8(f1);
        uint8_t r2 = entry.r;
        r1 = scale8_video(r1, f1);
        r2 = scale8_video(r2, f2);
        r1 += r2;

        uint8_t g2 = entry.g;
        g1 = scale8_video(g1, f1);
        g2 = scale8_video(g2, f2);
        g1 += g2;

        uint8_t b2 = entry.b;
        b1 = scale8_video(b1, f1);
        b2 = scale8_video(b2, f2);
        b1 += b2;
    }

    if (brightness != 255)
    {
        brightness++; // adjust for rounding

        // Now, since brightness is nonzero, we don't need the full scale8_video logic;
        // we can just to scale8 and then add one (unless scale8 fixed) to all nonzero inputs.
        if (r1 != 0)
            r1 = scale8(r1, brightness);

        if (g1 != 0)
            g1 = scale8(g1, brightness);

        if (b1 != 0)
            b1 = scale8(b1, brightness);
    }

    return CRGB(r1, g1, b1);
}
