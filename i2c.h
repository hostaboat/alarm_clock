#ifndef _I2C_H_
#define _I2C_H_

#include "pin.h"
#include "module.h"
#include "types.h"

#define I2C_C1_DMAEN  (1 << 0)
#define I2C_C1_WUEN   (1 << 1)
#define I2C_C1_RSTA   (1 << 2)
#define I2C_C1_TXAK   (1 << 3)
#define I2C_C1_TX     (1 << 4)
#define I2C_C1_MST    (1 << 5)
#define I2C_C1_IICIE  (1 << 6)
#define I2C_C1_IICEN  (1 << 7)

#define I2C_S_RXAK   (1 << 0)
#define I2C_S_IICIF  (1 << 1)
#define I2C_S_SRW    (1 << 2)
#define I2C_S_RAM    (1 << 3)
#define I2C_S_ARBL   (1 << 4)
#define I2C_S_BUSY   (1 << 5)
#define I2C_S_IAAS   (1 << 6)
#define I2C_S_TCF    (1 << 7)

#define I2C_C2_AD(usa) ((usa) & 0x07)
#define I2C_C2_RMEM    (1 << 3)
#define I2C_C2_SBRC    (1 << 4)
#define I2C_C2_HDRS    (1 << 5)
#define I2C_C2_ADEXT   (1 << 6)
#define I2C_C2_GCAEN   (1 << 7)

enum i2c_e { I2C_0, I2C_1 };

// XXX This only implements TX, one master and one slave with the MCU as master.
template < pin_t SDA, pin_t SCL, i2c_e X >
class I2C : public Module
{
    static_assert (X <= I2C_1, "Invalid I2Cx");
    static_assert (
            ((X == I2C_0) && ((SDA == 17) || (SDA == 18)) && ((SCL == 16) || (SCL == 19))) ||
            ((X == I2C_1) && (SDA == 30) && (SCL == 29)), "Invalid I2C pins");

    public:
        static I2C & acquire(void) { static I2C i2c; return i2c; }

        bool valid(void) { return _sda.taken() && _scl.taken(); }

        void start(void);
        void stop(void);
        bool running(void) { return *_c1 & I2C_C1_IICEN; }

        bool begin(uint8_t addr, uint32_t freq);
        void end(void);
        // The line is considered busy from the time start() is called to the
        // time end() is called and the data is actually sent so can't just
        // check for the register status flag.
        bool busy(void) { return _busy; }

        void tx8(uint8_t tx);
        void tx16(uint16_t tx);
        void tx32(uint32_t tx);
        void tx(uint8_t * tx, uint16_t tx_len);

        I2C(I2C const &) = delete;
        I2C & operator=(I2C const &) = delete;

    private:
        I2C(void);

        PinSDA < SDA > & _sda = PinSDA < SDA >::acquire();
        PinSCL < SCL > & _scl = PinSCL < SCL >::acquire();

        uint8_t remaining(void) { return sizeof(_tx) - _tx_len; }
        void setClock(uint32_t freq);

        uint32_t _freq = 400000;  // Default to 400kHz
        uint8_t _tx[128];
        uint8_t _tx_len = 0;
        bool _busy = false;

        reg8 _base_reg = (reg8)0x40066000;
        reg8 _a1   = _base_reg + (0x1000 * X) + 0x0;
        reg8 _f    = _base_reg + (0x1000 * X) + 0x1;
        reg8 _c1   = _base_reg + (0x1000 * X) + 0x2;
        reg8 _s    = _base_reg + (0x1000 * X) + 0x3;
        reg8 _d    = _base_reg + (0x1000 * X) + 0x4;
        reg8 _c2   = _base_reg + (0x1000 * X) + 0x5;
        reg8 _flt  = _base_reg + (0x1000 * X) + 0x6;
        reg8 _ra   = _base_reg + (0x1000 * X) + 0x7;
        reg8 _smb  = _base_reg + (0x1000 * X) + 0x8;
        reg8 _a2   = _base_reg + (0x1000 * X) + 0x9;
        reg8 _slth = _base_reg + (0x1000 * X) + 0xA;
        reg8 _sltl = _base_reg + (0x1000 * X) + 0xB;
};

template < pin_t SDA, pin_t SCL, i2c_e X >
I2C < SDA, SCL, X >::I2C(void)
{
    if (X == I2C_0) Module::enable(MOD_I2C0);
    else Module::enable(MOD_I2C1);

    setClock(_freq);
    start();
}

template < pin_t SDA, pin_t SCL, i2c_e X >
void I2C < SDA, SCL, X >::start(void)
{
    if (running()) return;
    _tx_len = 0;
    *_c2 = I2C_C2_HDRS;
    *_c1 = I2C_C1_IICEN;
}

template < pin_t SDA, pin_t SCL, i2c_e X >
void I2C < SDA, SCL, X >::stop(void)
{
    if (!running() || _busy) return;
    _tx_len = 0;
    *_c1 = 0;
    *_c2 = 0;
}

template < pin_t SDA, pin_t SCL, i2c_e X >
bool I2C < SDA, SCL, X >::begin(uint8_t addr, uint32_t freq)
{
    if (_busy) return false;
    if (!running()) start();

    _busy = true;

    if (freq != _freq)
    {
        stop();
        _freq = freq;
        setClock(_freq);
        start();
    }

    _tx[0] = addr << 1;  // First bit is 0 for Write since only that is implemented
    _tx_len = 1;

    return true;
}

// This is when the data actually gets sent
template < pin_t SDA, pin_t SCL, i2c_e X >
void I2C < SDA, SCL, X >::end(void)
{
    if (!_busy || !running())
        return;

    ////////////////////////////////////////////////////////////////////////////

    auto ready = [&](void) -> bool
    {
        // Wait until START condition, i.e. wait for it to become BUSY
        return *_s & I2C_S_BUSY;
    };

    auto init = [&](void) -> void
    {
        *_s = I2C_S_ARBL | I2C_S_IICIF;

        // If already master use repeated START
        if (*_c1 & I2C_C1_MST)
            *_c1 |= I2C_C1_RSTA | I2C_C1_TX;
        else
            *_c1 |= I2C_C1_MST | I2C_C1_TX;

        while (!ready());
    };

    // Number of times to try a repeated START if a byte transmission fails.
    uint8_t retries = 16;

    enum status_e { SOK, STIME, SRETRY };

    auto send = [&](uint8_t byte) -> status_e
    {
        *_d = byte;

        // This flag actually gets set regardless of the IICIE flag in C1
        // Whether or not an actual interrupt occurs and interrupt routine
        // called depends on the IICIE flag in C1.
        while (!(*_s & I2C_S_IICIF));
        *_s |= I2C_S_IICIF;

        // Transfer Complete flag set so successful
        if (*_s & I2C_S_TCF)
            return SOK;

        if (--retries == 0)
            return STIME;

        return SRETRY;
    };

    ////////////////////////////////////////////////////////////////////////////

    init();

    for (uint8_t i = 0; i < _tx_len; i++)
    {
        status_e s = send(_tx[i]);

        if (s == SOK)
            continue;

        if (s == STIME)
        {
            *_c1 &= ~I2C_C1_MST;
            break;
        }

        // A repeated START has to send everything again, including
        // the address in the first byte.
        *_c1 |= I2C_C1_RSTA;
        i = 0;

        while (!ready());
    }

    _tx_len = 0;
    _busy = false;
}

template < pin_t SDA, pin_t SCL, i2c_e X >
void I2C < SDA, SCL, X >::tx8(uint8_t tx)
{
    if (remaining() < sizeof(tx))
        return;

    _tx[_tx_len++] = tx;
}

template < pin_t SDA, pin_t SCL, i2c_e X >
void I2C < SDA, SCL, X >::tx16(uint16_t tx)
{
    if (remaining() < sizeof(tx))
        return;

    // MSB first
    _tx[_tx_len++] = (uint8_t)(tx >> 8);
    _tx[_tx_len++] = (uint8_t)(tx >> 0);
}

template < pin_t SDA, pin_t SCL, i2c_e X >
void I2C < SDA, SCL, X >::tx32(uint32_t tx)
{
    if (remaining() < sizeof(tx))
        return;

    // MSB first
    _tx[_tx_len++] = (uint8_t)(tx >> 24);
    _tx[_tx_len++] = (uint8_t)(tx >> 16);
    _tx[_tx_len++] = (uint8_t)(tx >>  8);
    _tx[_tx_len++] = (uint8_t)(tx >>  0);
}

template < pin_t SDA, pin_t SCL, i2c_e X >
void I2C < SDA, SCL, X >::tx(uint8_t * tx, uint16_t tx_len)
{
    if (remaining() < tx_len)
        return;

    for (int i = 0; i < tx_len; i++)
        _tx[_tx_len++] = tx[i];
}

template < pin_t SDA, pin_t SCL, i2c_e X >
void I2C < SDA, SCL, X >::setClock(uint32_t frequency)
{
#if F_BUS == 48000000
    if      (frequency <  400000) *_f = 0x27;  // 100 kHz 
    else if (frequency < 1000000) *_f = 0x1A;  // 400 kHz
    else                          *_f = 0x0D;  // 1 MHz
    *_flt = 4;
#elif F_BUS == 36000000
    if      (frequency <  400000) *_f = 0x28;  // 113 kHz
    else if (frequency < 1000000) *_f = 0x19;  // 375 kHz
    else                          *_f = 0x0A;  // 1 MHz
    *_flt = 3;
#else
# error "F_BUS not valid"
#endif
}

template < pin_t SDA, pin_t SCL > using I2C0 = I2C < SDA, SCL, I2C_0 >;
template < pin_t SDA, pin_t SCL > using I2C1 = I2C < SDA, SCL, I2C_1 >;

#endif
