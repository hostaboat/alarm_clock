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

using dd_t = void *;
enum dd_e { DD_READ, DD_WRITE };

template < uint16_t READ_BLK_LEN,
         pin_t CS, template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
class DevDisk : public DevSPI < CS, SPI, MOSI, MISO, SCK >
{
    public:
        virtual bool busy(void) = 0;
        virtual bool read(uint32_t address, uint8_t (&buf)[READ_BLK_LEN]) = 0;
        virtual bool write(uint32_t address, uint8_t (&buf)[READ_BLK_LEN]) = 0;
        virtual uint32_t capacity(void) = 0;  // In kilobytes
        virtual uint32_t blocks(void) = 0;
        virtual dd_t open(uint32_t addr, uint16_t num_blocks, dd_e dir) = 0;
        virtual uint16_t read(dd_t dd, uint8_t * buf, uint16_t blen) = 0;
        virtual uint16_t write(dd_t dd, uint8_t * data, uint16_t dlen) = 0;
        virtual bool close(dd_t dd) = 0;

    protected:
        DevDisk(void) {}
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
