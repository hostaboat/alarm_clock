#ifndef _UTILITY_H_
#define _UTILITY_H_

#include "types.h"
#include "armv7m.h"

// Member Function Call
#define MFC(member_func) ((*this).*(member_func))

// Sets the PRIMASK to 1, raising the execution priority to 0
// effectively making the current executing handler uninterruptable
// except by Reset, NMI and HardFault exceptions.
#define __disable_irq() __asm__ volatile ("CPSID i":::"memory");

// Sets PRIMASK to 0, thus setting execution priority back to what
// it is configured as.
#define __enable_irq()  __asm__ volatile ("CPSIE i":::"memory");

inline uint32_t msecs(void)
{
    return SysTick::intervals();
}

inline uint32_t usecs(void)
{
    __disable_irq();

    uint32_t ms = SysTick::intervals();  // Won't increment while interrupts disabled
    uint32_t cv = SysTick::currentValue();  // Read this before reading pending
    bool pending = SCB::sysTickIntrPending();

    __enable_irq();

    // If there is an interrupt pending then CVR reached zero before reading CVR,
    // CVR was zero when reading CVR or CVR reached zero after reading CVR. Only
    // off by a couple of ticks in any case (including the below case where there
    // is no pending interrupt) so just add an extra millisecond.
    if (pending) return (ms + 1) * 1000;

    return (ms * 1000) + ((TICKS_PER_MSEC - cv) / TICKS_PER_USEC);
}

inline void delay_msecs(uint32_t ms)
{
    if (ms == 0)
        return;

    uint32_t us = usecs();

    while (ms > 0)
    {
        if ((usecs() - us) >= 1000)
        {
            ms--;
            us += 1000;
        }
    }
}

inline void delay_usecs(uint32_t us)
{
    us *= F_CPU / 2000000;  // 1000000 - 3000000
    if (us == 0)
        return;

    __asm__ volatile
        (
         "L_%=_delay_usecs:\n\t"
         "subs %0, #1\n\t"
         "bne L_%=_delay_usecs\n\t" : "+r" (us)
        );
}

inline uint16_t ntohs(uint16_t val) { return ((val & 0x00FF) << 8) | ((val & 0xFF00) >> 8); }
inline uint16_t htons(uint16_t val) { return ntohs(val); }

inline uint32_t ntohl(uint32_t val)
{
    return ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) <<  8) |
           ((val & 0x00FF0000) >>  8) | ((val & 0xFF000000) >> 24);
}
inline uint32_t htonl(uint32_t val) { return ntohl(val); }

// Set bit
template < typename T, typename = typename enable_if < !is_pointer < T >::value >::type >
inline void setBit(T & a, uint8_t offset) { a |= (1 << offset); }

template < typename T, typename = typename enable_if < is_pointer < T >::value >::type >
inline void setBit(T a, uint8_t offset) { *a |= (1 << offset); }

// Clear bit
template < typename T, typename = typename enable_if < !is_pointer < T >::value >::type >
inline void clearBit(T & a, uint8_t offset) { a &= ~(1 << offset); }

template < typename T, typename = typename enable_if < is_pointer < T >::value >::type >
inline void clearBit(T a, uint8_t offset) { *a &= ~(1 << offset); }

// Check bit
template < typename T, typename = typename enable_if < !is_pointer < T >::value >::type >
inline bool checkBit(T const & a, uint8_t offset) { return (a & (1 << offset)); }

template < typename T, typename = typename enable_if < is_pointer < T >::value >::type >
inline bool checkBit(T const a, uint8_t offset) { return (*a & (1 << offset)); }

// Set bits
template < typename T, typename = typename enable_if < !is_pointer < T >::value >::type >
inline void setBits(T & a, T val, uint8_t offset) { a |= (val << offset); }

template < typename T, typename = typename enable_if < is_pointer < T >::value >::type >
inline void setBits(T a, typename pointer_type < T >::type val, uint8_t offset)
{
    *a |= (val << offset);
}

// Clear bits
template < typename T, typename = typename enable_if < !is_pointer < T >::value >::type >
inline void clearBits(T & a, T val, uint8_t offset) { a &= ~(val << offset); }

template < typename T, typename = typename enable_if < is_pointer < T >::value >::type >
inline void clearBits(T a, typename pointer_type < T >::type val, uint8_t offset)
{
    *a &= ~(val << offset);
}

// Check bits
template < typename T, typename = typename enable_if < !is_pointer < T >::value >::type >
inline bool checkBits(T const & a, T val, uint8_t offset) { return ((a & (val << offset)) >> offset) == val; }

template < typename T, typename = typename enable_if < is_pointer < T >::value >::type >
inline bool checkBits(T a, typename pointer_type < T >::type val, uint8_t offset)
{
    return ((*a & (val << offset)) >> offset) == val;
}

// Taken from FastLED code - see opti.* for license
template < uint16_t WAIT >
class UsMinWait
{
    public:
        UsMinWait(void) {}
        void wait(void) { while (((usecs() & 0xFFFF) - _last_micros) < WAIT); }
        void mark(void) { _last_micros = usecs() & 0xFFFF; }

    private:
        uint16_t _last_micros = 0;
};

class Toggle
{
    public:
        Toggle(void) { reset(); }
        Toggle(uint32_t time) { reset(time); }
        Toggle(uint32_t on_time, uint32_t off_time) { reset(on_time, off_time); }

        virtual bool toggled(void)
        {
            if (_disabled)
                return false;

            uint32_t cm = msecs();

            if ((cm - _last_msec) < _toggle_time)
                return false;

            _on = !_on;
            _toggle_time = _on ? _on_time : _off_time;
            _last_msec = cm;

            return true;
        }

        virtual bool on(void) const { return (_disabled ? false : _on); }
        virtual bool off(void) const { return (_disabled ? true : !_on); }

        virtual uint32_t onTime(void) const { return _on_time; }
        virtual uint32_t offTime(void) const { return _off_time; }

        virtual void disable(void) { _disabled = true; }
        virtual void enable(void) { _disabled = false; }
        virtual bool disabled(void) const { return _disabled; }

        virtual void reset(void) { reset(_on_time, _off_time); }
        virtual void reset(uint32_t time) { reset(time, time); }

        virtual void reset(uint32_t on_time, uint32_t off_time)
        {
            _on_time = on_time;
            _off_time = off_time;
            _toggle_time = _on_time;
            _last_msec = msecs();
            _on = true;
            _disabled = false;
        }

    protected:
        uint32_t _toggle_time = 1000;  // Arbitrary 1 second
        uint32_t _on_time = _toggle_time;
        uint32_t _off_time = _toggle_time;

    private:
        uint32_t _last_msec = 0;
        bool _on = true;
        bool _disabled = false;
};

template < typename T, int S >
class Queue
{
    static_assert(S > 0, "Invalid Queue size - must be > 0");

    public:
        // Adding and removing an element
        bool enqueue(T const & t)
        {
            if (isFull()) return false;
            _enqueue(t);
            return true;
        }

        bool dequeue(T & t)
        {
            if (isEmpty()) return false;
            _dequeue(t);
            return true;
        }

        // Direct access to first and last elements
        bool head(T & t)
        {
            if (isEmpty()) return false;
            _get(_head, t);
            return true;
        }

        bool tail(T & t)
        {
            if (isEmpty()) return false;
            _get(_tail, t);
            return true;
        }

        // For iterating through elements
        bool first(T & t)
        {
            if (isEmpty()) return false;
            _begin(t);
            return true;
        }

        bool next(T & t)
        {
            if (isEmpty() || (_curr == _tail)) return false;
            _iter(t);
            return true;
        }

        // Checks on status of queue
        bool isEmpty(void) const { return _count == 0; }
        bool isFull(void) const { return _count == S; }
        int count(void) const { return _count; }
        int available(void) const { return S - _count; }

    private:
        void _advance(int & i) { i = ((i + 1) == S) ? 0 : i + 1; }
        void _get(int i, T & t) { t = _queue[i]; }
        void _set(int i, T const & t) { _queue[i] = t; }
        void _enqueue(T const & t) { _advance(_tail); _set(_tail, t); _count++; }
        void _dequeue(T & t) { _get(_head, t); _advance(_head); _count--; }
        void _begin(T & t) { _curr = _head; _get(_curr, t); }
        void _iter(T & t) { _advance(_curr); _get(_curr, t); }

        int _head = 0;
        int _tail = -1;
        int _curr = 0;
        int _count = 0;
        T _queue[S];
};

#endif

