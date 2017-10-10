#ifndef _ELECMECH_H_
#define _ELECMECH_H_

#include "dev.h"
#include "pin.h"
#include "utility.h"
#include "types.h"

// All of these should be implementing and accepting ISRs for all of the
// different IRQC modes, but the implemetaion only uses IRQC_INTR_CHANGE

////////////////////////////////////////////////////////////////////////////////
// Switch //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < template < pin_t > class PIN, pin_t N, bool NH = true >  // NH -> Normally High
class DevSwitch : public DevPin < PIN, N >
{
    public:
        static DevSwitch & acquire(irqc_e irqc = IRQC_DISABLED) { static DevSwitch sw(irqc); return sw; }

        virtual bool closed(void) { return NH ? this->_pin.isLow() : this->_pin.isHigh(); }

        // Used if isr() enabled
        virtual uint32_t closeStart(void) { return _close_start; }
        virtual uint32_t closeDuration(void)
        {
            // Behaves like a w1c - calling this function will zero out the duration
            uint32_t cd = _close_duration;
            _close_duration = 0;
            return cd;
        }

        DevSwitch(DevSwitch const &) = delete;
        DevSwitch & operator=(DevSwitch const &) = delete;

    protected:
        DevSwitch(irqc_e irqc = IRQC_DISABLED) : DevPin < PIN, N > (irqc) {}
        virtual void isrChange(void);

        uint32_t volatile _close_start = 0;
        uint32_t volatile _close_duration = 0;
};

template < template < pin_t > class PIN, pin_t N, bool NH >
void DevSwitch < PIN, N, NH >::isrChange(void)
{
    if (closed())
    {
        _close_start = msecs();
        _close_duration = 0;
    }
    else if (_close_start != 0)
    {
        _close_duration = msecs() - _close_start;
        _close_start = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Rotary Switch ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < class... SWs >
class DevRotarySwitch : public SWs...
{
    public:
        static DevRotarySwitch & acquire(void) { static DevRotarySwitch rsw; return rsw; }

        virtual bool valid(void) const { return valid((SWs::valid())...); }

        // This has to be called with the function and not the result of the function
        // For some reason if called like valid() they are in reverse order.
        int8_t position(void) { return position<0>((&SWs::closed)...); }

        DevRotarySwitch(DevRotarySwitch const &) = delete;
        DevRotarySwitch & operator=(DevRotarySwitch const &) = delete;

    private:
        DevRotarySwitch(void) {}

        template < typename V > bool valid(V v) const { return v; }
        template < typename V, typename... Vs >
        bool valid(V v, Vs... vs) const { if (!v) return false; return valid(vs...); }

        template < uint8_t I, typename FC > int8_t position(FC fc) { return MFC(fc)() ? I : -1; }
        template < uint8_t I, typename FC, typename... FCs >
        int8_t position(FC fc, FCs... fcs)
        {
            if (MFC(fc)()) return I;
            return position<I+1>(fcs...);
        }
};

////////////////////////////////////////////////////////////////////////////////
// Rotary Encoder //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Encoder Value when turned ... or not
enum ev_e : int8_t
{
    EV_NEG  = -1,
    EV_ZERO =  0,
    EV_POS  =  1 
};  

template < typename T >
inline void evUpdate(T & val, ev_e ev, T const & min, T const & max, bool wrap = true)
{
    if (ev == EV_NEG)
        val = (val <= min) ? (wrap ? max : min) : static_cast < T > (val - 1);
    else if (ev == EV_POS)
        val = (val >= max) ? (wrap ? min : max) : static_cast < T > (val + 1);
}

enum epos_e  // Encoder Position
{
    EP_N,  // Nominal position
    EP_L,  // Left of nominal
    EP_R,  // Right of nominal
    EP_E,  // Edge of range
    EP_CNT,
};

template < template < pin_t > class PINA, pin_t CHA, template < pin_t > class PINB, pin_t CHB >
class DevEncoder : public DevPin < PINA, CHA >, public DevPin < PINB, CHB >
{
    using TchanA = DevPin < PINA, CHA >;
    using TchanB = DevPin < PINB, CHB >;

    public:
        static DevEncoder & acquire(irqc_e irqc = IRQC_DISABLED) { static DevEncoder en(irqc); return en; }

        virtual bool valid(void) const { return TchanA::valid() && TchanB::valid(); }
        virtual epos_e position(void) const { return (epos_e)((TchanA::_pin.read() << 1) | TchanB::_pin.read()); }

        // Behaves like a w1c, calling this function will reset turn value to zero
        virtual ev_e turn(void) { ev_e ev = _ev; _ev = EV_ZERO; return ev; }

        DevEncoder(DevEncoder const &) = delete;
        DevEncoder & operator=(DevEncoder const &) = delete;

    protected:
        DevEncoder(irqc_e irqcA, irqc_e irqcB = IRQC_DISABLED) : TchanA(irqcA), TchanB(irqcB) {}

        virtual void isrChange(void)
        {
            if (TchanA::_pin.read() != TchanB::_pin.read())
                _ev = EV_POS;
            else
                _ev = EV_NEG;
        }

        ev_e volatile _ev = EV_ZERO;
};

////////////////////////////////////////////////////////////////////////////////
// Rotary Encoder w/ Detents ///////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < template < pin_t > class PINA, pin_t CHA, template < pin_t > class PINB, pin_t CHB >
class DevDetentEncoder : public DevEncoder < PINA, CHA, PINB, CHB >
{
    public:
        static DevDetentEncoder & acquire(irqc_e irqc = IRQC_DISABLED)
        { static DevDetentEncoder den(irqc); return den; }

        DevDetentEncoder(DevDetentEncoder const &) = delete;
        DevDetentEncoder & operator=(DevDetentEncoder const &) = delete;

    protected:
        DevDetentEncoder(irqc_e irqc = IRQC_DISABLED)
            : DevEncoder < PINA, CHA, PINB, CHB > (irqc, irqc)
        {
            if (!this->valid())
                return;

            // Since initial state of the encoder may not be in the nominal position
            // of both reading zero, need to set initial number of transitions
            // before a turn is counted.  If already zero set to default number of
            // transistions.
            normInit(this->position());
        }

        virtual void isrChange(void);

    private:
        int8_t volatile _ctrans = 0;
        epos_e volatile _cpos = EP_N;
        int8_t volatile _trans_pos = 0;
        int8_t volatile _trans_neg = 0;

        void normInit(epos_e pos);
        bool normalized(void);

        static constexpr uint8_t const _s_num_trans = (uint8_t)EP_CNT;
        int8_t const _s_ntrans[EP_CNT][EP_CNT] =  // Next transition
        {
            {  0, -1,  1,  0 },
            {  1,  0,  0, -1 },
            { -1,  0,  0,  1 },
            {  0,  1, -1,  0 },
        };
};

template < template < pin_t > class PINA, pin_t CHA, template < pin_t > class PINB, pin_t CHB >
void DevDetentEncoder < PINA, CHA, PINB, CHB >::isrChange(void)
{
    epos_e npos = this->position();
    int8_t ntrans = _s_ntrans[_cpos][npos];

    // A zero indicates that a position was skipped over
    if (ntrans == 0)
        normInit(npos);
    else
        _ctrans += ntrans;

    _cpos = npos;

    // Since initial encoder position might not be zero, potentially
    // normalize encoder so that zero is the nominal state.
    if (!normalized())
        return;

    if ((_ctrans == _s_num_trans) || (_ctrans == -_s_num_trans))
    {
        this->_ev = (_ctrans == _s_num_trans) ? EV_POS : EV_NEG;
        _ctrans = 0;
    }
}

template < template < pin_t > class PINA, pin_t CHA, template < pin_t > class PINB, pin_t CHB >
void DevDetentEncoder < PINA, CHA, PINB, CHB >::normInit(epos_e pos)
{
    switch (pos)
    {
        case EP_N:  // A low and B low - Nominal Detent position
            _ctrans = 0;
            break;
        case EP_L:  // A low and B high - one position below nominal
            _ctrans = -1;
            break;
        case EP_R:  // A high and B low - one position above nominal
            _ctrans = 1;
            break;
        case EP_E:  // A high and B high - 2 positions above/below nominal - edge of detent range
            _trans_pos = 2;
            _trans_neg = -2;
            break;
        default:
            break;
    }

    _cpos = pos;
}

template < template < pin_t > class PINA, pin_t CHA, template < pin_t > class PINB, pin_t CHB >
bool DevDetentEncoder < PINA, CHA, PINB, CHB >::normalized(void)
{
    if ((_trans_pos == 0) && (_trans_neg == 0))
        return true;
    else if ((_ctrans != _trans_pos) && (_ctrans != _trans_neg))
        return false;

    _ctrans = (_ctrans == _trans_pos) ? _s_num_trans : -_s_num_trans;
    _trans_pos = _trans_neg = 0;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Encoder w/ or wo/ Detents w/ Switch /////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < class EN, class SW >
class DevEncoderSwitch : public EN, public SW
{
    public:
        static DevEncoderSwitch & acquire(irqc_e irqc_en, irqc_e irqc_sw)
        { static DevEncoderSwitch es(irqc_en, irqc_sw); return es; }

        bool valid(void) const { return EN::valid() && SW::valid(); }

        // Call the following:

        // For the switch part:
        // virtual bool closed(void);
        // virtual uint32_t closeStart(void);
        // virtual uint32_t closeDuration(void);

        // For the encoder part:
        // virtual uint8_t position(void);
        // ev_e turn(void) - this will give value from isr

        DevEncoderSwitch(DevEncoderSwitch const &) = delete;
        DevEncoderSwitch & operator=(DevEncoderSwitch const &) = delete;

    protected:
        DevEncoderSwitch(irqc_e irqc_en, irqc_e irqc_sw) : EN(irqc_en), SW(irqc_sw) {}
};

////////////////////////////////////////////////////////////////////////////////
// Templates for instantiation /////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

using TSwPrev = DevSwitch < PinIn, PIN_PREV, false >;
using TSwPlay = DevSwitch < PinIn, PIN_PLAY, false >;
using TSwNext = DevSwitch < PinIn, PIN_NEXT, false >;

using TSwRs0 = DevSwitch < PinInPU, PIN_RS_R >;
using TSwRs1 = DevSwitch < PinInPU, PIN_RS_L >;
using TSwRs2 = DevSwitch < PinInPU, PIN_RS_M >;
using TRsw = DevRotarySwitch < TSwRs0, TSwRs1, TSwRs2 >;

using TBrEnc = DevEncoder < PinInPU, PIN_EN_BR_A, PinInPU, PIN_EN_BR_B >;

using TUiEnc = DevDetentEncoder < PinInPU, PIN_EN_A, PinInPU, PIN_EN_B >;
using TUiSw = DevSwitch < PinIn, PIN_EN_SW, false >;
using TUiEncSw = DevEncoderSwitch < TUiEnc, TUiSw >;

#endif

