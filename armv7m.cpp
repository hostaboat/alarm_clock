#include "armv7m.h"
#include "types.h"

////////////////////////////////////////////////////////////////////////////////
// System Control Space (SCS) //////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// System Control Block (SCB) //////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// B3.2.2 System control and ID registers
reg32 SCB::_s_base = (reg32)0xE000ED00;
const reg32 SCB::_s_cpuid = (const reg32)_s_base;
reg32 SCB::_s_icsr  = _s_base +  1;
reg32 SCB::_s_vtor  = _s_base +  2;
reg32 SCB::_s_aircr = _s_base +  3;
reg32 SCB::_s_scr   = _s_base +  4;
reg32 SCB::_s_ccr   = _s_base +  5;
reg32 SCB::_s_shpr1 = _s_base +  6;
reg32 SCB::_s_shpr2 = _s_base +  7;
reg32 SCB::_s_shpr3 = _s_base +  8;
reg8 SCB::_s_shpr_base = (reg8)_s_shpr1;  // For ease of indexing
reg32 SCB::_s_shcsr = _s_base +  9;
reg32 SCB::_s_cfsr  = _s_base + 10;
reg32 SCB::_s_hfsr  = _s_base + 11;
reg32 SCB::_s_dfsr  = _s_base + 12;
reg32 SCB::_s_mmfar = _s_base + 13;
reg32 SCB::_s_bfar  = _s_base + 14;
reg32 SCB::_s_afsr  = _s_base + 15;
// 0xE000ED40 - 0xE000ED84  // Reserved for CPUID registers
reg32 SCB::_s_cpacr = _s_base + 34;
// 0xE0000ED8C  // Reserved
// Additional SCB registers for the FP extension
reg32 SCB::_s_fp_base = (reg32)0xE000EF34;
reg32 SCB::_s_fpccr  = _s_fp_base;
reg32 SCB::_s_fpcar  = _s_fp_base + 1;
reg32 SCB::_s_fpdscr = _s_fp_base + 2;
const reg32 SCB::_s_mvfr0  = (const reg32)(_s_fp_base + 3);
const reg32 SCB::_s_mvfr1  = (const reg32)(_s_fp_base + 4);
const reg32 SCB::_s_mvfr2  = (const reg32)(_s_fp_base + 5);

// B1 System Level Programmers’ Model
// B1.5 ARMv7-M exception model / ARM DDI 0403E.b ID120114
// ExecutionPriority()  B1-584
// ===================
// Determine the current execution priority
int SCB::executionPriority(void)
{
    int highestpri = 256; // Priority of Thread mode with no active exceptions
                          // The value is PriorityMax + 1 = 256
                          // (configurable priority maximum bit field is 8 bits)
    int boostedpri = 256; // Priority influence of BASEPRI, PRIMASK and FAULTMASK

    // B3.2.6 - B3-658
    int subgroupshift = (*SCB::_s_aircr >> 8) & 0x07;
    int groupvalue = 0x02 << subgroupshift; // Used by priority grouping
    // PRIGROUP    0    1    2    3    4    5    6    7
    // groupvalue  2    4    8   16   32   64  128  256
    //
    // PRIGROUP   Group priority    Subpriority
    //    0           [7:1]             [0]
    //    1           [7:2]            [1:0]
    //    2           [7:3]            [2:0]
    //    3           [7:4]            [3:0]
    //    4           [7:5]            [4:0]
    //    5           [7:6]            [5:0]
    //    6            [7]             [6:0]
    //    7             -              [7:0]

    uint32_t ipsr, basepri, primask, faultmask;

    __asm__ volatile ("mrs %0, ipsr" : "=r" (ipsr));
    if (ipsr != 0)
    {
        int exceptpri;

        if (ipsr < 16)
            exceptpri = getPriority((except_e)ipsr);
        else
            exceptpri = NVIC::getPriority((irq_e)(ipsr - 16));

        if (exceptpri < highestpri)
        {
            highestpri = exceptpri;

            // The group priorities of Reset, NMI and HardFault are -3, -2 and -1
            // respectively, regardless of the value of PRIGROUP.
            if (ipsr > 3)
            {
                // Include the PRIGROUP effect
                int subgroupvalue = highestpri % groupvalue;
                highestpri -= subgroupvalue;
            }
        }
    }

    __asm__ volatile ("mrs %0, basepri" : "=r" (basepri));
    if (basepri != 0)
    {
        boostedpri = basepri;

        // Include the PRIGROUP effect
        int subgroupvalue = boostedpri % groupvalue;
        boostedpri -= subgroupvalue;
    }

    __asm__ volatile ("mrs %0, primask" : "=r" (primask));
    if (primask == 1)
        boostedpri = 0;

    __asm__ volatile ("mrs %0, faultmask" : "=r" (faultmask));
    if (faultmask == 1)
        boostedpri = -1;

    if (boostedpri < highestpri)
        return boostedpri;

    return highestpri;
}

////////////////////////////////////////////////////////////////////////////////
// The system timer (SysTick) //////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// B3.3.2 System timer register support in the SCS
reg32 SysTick::_s_base = (reg32)0xE000E010;
reg32 SysTick::_s_csr   = _s_base;
reg32 SysTick::_s_rvr   = _s_base + 1;
reg32 SysTick::_s_cvr   = _s_base + 2;
reg32 SysTick::_s_calib = _s_base + 3;
// 0xE000E020 - 0xE000E0FC  // Reserved

v32 SysTick::_intervals = 0;

void systick_isr(void)
{
    SysTick::_intervals++;
}

////////////////////////////////////////////////////////////////////////////////
// Nested Vectored Interrupt Controller (NVIC) /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// B3.4.3 NVIC register support in the SCS
reg32 NVIC::_s_base = (reg32)0xE000E100;
reg32 NVIC::_s_iser = _s_base;
reg32 NVIC::_s_icer = _s_base +  32;
reg32 NVIC::_s_ispr = _s_base +  64;
reg32 NVIC::_s_icpr = _s_base +  96;
reg32 NVIC::_s_iabr = _s_base + 128;
reg8  NVIC::_s_ipr = (reg8)(_s_base + 192);


////////////////////////////////////////////////////////////////////////////////
// Debug ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// C1.6 Debug system registers
reg32 Debug::_s_base = (reg32)0xE000EDF0;
reg32 Debug::_s_dhcsr = _s_base;
reg32 Debug::_s_dcrsr = _s_base + 1;
reg32 Debug::_s_dcrdr = _s_base + 2;
reg32 Debug::_s_demcr = _s_base + 3;

// C1.8.6 DWT register summary
reg32 DWT::_s_base = (reg32)0xE0001000;
reg32 DWT::_s_ctrl     = _s_base;
reg32 DWT::_s_cyccnt   = _s_base + 1;
reg32 DWT::_s_cpicnt   = _s_base + 2;
reg32 DWT::_s_exccnt   = _s_base + 3;
reg32 DWT::_s_sleepcnt = _s_base + 4;
reg32 DWT::_s_lsucnt   = _s_base + 5;
reg32 DWT::_s_foldcnt  = _s_base + 6;
reg32 DWT::_s_pcsr     = _s_base + 7;

