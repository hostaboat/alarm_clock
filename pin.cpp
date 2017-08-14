#include "pin.h"
#include "types.h"

reg32 PORT::_s_pcr_base  = (reg32)0x40049000;
reg32 PORT::_s_isfr_base = (reg32)0x42921400;  // Bit-band alias

reg32 PortA::_s_isfr = (reg32)0x400490A0;
reg32 PortB::_s_isfr = (reg32)0x4004A0A0;
reg32 PortC::_s_isfr = (reg32)0x4004B0A0;
reg32 PortD::_s_isfr = (reg32)0x4004C0A0;
reg32 PortE::_s_isfr = (reg32)0x4004D0A0;

reg32 GPIO::_s_base = (reg32)0x43FE0000;  // Bit-band alias

void porta_isr(void)
{
    uint32_t isfr = *PortA::_s_isfr;
    do {
        PortA::_s_isr[__builtin_ctz(isfr)]->isr();
        isfr &= isfr - 1;
    } while (isfr);
}

void portb_isr(void)
{
    uint32_t isfr = *PortB::_s_isfr;
    do {
        PortB::_s_isr[__builtin_ctz(isfr)]->isr();
        isfr &= isfr - 1;
    } while (isfr);
}

void portc_isr(void)
{
    uint32_t isfr = *PortC::_s_isfr;
    do {
        PortC::_s_isr[__builtin_ctz(isfr)]->isr();
        isfr &= isfr - 1;
    } while (isfr);
}

void portd_isr(void)
{
    uint32_t isfr = *PortD::_s_isfr;
    do {
        PortD::_s_isr[__builtin_ctz(isfr)]->isr();
        isfr &= isfr - 1;
    } while (isfr);
}

void porte_isr(void)
{
    uint32_t isfr = *PortE::_s_isfr;
    do {
        PortE::_s_isr[__builtin_ctz(isfr)]->isr();
        isfr &= isfr - 1;
    } while (isfr);
}
