#ifndef _DEV_H_
#define _DEV_H_

#include "pin.h"
#include "types.h"

class Dev
{
    public:
        virtual bool valid(void) const = 0;
    protected:
        Dev(void) {}
};

template < template < pin_t > class PIN, pin_t N >
class DevPin : public Dev, public Isr
{
    using Tpin = PIN < N >;

    public:
        virtual bool valid(void) const { return _pin.taken(); }

        virtual bool attach(irqc_e irqc) { return _pin.attach(this, irqc); }
        virtual bool update(irqc_e irqc) { return _pin.update(irqc); }
        virtual bool detach(void) { return _pin.detach(); }

    protected:
        DevPin(irqc_e irqc = IRQC_DISABLED)
            : Isr(N)
        {
            if ((irqc != IRQC_DISABLED) && !attach(irqc))
                _pin.release();
        }

        Tpin & _pin = Tpin::acquire();
};

template < pin_t CS, template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
class DevSPI : public DevPin < PinOut, CS >
{
    using Tdp = DevPin < PinOut, CS >;
    using Tspi = SPI < MOSI, MISO, SCK >;

    public:
        virtual bool busy(void) = 0;
        virtual bool valid(void) const { return Tdp::valid() && _spi.valid(); }

    protected:
        DevSPI(void) { if (!valid()) return; this->_pin.set(); }

        Tspi & _spi = Tspi::acquire();
        uint32_t _cta;
};

using dd_desc_t = void *;
enum dd_dir_e { DD_READ, DD_WRITE };
enum dd_err_e : int
{
    DD_ERR_BUSY      = -1,  // Device or resource busy
    DD_ERR_INVAL     = -2,  // Invalid argument
    DD_ERR_BADF      = -3,  // Bad file descriptor
    DD_ERR_BADFD     = -4,  // File descriptor in bad state
    DD_ERR_IO        = -5,  // I/O error
    DD_ERR_TIMED_OUT = -6,  // Connection timed out
};

template < uint16_t READ_BLK_LEN,
         pin_t CS, template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
class DevDisk : public DevSPI < CS, SPI, MOSI, MISO, SCK >
{
    public:
        virtual bool busy(void) = 0;

        virtual int read(uint32_t address, uint8_t (&buf)[READ_BLK_LEN]) = 0;
        virtual int write(uint32_t address, uint8_t (&buf)[READ_BLK_LEN]) = 0;

        virtual dd_desc_t open(uint32_t addr, uint16_t num_blocks, dd_dir_e dir) = 0;
        virtual int read(dd_desc_t dd, uint8_t * buf, uint16_t blen) = 0;
        virtual int write(dd_desc_t dd, uint8_t * data, uint16_t dlen) = 0;
        virtual int close(dd_desc_t dd) = 0;

        virtual uint32_t capacity(void) = 0;  // In kilobytes
        virtual uint32_t blocks(void) = 0;

        virtual dd_err_e errno(void) const final { return _errno; }

    protected:
        DevDisk(void) {}

        dd_err_e _errno;
};

template < uint8_t ADDR, template < pin_t, pin_t > class I2C, pin_t SDA, pin_t SCL >
class DevI2C : public Dev
{
    using Ti2c = I2C < SDA, SCL >;

    public:
        virtual bool busy(void) = 0;
        virtual bool valid(void) const { return _i2c.valid(); }

    protected:
        DevI2C(void) {}

        Ti2c & _i2c = Ti2c::acquire();
        uint32_t _frequency;
};

#endif
