#ifndef _PIN_H_
#define _PIN_H_

#include "types.h"
#include "module.h"
#include "utility.h"

#define PIN_RS_R         0 // Rotary Switch - Right
#define PIN_RS_M         1 // Rotary Switch - Middle
#define PIN_RS_L         2 // Rotary Switch - Left
#define PIN_PREV         3 // Play previous audio file buttion
#define PIN_PLAY         4 // Play / Pause button
#define PIN_NEXT         5 // Play next audio file button
#define PIN_EN_BR_A      6 // Rotary Encoder A - Brightness
#define PIN_EN_BR_B      7 // Rotary Encoder B - Brightness
#define PIN_NEO          8 // Neopixel strip
#define PIN_AMP_SDWN     9 // Amplifier Shutdown
#define PIN_VS_DREQ     10 // VS1053 Data Request
#define PIN_MOSI        11 // SPI - MOSI / DOUT
#define PIN_MISO        12 // SPI - MISO / DIN
#define PIN_VS_RST      13 // VS1053 Hardware Reset
#define PIN_SCK         14 // SPI - SCK
#define PIN_VS_CS       15 // SPI - VS1053 Command Chip Select (SCI)
#define PIN_SD_CS       16 // SPI - SD Chip Select
#define PIN_VS_DCS      17 // SPI - VS1053 Data Chip Select (SDI)
#define PIN_SDA         18 // I2C - SDA  (7-Segment Display)
#define PIN_SCL         19 // I2C - SCL  (7-Segment Display)
#define PIN_BEEPER      20 // Beeper
#define PIN_EN_SW       21 // Rotary Encoder Switch
#define PIN_EN_A        22 // Rotary Encoder A
#define PIN_EN_B        23 // Rotary Encoder B
#define PIN_CNT         24 // Number of digital pins
#define PIN_TOUCH       25 // Touch - Alarm snooze : uses touchRead()

#define PORT_PCR_ISF     (1 << 24)
#define PORT_PCR_IRQC(i) ((uint32_t)((i) & 0x0F) << 16)
#define PORT_PCR_MUX(m)  ((uint32_t)((m) & 0x07) <<  8)
#define PORT_PCR_DSE     (1 << 6)
#define PORT_PCR_ODE     (1 << 5)
#define PORT_PCR_PFE     (1 << 4)
#define PORT_PCR_SRE     (1 << 2)
#define PORT_PCR_PE      (1 << 1)
#define PORT_PCR_PS      (1 << 0)

extern "C"
{
    void porta_isr(void);
    void portb_isr(void);
    void portc_isr(void);
    void portd_isr(void);
    void porte_isr(void);
}

////////////////////////////////////////////////////////////////////////////////
// Teensy pin to Port and Port pins ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
enum port_e : uint32_t { PORTA, PORTB, PORTC, PORTD, PORTE };

constexpr port_e const port_map[] =
{
    PORTB /* 0 16*/, PORTB /* 1 17*/, PORTD /* 2  0*/, PORTA /* 3 12*/,
    PORTA /* 4 13*/, PORTD /* 5  7*/, PORTD /* 6  4*/, PORTD /* 7  2*/,
    PORTD /* 8  3*/, PORTC /* 9  3*/, PORTC /*10  4*/, PORTC /*11  6*/,
    PORTC /*12  7*/, PORTC /*13  5*/, PORTD /*14  1*/, PORTC /*15  0*/,
    PORTB /*16  0*/, PORTB /*17  1*/, PORTB /*18  3*/, PORTB /*19  2*/,
    PORTD /*20  5*/, PORTD /*21  6*/, PORTC /*22  1*/, PORTC /*23  2*/,
    PORTA /*24  5*/, PORTB /*25 19*/, PORTE /*26  1*/, PORTC /*27  9*/,
    PORTC /*28  8*/, PORTC /*29 10*/, PORTC /*30 11*/, PORTE /*31  0*/,
    PORTB /*32 18*/, PORTA /*33  4*/,
};

constexpr uint8_t const pin_map[] =
{
    16 /* 0 PTB*/, 17 /* 1 PTB*/,  0 /* 2 PTD*/, 12 /* 3 PTA*/,
    13 /* 4 PTA*/,  7 /* 5 PTD*/,  4 /* 6 PTD*/,  2 /* 7 PTD*/,
     3 /* 8 PTD*/,  3 /* 9 PTC*/,  4 /*10 PTC*/,  6 /*11 PTC*/,
     7 /*12 PTC*/,  5 /*13 PTC*/,  1 /*14 PTD*/,  0 /*15 PTC*/,
     0 /*16 PTB*/,  1 /*17 PTB*/,  3 /*18 PTB*/,  2 /*19 PTB*/,
     5 /*20 PTD*/,  6 /*21 PTD*/,  1 /*22 PTC*/,  2 /*23 PTC*/,
     5 /*24 PTA*/, 19 /*25 PTB*/,  1 /*26 PTE*/,  9 /*27 PTC*/,
     8 /*28 PTC*/, 10 /*29 PTC*/, 11 /*30 PTC*/,  0 /*31 PTE*/,
    18 /*32 PTB*/,  4 /*33 PTA*/,
};

////////////////////////////////////////////////////////////////////////////////
// PORT registers //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class Isr;

class PORT
{
    friend class Isr;
    protected:
        static reg32 pcr(pin_t p)  { return _s_pcr_base  + ((port_map[p] * 0x0400) + pin_map[p]); }
        static reg32 isfr(pin_t p) { return _s_isfr_base + ((port_map[p] * 0x8000) + pin_map[p]); }
    private:
        static reg32 _s_pcr_base;
        static reg32 _s_isfr_base;
};

////////////////////////////////////////////////////////////////////////////////
// Isr /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
enum irqc_e : uint32_t
{
    IRQC_DISABLED     = 0x00,
    IRQC_DMA_RISING   = 0x01,
    IRQC_DMA_FALLING  = 0x02,
    IRQC_DMA_CHANGE   = 0x03,
  //IRQC_RES0         = 0x04,  // Reserved
  //IRQC_RES1         = 0x05,
  //IRQC_RES2         = 0x06,
  //IRQC_RES3         = 0x07,
    IRQC_INTR_LOW     = 0x08,
    IRQC_INTR_RISING  = 0x09,
    IRQC_INTR_FALLING = 0x0A,
    IRQC_INTR_CHANGE  = 0x0B,
    IRQC_INTR_HIGH    = 0x0C,
    IRQC_MASK         = 0x0F,
};

template < port_e > class Port;

class Isr
{
    template < port_e > friend class Port;

    using isr_t = void (Isr::*)(void);

    public:
        void isr(void) { *_isfr = 1; MFC(_isr)(); }
        virtual irqc_e irqc(void) const final { return _irqc; }

    protected:
        Isr(pin_t pin) : _isfr(PORT::isfr(pin)) { }

        virtual void isrLow(void) {}
        virtual void isrRising(void) {}
        virtual void isrFalling(void) {}
        virtual void isrChange(void) {}
        virtual void isrHigh(void) {}
        virtual void isrNone(void) {}

    private:
        void update(irqc_e irqc)
        {
            _irqc = irqc;

            // Outside of this class, this can only be called from the Port
            // class via an attach() or update() call.  It disables interrupts.

            switch (irqc)
            {
                case IRQC_INTR_LOW:     _isr = &Isr::isrLow; break;
                case IRQC_INTR_RISING:  _isr = &Isr::isrRising; break;
                case IRQC_INTR_FALLING: _isr = &Isr::isrFalling; break;
                case IRQC_INTR_CHANGE:  _isr = &Isr::isrChange; break;
                case IRQC_INTR_HIGH:    _isr = &Isr::isrHigh; break;
                default:                _isr = &Isr::isrNone; break;
            }
        }

        irqc_e _irqc = IRQC_DISABLED;
        isr_t _isr = &Isr::isrNone;
        reg32 _isfr;
};

////////////////////////////////////////////////////////////////////////////////
// Port ////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
constexpr irq_e const port_irq_map[] = { IRQ_PORTA, IRQ_PORTB, IRQ_PORTC, IRQ_PORTD, IRQ_PORTE };
constexpr module_e const port_module_map[] = { MOD_PORTA, MOD_PORTB, MOD_PORTC, MOD_PORTD, MOD_PORTE };

enum mux_e : uint32_t { MUX_ALT0, MUX_ALT1, MUX_ALT2, MUX_ALT3, MUX_ALT4, MUX_ALT5, MUX_ALT6, MUX_ALT7 };

template < port_e P >
class Port : public PORT, public Module
{
    public:
        virtual bool acquire(uint8_t flags, mux_e mux) final;
        virtual bool release(void) final;

        virtual bool taken(void) const final { return _s_pins & (1 << _ptp); }

        virtual bool attach(Isr * isr, irqc_e irqc) final;
        virtual bool update(irqc_e irqc) final;
        virtual bool detach(void) final;

        virtual irqc_e irqc(void) const final
        { return (_s_isr[_ptp] != nullptr) ? _s_isr[_ptp]->irqc() : IRQC_DISABLED; }

    protected:
        Port(pin_t pin, uint8_t flags, mux_e mux);

        uint8_t _ptp = 0;  // Port Pin, e.g. PTA17
        reg32 _pcr;

        static Isr * _s_isr[32];

    private:
        bool configure(uint8_t flags, mux_e mux);
        void updateIrqc(irqc_e irqc);

        static uint32_t _s_pins;
        static uint32_t _s_isrs;
};

template < port_e P >
Port < P >::Port(pin_t pin, uint8_t flags, mux_e mux)
    : _ptp(pin_map[pin]), _pcr(PORT::pcr(pin))
{
    if (taken())
        return;

    if (0 == _s_pins)
        Module::enable(port_module_map[P]);

    _s_pins |= (1 << _ptp);

    (void)configure(flags, mux);
}

template < port_e P >
bool Port < P >::configure(uint8_t flags, mux_e mux)
{
    if (!taken())
        return false;

    *_pcr = PORT_PCR_ISF;
    *_pcr &= ~0xFFFF;
    *_pcr |= PORT_PCR_MUX(mux) | flags;

    return true;
}

template < port_e P >
bool Port < P >::acquire(uint8_t flags, mux_e mux)
{
    if (taken())
        return false;

    _s_pins |= (1 << _ptp);

    return configure(flags, mux);
}

template < port_e P >
bool Port < P >::attach(Isr * isr, irqc_e irqc)
{
    if (!taken() || (isr == nullptr) || (_s_isr[_ptp] != nullptr))
        return false;

    if (0 == _s_isrs)
        NVIC::enable(port_irq_map[P]);

    _s_isrs |= (1 << _ptp);

    __disable_irq();

    _s_isr[_ptp] = isr;
    updateIrqc(irqc);

    __enable_irq();

    return true;
}

template < port_e P >
bool Port < P >::update(irqc_e irqc)
{
    if (!taken() || (_s_isr[_ptp] == nullptr))
        return false;

    __disable_irq();
    updateIrqc(irqc);
    __enable_irq();

    return true;
}

template < port_e P >
void Port < P >::updateIrqc(irqc_e irqc)
{
    _s_isr[_ptp]->update(irqc);
    *_pcr |= PORT_PCR_ISF;
    *_pcr &= ~PORT_PCR_IRQC(0x0F);
    *_pcr |= PORT_PCR_IRQC(irqc);
}

template < port_e P >
bool Port < P >::detach(void)
{
    if (!taken() || (_s_isr[_ptp] == nullptr))
        return false;

    __disable_irq();

    *_pcr &= ~PORT_PCR_IRQC(0x0F);
    *_pcr |= PORT_PCR_ISF;

    _s_isr[_ptp]->update(IRQC_DISABLED);
    _s_isr[_ptp] = nullptr;

    __enable_irq();

    _s_isrs &= ~(1 << _ptp);

    if (_s_isrs == 0)
        NVIC::disable(port_irq_map[P]);

    return true;
}

template < port_e P >
bool Port < P >::release(void)
{
    if (!taken())
        return false;

    *_pcr = PORT_PCR_ISF;

    (void)detach();

    _s_pins &= ~(1 << _ptp);

    if (_s_pins == 0)
        Module::disable(port_module_map[P]);

    return true;
}

template < port_e P > Isr * Port < P >::_s_isr[32] = {};
template < port_e P > uint32_t Port < P >::_s_pins = 0;
template < port_e P > uint32_t Port < P >::_s_isrs = 0;

////////////////////////////////////////////////////////////////////////////////
// PortX ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < port_e PORT > class PortX {};

template < >
class PortX < PORTA > : public Port < PORTA >
{
    friend void porta_isr(void);
    protected:
        PortX < PORTA > (pin_t pin, uint8_t flags, mux_e mux)
            : Port < PORTA > (pin, flags, mux) {}
    private:
        static reg32 _s_isfr;
};

template < >
class PortX < PORTB > : public Port < PORTB >
{
    friend void portb_isr(void);
    protected:
        PortX < PORTB > (pin_t pin, uint8_t flags, mux_e mux)
            : Port < PORTB > (pin, flags, mux) {}
    private:
        static reg32 _s_isfr;
};

template < >
class PortX < PORTC > : public Port < PORTC >
{
    friend void portc_isr(void);
    protected:
        PortX < PORTC > (pin_t pin, uint8_t flags, mux_e mux)
            : Port < PORTC > (pin, flags, mux) {}
    private:
        static reg32 _s_isfr;
};

template < >
class PortX < PORTD > : public Port < PORTD >
{
    friend void portd_isr(void);
    protected:
        PortX < PORTD > (pin_t pin, uint8_t flags, mux_e mux)
            : Port < PORTD > (pin, flags, mux) {}
    private:
        static reg32 _s_isfr;
};

template < >
class PortX < PORTE > : public Port<PORTE>
{
    friend void porte_isr(void);
    public:
        PortX < PORTE > (pin_t pin, uint8_t flags, mux_e mux)
            : Port < PORTE > (pin, flags, mux) {}
    private:
        static reg32 _s_isfr;
};

using PortA = PortX < PORTA >;
using PortB = PortX < PORTB >;
using PortC = PortX < PORTC >;
using PortD = PortX < PORTD >;
using PortE = PortX < PORTE >;

////////////////////////////////////////////////////////////////////////////////
// PinAltN - Generic digital pin
////////////////////////////////////////////////////////////////////////////////
template < pin_t PIN, mux_e MUX >
class PinAltN : public PortX < port_map[PIN] >
{
    static_assert(PIN < PIN_CNT, "Invalid PIN");
    static_assert(MUX != MUX_ALT0, "Invalid MUX for digital pin");
    public:
        static PinAltN & acquire(uint8_t flags) { static PinAltN pin(flags); return pin; }
    protected:
        PinAltN(uint8_t flags) : PortX < port_map[PIN] > (PIN, flags, MUX) {}
};

// Analog pins not applicable
template < pin_t PIN > class PinAltN < PIN, MUX_ALT0 > {};
template < pin_t PIN > using PinAnalog = PinAltN < PIN, MUX_ALT0 >;

////////////////////////////////////////////////////////////////////////////////
// PinSPI - SPI specific pins
////////////////////////////////////////////////////////////////////////////////
template < pin_t PIN >
class PinSPI : public PinAltN < PIN, MUX_ALT2 >
{
    protected:
        PinSPI(uint8_t flags) : PinAltN < PIN, MUX_ALT2 > (flags) {}
};

// PIN_PCR_DSE is Drive Strength Enable
// The core teensy code sometimes uses this and sometimes not for
// certain pins.  Not sure if/when it's necessary.
// Found it to be necessary for pin 14 SCK

template < pin_t PIN >
class PinMOSI : public PinSPI < PIN >
{
    static_assert((PIN == 7) || (PIN == 11), "Invalid MOSI pin");
    public:
        static PinMOSI & acquire(void) { static PinMOSI pin; return pin; }
    private:
        PinMOSI(void) : PinSPI < PIN > (_s_flags) {}
        static constexpr uint8_t const _s_flags = PORT_PCR_DSE;
};

template < pin_t PIN >
class PinMISO : public PinSPI < PIN >
{
    static_assert((PIN == 8) || (PIN == 12), "Invalid MISO pin");
    public:
        static PinMISO & acquire(void) { static PinMISO pin; return pin; }
    private:
        PinMISO(void) : PinSPI < PIN > (_s_flags) {}
        static constexpr uint8_t const _s_flags = 0;
};

template < pin_t PIN >
class PinSCK : public PinSPI < PIN >
{
    static_assert((PIN == 13) || (PIN == 14), "Invalid SCK pin");
    public:
        static PinSCK & acquire(void) { static PinSCK pin; return pin; }
    private:
        PinSCK(void) : PinSPI < PIN > (_s_flags) {}
        static constexpr uint8_t const _s_flags = PORT_PCR_DSE;
};

////////////////////////////////////////////////////////////////////////////////
// PinI2C - I2C specific pins
////////////////////////////////////////////////////////////////////////////////
template < pin_t PIN >
class PinI2C : public PinAltN < PIN, MUX_ALT2 >
{
    protected:
        PinI2C(uint8_t flags) : PinAltN < PIN, MUX_ALT2 > (flags) {}
};

template < pin_t PIN >
class PinSDA : public PinI2C < PIN >
{
    static_assert((PIN == 17) || (PIN == 18) || (PIN == 30), "Invalid SDA pin");
    public:
        static PinSDA & acquire(void) { static PinSDA pin; return pin; }
    private:
        PinSDA(void) : PinI2C < PIN > (_s_flags) {}
        static constexpr uint8_t const _s_flags = PORT_PCR_ODE | PORT_PCR_SRE | PORT_PCR_DSE;
};

template < pin_t PIN >
class PinSCL : public PinI2C < PIN >
{
    static_assert((PIN == 16) || (PIN == 19) || (PIN == 29), "Invalid SCL pin");
    public:
        static PinSCL & acquire(void) { static PinSCL pin; return pin; }
    private:
        PinSCL(void) : PinI2C < PIN > (_s_flags) {}
        static constexpr uint8_t const _s_flags = PORT_PCR_ODE | PORT_PCR_SRE | PORT_PCR_DSE;
};

////////////////////////////////////////////////////////////////////////////////
// GPIO registers //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class GPIO
{
    protected:
        static reg32 pdor(pin_t p) { return _s_base + ((port_map[p] * 0x0200) + 0x00 + pin_map[p]); }
        static reg32 psor(pin_t p) { return _s_base + ((port_map[p] * 0x0200) + 0x20 + pin_map[p]); }
        static reg32 pcor(pin_t p) { return _s_base + ((port_map[p] * 0x0200) + 0x40 + pin_map[p]); }
        static reg32 ptor(pin_t p) { return _s_base + ((port_map[p] * 0x0200) + 0x60 + pin_map[p]); }
        static reg32 pdir(pin_t p) { return _s_base + ((port_map[p] * 0x0200) + 0x80 + pin_map[p]); }
        static reg32 pddr(pin_t p) { return _s_base + ((port_map[p] * 0x0200) + 0xA0 + pin_map[p]); }
    private:
        static reg32 _s_base;
};

////////////////////////////////////////////////////////////////////////////////
// PinGPIO - Base class for pins configured as GPIO
////////////////////////////////////////////////////////////////////////////////
enum dir_e : uint8_t { DIR_INPUT, DIR_OUTPUT };

template < pin_t PIN, dir_e DIR >
class PinGPIO : public GPIO, public PinAltN < PIN, MUX_ALT1 >
{
    public:
        virtual dir_e dir(void) const final     { return (dir_e)*_pddr; }
        virtual bool isInput(void) const final  { return dir() == DIR_INPUT; }
        virtual bool isOutput(void) const final { return dir() == DIR_OUTPUT; }

    protected:
        PinGPIO(uint8_t flags) : PinAltN < PIN, MUX_ALT1 > (flags) { setDir(DIR); }

    private:
        void setDir(dir_e d)  { *_pddr = d; }
        void setInput(void)   { *_pddr = 0; }
        void setOutput(void)  { *_pddr = 1; }

        reg32 _pddr = GPIO::pddr(PIN);
};

// Output
enum out_e : uint8_t { OUT_LOW, OUT_HIGH };

template < pin_t PIN >
class PinOutput : public PinGPIO < PIN, DIR_OUTPUT >
{
    public:
        static PinOutput & acquire(void) { static PinOutput pin; return pin; }

        out_e get(void) const   { return (out_e)*_pdor; }
        bool isSet(void) const  { return get() == OUT_HIGH; }
        void set(void)    { *_psor = 1; }
        void clear(void)  { *_pcor = 1; }
        void toggle(void) { *_ptor = 1; }

    private:
        PinOutput(void) : PinGPIO < PIN, DIR_OUTPUT > (_s_flags) {}
        static constexpr uint8_t const _s_flags = PORT_PCR_SRE | PORT_PCR_DSE;
        reg32 _pdor = GPIO::pdor(PIN);
        reg32 _psor = GPIO::psor(PIN);
        reg32 _pcor = GPIO::pcor(PIN);
        reg32 _ptor = GPIO::ptor(PIN);
};

// Input base class
enum in_e : uint8_t { IN_LOW, IN_HIGH };

template < pin_t PIN >
class PinInputX : public PinGPIO < PIN, DIR_INPUT >
{
    public:
        virtual in_e read(void) const final { return (in_e)*_pdir; }
        virtual bool isHigh(void) const final { return read() == IN_HIGH; }
        virtual bool isLow(void) const final { return read() == IN_LOW; }

    protected:
        PinInputX(uint8_t flags) : PinGPIO < PIN, DIR_INPUT > (flags) {}

    private:
        reg32 _pdir = GPIO::pdir(PIN);
};

// Input
template < pin_t PIN >
class PinInput : public PinInputX < PIN >
{
    public:
        static PinInput & acquire(void) { static PinInput pin; return pin; }
    private:
        PinInput(void) : PinInputX < PIN > (_s_flags) {}
        static constexpr uint8_t const _s_flags = 0;
};

// Input w/ pullup
template < pin_t PIN >
class PinInputPullUp : public PinInputX < PIN >
{
    public:
        static PinInputPullUp & acquire(void) { static PinInputPullUp pin; return pin; }
    private:
        PinInputPullUp(void) : PinInputX < PIN > (_s_flags) {}
        static constexpr uint8_t const _s_flags = PORT_PCR_PE | PORT_PCR_PS;
};

// Input w/ pulldown
template < pin_t PIN >
class PinInputPullDown : public PinInputX < PIN >
{
    public:
        static PinInputPullDown & acquire(void) { static PinInputPullDown pin; return pin; }
    private:
        PinInputPullDown(void) : PinInputX < PIN > (_s_flags) {}
        static constexpr uint8_t const _s_flags = PORT_PCR_PE;
};

template < pin_t PIN > using PinOut = PinOutput < PIN >;
template < pin_t PIN > using PinIn = PinInput < PIN >;
template < pin_t PIN > using PinInPU = PinInputPullUp < PIN >;
template < pin_t PIN > using PinInPD = PinInputPullDown < PIN >;

#endif
