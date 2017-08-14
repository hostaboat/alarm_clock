#include "tsi.h"
#include "types.h"

reg32 Tsi::_s_conf_base = (reg32)0x40045000;
reg32 Tsi::_s_gencs  = _s_conf_base;
reg32 Tsi::_s_scanc  = _s_conf_base +  1;
reg32 Tsi::_s_pen    = _s_conf_base +  2;
reg32 Tsi::_s_wucntr = _s_conf_base +  3;
reg32 Tsi::_s_cntr_base  = (reg32)0x40045100;
reg16 Tsi::_s_cntr = (reg16)_s_cntr_base;
//reg32 Tsi::_s_cntr1  = _s_cntr_base;
//reg32 Tsi::_s_cntr3  = _s_cntr_base + 1;
//reg32 Tsi::_s_cntr5  = _s_cntr_base + 2;
//reg32 Tsi::_s_cntr7  = _s_cntr_base + 3;
//reg32 Tsi::_s_cntr9  = _s_cntr_base + 4;
//reg32 Tsi::_s_cntr11 = _s_cntr_base + 5;
//reg32 Tsi::_s_cntr13 = _s_cntr_base + 6;
//reg32 Tsi::_s_cntr15 = _s_cntr_base + 7;
reg32 Tsi::_s_threshold = _s_cntr_base + 8;

