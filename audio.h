#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "types.h"
#include "utility.h"
#include "dev.h"
#include "file.h"
#include "spi.h"
#include "spi_cta.h"
#include "vs1053_plugins.h"

////////////////////////////////////////////////////////////////////////////////
// Beeper //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < pin_t PIN >
class DevBeeper : public DevPin < PinOut, PIN > , public Toggle
{
    public:
        static DevBeeper & acquire(
                uint32_t on_time = _s_beep_on_time,
                uint32_t off_time = _s_beep_off_time) { static DevBeeper b(on_time, off_time); return b; }

        virtual void start(void) { reset(); this->_pin.set(); }
        virtual void stop(void) { this->_pin.clear(); }
        virtual bool toggled(void)
        {
            if (!Toggle::toggled()) return false;
            this->_pin.toggle(); return true;
        }

        DevBeeper(DevBeeper const &) = delete;
        DevBeeper & operator=(DevBeeper const &) = delete;

    protected:
        DevBeeper(void) : Toggle(_s_beep_on_time, _s_beep_off_time) { init(); }
        DevBeeper(uint32_t on_time, uint32_t off_time) : Toggle(on_time, off_time) { init(); }

    private:
        void init(void) { if (!this->valid()) return; this->_pin.clear(); }

        static constexpr uint32_t _s_beep_on_time = 500;
        static constexpr uint32_t _s_beep_off_time = 80;
};

////////////////////////////////////////////////////////////////////////////////
// Amplifier ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < pin_t PIN >
class DevAmplifier : public DevPin < PinOut, PIN >
{
    public:
        static DevAmplifier & acquire(void) { static DevAmplifier amp; return amp; }

        void start(void) { this->_pin.set(); }
        void stop(void) { this->_pin.clear(); }
        bool running(void) { return this->_pin.isSet(); }

        DevAmplifier(DevAmplifier const &) = delete;
        DevAmplifier & operator=(DevAmplifier const &) = delete;

    protected:
        DevAmplifier(void) { if (!this->valid()) return; this->_pin.clear(); }
};

////////////////////////////////////////////////////////////////////////////////
// VS1053B /////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// VS1053B Commands
enum vc_e : uint8_t
{
    VC_MODE,         // RW
    VC_STATUS,       // RW
    VC_BASS,         // RW
    VC_CLOCKF,       // RW
    VC_DECODE_TIME,  // RW
    VC_AUDATA,       // RW
    VC_WRAM,         // RW
    VC_WRAMADDR,     //  W
    VC_HDAT0,        // R
    VC_HDAT1,        // R
    VC_AIADDR,       // RW
    VC_VOL,          // RW
    VC_AICTRL0,      // RW
    VC_AICTRL1,      // RW
    VC_AICTRL2,      // RW
    VC_AICTRL3       // RW
};

#define VC_WRITE    0x02
#define VC_READ     0x03

#define VS1053_SM_DIFF            0x0001
#define VS1053_SM_LAYER12         0x0002
#define VS1053_SM_RESET           0x0004
#define VS1053_SM_CANCEL          0x0008
#define VS1053_SM_EARSPEAKER_LO   0x0010
#define VS1053_SM_TESTS           0x0020
#define VS1053_SM_STREAM          0x0040
#define VS1053_SM_EARSPEAKER_HI   0x0080
#define VS1053_SM_DACT            0x0100
#define VS1053_SM_SDIORD          0x0200
#define VS1053_SM_SDISHARE        0x0400
#define VS1053_SM_SDINEW          0x0800
#define VS1053_SM_ADPCM           0x1000
#define VS1053_SM_RESERVED        0x2000
#define VS1053_SM_LINE1           0x4000
#define VS1053_SM_CLK_RANGE       0x8000
#define VS1053_SM_DEFAULT   (VS1053_SM_LINE1 | VS1053_SM_SDINEW)

#define VS1053_END_FILL_BYTE_ADDR  0x1E06

#define VS1053_VOLUME_MAX   255
#define VS1053_VOLUME_MIN   192

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
         template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
class DevVS1053B :
    public DevSPI < CS, SPI, MOSI, MISO, SCK >,
    public DevSPI < DCS, SPI, MOSI, MISO, SCK >,
    public DevPin < PinIn, DREQ >, public DevPin < PinOut, RST >
{
    using TCtrl = DevSPI < CS, SPI, MOSI, MISO, SCK >;
    using TData = DevSPI < DCS, SPI, MOSI, MISO, SCK >;
    using TDreq = DevPin < PinIn, DREQ >;
    using TReset = DevPin < PinOut, RST >;

    public:
        static DevVS1053B & acquire(void) { static DevVS1053B vs1053b; return vs1053b; }

        virtual bool busy(void) { return TCtrl::_spi.busy() || TData::_spi.busy(); }
        virtual bool valid(void) { return TDreq::valid() && TReset::valid() && TCtrl::valid() && TData::valid(); }

        void start(void);
        void stop(void);
        bool running(void) { return TReset::_pin.isSet(); }
        void hardReset(void) { stop(); start(); }
        void softReset(void);

        uint8_t volume(void) { return _volume; }
        void setVolume(uint8_t mono);
        bool send(File * fp, uint16_t len);
        void send(uint8_t const * data, uint16_t len);
        void cancel(File * fp);

        DevVS1053B(DevVS1053B const &) = delete;
        DevVS1053B & operator=(DevVS1053B const &) = delete;

    protected:
        DevVS1053B(void);

        void finish(void);
        bool ready(void) { return TDreq::_pin.isHigh(); }

        void ctrlSet(vc_e cmd, uint16_t val);
        uint16_t ctrlGet(vc_e cmd);

        void setClock(uint16_t val) { ctrlSet(VC_CLOCKF, val); }
        void setCancel(void) { ctrlSet(VC_MODE, ctrlGet(VC_MODE) | VS1053_SM_CANCEL); }
        void setVolume(uint8_t r, uint8_t l) { ctrlSet(VC_VOL, ((uint16_t)r << 8) | l); }

        void send(uint8_t byte, uint16_t num_times);

        uint8_t getEFB(void) // End Fill Byte
        { ctrlSet(VC_WRAMADDR, VS1053_END_FILL_BYTE_ADDR); return (uint8_t)ctrlGet(VC_WRAM); }

        bool cancelled(void) { return !(ctrlGet(VC_MODE) & VS1053_SM_CANCEL); }
        // According to documentation, both of these should read 0.
        bool finished(void) { return ((ctrlGet(VC_HDAT0) == 0) && (ctrlGet(VC_HDAT1) == 0)); } 

        uint8_t _volume = VS1053_VOLUME_MAX;

    private:
        void loadPlugin(uint16_t const * plugin, uint16_t plugin_size);

        uint32_t _stop_time = 0;
};

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::DevVS1053B(void)
{
    // Max writes are CLKI/4 and reads CLKI/7
    // Initially set SPI clock to 1Mhz until SCI_CLOCKF is set which should be around 43MHz
    // Then to 10MHz for writes and 6MHz for reads.
    // Maybe just set to 10MHz when sending data and 6MHz otherwise

    if (!valid())
        return;

    // Starts out in the off state
    TReset::_pin.clear();
    _stop_time = msecs();

    // CLKI / 4 = 43008000 / 4 = 10752000
    TData::_cta = spi_cta(10000000, 5, 24, 0); // Set this now since it doesn't change
}

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
void DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::start(void)
{
    if (running())
        return;

    // XXX 100ms seems kind of long - can't remember why I set it this high
    uint32_t now = msecs();
    if ((now - _stop_time) < 100)
        delay_msecs(now - _stop_time);

    TReset::_pin.set();
    delay_msecs(5);  // Datasheet says about a 1.8 ms delay before DREQ goes back up

    // This needs to be set low until clock is set
    // CLKI == XTALI == 12288000
    // CLKI / 7 = 12288000 / 7 = 1755428
    TCtrl::_cta = spi_cta(1000000, 5, 82, 164);

    // Set CLOCKI to 3.5 x XTALI = 3.5 * 12288000 = 43008000
    setClock(0x8800);
    setVolume(_volume);

    // CLKI / 7 = 43008000 / 7 = 6144000
    TCtrl::_cta = spi_cta(6000000, 5, 24, 48);

    loadPlugin(vs1053b_patches_plugin, VS1053B_PATCH_PLUGIN_SIZE);
}

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
void DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::stop(void)
{
    if (!running())
        return;

    TReset::_pin.clear();
    _stop_time = msecs();
}

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
void DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::softReset(void)
{
    // This function should be called if there was a problem with
    // finish() or cancel()

    // Don't get and set using old value since it might have SM_CANCEL set.
    // Set to default startup since this is all that should be set.
    // So don't do this:
    //  uint16_t mode = ctrlGet(VS1053_CMD_MODE);
    //  ctrlSet(VC_MODE, mode | VS1053_SM_RESET);
    // But do this:
    ctrlSet(VC_MODE, VS1053_SM_DEFAULT | VS1053_SM_RESET);
    //delay_msecs(5);
    //while (!ready());

    loadPlugin(vs1053b_patches_plugin, VS1053B_PATCH_PLUGIN_SIZE);
}

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
void DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::ctrlSet(vc_e cmd, uint16_t val)
{
    // These are read-only
    if ((cmd == VC_HDAT0) || (cmd == VC_HDAT1))
        return;

    while (!ready());

    TCtrl::_spi.begin(TCtrl::_pin, TCtrl::_cta);
    TCtrl::_spi.tx16(VC_WRITE << 8 | cmd);
    TCtrl::_spi.tx16(val);
    TCtrl::_spi.end(TCtrl::_pin);
}

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
uint16_t DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::ctrlGet(vc_e cmd)
{
    // This is write-only
    if (cmd == VC_WRAMADDR)
        return 0;

    while (!ready());

    TCtrl::_spi.begin(TCtrl::_pin, TCtrl::_cta);
    uint16_t val = TCtrl::_spi.trans16(VC_READ << 8 | cmd);
    TCtrl::_spi.end(TCtrl::_pin);

    return val;
}

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
void DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::setVolume(uint8_t mono)
{
    // The lower the value the higher the volume, so subtract the
    // value passed in from 255, the max uint8_t.
    // If 1 is passed in, then the value will be: 0xFE = Mute
    // If 0 is passed in, then the value will be:
    //   0xFF = disable analog drivers for power savings.
    uint16_t v = VS1053_VOLUME_MAX - mono;
    setVolume(v, v);
    _volume = mono;
}

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
bool DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::send(File * fp, uint16_t len)
{
    if ((fp == nullptr) || !fp->valid() || fp->eof())
        return false;

    static uint8_t buf[32];

    uint8_t const * p;
    int read = 0;

    if ((read = fp->read(&p, len)) > 0)
        send(p, read);
    else if ((len <= sizeof(buf)) && ((read = fp->read(buf, len)) > 0))
        send(buf, read);

    if ((read > 0) && fp->eof())
        finish();

    return read > 0;
}

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
void DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::send(uint8_t const * data, uint16_t len)
{
    // The most one can send at a time without having to recheck the DREQ
    // pin is 32 bytes per the VS1053b data sheet/reference guide
    uint16_t send32s = len / 32;
    uint16_t sendleft = len % 32;

    TData::_spi.begin(TData::_pin, TData::_cta);

    for (uint16_t i = 0; i < send32s; i++)
    {
        while (!ready());

        //TData::_spi.begin(TData::_pin, TData::_cta);
        TData::_spi.tx(data, 32);
        //TData::_spi.end(TData::_pin);

        data += 32;
    }

    if (sendleft != 0)
    {
        while (!ready());
        //TData::_spi.begin(TData::_pin, TData::_cta);
        TData::_spi.tx(data, sendleft);
        //TData::_spi.end(TData::_pin);
    }

    TData::_spi.end(TData::_pin);
}

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
void DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::send(uint8_t byte, uint16_t num_times)
{
    uint16_t send32s = num_times / 32;
    uint16_t sendleft = num_times % 32;

    TData::_spi.begin(TData::_pin, TData::_cta);

    for (uint16_t i = 0; i < send32s; i++)
    {
        while (!ready());
        //TData::_spi.begin(TData::_pin, TData::_cta);
        TData::_spi.tx(byte, 32);
        //TData::_spi.end(TData::_pin);
    }

    if (sendleft != 0)
    {
        while (!ready());
        //TData::_spi.begin(TData::_pin, TData::_cta);
        TData::_spi.tx(byte, sendleft);
        //TData::_spi.end(TData::_pin);
    }

    TData::_spi.end(TData::_pin);
}

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
void DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::finish(void)
{
    // Get endFillByte
    uint8_t efb = getEFB();

    // Need to send at least 2052 bytes of endFillByte
    send(efb, 2052);

    // Set SM_CANCEL
    setCancel();

    // Send endFillByte 32 bytes at a time checking SM_CANCEL
    // after each send.  If 2048 bytes or more are sent and
    // SM_CANCEL still isn't clear need to do a soft reset.
    // Potentially sending 2048 bytes, 32 bytes at a time, so 64 times.

    uint8_t sends = 0;
    for (; sends < 64; sends++)
    {
        send(efb, 32);

        if (cancelled())
            break;
    }

    // Maybe delay a microsecond to wait for
    // HDAT0 and HDAT1
    //delayUsecs(1);
    if ((sends == 64) || !finished())
        softReset();
}

#define VS_CANCEL_SOFT_RESET
template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
void DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::cancel(File * fp)
{
#ifdef VS_CANCEL_SOFT_RESET
    softReset();
#else
    if (fp == nullptr)
    {
        softReset():
        return;
    }

    // Set SM_CANCEL
    setCancel();

    // A max of 2048 more bytes of current file needs to be sent until
    // SM_CANCEL is cleared.  Will be sending 32 bytes at a time so 64 sends.
    // If not cleared within 2048 bytes need to software reset (documentation
    // says this should be extremely rare).
    // Documentation seems to be wrong.  In testing it is *not* rare for this
    // to happen, in fact it happens at least one in ten times.
    uint8_t sends = 0;
    int bytes = 0;

    for (; sends < 64; sends++)
    {
        if (!send(fp, 32) || cancelled())
            break;
    }

    // Sent 2048 bytes without SM_CANCEL clearing, are at end of file
    // or got a file read error.
    if ((sends == 64) || !fp->valid() || fp->eof())
    {
        softReset();
        return;
    }

    // Get endFillByte
    uint8_t efb = getEFB();

    // Need to send 2052 bytes of endFillByte
    send(efb, 2052);

    // According to documentation, both of HDAT0 and HDAT1 should read 0.
    // Again, documentation seems wrong.  HDAT1 is almost *never* 0.
    // There was a user that posted to the VLSI forum and said that
    // it often took ~15 ms for these to clear:
    // http://www.vsdsp-forum.com/phpbb/viewtopic.php?t=920
    // Also, it doesn't seem to affect decoding the next song so just
    // don't bother checking.
    // On second thought just do a softReset()
    if (!finished())
        softReset();
#endif
}

template < pin_t CS, pin_t DCS, pin_t DREQ, pin_t RST,
    template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
void DevVS1053B < CS, DCS, DREQ, RST, SPI, MOSI, MISO, SCK >::loadPlugin(uint16_t const * plugin, uint16_t plugin_size)
{
    auto isRepRun = [&](uint16_t n) -> bool
    {
        return n & 0x8000;
    };

    // RLE run, replicate n samples
    auto replicate = [&](vc_e cmd, uint16_t n) -> void
    {
        n &= 0x7FFF;
        uint16_t val = *plugin++;
        while (n--) ctrlSet(cmd, val);
    };

    // Copy run, copy n samples
    auto copy = [&](vc_e cmd, uint16_t n) -> void
    {
        while (n--) ctrlSet(cmd, *plugin++);
    };

    uint16_t const * plugin_end = plugin + plugin_size;

    while (plugin < plugin_end)
    {
        vc_e cmd = (vc_e)(*plugin++);
        uint16_t n = *plugin++;

        isRepRun(n) ? replicate(cmd, n) : copy(cmd, n);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Audio - VS1053B & Amplifier /////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < class VS1053B, class AMP >
class DevAudio : public VS1053B, public AMP
{
    public:
        static DevAudio & acquire(void) { static DevAudio audio; return audio; }

        bool valid(void) { return VS1053B::valid() && AMP::valid(); }
        bool running(void) { return VS1053B::running() && AMP::running(); }
        bool ready(void) { return valid() && running() && VS1053B::ready(); }

        void start(void)
        {
            if (running())
                return;

            VS1053B::start();
            //delay_msecs(350);  // I thought this helped speaker pop but don't think it really does
            AMP::start();
        }

        void stop(void)
        {
            if (!running())
                return;

            AMP::stop();
            VS1053B::stop();
        }

        bool send(File * fp, uint16_t amt)
        {
            if (!running())
                start();

            return VS1053B::send(fp, amt);
        }

        void send(uint8_t const * data, uint16_t dlen)
        {
            if (!running())
                start();

            VS1053B::send(data, dlen);
        }

        // Call this if starting a new file before current is done
        // Note this requires a File object since it needs to send additional
        // data from it before it can cancel.
        void cancel(File * fp)
        {
            if (!running())
                return;

            VS1053B::cancel(fp);
        }

        DevAudio(DevAudio const &) = delete;
        DevAudio & operator=(DevAudio const &) = delete;

    protected:
        DevAudio(void) {}
};

////////////////////////////////////////////////////////////////////////////////
// Templates ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
using TBeep = DevBeeper < PIN_BEEPER >;
using TAmp = DevAmplifier < PIN_AMP_SDWN >;
using TVs = DevVS1053B < PIN_VS_CS, PIN_VS_DCS, PIN_VS_DREQ, PIN_VS_RST, SPI0, PIN_MOSI, PIN_MISO, PIN_SCK >;
using TAudio = DevAudio < TVs, TAmp >;

#endif
