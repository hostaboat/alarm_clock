#ifndef _DMA_H_
#define _DMA_H_

#include "types.h"
#include "utility.h"
#include "module.h"
#include <string.h>

// DMA Multiplexer
#define DMAMUX_CHCFGn_TRIG   (1 << 6)
#define DMAMUX_CHCFGn_ENBL   (1 << 7)

// Control
#define DMA_CR_EDBG  (1 << 1)
#define DMA_CR_ERCA  (1 << 2)
#define DMA_CR_HOE   (1 << 4)
#define DMA_CR_HALT  (1 << 5)
#define DMA_CR_CLM   (1 << 6)
#define DMA_CR_EMLM  (1 << 7)
#define DMA_CR_ECX   (1 << 16)
#define DMA_CR_CX    (1 << 17)

// For use with the following registers
//  DMA_CEEI  DMA_SEEI  DMA_CERQ  DMA_SERQ
//  DMA_CDNE  DMA_SSRT  DMA_CERR  DMA_CINT
#define DMA_NOP (1 << 7)
#define DMA_ALL (1 << 6)

// Channel n Priority
#define DMA_DCHPRIn_DPA    (1 << 6)
#define DMA_DCHPRIn_ECP    (1 << 7)

struct alignas(32) TCD
{
    // Source Address
    void volatile * saddr;

    // Signed Source Address Offset
    uint16_t soff;

    // Transfer Attributes

    // Allowable values for dsize and ssize
    enum ts_e : uint8_t { TS_8_BIT, TS_16_BIT, TS_32_BIT, TS_16_BYTE = 4 };

    union
    {
        uint16_t attr;

        // xMOD indicates the number of mask bits - applied after xADDR + xOFF
        // Note: For the mod feature to work properly, saddr and/or daddr MUST
        // be aligned to the MOD boundary.
        struct
        {
            uint16_t dsize : 3;
            uint16_t dmod  : 5;
            uint16_t ssize : 3;
            uint16_t smod  : 5;
        };
    };

    // Minor Byte Count
    union
    {
        uint32_t nbytes;

        // Minor Loop Disabled - DMA_CR[EMLM] = 0
        uint32_t nbytes_mlno;

        // Minor Loop Enabled - DMA_CR[EMLM] = 1
        // DMLOE = SMLOE = 0
        struct
        {
            uint32_t nbytes_mloffno : 30;
            uint32_t dmloe_mloffno  :  1;
            uint32_t smloe_mloffno  :  1;
        };

        // Minor Loop Enabled - DMA_CR[EMLM] = 1
        // DMLOE = 1 and/or SMLOE = 1
        struct
        {
            uint32_t nbytes_mloffyes : 10;
            uint32_t mloff_mloffyes  : 20;
            uint32_t dmloe_mloffyes  :  1;
            uint32_t smloe_mloffyes  :  1;
        };
    };

    // Last Source Address Adjustment
    // Applied after completion, i.e. CITER = 0
    uint32_t slast;

    // Destination Address
    void volatile * daddr;

    // Signed Destination Address Offset
    // Applied after each minor loop iteration
    uint16_t doff;

    // Current Major Loop Iteration
    union
    {
        uint16_t citer;

        // ELINK = 0
        struct
        {
            uint16_t citer_nlc       : 15;
            uint16_t citer_nlc_elink :  1;
        };

        // ELINK = 1
        struct
        {
            uint16_t citer_lc        : 9;  // Current Major Iteration Count
            uint16_t citer_lc_linkch : 4;  // Link Channel Number
            uint16_t citer_lc_rsvd   : 2;  // reserved
            uint16_t citer_lc_elink  : 1;  // Enable channel-to-channel linking on minor loop completion
        };
    };

    union
    {
        // Last Source Address Adjustment - DMA_TCDn_CSR[ESG] = 0
        // Applied after completion, i.e. CITER = 0
        uint32_t dlast;

        // Scatter/Gather Address - DMA_TCDn_CSR[ESG] = 1
        // Address pointing to a TCD to be loaded after channel completion
        void volatile * sga;
    };

    // Control and Status

    // Allowable values for Bandwidth Control
    enum bwc_e : uint8_t { BWC_NO_STALL, BWC_STALL_4 = 2, BWC_STALL_8 };

    union
    {
        uint16_t csr;

        struct
        {
            uint16_t start       : 1;  // Channel Start
            uint16_t intmajor    : 1;  // Enable interrupt when channel completes
            uint16_t inthalf     : 1;  // Enable interrupt when major counter is half
            uint16_t dreq        : 1;  // Disable Request - clear ERQ bit when channel completes
            uint16_t esg         : 1;  // Enable Scatter/Gather processing
            uint16_t majorelink  : 1;  // Enable channel-to-channel linking when channel completes
            uint16_t active      : 1;  // Channel is Active
            uint16_t done        : 1;  // Channel is Done
            uint16_t majorlinkch : 4;  // Channel to link to if channel-to-channel linking enabled
            uint16_t csr_rsvd    : 2;  // reserved
            uint16_t bwc         : 2;  // Bandwidth Control
        };
    };

    // Beginning Major Loop Iteration
    union
    {
        uint16_t biter;

        // ELINK = 0
        struct
        {
            uint16_t biter_nlc       : 15;
            uint16_t biter_nlc_elink :  1;
        };

        // ELINK = 1
        struct
        {
            uint16_t biter_lc        : 9;  // Beginning Major Iteration Count
            uint16_t biter_lc_linkch : 4;  // Link Channel Number
            uint16_t biter_lc_rsvd   : 2;  // reserved
            uint16_t biter_lc_elink  : 1;  // Enable channel-to-channel linking on minor loop completion
        };
    };
};

extern "C"
{
    void dma_ch0_isr(void);
    void dma_ch1_isr(void);
    void dma_ch2_isr(void);
    void dma_ch3_isr(void);
    void dma_ch4_isr(void);
    void dma_ch5_isr(void);
    void dma_ch6_isr(void);
    void dma_ch7_isr(void);
    void dma_ch8_isr(void);
    void dma_ch9_isr(void);
    void dma_ch10_isr(void);
    void dma_ch11_isr(void);
    void dma_ch12_isr(void);
    void dma_ch13_isr(void);
    void dma_ch14_isr(void);
    void dma_ch15_isr(void);
    void dma_error_isr(void);
}

class DMA : public Module
{
    friend void dma_ch0_isr(void);
    friend void dma_ch1_isr(void);
    friend void dma_ch2_isr(void);
    friend void dma_ch3_isr(void);
    friend void dma_ch4_isr(void);
    friend void dma_ch5_isr(void);
    friend void dma_ch6_isr(void);
    friend void dma_ch7_isr(void);
    friend void dma_ch8_isr(void);
    friend void dma_ch9_isr(void);
    friend void dma_ch10_isr(void);
    friend void dma_ch11_isr(void);
    friend void dma_ch12_isr(void);
    friend void dma_ch13_isr(void);
    friend void dma_ch14_isr(void);
    friend void dma_ch15_isr(void);
    friend void dma_error_isr(void);

    public:
        enum error_e
        {
            DBE = (1 << 0),  // Destination Bus Error - bus error on a destination write
            SBE = (1 << 1),  // Source Bus Error - bus error on a source read
            SGE = (1 << 2),  // DLASTSGA if ESG enabled - scatter/gather address not on a 32 byte boundary
            NCE = (1 << 3),  // NBYTES or CITER
            // * NBYTES not a multiple of SSIZE and DSIZE
            // * CITER is equal to zero
            // * CITER:ELINK is not equal to BITER:ELINK
            DOE = (1 << 4),  // Destination Offset Error - DOFF is inconsistent with DSIZE
            DAE = (1 << 5),  // Destination Address Error - DADDR is inconsistent with DSIZE
            SOE = (1 << 6),  // Source Offset Error - SOFF is inconsistent with SSIZE
            SAE = (1 << 7),  // Source Address Error - SADDR inconsistent with SSIZE
            CPE = (1 << 14), // Channel Priority Error - configuration error
            ECX = (1 << 16), // Transfer Cancelled by error cancel transfer input
            ERR = (1 << 31), // Logical OR of all ERR status bits - at least one ERR bit is set
        };

        class Channel;

        class Isr
        {
            friend class DMA;

            protected:
                Isr(void) = default;

                virtual void isr(Channel & ch) = 0;
        };

        class Channel
        {
            friend class DMA;

            public:
                // DMA hardware request sources
                enum src_e : uint8_t
                {
                    //CHANNEL_DISABLED = 0,
                    UART0_RX = 2, UART0_TX, UART1_RX, UART1_TX, UART2_RX, UART2_TX,
                    I2S0_RX = 14, I2S0_TX,
                    SPI0_RX, SPI0_TX, SPI1_RX, SPI1_TX,
                    I2C0 = 22, I2C1,
                    FTM0_CH0, FTM0_CH1, FTM0_CH2, FTM0_CH3, FTM0_CH4, FTM0_CH5, FTM0_CH6, FTM0_CH7,
                    FTM1_CH0, FTM1_CH1, FTM2_CH0, FTM2_CH1,
                    ADC0 = 40, ADC1,
                    CMP0, CMP1, CMP2,
                    DAC0,
                    CMT = 47,
                    PDB,
                    PCM_PORTA, PCM_PORTB, PCM_PORTC, PCM_PORTD, PCM_PORTE,
                    ALWAYS_ENABLED_0, ALWAYS_ENABLED_1, ALWAYS_ENABLED_2, ALWAYS_ENABLED_3,
                    ALWAYS_ENABLED_4, ALWAYS_ENABLED_5, ALWAYS_ENABLED_6, ALWAYS_ENABLED_7,
                    ALWAYS_ENABLED_8, ALWAYS_ENABLED_9,
                };

                void preemptable(bool enable = true);
                void preemptAbility(bool enable = true);
                bool priority(uint8_t priority);
                uint8_t priority(void) const { return *_dchpri & 0x0F; }

                void start(TCD const & tcd, src_e source, Isr * isr = nullptr);
                void update(TCD const & tcd) { memcpy((void *)_tcd, &tcd, sizeof(TCD)); }
                void resume(void) { *_erq = 1; }
                void stop(void);
                //bool running(void) { return *_hrs == 1; }

                uint8_t channel(void) const { return _ch_num; }

            private:
                Channel(uint8_t ch_num) : _ch_num(ch_num) {}

                static constexpr uint8_t const _s_max_priority = 15;

                uint8_t const _ch_num;
                Isr * _isr = nullptr;

                reg8 _chcfg = (reg8)(0x40021000 + _ch_num);

                // Channel Priority Register
                // The 8 bit registers are arranged in this order:
                // 3 2 1 0 | 7 6 5 4 | 11 10 9 8 | 15 14 13 12
                // The below is masking the most significant 2 bits with the channel number : 1100
                // then subtracting the mask of least significant 2 bits : 0011
                // Base register in calculation starts at channel 0
                reg8 _dchpri = (reg8)(0x40008103 + (_ch_num & 0x0C) - (_ch_num & 0x03));

                reg32 _erq = (reg32)BITBAND_ALIAS(0x4000800C, _ch_num);
                reg32 _eei = (reg32)BITBAND_ALIAS(0x40008014, _ch_num);
                reg32 _int = (reg32)BITBAND_ALIAS(0x40008024, _ch_num);
                reg32 _err = (reg32)BITBAND_ALIAS(0x4000802C, _ch_num);
                reg32 _hrs = (reg32)BITBAND_ALIAS(0x40008034, _ch_num);

                TCD volatile * const _tcd =
                    reinterpret_cast < TCD volatile * const > (0x40009000 + (32 * _ch_num));

                uint32_t _error = 0;
        };

        static Channel * acquire(bool pit_channel = false);
        static void release(Channel * ch);
        static uint8_t available(bool pit_channels = false);

        void cancel(bool clear_ecx = false) { clear_ecx ? *_s_cr |= DMA_CR_ECX : *_s_cr |= DMA_CR_CX; }
        void enableMinorLoopMapping(bool enable = true) { enable ? *_s_cr |= DMA_CR_EMLM : *_s_cr &= ~DMA_CR_EMLM; }
        void continuousLinkMode(bool enable = true) { enable ? *_s_cr |= DMA_CR_CLM : *_s_cr &= ~DMA_CR_CLM; }
        void halt(void) { *_s_cr |= DMA_CR_HALT; }
        void resume(void) { *_s_cr &= ~DMA_CR_HALT; }
        void haltOnError(bool enable = true) { enable ? *_s_cr |= DMA_CR_HOE : *_s_cr &= ~DMA_CR_HOE; }

        // Arbitration types
        enum arb_e { FIXED_PRIORITY, ROUND_ROBIN };

        void arbitration(arb_e arb) { (arb == FIXED_PRIORITY) ? *_s_cr &= ~DMA_CR_ERCA : *_s_cr |= DMA_CR_ERCA; }

    private:
        DMA(void) = default;

        static void errorIsr(void);

        template < uint8_t CH > __attribute__ ((always_inline))
        static void chIsr(void)
        {
            *_s_cint = CH;
            //*_s_cdne = CH;

            if ((_s_available & (1 << CH)) || (_s_channels[CH]._isr == nullptr))
                return;

            _s_channels[CH]._isr->isr(_s_channels[CH]);
        }

        static constexpr uint8_t const _s_num_channels = 16;
        static constexpr uint32_t const _s_pit_mask = 0x0000000F;
        static constexpr uint32_t const _s_max_available = 0x0000FFFF;
        static constexpr uint32_t const _s_error_mask = 0x000140FF;  // Masks out VLD and ERRCHN

        static uint32_t _s_available;
        static Channel _s_channels[_s_num_channels];

        static reg8 _s_base_reg;
        static reg32 _s_cr;
        static reg32 _s_es;
        static reg32 _s_erq;
        //static reg32 _s_eei;
        static reg8 _s_ceei;
        static reg8 _s_seei;
        static reg8 _s_cerq;
        static reg8 _s_serq;
        static reg8 _s_cdne;
        static reg8 _s_ssrt;
        static reg8 _s_cerr;
        static reg8 _s_cint;
        //static reg32 _s_intr;
        //static reg32 _s_err;
        //static reg32 _s_hrs;
};

#endif
