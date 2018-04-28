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

using chr_t = uint8_t;
using wchr_t = uint16_t;  // Expects one 16-bit code unit in UTF-16LE

// Pointer check
template < typename T >
struct is_pointer { static const bool value = false; };

template < typename T >
struct is_pointer < T * > { static const bool value = true; };

template < typename T >
struct is_pointer < T * const > { static const bool value = true; };

template < typename T >
struct is_pointer < T * volatile > { static const bool value = true; };

// Pointer type
template < typename T >
struct pointer_type { };

template < typename T >
struct pointer_type < T * > { typedef T type; };

template < typename T >
struct pointer_type < T * const > { typedef T type; };

template < typename T >
struct pointer_type < T * volatile > { typedef T type; };

// Enable if
template < bool E, typename T = void >
struct enable_if { };

template < typename T >
struct enable_if < true, T > { typedef T type; };

// Register class
template < typename REG, typename = typename enable_if < is_pointer < REG >::value >::type >
class Reg
{
    using RT = typename pointer_type < REG >::type;

    public:
        Reg(REG reg, bool clear = false) : _reg(*reg) { if (clear) _reg = 0; }
        Reg(RT & reg, bool clear = false) : _reg(reg) { if (clear) _reg = 0; }

        template < uint8_t OFFSET, uint8_t BITS >
        constexpr void validate(void)
        {
            static_assert((BITS != 0) && ((OFFSET + BITS) <= (sizeof(RT) * 8)), "Reg Invalid");
        }

        template < uint8_t OFFSET, uint8_t BITS >
        constexpr RT mask(void) { return ((1 << BITS) - 1) << OFFSET; };

        constexpr void set(RT val) { _reg = val; }

        template < uint8_t OFFSET, uint8_t BITS >
        constexpr void set(RT val)
        {
            validate < OFFSET, BITS > (); clear < OFFSET, BITS > ();
            _reg |= (val << OFFSET) & mask < OFFSET, BITS > ();
        }

        constexpr RT get(void) { return _reg; }

        template < uint8_t OFFSET, uint8_t BITS >
        constexpr RT get(void)
        {
            validate < OFFSET, BITS > ();
            return (_reg & mask < OFFSET, BITS > ()) >> OFFSET;
        }

        template < uint8_t OFFSET, uint8_t BITS >
        constexpr bool isSet(RT val)
        {
            validate < OFFSET, BITS > ();
            return get < OFFSET, BITS > () == val;
        }

        constexpr void clear(void) { _reg = 0; }

        template < uint8_t OFFSET, uint8_t BITS >
        constexpr void clear(void)
        {
            validate < OFFSET, BITS > ();
            _reg &= ~mask < OFFSET, BITS > ();
        }

        template < uint8_t OFFSET >
        constexpr void set(void)
        {
            validate < OFFSET, 1 > ();
            _reg |= (1 << OFFSET);
        }

        template < uint8_t OFFSET >
        constexpr bool isSet(void)
        {
            validate < OFFSET, 1 > ();
            return _reg & (1 << OFFSET);
        }

        template < uint8_t OFFSET >
        constexpr bool isClear(void)
        {
            validate < OFFSET, 1 > ();
            return !(_reg & (1 << OFFSET));
        }

        template < uint8_t OFFSET >
        constexpr void clear(void)
        {
            validate < OFFSET, 1 > ();
            _reg &= ~(1 << OFFSET);
        }

        RT operator |(RT val) { return _reg | val; }
        RT operator |(Reg const & reg) { return *this | reg._reg; }

        Reg & operator |=(RT val) { _reg |= val; return *this; }
        Reg & operator |=(Reg const & reg) { *this |= reg._reg; return *this; }

        RT operator &(RT val) { return _reg & val; }
        RT operator &(Reg const & reg) { return *this & reg._reg; }

        Reg & operator &=(RT val) { _reg &= val; return *this; }
        Reg & operator &=(Reg const & reg) { *this &= reg._reg; return *this; }

        RT operator ~(void) { return ~_reg; }

        RT operator ^(RT val) { return _reg ^ val; }
        RT operator ^(Reg const & reg) { return *this ^ reg._reg; }

        Reg & operator ^=(RT val) { _reg ^= val; return *this; }
        Reg & operator ^=(Reg const & reg) { *this ^= reg._reg; return *this; }

        // Don't enable this - look into a safe bool solution
        //operator bool(void) { return _reg != 0; }

        Reg & operator =(RT val) { _reg = val; return *this; }
        Reg & operator =(Reg const & reg) { *this = reg._reg; return *this; }

        bool operator ==(RT val) { return _reg == val; }
        bool operator ==(Reg const & reg) { return *this == reg._reg; }

        bool operator !=(RT val) { return !(*this == val); }
        bool operator !=(Reg const & reg) { return !(*this == reg); }

        RT operator >>(RT val) { return _reg >> val; }
        RT operator >>(Reg const & reg) { return *this >> reg._reg; }

        Reg & operator >>=(RT val) { _reg >>= val; return *this; }
        Reg & operator >>=(Reg const & reg) { *this >>= reg._reg; return *this; }

        RT operator <<(RT val) { return _reg << val; }
        RT operator <<(Reg const & reg) { return *this << reg._reg; }

        Reg & operator <<=(RT val) { _reg <<= val; return *this; }
        Reg & operator <<=(Reg const & reg) { *this <<= reg._reg; return *this; }

        bool operator <(RT val) { return _reg < val; }
        bool operator <(Reg const & reg) { return *this < reg._reg; }

        bool operator <=(RT val) { return _reg <= val; }
        bool operator <=(Reg const & reg) { return *this <= reg._reg; }

        bool operator >(RT val) { return _reg > val; }
        bool operator >(Reg const & reg) { return *this > reg._reg; }

        bool operator >=(RT val) { return _reg >= val; }
        bool operator >=(Reg const & reg) { return _reg >= reg._reg; }

        bool operator ||(RT val) { return _reg || val; }
        bool operator ||(Reg const & reg) { return *this || reg._reg; }

        bool operator &&(RT val) { return _reg && val; }
        bool operator &&(Reg const & reg) { return *this && reg._reg; }

    private:
        Reg(void) {}
        RT & _reg;
};

#endif
