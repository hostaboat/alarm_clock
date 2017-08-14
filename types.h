#ifndef _TYPES_H_
#define _TYPES_H_

#include <cstdint>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

using v8 = u8 volatile;
using v16 = u16 volatile;
using v32 = u32 volatile;

using reg8 = v8 * const;
using reg16 = v16 * const;
using reg32 = v32 * const;

using pin_t = uint8_t;

#endif
