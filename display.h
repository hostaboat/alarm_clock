#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "dev.h"
#include "i2c.h"
#include "types.h"

////////////////////////////////////////////////////////////////////////////////
// Generic Display /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
enum br_e : uint8_t
{
    BR_OFF,
    BR_LOW,
    BR_MID,
    BR_MAX,
};

class Display
{
    public:
        virtual void wake(void) = 0;
        virtual void sleep(void) = 0;
        virtual bool isAwake(void) = 0;

        virtual void on(void) = 0;
        virtual void off(void) = 0;
        virtual bool isOn(void) = 0;

        virtual br_e brightness(void) = 0;
        virtual void brightness(br_e level) = 0;
        virtual void up(void) = 0;
        virtual void down(void) = 0;

        virtual void show(void) = 0;
};

////////////////////////////////////////////////////////////////////////////////
// 7 Segment Display ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Display Flags
#define DF_NONE      0x0000
#define DF_COLON     0x0001  // Show colon
#define DF_BL0       0x0002  // Blank first digit
#define DF_BL1       0x0004  // Blank second 
#define DF_BL2       0x0008  // Blank third
#define DF_BL3       0x0010  // Blank fourth
#define DF_BL_BC     (DF_BL0 | DF_BL1)  // Blank before colon
#define DF_BL_AC     (DF_BL2 | DF_BL3)  // Blank after colon
#define DF_BL_MASK   (DF_BL0 | DF_BL1 | DF_BL2 | DF_BL3)
#define DF_BL        DF_BL_MASK  // Blank all
#define DF_LZ_BC     0x0020  // Leading zeros before colon
#define DF_LZ_AC     0x0040  // Leading zeros after colon
#define DF_LZ        (DF_LZ_BC | DF_LZ_AC)  // Leading zeros where applicable
#define DF_NO_LZ     0x0080  // No leading zeros where applicable
#define DF_DP0       0x0100  // Decimal point on first digit
#define DF_DP1       0x0200  // Decimal point on second
#define DF_DP2       0x0400  // Decimal point on third
#define DF_DP3       0x0800  // Decimal point on fourth
#define DF_DP        DF_DP3  // Decimal point at end
#define DF_DP_MASK   (DF_DP0 | DF_DP1 | DF_DP2 | DF_DP3)
#define DF_NEG       DF_DP3  // Display negative - just puts a decimal at end
#define DF_PM        DF_DP3  // Display time as PM
#define DF_OCT       0x1000  // Display number as octal
#define DF_HEX       0x2000  // Display number as hexidecimal
#define DF_12H       0x4000  // Display time in 12 hour format
#define DF_24H       0x8000  // Display time in 24 hour format
#define DF_MASK      0xFFFF

//#define USE_STR_FUNC
//#define USE_STR_STATE_FUNC

using df_t = uint16_t;

class Seg7Display : public Display
{
    public:
        Seg7Display(void) {}
        Seg7Display(uint8_t colon, uint8_t decimal) : _colon(colon), _decimal(decimal) {}

        virtual void wake(void) = 0;
        virtual void sleep(void) = 0;
        virtual bool isAwake(void) = 0;
        virtual void on(void) = 0;
        virtual void off(void) = 0;
        virtual bool isOn(void) = 0;
        virtual br_e brightness(void) = 0;
        virtual void brightness(br_e level) = 0;
        virtual void up(void) = 0;
        virtual void down(void) = 0;
        virtual void show(void) = 0;

        void setBlank(void);
        void blank(void);

        void setNum(uint8_t dX, uint8_t X, df_t flags = DF_NONE);
        void showNum(uint8_t dX, uint8_t X, df_t flags = DF_NONE);
        void setChar(uint8_t cX, uint8_t X, df_t flags = DF_NONE);
        void showChar(uint8_t cX, uint8_t X, df_t flags = DF_NONE);

        void setDigits(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, df_t flags = DF_NONE);
        void showDigits(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, df_t flags = DF_NONE);

        void setInteger(int32_t integer, df_t flags = DF_NONE);
        void showInteger(int32_t integer, df_t flags = DF_NONE);

        void setTime(uint8_t hour_min, uint8_t min_sec, df_t flags = DF_NONE);
        void showTime(uint8_t hour_min, uint8_t min_sec, df_t flags = DF_NONE);
        void setTimer(uint8_t hour_min, uint8_t min_sec, df_t flags = DF_NONE);
        void showTimer(uint8_t hour_min, uint8_t min_sec, df_t flags = DF_NONE);
        void setClock(uint8_t hour, uint8_t minute, df_t flags = DF_NONE);
        void showClock(uint8_t hour, uint8_t minute, df_t flags = DF_NONE);
        void setClock12(uint8_t hour, uint8_t minute, df_t flags = DF_NONE);
        void showClock12(uint8_t hour, uint8_t minute, df_t flags = DF_NONE);
        void setClock24(uint8_t hour, uint8_t minute, df_t flags = DF_NONE);
        void showClock24(uint8_t hour, uint8_t minute, df_t flags = DF_NONE);

        void setDate(uint8_t month, uint8_t day, df_t flags = DF_NONE);
        void showDate(uint8_t month, uint8_t day, df_t flags = DF_NONE);

        void setString(char const * str, df_t flags = DF_NONE);
        void showString(char const * str, df_t flags = DF_NONE);

        // Convenience functions
        void showDashes(df_t flags = DF_NONE);
        void showError(df_t flags = DF_NONE);
        void showDone(df_t flags = DF_NONE);
        void showLowBattery(df_t flags = DF_NONE);
        void showOff(df_t flags = DF_NONE);
        void showBeep(df_t flags = DF_NONE);
        void showPlay(df_t flags = DF_NONE);
        void show12Hour(df_t flags = DF_NONE);
        void show24Hour(df_t flags = DF_NONE);
        void showTouch(df_t flags = DF_NONE);

    protected:
        uint16_t validateValue(uint16_t n, uint16_t min, uint16_t max);
        df_t digitFlags(uint8_t X, df_t flags);
        void setDPFlag(uint8_t X, df_t & flags);
        uint8_t mPos(uint8_t X);
        void setDigit(uint8_t mX, uint8_t X, df_t flags = DF_NONE);
        void setColon(df_t flags);
        void numberTypeValues(uint16_t & max, uint16_t & num, uint16_t & den, df_t flags);

        // Digit, Digit, Colon, Digit, Digit
        uint8_t _positions[5] = {};

        uint8_t _colon = 0;
        uint8_t _decimal = 0;

        // Maps a number to bit mask to display number
        static constexpr uint8_t const _s_number_map[] =
        {
            0x3F,  // 0
            0x06,  // 1
            0x5B,  // 2
            0x4F,  // 3
            0x66,  // 4
            0x6D,  // 5
            0x7D,  // 6
            0x07,  // 7
            0x7F,  // 8
            0x6F,  // 9
            0x77,  // A
            0x7C,  // B
            0x39,  // C
            0x5E,  // D
            0x79,  // E
            0x71   // F
        };

        static constexpr uint8_t const _s_char_map[] =
        {
          // nul      soh      stx      etx      eot      enq      ack      bel
               0,       0,       0,       0,       0,       0,       0,       0,
          //  bs       ht       nl       vt       np       cr       so       si
               0,       0,       0,       0,       0,       0,       0,       0,
          // dle      dc1      dc2      dc3      dc4      nak      syn      etb
               0,       0,       0,       0,       0,       0,       0,       0,
          // can       em      sub      esc       fs       gs       rs       us
               0,       0,       0,       0,       0,       0,       0,       0,
          //  sp        !        "        #        $        %        &        '
            0x00,       0,       0,       0,       0,       0,       0,       0,
          //   (        )        *        +        ,        -        .        /
               0,       0,       0,       0,       0,    0x40,       0,       0,
          //   0        1        2        3        4        5        6        7 
            0x3F,    0x06,    0x5B,    0x4F,    0x66,    0x6D,    0x7D,    0x07,
          //   8        9        :        ;        <        =        >        ?
            0x7F,    0x6F,       0,       0,       0,       0,       0,       0,
          //   @        A        B        C        D        E        F        G
               0,    0x77,    0x7C,    0x39,    0x5E,    0x79,    0x71,       0,
          //   H        I        J        K        L        M        N        O
            0x76,    0x06,    0x1E,       0,    0x38,       0,    0x54,    0x3F,
          //   P        Q        R        S        T        U        V        W
            0x73,       0,    0x50,    0x6D,    0x78,    0x3E,       0,       0,
          //   X        Y        Z        [        \        ]        ^        _
               0,    0x6E,       0,    0x39,       0,    0x0F,       0,    0x08,
          //   `        a        b        c        d        e        f        g
               0,    0x77,    0x7C,    0x58,    0x5E,    0x79,    0x71,       0,
          //   h        i        j        k        l        m        n        o
            0x74,    0x04,    0x1E,       0,    0x06,       0,    0x54,    0x5C,
          //   p        q        r        s        t        u        v        w
            0x73,       0,    0x50,    0x6D,    0x78,    0x1C,       0,       0,
          //   x        y        z        {        |        }        ~       del
               0,    0x6E,       0,       0,       0,       0,       0,       0
        };
};

////////////////////////////////////////////////////////////////////////////////
// Adafruit 0.56" 4-Digit 7-Segment Display w/I2C Backpack - HT16K33 Chip //////
////////////////////////////////////////////////////////////////////////////////
#define HT16K33_DEV_ADDR    0x70

#define HT16K33_SYSTEM_SETUP_REG  0x20
#define HT16K33_OSCILLATOR_OFF    0x00
#define HT16K33_OSCILLATOR_ON     0x01
#define HT16K33_STANDBY           HT16K33_SYSTEM_SETUP_REG | HT16K33_OSCILLATOR_OFF
#define HT16K33_WAKEUP            HT16K33_SYSTEM_SETUP_REG | HT16K33_OSCILLATOR_ON

#define HT16K33_DISPLAY_SETUP_REG  0x80 
#define HT16K33_DISPLAY_OFF        0x00 
#define HT16K33_DISPLAY_ON         0x01 
#define HT16K33_BLINK_OFF         (0 << 1)
#define HT16K33_BLINK_2HZ         (1 << 1)
#define HT16K33_BLINK_1HZ         (2 << 1)
#define HT16K33_BLINK_HALFHZ      (3 << 1)
#define HT16K33_ON                HT16K33_DISPLAY_SETUP_REG | HT16K33_DISPLAY_ON | HT16K33_BLINK_OFF
#define HT16K33_OFF               HT16K33_DISPLAY_SETUP_REG | HT16K33_DISPLAY_OFF | HT16K33_BLINK_OFF

#define HT16K33_BRIGHTNESS_REG  0xE0 
#define HT16K33_MIN_BRIGHTNESS  0x00
#define HT16K33_MID_BRIGHTNESS  0x07
#define HT16K33_MAX_BRIGHTNESS  0x0F

#define HT16K33_COLON    0x02
#define HT16K33_DECIMAL  0x80

template < uint8_t ADDR, template < pin_t, pin_t > class I2C, pin_t SDA, pin_t SCL >
class DevHT16K33 : public DevI2C < ADDR, I2C, SDA, SCL > , public Seg7Display
{
    using Ti2c = DevI2C < ADDR, I2C, SDA, SCL >;

    public:
        static DevHT16K33 & acquire(void) { static DevHT16K33 ht16k33; return ht16k33; }
        virtual bool valid(void) { return Ti2c::valid(); }

        virtual bool busy(void) { return this->_i2c.busy(); }

        virtual void wake(void);
        virtual void sleep(void);
        virtual bool isAwake(void) { return _awake && this->_i2c.running(); }
        virtual void on(void);
        virtual void off(void);
        virtual bool isOn(void) { return _on && isAwake(); }
        virtual br_e brightness(void) { return (br_e)_brightness; }
        virtual void brightness(br_e level);
        virtual void up(void);
        virtual void down(void);
        virtual void show(void);

        DevHT16K33(DevHT16K33 const &) = delete;
        DevHT16K33 & operator=(DevHT16K33 const &) = delete;

    private:
        DevHT16K33(void);

        void send(uint8_t tx)
        {
            this->_i2c.begin(ADDR, this->_frequency);
            this->_i2c.tx8(tx);
            this->_i2c.end();
        }

        bool _awake = false;
        bool _on = false;
        uint8_t _brightness = BR_MID;
};

template < uint8_t ADDR, template < pin_t, pin_t > class I2C, pin_t SDA, pin_t SCL >
DevHT16K33 < ADDR, I2C, SDA, SCL >::DevHT16K33(void)
    : Seg7Display(HT16K33_COLON, HT16K33_DECIMAL)
{
    this->_frequency = 400000;

    wake();
    brightness(BR_MID);
    blank();
    on();
}

template < uint8_t ADDR, template < pin_t, pin_t > class I2C, pin_t SDA, pin_t SCL >
void DevHT16K33 < ADDR, I2C, SDA, SCL >::wake(void)
{
    if (isAwake()) return;
    this->_i2c.start(); // Have to turn the I2C module back on
    send(HT16K33_WAKEUP);
    _awake = true;
}

template < uint8_t ADDR, template < pin_t, pin_t > class I2C, pin_t SDA, pin_t SCL >
void DevHT16K33 < ADDR, I2C, SDA, SCL >::sleep(void)
{
    if (!isAwake()) return;
    send(HT16K33_STANDBY);
    this->_i2c.stop(); // This saves some power by turning the I2C module off
    _awake = false;
}

template < uint8_t ADDR, template < pin_t, pin_t > class I2C, pin_t SDA, pin_t SCL >
void DevHT16K33 < ADDR, I2C, SDA, SCL >::on(void)
{
    if (isOn()) return;
    wake();
    send(HT16K33_ON);
    _on = true;
}

template < uint8_t ADDR, template < pin_t, pin_t > class I2C, pin_t SDA, pin_t SCL >
void DevHT16K33 < ADDR, I2C, SDA, SCL >::off(void)
{
    if (!isOn()) return;
    wake();
    send(HT16K33_OFF);
    _on = false;
}

template < uint8_t ADDR, template < pin_t, pin_t > class I2C, pin_t SDA, pin_t SCL >
void DevHT16K33 < ADDR, I2C, SDA, SCL >::up(void)
{
    wake();

    if (_brightness == HT16K33_MAX_BRIGHTNESS)
        return;

    _brightness++;
    send(HT16K33_BRIGHTNESS_REG | _brightness);
    on();
}

template < uint8_t ADDR, template < pin_t, pin_t > class I2C, pin_t SDA, pin_t SCL >
void DevHT16K33 < ADDR, I2C, SDA, SCL >::down(void)
{
    wake();

    if (_brightness == HT16K33_MIN_BRIGHTNESS)
    {
        off();
        return;
    }

    _brightness--;
    send(HT16K33_BRIGHTNESS_REG | _brightness);
    on();
}

template < uint8_t ADDR, template < pin_t, pin_t > class I2C, pin_t SDA, pin_t SCL >
void DevHT16K33 < ADDR, I2C, SDA, SCL >::brightness(br_e level)
{
    wake();

    if (level == BR_OFF)
    {
        off();
        _brightness = 0;
        return;
    }

    if (level == BR_LOW)
        _brightness = HT16K33_MIN_BRIGHTNESS;
    else if (level == BR_MID)
        _brightness = HT16K33_MID_BRIGHTNESS;
    else if (level == BR_MAX)
        _brightness = HT16K33_MAX_BRIGHTNESS;

    send(HT16K33_BRIGHTNESS_REG | _brightness);
    on();
}

template < uint8_t ADDR, template < pin_t, pin_t > class I2C, pin_t SDA, pin_t SCL >
void DevHT16K33 < ADDR, I2C, SDA, SCL >::show(void)
{
    wake();

    this->_i2c.begin(ADDR, this->_frequency);
    this->_i2c.tx8(0x00);  // Command code

    uint8_t i = 0;
    for (; i < 5; i++)
    {
        this->_i2c.tx8(_positions[i]);
        this->_i2c.tx8(0x00);
    }

    // Not sure if this is necessary
    for (; i < 8; i++)
    {
        this->_i2c.tx8(0x00);
        this->_i2c.tx8(0x00);
    }

    this->_i2c.end();

    on();
}

////////////////////////////////////////////////////////////////////////////////
// Templates ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

using TDisplay = DevHT16K33 < HT16K33_DEV_ADDR, I2C0, PIN_SDA, PIN_SCL >;

#endif