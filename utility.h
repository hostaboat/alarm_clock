#ifndef _UTILITY_H_
#define _UTILITY_H_

#include "types.h"
#include "armv7m.h"
#include <string.h>

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

inline constexpr uint32_t BITBAND_ALIAS(uint32_t addr, uint8_t bit)
{
    return ((addr - 0x40000000) * (8 * 4)) + (bit * 4) + 0x42000000;
}

inline uint16_t ntohs(uint16_t val) { return ((val & 0x00FF) << 8) | ((val & 0xFF00) >> 8); }
inline uint16_t htons(uint16_t val) { return ntohs(val); }

inline uint32_t ntohl(uint32_t val)
{
    return ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) <<  8) |
           ((val & 0x00FF0000) >>  8) | ((val & 0xFF000000) >> 24);
}
inline uint32_t htonl(uint32_t val) { return ntohl(val); }

inline uint8_t clz8(uint8_t x)  { return (x == 0) ?  8 : __builtin_clz(x) - 24; }
inline uint8_t clz16(uint8_t x) { return (x == 0) ? 16 : __builtin_clz(x) - 16; }
inline uint8_t clz32(uint8_t x) { return (x == 0) ? 32 : __builtin_clz(x); }

inline uint8_t ctz8(uint8_t x)   { return (x == 0) ?  8 : __builtin_ctz(x); }
inline uint8_t ctz16(uint8_t x)  { return (x == 0) ? 16 : __builtin_ctz(x); }
inline uint8_t ctz32(uint8_t x)  { return (x == 0) ? 32 : __builtin_ctz(x); }

inline int ceiling(int divisor, int dividend)
{
    if ((divisor % dividend) == 0)
        return divisor / dividend;
    else
        return (divisor / dividend) + 1;
}

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

template < typename C > inline bool cmpEQ(C & c1, C & c2)  { return c1 == c2; }
template < typename C > inline bool cmpEQ(C * c1, C * c2)  { return cmpEQ(*c1, *c2); }

template < typename C > inline bool cmpNEQ(C & c1, C & c2) { return !cmpEQ(c1, c2); }
template < typename C > inline bool cmpNEQ(C * c1, C * c2) { return cmpNEQ(*c1, *c2); }

template < typename C > inline bool cmpLT(C & c1, C & c2)  { return c1 < c2; }
template < typename C > inline bool cmpLT(C * c1, C * c2)  { return cmpLT(*c1, *c2); }

template < typename C > inline bool cmpLTE(C & c1, C & c2) { return cmpLT(c1, c2) || cmpEQ(c1, c2); }
template < typename C > inline bool cmpLTE(C * c1, C * c2) { return cmpLTE(*c1, *c2); }

template < typename C > inline bool cmpGT(C & c1, C & c2)  { return cmpLT(c2, c1); }
template < typename C > inline bool cmpGT(C * c1, C * c2)  { return cmpGT(*c1, *c2); }

template < typename C > inline bool cmpGTE(C & c1, C & c2) { return cmpLTE(c2, c1); }
template < typename C > inline bool cmpGTE(C * c1, C * c2) { return cmpGTE(*c1, *c2); }

template < typename T >
void quicksort(T * A, int lo, int hi)
{
    if (A == nullptr)
        return;

    auto partition = [](T * A, int lo, int hi) __attribute__ ((noinline)) -> int
    {
        T pivot = A[lo];
        int i = lo;
        int j = hi;

        while (true)
        {
            while ((j > i) && cmpLT(pivot, A[j]))
                j--;

            if (j <= i)
                break;

            A[i++] = A[j];

            while ((i < j) && cmpGT(pivot, A[i]))
                i++;

            if (i >= j)
                break;

            A[j--] = A[i];
        }

        if (i != lo)
            A[i] = pivot;

        return i;
    };

    while (lo < hi)
    {
        int p = partition(A, lo, hi);

        if ((p - lo) < (hi - p))
        {
            if (lo < (p - 1))
                quicksort(A, lo, p - 1);

            lo = p + 1;
        }
        else
        {
            if ((p + 1) < hi)
                quicksort(A, p + 1, hi);

            hi = p - 1;
        }
    }
}

template < typename T >
void shellsort(T * A, int n)
{
    int h = 1;
    while (h < (n / 3))
        h = (3 * h) + 1;

    while (h > 0)
    {
        for (int i = h; i < n; i++)
        {
            T tmp = A[i];

            int j = i;
            for (; (j >= h) && (tmp < A[j-h]); j -= h)
                A[j] = A[j-h];

            A[j] = tmp;
        }

        h /= 3;
    }
}

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

template < typename T, int S >
class MinHeap
{
    static_assert(S > 0, "Invalid MinHeap size");

    public:
        MinHeap(void) = default;

        bool isEmpty(void) const { return _count == 0; }
        bool isFull(void) const { return _count == S; }
        uint16_t count(void) const { return _count; }

        bool peek(T & t)
        {
            if (isEmpty())
                return false;
            t = _nodes[1];
            return true;
        }

        bool insert(T const & t)
        {
            if (isFull())
                return false;

            _nodes[++_count] = t;

            int child = _count;
            int parent = child / 2;

            while ((parent != 0) && cmpLT(_nodes[child], _nodes[parent]))
            {
                swap(child, parent);
                child = parent;
                parent /= 2;
            }

            return true;
        }

        bool remove(T & t)
        {
            if (isEmpty() || !peek(t))
                return false;
            _nodes[1] = _nodes[_count--];
            heapify();
            return true;
        }

        void heapify(void)
        {
            int parent = 1;

            while ((parent * 2) <= _count)
            {
                int s0 = parent * 2;
                int s1 = s0 + 1;
                int sibling = ((s0 == _count) || cmpLT(_nodes[s0], _nodes[s1])) ? s0 : s1;

                if (cmpLTE(_nodes[parent], _nodes[sibling]))
                    break;

                swap(sibling, parent);
                parent = sibling;
            }
        }

    private:
        void swap(int i, int j)
        {
            T tmp = _nodes[i];
            _nodes[i] = _nodes[j];
            _nodes[j] = tmp;
        }

        int _count = 0;
        T _nodes[S + 1];
};

template < uint16_t BSIZE >
class ProducerConsumer
{
    public:
        ProducerConsumer(void) = default;

        virtual uint16_t produceLen(uint16_t len = BSIZE) const final;
        virtual uint16_t consumeLen(uint16_t len = BSIZE) const final;

        virtual bool canProduce(uint16_t len) const final { return produceLen(len) == len; }
        virtual bool canConsume(uint16_t len) const final { return consumeLen(len) == len; }

        virtual uint16_t produce(uint8_t * data, uint16_t dlen, bool allow_trunc = false) final;
        virtual uint16_t consume(uint8_t * buf, uint16_t blen, bool allow_trunc = false) final;

        virtual uint32_t consumed(void) const final { return _consumed; }
        virtual uint32_t produced(void) const final { return _produced; }

        virtual bool consumeDone(void) const final { return _consumed == _total; }
        virtual bool produceDone(void) const final { return _produced == _total; }

    protected:
        uint8_t _buffer[BSIZE];
        uint32_t volatile _produced = 0;
        uint32_t volatile _consumed = 0;
        uint32_t volatile _total = 0;
};

template < uint16_t BSIZE >
uint16_t ProducerConsumer < BSIZE >::produceLen(uint16_t len) const
{
    if (produceDone())
        return 0;

    if (len > BSIZE)
        len = BSIZE;

    if ((_produced + len) > _total)
        len = _total - _produced;

    if (_produced > _consumed)
    {
        uint16_t poff = _produced % BSIZE;
        uint16_t coff = _consumed % BSIZE;

        if (poff == coff)
            len = 0;
        else if ((poff < coff) && ((poff + len) > coff))
            len = coff - poff;
        else if (((poff + len) > BSIZE) && (coff < ((poff + len) - BSIZE)))
            len = (BSIZE - poff) + coff;
    }

    return len;
}

template < uint16_t BSIZE >
uint16_t ProducerConsumer < BSIZE >::consumeLen(uint16_t len) const
{
    if (consumeDone() || (_consumed == _produced))
        return 0;

    if (len > BSIZE)
        len = BSIZE;

    if ((_consumed + len) > _total)
        len = _total - _consumed;

    uint16_t poff = _produced % BSIZE;
    uint16_t coff = _consumed % BSIZE;

    if ((coff < poff) && ((coff + len) > poff))
        len = poff - coff;
    else if (((coff + len) > BSIZE) && (poff < ((coff + len) - BSIZE)))
        len = (BSIZE - coff) + poff;

    return len;
}

template < uint16_t BSIZE >
uint16_t ProducerConsumer < BSIZE >::produce(uint8_t * data, uint16_t dlen, bool allow_trunc)
{
    uint16_t len = produceLen(dlen);

    if ((len == 0) || ((len != dlen) && !allow_trunc))
        return 0;

    uint16_t poff = _produced % BSIZE;
    uint16_t copied = len;

    if ((poff + len) > BSIZE)
    {
        uint16_t cpy = BSIZE - poff;
        memcpy(_buffer + poff, data, cpy);

        _produced += cpy;
        data += cpy;
        len -= cpy;
        poff = 0;
    }

    memcpy(_buffer + poff, data, len);
    _produced += len;

    return copied;
}

template < uint16_t BSIZE >
uint16_t ProducerConsumer < BSIZE >::consume(uint8_t * buf, uint16_t blen, bool allow_trunc)
{
    uint16_t len = consumeLen(blen);

    if ((len == 0) || ((len != blen) && !allow_trunc))
        return 0;

    uint16_t coff = _consumed % BSIZE;
    uint16_t copied = len;

    if ((coff + len) > BSIZE)
    {
        uint16_t cpy = BSIZE - coff;
        memcpy(buf, _buffer + coff, cpy);

        _consumed += cpy;
        buf += cpy;
        len -= cpy;
        coff = 0;
    }

    memcpy(buf, _buffer + coff, len);
    _consumed += len;

    return copied;
}

class Serializable
{
    public:
        virtual int serialize(uint8_t * buf, uint16_t blen) const = 0;
        virtual int deserialize(uint8_t const * buf, uint16_t blen) = 0;
};

inline chr_t const * itoa(int integer)
{
    // Max 10 digits + minus sign (if negative) + null byte
    static chr_t buf[12] = {};

    uint32_t ni = (integer < 0) ? -integer : integer;
    uint8_t i = sizeof(buf) - 2;

    while (true)
    {
        buf[i] = '0' + (ni % 10); 
        ni /= 10;

        if (ni == 0)
            break;

        i--;
    }

    if (integer < 0)
        buf[--i] = '-';

    return &buf[i];
}

static constexpr int const CASE_SPAN = 'a' - 'A';

inline int toupper(int c)
{
    return ((c >= 'a') && (c <= 'z')) ? c - CASE_SPAN : c;
}

inline int tolower(int c)
{
    return ((c >= 'A') && (c <= 'Z')) ? c + CASE_SPAN : c;
}

inline int chr_case(int c, bool cs = false)
{
    return cs ? c : toupper(c);
}

template < uint16_t S >
class String
{
    static_assert((S > 0) && (S < UINT16_MAX), "Invalid String size.");

    public:
        String(void) = default;

        String(chr_t c) { set(c); }

        String(chr_t const * str) { set(str); }
        String(chr_t const * str, uint16_t slen) { set(str, slen); }

        String(wchr_t const * str) { set(str); }
        String(wchr_t const * str, uint16_t slen) { set(str, slen); }

        String(String const & copy) { set(copy); }

        static constexpr uint16_t maxLen(void) { return S; }

        static uint16_t strLen(chr_t const * str) { return _strLen(str); }
        static uint16_t strLen(chr_t const * str, uint16_t slen) { return _strLen(str, slen); }
        static uint16_t strLen(wchr_t const * str) { return _strLen(str); }
        static uint16_t strLen(wchr_t const * str, uint16_t slen) { return _strLen(str, slen); }

        chr_t const * str(void) const { return _str; }
        uint16_t len(void) const { return _len; }
        uint16_t size(void) const { return maxLen(); }

        chr_t & operator [](uint16_t index) { return _get(index); }
        chr_t const & operator [](uint16_t index) const { return _get(index); }

        String & operator =(chr_t rval) { set(rval); return *this; }
        String & operator =(chr_t const * rval) { set(rval); return *this; }
        String & operator =(String const & rval) { if (this != &rval) set(rval); return *this; }

        String & operator +=(chr_t rhs) { postpend(rhs); return *this; }
        String & operator +=(chr_t const * rhs) { postpend(rhs); return *this; }
        String & operator +=(String const & rhs) { postpend(rhs); return *this; }

        friend String operator +(String const & lhs, String const & rhs)
        {
            String s(lhs);
            s += rhs;
            return s;
        }

        friend String operator +(String const & lhs, chr_t rhs)
        {
            String s(lhs);
            s += rhs;
            return s;
        }

        void clear(void) { _str[0] = 0; _len = 0; }

        void set(chr_t chr) { _set(chr); }
        void set(chr_t const * str) { _set(str); }
        void set(chr_t const * str, uint16_t slen) { _set(str, slen); }
        void set(wchr_t const * str) { _set(str); }
        void set(wchr_t const * str, uint16_t slen) { _set(str, slen); }
        void set(String const & str) { _set(str._str, str._len); }

        void insert(chr_t chr, int index) { _insert(chr, index); }
        void insert(chr_t const * str, int index) { _insert(str, index); }
        void insert(chr_t const * str, uint16_t slen, int index) { _insert(str, index, slen); }
        void insert(wchr_t const * str, int index) { _insert(str, index); }
        void insert(wchr_t const * str, uint16_t slen, int index) { _insert(str, index, slen); }
        void insert(String const & str, int index) { _insert(str._str, index, str._len); }

        void prepend(chr_t chr) { insert(chr, 0); }
        void prepend(chr_t const * str) { insert(str, 0); }
        void prepend(chr_t const * str, uint16_t slen) { insert(str, slen, 0); }
        void prepend(wchr_t const * str) { insert(str, 0); }
        void prepend(wchr_t const * str, uint16_t slen) { insert(str, slen, 0); }
        void prepend(String const & str) { insert(str, 0); }

        void postpend(chr_t chr) { insert(chr, _len); }
        void postpend(chr_t const * str) { insert(str, _len); }
        void postpend(chr_t const * str, uint16_t slen) { insert(str, slen, _len); }
        void postpend(wchr_t const * str) { insert(str, _len); }
        void postpend(wchr_t const * str, uint16_t slen) { insert(str, slen, _len); }
        void postpend(String const & str) { insert(str, _len); }

        int find(chr_t chr, int index = 0) const { return _find(chr, index); }
        int find(chr_t const * str, int index = 0) const { return _find(str, index); }
        int find(chr_t const * str, uint16_t slen, int index = 0) const { return _find(str, index, slen); }
        int find(wchr_t const * str, int index = 0) const { return _find(str, index); }
        int find(wchr_t const * str, uint16_t slen, int index = 0) const { return _find(str, index, slen); }
        int find(String const & str, int index = 0) const { return _find(str._str, index, str._len); }

        int rfind(chr_t chr, int index = maxLen()) const { return _rfind(chr, index); }
        int rfind(chr_t const * str, int index = maxLen()) const { return _rfind(str, index); }
        int rfind(chr_t const * str, uint16_t slen, int index = maxLen()) const { return _rfind(str, index, slen); }
        int rfind(wchr_t const * str, int index = maxLen()) const { return _rfind(str, index); }
        int rfind(wchr_t const * str, uint16_t slen, int index = maxLen()) const { return _rfind(str, index, slen); }
        int rfind(String const & str, int index = maxLen()) const { return _rfind(str._str, index, str._len); }

        int cmp(chr_t chr, int index = 0) const { return _cmp(chr, index); }
        int cmp(chr_t const * str, int index = 0) const { return _cmp(str, index); }
        int cmp(chr_t const * str, uint16_t slen, int index = 0) const { return _cmp(str, index, slen); }
        int cmp(wchr_t const * str, int index = 0) const { return _cmp(str, index); }
        int cmp(wchr_t const * str, uint16_t slen, int index = 0) const { return _cmp(str, index, slen); }
        int cmp(String const & str, int index = 0) const { return _cmp(str._str, index, str._len); }

        void rstrip(chr_t c = ' ')
        {
            int i = _len - 1;

            while ((i >= 0) && (_str[i] == c))
                i--;

            if (i != (_len - 1))
                _str[i+1] = 0;
        }

        void lstrip(chr_t c = ' ')
        {
            uint16_t i = 0;

            while ((i < _len) && (_str[i] == c))
                i++;

            if (i != 0)
            {
                uint16_t j = 0;

                while (i < _len)
                    _str[j++] = _str[i++];

                _str[j] = 0;
            }
        }

        void strip(chr_t c = ' ') { rstrip(c); lstrip(c); }

        void lower(void)
        {
            for (uint16_t i = 0; i < _len; i++)
                _str[i] = tolower(_str[i]);
        }

        void upper(void)
        {
            for (uint16_t i = 0; i < _len; i++)
                _str[i] = toupper(_str[i]);
        }

        String & reverse(void)
        {
            for (int i = 0, j = _len - 1; i < j; i++, j--)
            {
                chr_t tmp = _str[i];
                _str[i] = _str[j];
                _str[j] = tmp;
            }

            return *this;
        }

        String reverse(void) const
        {
            String s(*this);
            s.reverse();
            return s;
        }

        String sub(int index, uint16_t len = 0) const
        {
            if (index < 0)
            {
                if ((_len + index) < 0)
                    index = 0;
                else
                    index = _len + index;
            }

            if ((len == 0) || ((index + len) > _len))
                len = _len - index;

            return String(&_str[index], len);
        }

        friend bool operator ==(String const & lhs, String const & rhs)
        {
            if (lhs._len != rhs._len)
                return false;

            for (uint16_t i = 0; i < lhs._len; i++)
            {
                if (chr_case(lhs._str[i]) != chr_case(rhs._str[i]))
                    return false;
            }

            return true;
        }

        friend bool operator >(String const & lhs, String const & rhs)
        {
            uint16_t len = (lhs._len > rhs._len) ? rhs._len : lhs._len;
            int j = -1;

            for (uint16_t i = 0; i < len; i++)
            {
                chr_t lc = chr_case(lhs._str[i]);
                chr_t rc = chr_case(rhs._str[i]);

                if (lc > rc)
                    return true;
                else if (lc < rc)
                    return false;
                else if ((j == -1) && (lhs._str[i] != rhs._str[i]))
                    j = i;
            }

            if ((j != -1) && (lhs._len == rhs._len))
                return lhs._str[j] > rhs._str[j];
            else
                return lhs._len > rhs._len;
        }

        friend bool operator >=(String const & lhs, String const & rhs) { return (lhs > rhs) || (lhs == rhs); }
        friend bool operator !=(String const & lhs, String const & rhs) { return !(lhs == rhs); }
        friend bool operator <(String const & lhs, String const & rhs) { return !(lhs >= rhs); }
        friend bool operator <=(String const & lhs, String const & rhs) { return !(lhs > rhs); }

        friend bool operator ==(String const & lhs, chr_t rhs) {
            return (lhs._len == 1) && (chr_case(lhs[0]) == chr_case(rhs));
        }
        friend bool operator ==(chr_t lhs, String const & rhs) { return rhs == lhs; }
        friend bool operator !=(String const & lhs, chr_t rhs) { return !(lhs == rhs); }
        friend bool operator !=(chr_t lhs, String const & rhs) { return !(rhs == lhs); }
        friend bool operator <(String const & lhs, chr_t rhs) {
            return chr_case(lhs[0]) < chr_case(rhs);
        }
        friend bool operator >(chr_t lhs, String const & rhs) {
            return chr_case(lhs) > chr_case(rhs[0]);
        }
        friend bool operator <=(String const & lhs, chr_t rhs) { return (lhs < rhs) || (lhs == rhs); }
        friend bool operator <=(chr_t lhs, String const & rhs) { return !(lhs > rhs); }
        friend bool operator >=(String const & lhs, chr_t rhs) { return (rhs < lhs) || (lhs == rhs); }
        friend bool operator >=(chr_t lhs, String const & rhs) { return !(rhs > lhs); }
        friend bool operator >(String const & lhs, chr_t rhs) { return !(lhs <= rhs); }
        friend bool operator <(chr_t lhs, String const & rhs) { return !(lhs >= rhs); }

    private:
        uint16_t _index(int i) const
        {
            if (i < 0)
            {
                if (((int)_len + i) < 0)
                    i = 0;
                else
                    i = _len + i;
            }

            return (i < _len) ? i : _len;
        }

        chr_t & _get(int index)
        {
            uint16_t idx = _index(index);

            if (idx == _len)
                _str[++_len] = 0;

            return _str[idx];
        }

        chr_t const & _get(int index) const
        {
            uint16_t idx = _index(index);
            return _str[idx];
        }

        template < typename CHR >
        static uint16_t _strLen(CHR const * str, uint16_t mlen = maxLen())
        {
            if (mlen > maxLen())
                mlen = maxLen();

            uint16_t i = 0;
            while ((i < mlen) && (str[i] != 0))
                i++;

            return i;
        }

        template < typename CHR >
        void _set(CHR chr)
        {
            if (chr == 0)
            {
                clear();
            }
            else
            {
                _str[0] = (chr_t)chr;
                _str[1] = 0;
                _len = 1;
            }
        }

        template < typename CHR >
        void _set(CHR const * str, uint16_t slen = maxLen())
        {
            if (slen > maxLen())
                slen = maxLen();

            uint16_t i = 0;
            for (; (i < slen) && (str[i] != 0); i++)
                _str[i] = (chr_t)str[i];

            _len = i;
            _str[_len] = 0;
        }

        template < typename CHR >
        void _insert(CHR chr, int index)
        {
            uint16_t idx = _index(index);

            if (chr != 0)
                rshift(1, idx);

            _str[idx] = chr;

            if (chr == 0)
                _len = idx;
        }

        template < typename CHR >
        void _insert(CHR const * str, int index, uint16_t slen = maxLen())
        {
            static String < S > s;
            chr_t * ss = nullptr;

            slen = _strLen(str, slen);
            if (slen == 0)
                return;

            // In case string inserts into itself
            if (((chr_t *)str >= _str) && ((chr_t *)str < (_str + _len)))
            {
                s = *this;
                ss = (chr_t *)s.str();
            }

            uint16_t idx = _index(index);

            rshift(slen, idx);

            if ((idx + slen) > _len)
                slen = _len - idx;

            if (ss == nullptr)
            {
                for (uint16_t i = idx, j = 0; j < slen; i++, j++)
                    _str[i] = str[j];
            }
            else
            {
                for (uint16_t i = idx, j = 0; j < slen; i++, j++)
                    _str[i] = ss[j];
            }
        }

        template < typename CHR >
        int _cmp(CHR chr, int index) const
        {
            if ((chr == 0) || (_len == 0) || (index >= _len))
                return 0;

            if (chr_case(_str[index]) == chr_case(chr))
                return 1;

            return 0;
        }

        template < typename CHR >
        int _cmp(CHR const * str, int index, uint16_t slen = maxLen()) const
        {
            slen = _strLen(str, slen);
            int i = index, j = 0;
            for (; (i < _len) && (j < slen); i++, j++)
            {
                if (chr_case(_str[i]) != chr_case(str[j]))
                    break;
            }

            return j;
        }

        template < typename CHR >
        int _find(CHR chr, int index) const
        {
            if ((chr == 0) || (_len == 0))
                return -1;

            uint16_t idx = _index(index);

            for (int i = idx; i < _len; i++)
            {
                if (chr_case(_str[i]) == chr_case(chr))
                    return i;
            }

            return -1;
        }

        template < typename CHR >
        int _find(CHR const * str, int index, uint16_t slen = maxLen()) const
        {
            uint16_t idx = _index(index);

            slen = _strLen(str, slen);
            if ((slen == 0) || ((idx + slen) > _len))
                return -1;

            index = idx + (slen - 1);
            while (index < _len)
            {
                int i = index, j = slen - 1;

                while ((j >= 0) && (chr_case(_str[i]) == chr_case(str[j])))
                {
                    i--;
                    j--;
                }

                if (j < 0)
                    break;

                index++;
            }

            return (index < _len) ? index - (slen - 1) : -1;
        }

        template < typename CHR >
        int _rfind(CHR chr, int index) const
        {
            if ((chr == 0) || (_len == 0))
                return -1;

            uint16_t idx = _index(index);

            if (idx == _len)
                idx = _len - 1;

            for (int i = idx; i >= 0; i--)
            {
                if (chr_case(_str[i]) == chr_case(chr))
                    return i;
            }

            return -1;
        }

        template < typename CHR >
        int _rfind(CHR const * str, int index, uint16_t slen = maxLen()) const
        {
            uint16_t idx = _index(index);

            if (idx == _len)
                idx = _len - 1;

            slen = _strLen(str, slen);
            if ((slen == 0) || ((slen - 1) > idx))
                return -1;

            index = idx;
            while (index >= (slen - 1))
            {
                int i = index, j = slen - 1;

                while ((j >= 0) && (chr_case(_str[i]) == chr_case(str[j])))
                {
                    i--;
                    j--;
                }

                if (j < 0)
                    break;

                index--;
            }

            return index - (slen - 1);
        }

        void rshift(uint16_t amt, uint16_t index = 0)
        {
                                             // Overflow
            if (((_len + amt) > maxLen()) || ((_len + amt) < _len))
                _len = maxLen();
            else
                _len += amt;

            _str[_len] = 0;

            if (_len <= amt)
                return;

            for (int i = _len - 1, j = (_len - 1) - amt; j >= (int)index; i--, j--)
                _str[i] = _str[j];
        }

        chr_t _str[S + 1];
        uint16_t _len = 0;
};

#endif
