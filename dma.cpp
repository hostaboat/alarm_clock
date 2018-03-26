#include "dma.h"
#include "types.h"
#include "utility.h"

void DMA::Channel::preemptable(bool enable)
{
    if (enable) *_dchpri |= DMA_DCHPRIn_ECP;
    else *_dchpri &= ~DMA_DCHPRIn_ECP;
}

void DMA::Channel::preemptAbility(bool enable)
{
    if (!enable) *_dchpri |= DMA_DCHPRIn_DPA;
    else *_dchpri &= ~DMA_DCHPRIn_DPA;
}

bool DMA::Channel::priority(uint8_t priority)
{
    if (priority > _s_max_priority) return false;
    *_dchpri |= priority;
    return true;
}

void DMA::Channel::start(TCD const & tcd, src_e source, Isr * isr)
{
    *_chcfg = 0;

    memcpy((void *)_tcd, &tcd, sizeof(TCD));

    if ((tcd.inthalf || tcd.intmajor) && !NVIC::isEnabled((irq_e)(IRQ_DMA_CH0 + _ch_num)))
        NVIC::enable((irq_e)(IRQ_DMA_CH0 + _ch_num));

    _isr = isr;

    *_erq = 1;
    *_int = 1;
    //*_eei = 1;

    *_chcfg = source | DMAMUX_CHCFGn_ENBL; 
}

void DMA::Channel::stop(void)
{
    *_erq = 0;
    *_chcfg = 0;
    _isr = nullptr;
    NVIC::disable((irq_e)(IRQ_DMA_CH0 + _ch_num));
    *_int = 1;
}

uint32_t DMA::_s_available = _s_max_available;

reg8 DMA::_s_base_reg = (reg8)0x40008000;
reg32 DMA::_s_cr = (reg32)_s_base_reg;
reg32 DMA::_s_es = (reg32)(_s_base_reg + 0x04);
reg32 DMA::_s_erq = (reg32)(_s_base_reg + 0x0C);
//reg32 DMA::_s_eei = (reg32)(_s_base_reg + 0x14);
reg8 DMA::_s_ceei = _s_base_reg + 0x18;
reg8 DMA::_s_seei = _s_base_reg + 0x19;
reg8 DMA::_s_cerq = _s_base_reg + 0x1A;
reg8 DMA::_s_serq = _s_base_reg + 0x1B;
reg8 DMA::_s_cdne = _s_base_reg + 0x1C;
reg8 DMA::_s_ssrt = _s_base_reg + 0x1D;
reg8 DMA::_s_cerr = _s_base_reg + 0x1E;
reg8 DMA::_s_cint = _s_base_reg + 0x1F;
//reg32 DMA::_s_intr = (reg32)(_s_base_reg + 0x24);
//reg32 DMA::_s_err = (reg32)(_s_base_reg + 0x2C);
//reg32 DMA::_s_hrs = (reg32)(_s_base_reg + 0x34);

DMA::Channel DMA::_s_channels[_s_num_channels] =
{
    Channel( 0), Channel( 1), Channel( 2), Channel( 3),
    Channel( 4), Channel( 5), Channel( 6), Channel( 7),
    Channel( 8), Channel( 9), Channel(10), Channel(11),
    Channel(12), Channel(13), Channel(14), Channel(15),
};

uint8_t DMA::available(bool pit_channels)
{
    return pit_channels
        ? __builtin_popcount(_s_available & _s_pit_mask)
        : __builtin_popcount(_s_available & ~_s_pit_mask);
}

DMA::Channel * DMA::acquire(bool pit_channel)
{
    if (!available(pit_channel))
        return nullptr;

    if (_s_available == _s_max_available)
    {
        NVIC::enable(IRQ_DMA_ERROR);
        Module::enable(MOD_DMAMUX);
        Module::enable(MOD_DMA);
    }

    uint16_t mask = pit_channel ? _s_pit_mask : ~_s_pit_mask;
    uint8_t index = ctz8(_s_available & mask);

    _s_available &= ~(1 << index);

    return &_s_channels[index];
}

void DMA::release(Channel * ch)
{
    if (ch == nullptr)
        return;

    uint8_t index = ch - &_s_channels[0];
    if (index >= _s_num_channels) return;

    // XXX Do cleanup of channel if necessary
    ch->stop();

    _s_available |= (1 << index);

    if (_s_available == _s_max_available)
    {
        NVIC::disable(IRQ_DMA_ERROR);
        Module::disable(MOD_DMAMUX);
        Module::disable(MOD_DMA);
    }
}

void DMA::errorIsr(void)
{
    uint32_t error = *_s_es;
    *_s_cerr = DMA_ALL;

    _s_channels[(error & (0x0F << 8)) >> 8]._error = error & _s_error_mask;
}

void dma_ch0_isr(void) { DMA::chIsr < 0 > (); }
void dma_ch1_isr(void) { DMA::chIsr < 1 > (); }
void dma_ch2_isr(void) { DMA::chIsr < 2 > (); }
void dma_ch3_isr(void) { DMA::chIsr < 3 > (); }
void dma_ch4_isr(void) { DMA::chIsr < 4 > (); }
void dma_ch5_isr(void) { DMA::chIsr < 5 > (); }
void dma_ch6_isr(void) { DMA::chIsr < 6 > (); }
void dma_ch7_isr(void) { DMA::chIsr < 7 > (); }
void dma_ch8_isr(void) { DMA::chIsr < 8 > (); }
void dma_ch9_isr(void) { DMA::chIsr < 9 > (); }
void dma_ch10_isr(void) { DMA::chIsr < 10 > (); }
void dma_ch11_isr(void) { DMA::chIsr < 11 > (); }
void dma_ch12_isr(void) { DMA::chIsr < 12 > (); }
void dma_ch13_isr(void) { DMA::chIsr < 13 > (); }
void dma_ch14_isr(void) { DMA::chIsr < 14 > (); }
void dma_ch15_isr(void) { DMA::chIsr < 15 > (); }
void dma_error_isr(void) { DMA::errorIsr(); }

