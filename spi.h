#ifndef _SPI_H_
#define _SPI_H_

#include "pin.h"
#include "module.h"
#include "utility.h"
#include "types.h"

#define SPI_MCR_HALT     (1 << 0)
#define SPI_MCR_CLR_RXF  (1 << 10)
#define SPI_MCR_CLR_TXF  (1 << 11)
#define SPI_MCR_MDIS     (1 << 14)
#define SPI_MCR_PCSIS(p) ((uint32_t)((p) & 0x1F) << 16)
#define SPI_MCR_ROOE     (1 << 24)
#define SPI_MCR_MSTR     (1 << 31)

#define SPI_CTAR_BR(v)     ((uint32_t)((v) & 0x0F) <<  0)
#define SPI_CTAR_DT(v)     ((uint32_t)((v) & 0x0F) <<  4)
#define SPI_CTAR_ASC(v)    ((uint32_t)((v) & 0x0F) <<  8)
#define SPI_CTAR_CSSCK(v)  ((uint32_t)((v) & 0x0F) << 12)
#define SPI_CTAR_PBR(v)    ((uint32_t)((v) & 0x03) << 16)
#define SPI_CTAR_PDT(v)    ((uint32_t)((v) & 0x03) << 18)
#define SPI_CTAR_PASC(v)   ((uint32_t)((v) & 0x03) << 20)
#define SPI_CTAR_PCSSCK(v) ((uint32_t)((v) & 0x03) << 22)
#define SPI_CTAR_LSBFE     (1 << 24)
#define SPI_CTAR_CPHA      (1 << 25)
#define SPI_CTAR_CPOL      (1 << 26)
#define SPI_CTAR_FMSZ(v)   ((uint32_t)((v) & 0x0F) << 27)
#define SPI_CTAR_DBR       (1 << 31)

#define SPI_CTAR0_FMSZ   SPI_CTAR_FMSZ(0x07)
#define SPI_CTAR1_FMSZ   SPI_CTAR_FMSZ(0x0F)

#define SPI_SR_RFDF  (1 << 17)
#define SPI_SR_RFOF  (1 << 19)
#define SPI_SR_TFFF  (1 << 25)
#define SPI_SR_TFUF  (1 << 27)
#define SPI_SR_EOQF  (1 << 28)
#define SPI_SR_TXRXS (1 << 30)
#define SPI_SR_TCF   (1 << 31)

#define SPI_RSER_RFDF_DIRS (1 << 16)
#define SPI_RSER_RFDF_RE   (1 << 17)
#define SPI_RSER_RFOF_RE   (1 << 19)
#define SPI_RSER_TFFF_DIRS (1 << 24)
#define SPI_RSER_TFFF_RE   (1 << 25)
#define SPI_RSER_TFUF_RE   (1 << 27)
#define SPI_RSER_EOQF_RE   (1 << 28)
#define SPI_RSER_TCF_RE    (1 << 31)

#define SPI_PUSHR_CTCNT      (1 << 26)
#define SPI_PUSHR_EOQ        (1 << 27)
#define SPI_PUSHR_CTAS0      (0) 
#define SPI_PUSHR_CTAS1      (1 << 28) 

template < pin_t MOSI, pin_t MISO, pin_t SCK >
class SPI0 : public Module
{
    public:
        static SPI0 & acquire(void) { static SPI0 spi0; return spi0; }

        bool valid(void) { return _mosi.taken() && _miso.taken() && _sck.taken(); }

        void start(void);
        void stop(void);
        bool running(void) { return *_sr & SPI_SR_TXRXS; }
        bool restart(uint32_t cta) { return (_cta != cta) || rxFifoCount() || txFifoCount(); }

        void dmaEnable(void);
        void dmaDisable(void);
        bool dmaEnabled(void) const { return *_rser & (SPI_RSER_TFFF_DIRS | SPI_RSER_RFDF_DIRS); }

        template < class CS > bool begin(CS & cs, uint32_t cta)
        {
            if (!begin(cta)) return false;
            cs.clear();     // Chip Select Assert
            delay_usecs(1); // Simulated delay before SCK
            return true;
        }

        template < class CS > void end(CS & cs)
        {
            delay_usecs(1); // Simulated delay after SCK
            cs.set();       // Chip Select deassert
            delay_usecs(1); // Simulated delay between chip select assertions
            end();
        }

        bool begin(uint32_t cta);
        void end(void);

        bool busy(void) { return _busy || dmaEnabled(); }

        // Use for just sending data with no expectation of receiving anything back
        void tx8(uint8_t tx = 0xFF) { txWait(3); push8(tx); }
        void tx16(uint16_t tx = 0xFFFF) { txWait(3); push16(tx); }
        void tx32(uint32_t tx = 0xFFFFFFFF) { txWait(2); push16(tx >> 16); push16(tx & 0xFFFF); }
        void tx(uint8_t const * tx, uint16_t tx_len);
        void tx(uint8_t tx, uint16_t num_times);

        // Use for full duplex send and receive
        uint8_t txrx8(uint8_t tx = 0xFF) { tx8(tx); rxWait(1); return (uint8_t)*_popr; }
        uint16_t txrx16(uint16_t tx = 0xFFFF) { tx16(tx); rxWait(1); return (uint16_t)*_popr; }
        uint32_t txrx32(uint32_t tx = 0xFFFFFFFF) { tx32(tx); rxWait(2); return (*_popr << 16) | *_popr; }
        void txrx(uint8_t * tx, uint16_t tx_len);  // Will put rx data in tx buffer sent in
        void txrx(uint8_t const * tx, uint8_t * rx, uint16_t len);  // Separate rx buffer

        // Use for half duplex request / response type transaction
        uint8_t trans8(uint8_t tx = 0xFF) { (void)txrx8(tx); return txrx8(); }
        uint16_t trans16(uint16_t tx = 0xFFFF) { (void)txrx16(tx); return txrx16(); }
        uint32_t trans32(uint32_t tx = 0xFFFFFFFF) { (void)txrx32(tx); return txrx32(); }
        void trans(uint8_t const * tx, uint16_t tx_len, uint8_t * rx, uint16_t rx_len);

        void complete(void) { while (!(*_sr & SPI_SR_TCF)); *_sr |= SPI_SR_TCF; }
        void flush(void) { while (rxFifoCount()) (void)*_popr; }

        void push8(uint8_t tx) { *_pushr = tx | SPI_PUSHR_CTAS0; complete(); }
        void push16(uint16_t tx) { *_pushr = tx | SPI_PUSHR_CTAS1; complete(); }

        void txWait(uint8_t num_entries) { while (txFifoCount() > num_entries); }
        void rxWait(uint8_t num_entries) { while (rxFifoCount() < num_entries); }

        uint8_t rxFifoCount(void) { return (*_sr & 0x000000F0) >>  4; }
        uint8_t txFifoCount(void) { return (*_sr & 0x0000F000) >> 12; }
        void clearFifos(void);
        void clearTxFifo(void);
        void clearRxFifo(void);
        void clearStatusFlags(void);

        uint16_t transfers(void) { return (*_tcr & 0xFFFF0000) >> 16; }

        reg32 writeReg(void) { return _pushr; }
        reg32 readReg(void) { return _popr; }

        SPI0(SPI0 const &) = delete;
        SPI0 & operator=(SPI0 const &) = delete;

    private:
        SPI0(void);

        PinMOSI < MOSI > & _mosi = PinMOSI < MOSI >::acquire();
        PinMISO < MISO > & _miso = PinMISO < MISO >::acquire();
        PinSCK < SCK > & _sck = PinSCK < SCK >::acquire();

        uint32_t _cta = 0;  // Clock and Transfer Attributes Register
        bool _busy = false;

        reg8 _base_reg = (reg8)0x4002C000;
        reg32 _mcr = (reg32)_base_reg;
        reg32 _tcr = (reg32)(_base_reg + 0x08);
        reg32 _ctar0 = (reg32)(_base_reg + 0x0C);
        reg32 _ctar1 = (reg32)(_base_reg + 0x10);
        reg32 _sr = (reg32)(_base_reg + 0x2C);
        reg32 _rser = (reg32)(_base_reg + 0x30);
        reg32 _pushr = (reg32)(_base_reg + 0x34);
        reg32 _popr = (reg32)(_base_reg + 0x38);
        //reg32 _txfr0 = (reg32)(_base_reg + 0x3C);
        //reg32 _txfr1 = (reg32)(_base_reg + 0x40);
        //reg32 _txfr2 = (reg32)(_base_reg + 0x44);
        //reg32 _txfr3 = (reg32)(_base_reg + 0x48);
        //reg32 _rxfr0 = (reg32)(_base_reg + 0x7C);
        //reg32 _rxfr1 = (reg32)(_base_reg + 0x80);
        //reg32 _rxfr2 = (reg32)(_base_reg + 0x84);
        //reg32 _rxfr3 = (reg32)(_base_reg + 0x88);
};

template < pin_t MOSI, pin_t MISO, pin_t SCK >
SPI0 < MOSI, MISO, SCK >::SPI0(void)
{
    Module::enable(MOD_SPI0); // Open gate

    *_ctar0 = SPI_CTAR0_FMSZ;
    *_ctar1 = SPI_CTAR1_FMSZ;

    start();
}

// Note that only one of the CS pins that are used is capable of being a PCS pin. For a pin
// to be a PCS pin it has to be configured in PORT_PCR with MUX set appropriately - none
// are configured as such.  So setting SPI_MCR_PCSIS is, I believe, of no effect. There are
// simulated delays before and after SCK and after transfer/between chip select assertions
// - see begin() and end().
template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::start(void)
{
    if (running())
        return;

    *_mcr = SPI_MCR_MSTR | SPI_MCR_CLR_TXF | SPI_MCR_CLR_RXF | SPI_MCR_HALT;

    clearStatusFlags();

    *_mcr &= ~SPI_MCR_HALT;

    while (!running());
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::stop(void)
{
    if (!running())
        return;

    *_mcr = SPI_MCR_HALT; // | SPI_MCR_PCSIS(0x1F) | SPI_MCR_MDIS

    while (running());
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::dmaEnable(void)
{
    if (dmaEnabled())
        return;

    stop();
    *_rser |= SPI_RSER_TFFF_RE | SPI_RSER_TFFF_DIRS | SPI_RSER_RFDF_RE | SPI_RSER_RFDF_DIRS;
    start();
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::dmaDisable(void)
{
    if (!dmaEnabled())
        return;

    stop();
    *_rser &= ~(SPI_RSER_TFFF_RE | SPI_RSER_TFFF_DIRS | SPI_RSER_RFDF_RE | SPI_RSER_RFDF_DIRS);
    start();
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
bool SPI0 < MOSI, MISO, SCK >::begin(uint32_t cta)
{
    if (_busy) return false;
    _busy = true;

    if (restart(cta))
    {
        stop();

        if (_cta != cta)
        {
            *_ctar0 = SPI_CTAR0_FMSZ | cta;
            *_ctar1 = SPI_CTAR1_FMSZ | cta;

            _cta = cta;
        }

        start();
    }

    return true;
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::end(void)
{
    clearStatusFlags();
    _busy = false;
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::clearFifos(void)
{
    stop();
    *_mcr |= SPI_MCR_CLR_TXF | SPI_MCR_CLR_RXF;
    start();
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::clearRxFifo(void)
{
    stop();
    *_mcr |= SPI_MCR_CLR_RXF;
    start();
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::clearTxFifo(void)
{
    stop();
    *_mcr |= SPI_MCR_CLR_TXF;
    start();
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::clearStatusFlags(void)
{
    *_sr |=
        SPI_SR_TCF |
        // SPI_SR_TXRXS |  // Clearing will stop the module.
        SPI_SR_EOQF |
        SPI_SR_TFUF |
        SPI_SR_TFFF |
        SPI_SR_RFOF |
        SPI_SR_RFDF |
        0;
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::tx(uint8_t const * tx, uint16_t tx_len)
{
    if ((tx == nullptr) || (tx_len == 0))
        return;

    uint16_t i = 0;

    txWait(0);

    if (tx_len & 1)
    {
        push8(tx[i]);
        i += 1;
    }

    while (i < tx_len)
    {
        push16(((uint16_t)tx[i] << 8) | tx[i+1]);
        i += 2;
    }
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::tx(uint8_t tx, uint16_t num_times)
{
    txWait(0);

    if (num_times & 1)
    {
        push8(tx);
        num_times -= 1;
    }

    while (num_times != 0)
    {
        push16(((uint16_t)tx << 8) | tx);
        num_times -= 2;
    }
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::txrx(uint8_t * tx, uint16_t tx_len)
{
    uint8_t * rx = tx;
    txrx(tx, rx, tx_len);
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::txrx(uint8_t const * tx, uint8_t * rx, uint16_t len)
{
    if ((tx == nullptr) || (rx == nullptr))
        return;

    uint16_t i = 0;

    if (len & 1)
    {
        *rx++ = txrx8(tx[i]);
        i += 1;
    }

    if (len & 2)
    {
        *rx++ = txrx8(tx[i]);
        *rx++ = txrx8(tx[i+1]);
        i += 2;
    }

    if (len & 4)
    {
        *rx++ = txrx8(tx[i]);
        *rx++ = txrx8(tx[i+1]);
        *rx++ = txrx8(tx[i+2]);
        *rx++ = txrx8(tx[i+3]);
        i += 4;
    }

    while (i < len)
    {
        txWait(0);

        *_pushr = ((uint16_t)tx[i]   << 8) | tx[i+1] | SPI_PUSHR_CTAS1;
        *_pushr = ((uint16_t)tx[i+2] << 8) | tx[i+3] | SPI_PUSHR_CTAS1;
        *_pushr = ((uint16_t)tx[i+4] << 8) | tx[i+5] | SPI_PUSHR_CTAS1;
        *_pushr = ((uint16_t)tx[i+6] << 8) | tx[i+7] | SPI_PUSHR_CTAS1;

        i += 8;

        rxWait(4);

        uint32_t rx32 = (*_popr << 16) | *_popr;

        *rx++ = (uint8_t)(rx32 >> 24);
        *rx++ = (uint8_t)(rx32 >> 16);
        *rx++ = (uint8_t)(rx32 >>  8);
        *rx++ = (uint8_t)(rx32 >>  0);

        rx32 = (*_popr << 16) | *_popr;

        *rx++ = (uint8_t)(rx32 >> 24);
        *rx++ = (uint8_t)(rx32 >> 16);
        *rx++ = (uint8_t)(rx32 >>  8);
        *rx++ = (uint8_t)(rx32 >>  0);
    }
}

template < pin_t MOSI, pin_t MISO, pin_t SCK >
void SPI0 < MOSI, MISO, SCK >::trans(uint8_t const * tx, uint16_t tx_len, uint8_t * rx, uint16_t rx_len)
{
    // Just send off the request if there is one
    if ((tx != nullptr) && (tx_len != 0))
    {
        this->tx(tx, tx_len);
        flush(); // Chuck the data that was shifted in
    }

    if ((rx == nullptr) || (rx_len == 0))
        return;

    // Now get the response by pushing all 1 bits
    uint8_t * rx_end = rx + rx_len;

    if (rx_len & 1)
        *rx++ = txrx8(0xFF);

    if (rx_len & 2)
    {
        *rx++ = txrx8(0xFF);
        *rx++ = txrx8(0xFF);
    }

    if (rx_len & 4)
    {
        *rx++ = txrx8(0xFF);
        *rx++ = txrx8(0xFF);
        *rx++ = txrx8(0xFF);
        *rx++ = txrx8(0xFF);
    }

    while (rx < rx_end)
    {
        *_pushr = 0xFFFF | SPI_PUSHR_CTAS1;
        *_pushr = 0xFFFF | SPI_PUSHR_CTAS1;
        *_pushr = 0xFFFF | SPI_PUSHR_CTAS1;
        *_pushr = 0xFFFF | SPI_PUSHR_CTAS1;

        rxWait(2);

        uint32_t rx32 = (*_popr << 16) | *_popr;

        *rx++ = (uint8_t)(rx32 >> 24);
        *rx++ = (uint8_t)(rx32 >> 16);
        *rx++ = (uint8_t)(rx32 >>  8);
        *rx++ = (uint8_t)(rx32 >>  0);

        rxWait(2);

        rx32 = (*_popr << 16) | *_popr;

        *rx++ = (uint8_t)(rx32 >> 24);
        *rx++ = (uint8_t)(rx32 >> 16);
        *rx++ = (uint8_t)(rx32 >>  8);
        *rx++ = (uint8_t)(rx32 >>  0);
    }
}

#endif

