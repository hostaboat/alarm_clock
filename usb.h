#ifndef _USB_H_
#define _USB_H_

#include "types.h"
#include "utility.h"
#include "module.h"
#include "scsi.h"
#include <string.h>

#define BULK_IN_EP_NUM   EndPoint::N1
#define BULK_OUT_EP_NUM  EndPoint::N1

// Endpoint 0 and number of Bulk EP numbers
#define EP_NUM_CNT  2

#define MAX_PKT_SIZE  MPS_64
#define NUM_USB_PKTS  32

#define MAX_LUN  0

#define USB_ISTAT_USBRST  (1 << 0)
#define USB_ISTAT_ERROR   (1 << 1)
#define USB_ISTAT_SOFTOK  (1 << 2)
#define USB_ISTAT_TOKDNE  (1 << 3)
#define USB_ISTAT_SLEEP   (1 << 4)
#define USB_ISTAT_RESUME  (1 << 5)
#define USB_ISTAT_ATTACH  (1 << 6)
#define USB_ISTAT_STALL   (1 << 7)

#define USB_INTEN_USBRSTEN  (1 << 0)
#define USB_INTEN_ERROREN   (1 << 1)
#define USB_INTEN_SOFTOKEN  (1 << 2)
#define USB_INTEN_TOKDNEEN  (1 << 3)
#define USB_INTEN_SLEEPEN   (1 << 4)
#define USB_INTEN_RESUMEEN  (1 << 5)
#define USB_INTEN_ATTACHEN  (1 << 6)
#define USB_INTEN_STALLEN   (1 << 7)

#define USB_ERRSTAT_PIDERR   (1 << 0)
#define USB_ERRSTAT_CRC5EOF  (1 << 1)
#define USB_ERRSTAT_CRC16    (1 << 2)
#define USB_ERRSTAT_DFN8     (1 << 3)
#define USB_ERRSTAT_BTOERR   (1 << 4)
#define USB_ERRSTAT_DMAERR   (1 << 5)
#define USB_ERRSTAT_BTSERR   (1 << 7)

#define USB_ERREN_PIDERREN   (1 << 0)
#define USB_ERREN_CRC5EOFEN  (1 << 1)
#define USB_ERREN_CRC16EN    (1 << 2)
#define USB_ERREN_DFN8EN     (1 << 3)
#define USB_ERREN_BTOERREN   (1 << 4)
#define USB_ERREN_DMAERREN   (1 << 5)
#define USB_ERREN_BTSERREN   (1 << 7)

#define USB_STAT_ODD     (1 << 2)
#define USB_STAT_TX      (1 << 3)
#define USB_STAT_ENDP(r) (((r) & 0xF0) >> 4)

#define USB_CTL_USBENSOFEN         (1 << 0)
#define USB_CTL_ODDRST             (1 << 1)
#define USB_CTL_RESUME             (1 << 2)
#define USB_CTL_HOSTMODEEN         (1 << 3)
#define USB_CTL_RESET              (1 << 4)
#define USB_CTL_TXSUSPENDTOKENBUSY (1 << 5)
#define USB_CTL_SE0                (1 << 6)
#define USB_CTL_JSTATE             (1 << 7)

#define USB_ADDR_ADDR(r) ((r) & 0x7F)
#define USB_ADDR_LSEN    (1 << 7)

#define USB_BDTPAGE1(addr)  (uint8_t)(((uint32_t)(addr) >> 8))
#define USB_BDTPAGE2(addr)  (uint8_t)(((uint32_t)(addr) >> 16))
#define USB_BDTPAGE3(addr)  (uint8_t)(((uint32_t)(addr) >> 24))

#define USB_ENDPTn_EPHSHK  (1 << 0)
#define USB_ENDPTn_EPSTALL (1 << 1)
#define USB_ENDPTn_EPTXEN  (1 << 2)
#define USB_ENDPTn_EPRXEN  (1 << 3)

#define USB_USBCTRL_PDE  (1 << 6)
#define USB_USBCTRL_SUSP (1 << 7)

#define USB_CONTROL_DPPULLUPNONOTG (1 << 4)

#define USB_USBTRC0_USB_RESUME_INT (1 << 0)
#define USB_USBTRC0_SYNC_DET       (1 << 1)
#define USB_USBTRC0_USBRESMEN      (1 << 5)
#define USB_USBTRC0_USBRESET       (1 << 7)

#define BD_BDT_STALL (1 << 2)
#define BD_DTS       (1 << 3)
#define BD_NINC      (1 << 4)
#define BD_KEEP      (1 << 5)
#define BD_DATA1     (1 << 6)
#define BD_OWN       (1 << 7)
#define BD_BC(bc)    (((uint32_t)((bc) & 0x03FF)) << 16)
#define BD_DATAX(d)  ((d) ? BD_DATA1 : 0)

// Max Packet Size values
enum mps_e : uint8_t
{
    MPS_8 = 8,
    MPS_16 = 16,
    MPS_32 = 32,
    MPS_64 = 64,
};

////////////////////////////////////////////////////////////////////////////////
// PID /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct PID
{
    // PID Type
    enum pt_e : uint8_t
    {
        SPECIAL,
        TOKEN,
        HANDSHAKE,
        DATA
    };

    // PID Name
    enum pid_e : uint8_t
    {
        OUT   = (0 << 2) | PID::TOKEN,  // 0001B = 0x1
        IN    = (2 << 2) | PID::TOKEN,  // 1001B = 0x9
        SOF   = (1 << 2) | PID::TOKEN,  // 0101B = 0x5
        SETUP = (3 << 2) | PID::TOKEN,  // 1101B = 0xD

        DATA0 = (0 << 2) | PID::DATA,  // 0011B = 0x3
        DATA1 = (2 << 2) | PID::DATA,  // 1011B = 0xB
        DATA2 = (1 << 2) | PID::DATA,  // 0111B = 0x7
        MDATA = (3 << 2) | PID::DATA,  // 1111B = 0xF

        ACK   = (0 << 2) | PID::HANDSHAKE,  // 0010B = 0x2
        NAK   = (2 << 2) | PID::HANDSHAKE,  // 1010B = 0xA
        STALL = (3 << 2) | PID::HANDSHAKE,  // 1110B = 0xE
        NYET  = (1 << 2) | PID::HANDSHAKE,  // 0110B = 0x6

        ERR      = (3 << 2) | PID::SPECIAL,  // 1100B = 0xC
        SPLIT    = (2 << 2) | PID::SPECIAL,  // 1000B = 0x8
        PING     = (1 << 2) | PID::SPECIAL,  // 0100B = 0x4
        RESERVED = (0 << 2) | PID::SPECIAL,  // 0000B = 0x0
    };

    static constexpr pid_e const PRE = PID::ERR;
};

////////////////////////////////////////////////////////////////////////////////
// BD - Buffer Descriptor //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct BD
{
    v32 desc;
    v8 * addr;

    ////////////////////////////////////////////////////////////////////////////

    uint16_t count(void) const { return ((desc >> 16) & 0x03FF); }
    PID::pid_e pid(void) const { return (PID::pid_e)((desc >> 2) & 0x0F); }

    void set(uint8_t * data, uint16_t dlen, uint8_t dataX)
    {
        addr = data;
        desc = BD_BC(dlen) | BD_DATAX(dataX) | BD_DTS;
        desc |= BD_OWN;
    }

    void clear(void) { desc = 0; addr = nullptr; }
    void stall(void) { desc |= BD_BDT_STALL | BD_OWN; }
    void unstall(void) { desc &= ~BD_BDT_STALL; }

} __attribute__ ((packed));

////////////////////////////////////////////////////////////////////////////////
// EndPoint ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < uint16_t, uint8_t > class UsbPacket;
using UsbPkt = UsbPacket < MAX_PKT_SIZE, NUM_USB_PKTS >;

class EndPoint
{
    friend UsbPkt;

    public:
        enum num_e : uint8_t
        {
            N0, N1,  N2,  N3,  N4,  N5,  N6,  N7,
            N8, N9, N10, N11, N12, N13, N14, N15,
        };

        enum dir_e : uint8_t { OUT, IN };
        enum type_e : uint8_t { CONTROL, ISOCHRONOUS, BULK, INTERRUPT };

        enum data_e : uint8_t { DATA0, DATA1 };
        enum bank_e : uint8_t { EVEN, ODD };

        ////////////////////////////////////////////////////////////////////////

        EndPoint(num_e num, type_e type, dir_e dir = OUT);
        ~EndPoint(void);

        virtual void enable(void) = 0;
        virtual void disable(void) = 0;
        virtual bool enabled(void);

        virtual void isr(dir_e dir, bank_e bank) = 0;

        virtual void stall(void) { *_endptN |= USB_ENDPTn_EPSTALL; }
        virtual void unstall(void) { *_endptN &= ~USB_ENDPTn_EPSTALL; }
        virtual bool stalled(void) { return *_endptN & USB_ENDPTn_EPSTALL; }

        num_e num(void) const { return _num; }
        dir_e dir(void) const { return _dir; }
        type_e type(void) const { return _type; }

        static EndPoint * get(num_e num, dir_e dir) { return _s_eps[num][dir]; }
        static void reset(void);

    protected:
        virtual bool give(UsbPkt * p) = 0;

        num_e const _num;
        type_e const _type;
        dir_e const _dir;
        uint8_t volatile _dataX = DATA0;
        reg8 _endptN;

    private:
        static reg8 _s_endpt0;
        static EndPoint * _s_eps[16][2];
};

////////////////////////////////////////////////////////////////////////////////
// UsbDesc - USB Standard Descriptors //////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct UsbDesc
{
    // Descriptor Types
    enum dt_e : uint8_t
    {
        DEVICE = 1,
        CONFIGURATION,
        STRING,
        INTERFACE,
        ENDPOINT,
        DEVICE_QUALIFIER,
        OTHER_SPEED_CONFIGURATION,
        INTERFACE_POWER
    };

    // USB Device Class codes
    enum class_e : uint8_t       // Descriptor
    {                            // ----------
        USE_IFACE_INFO,          // Device      Use class information in the Interface Descriptors
        AUDIO,                   // Interface
        CDC_CONTROL,             // Both        Communications and CDC Control
        HID,                     // Interface   Human Interface Device
        PHYSICAL = 0x05,         // Interface
        IMAGE,                   // Interface
        PRINTER,                 // Interface
        MASS_STORAGE,            // Interface
        HUB,                     // Device
        CDC_DATA,                // Interface
        SMART_CARD,              // Interface
        CONTENT_SECURITY = 0x0D, // Interface
        VIDEO,                   // Interface
        PERSONAL_HEALTHCARE,     // Interface
        AUDIO_VIDEO,             // Interface
        BILLBOARD,               // Device
        USB_TYPE_C_BRIDGE,       // Interface
        DIAGNOSTIC = 0xDC,       // Both
        WIRELESS = 0xE0,         // Interface
        MISC = 0xEF,             // Both
        APP_SPECIFIC = 0xFE,     // Interface
        VENDOR_SPECIFIC,         // Both
    };

    struct String
    {
        static constexpr uint8_t const max_len = 16;

        ////////////////////////////////////////////////////////////////////////

        // Initialize for language support
        uint8_t bLength = 4;
        uint8_t const bDescriptorType = STRING;
        uint16_t bString[max_len] = { lang_id };

        ////////////////////////////////////////////////////////////////////////

        static constexpr char const * const manufacturer_str = "KTW";
        static constexpr char const * const product_str = "Boogie Box";
        // Serial number must be at least 12 unicode digits and the
        // allowable digits are 0030h through 0039h ('0' through '9')
        // and 0041h through 0046h ('A' through 'F') per the
        // USB Mass Storage Class / Bulk-Only Transport specification
        // XXX Look into getting UID from SIM_UIDL, SIM_UIDML, SIM_UIDMH and SIM_UIDH
        static constexpr char const * const serial_number_str = "0000000000000001";

        static constexpr uint16_t const lang_id = 0x0409; // English (United States)

        String(char const * const str)
        {
            if ((str == nullptr) || (str[0] == '\0'))
                return;

            uint8_t i = 0;
            for (; (str[i] != '\0') && (i < max_len); i++)
                bString[i] = (uint16_t)str[i];

            bLength = 2 + (i * 2);
        }

        enum index_e : uint8_t { IMANUFACTURER = 1, IPRODUCT, ISERIAL_NUMBER, ICNT };

        static String const strs[ICNT];

    } __attribute__ ((packed));

    struct Device
    {
        uint8_t const bLength = sizeof(Device);
        uint8_t const bDescriptorType = DEVICE;
        uint16_t const bcdUSB = usb_version;
        uint8_t const bDeviceClass = USE_IFACE_INFO;
        uint8_t const bDeviceSubClass = USE_IFACE_INFO;
        uint8_t const bDeviceProtocol = USE_IFACE_INFO;
        uint8_t const bMaxPacketSize0 = MAX_PKT_SIZE;
        uint16_t const idVendor = vendor_id;
        uint16_t const idProduct = product_id;
        uint16_t const bcdDevice = device_release;
        uint8_t const iManufacturer = String::IMANUFACTURER;
        uint8_t const iProduct = String::IPRODUCT;
        uint8_t const iSerialNumber = String::ISERIAL_NUMBER;
        uint8_t const bNumConfigurations = num_confs;

        ////////////////////////////////////////////////////////////////////////

        // 1.1 full/low speed, 2.0 high speed - This is a full speed device
        static constexpr uint16_t const usb_version = 0x0101;    // Version 1.1
        static constexpr uint16_t const vendor_id = 0x16C0;      // Taken from Teensy code
        static constexpr uint16_t const product_id = 0x0001;     // Arbitrary
        static constexpr uint16_t const device_release = 0x0100; // 1.0

        static constexpr uint8_t const num_confs = 1;

    } __attribute__ ((packed));

    struct Configuration
    {
        uint8_t const bLength = sizeof(Configuration);
        uint8_t const bDescriptorType = CONFIGURATION;
        uint16_t wTotalLength;
        uint8_t const bNumInterfaces = num_ifaces;
        uint8_t const bConfigurationValue = conf_value;
        uint8_t const iConfiguration = 0;
        uint8_t const bmAttributes = attrs;
        uint8_t const bMaxPower = max_power;

        ////////////////////////////////////////////////////////////////////////

        enum attr_e : uint8_t
        {
            RSVD_ONE      = (1 << 7),  // MSb must always be set to one for historical reasons
            SELF_POWERED  = (1 << 6),
            REMOTE_WAKEUP = (1 << 5),
        };

        static constexpr uint8_t const attrs = RSVD_ONE | SELF_POWERED;
        static constexpr uint8_t const max_power = 50;  // 100 mA
        static constexpr uint8_t const num_ifaces = 1;
        static constexpr uint8_t const conf_value = 1;

        Configuration(uint16_t total_length) : wTotalLength(total_length) {}

    } __attribute__ ((packed));

    struct Interface
    {
        uint8_t const bLength = sizeof(Interface);
        uint8_t const bDescriptorType = INTERFACE;
        uint8_t const bInterfaceNumber = iface_number;
        uint8_t const bAlternateSetting = iface_alt;
        uint8_t bNumEndpoints;
        uint8_t const bInterfaceClass = MASS_STORAGE;
        uint8_t const bInterfaceSubClass = SCSI;
        uint8_t const bInterfaceProtocol = BBB;
        uint8_t const iInterface = 0;

        ////////////////////////////////////////////////////////////////////////

        // USB Mass Storage Subclass codes
        enum mss_e : uint8_t
        {
            RBC = 0x01,
            MMC5,
            UFI = 0x04,
            SCSI = 0x06,
            LSD_FS,
            IEEE_1667,
            MSS_VS = 0xFF,
        };

        // USB Mass Storage Protocol codes
        enum msp_e : uint8_t
        {
            CBI_INT = 0x00,
            CBI_NO_INT,
            BBB = 0x50,  // Bulk-Only
            UAS = 0x62,
            MSP_VS = 0xFF,
        };

        static constexpr uint8_t const iface_number = 0;
        static constexpr uint8_t const iface_alt = 0;

        Interface(uint8_t num_eps) : bNumEndpoints{num_eps} {}

    } __attribute__ ((packed));

    struct Endpoint
    {
        uint8_t const bLength = sizeof(Endpoint);
        uint8_t const bDescriptorType = ENDPOINT;
        uint8_t bEndpointAddress;
        uint8_t const bmAttributes = EndPoint::BULK;
        uint16_t const wMaxPacketSize = MAX_PKT_SIZE;
        uint8_t const bInterval = 0;

        ////////////////////////////////////////////////////////////////////////

        using num_e = EndPoint::num_e;
        using dir_e = EndPoint::dir_e;

        Endpoint(num_e num, dir_e dir) : bEndpointAddress((dir << 7) | num) {}

    } __attribute__ ((packed));

    struct BulkOnlyConfiguration
    {
        Configuration const conf;
        Interface const iface;
        Endpoint const bulk_in;
        Endpoint const bulk_out;

        ////////////////////////////////////////////////////////////////////////

        static constexpr uint8_t const num_eps = 2;

        BulkOnlyConfiguration(void)
            : conf(sizeof(BulkOnlyConfiguration)), iface(num_eps),
              bulk_in(BULK_IN_EP_NUM, EndPoint::IN),
              bulk_out(BULK_OUT_EP_NUM, EndPoint::OUT) {}

    } __attribute__ ((packed));
};

////////////////////////////////////////////////////////////////////////////////
// UsbPacket ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < uint16_t S, uint8_t N >
class UsbPacket
{
    // 1023 is the max packet size for full-speed isochronous endpoints
    static_assert(S < 1024, "Invalid UsbPacket size : S must be < 1024");
    static_assert(N <= 32, "Invalid UsbPacket number : N must be <= 32");

    public:
        alignas(4) uint8_t buffer[S];
        uint16_t volatile count = 0;

        ////////////////////////////////////////////////////////////////////////

        static UsbPacket * acquire(void);
        static UsbPacket * acquire(EndPoint & ep);
        static void release(UsbPacket * p);

        uint16_t set(uint8_t const * data, uint16_t dlen);

        static uint8_t available(void) { return (_s_available == 0) ? 0 : N - __builtin_ctz(_s_available); }
        static constexpr uint16_t const size = S;

        UsbPacket(UsbPacket const &) = delete;
        UsbPacket & operator =(UsbPacket const &) = delete;

    private:
        UsbPacket(void) = default;

        static UsbPacket * _acquire(void);
        static bool _give(UsbPacket * p);

        static uint32_t volatile _s_available;
        static UsbPacket _s_packets[N];
        static EndPoint * _s_eps[EP_NUM_CNT];
        static uint32_t volatile _s_ep_need[EP_NUM_CNT];
        static uint32_t volatile _s_need;

        static constexpr uint32_t const _s_max_available = 0xFFFFFFFF;
};

////////////////////////////////////////////////////////////////////////////////
// StreamPipe //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class StreamPipe : public EndPoint
{
    public:
        StreamPipe(num_e num, type_e type, dir_e dir);

        virtual void enable(void) = 0;
        virtual void disable(void);
        //virtual bool enabled(void);

        virtual void isr(dir_e dir, bank_e bank) = 0;

        virtual void stall(void) { _bd[_bank]->stall(); }
        virtual void unstall(void) { _bd[_bank]->unstall(); EndPoint::unstall(); }
        //virtual bool stalled(void);

        virtual bool peek(UsbPkt * & pkt) { return _pkt_queue.first(pkt); }

    protected:
        virtual bool give(UsbPkt * p) = 0;

        BD * const _bd[2];
        Queue < UsbPkt *, NUM_USB_PKTS > _pkt_queue;
        uint8_t volatile _bank = EVEN;
};

////////////////////////////////////////////////////////////////////////////////
// StreamPipeOut ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class StreamPipeOut : public StreamPipe
{
    public:
        StreamPipeOut(num_e num, type_e type) : StreamPipe(num, type, OUT) {}

        virtual void enable(void);
        //virtual void disable(void);
        //virtual bool enabled(void);

        virtual void isr(dir_e dir, bank_e bank) = 0;

        //virtual void stall(void);
        //virtual void unstall(void);
        //virtual bool stalled(void);

        //virtual bool peek(UsbPkt * & pkt);

        virtual bool recv(UsbPkt * & pkt) = 0;
        virtual bool canRecv(void) { return !_pkt_queue.isEmpty(); }

    protected:
        virtual bool give(UsbPkt * p);
};

////////////////////////////////////////////////////////////////////////////////
// StreamPipeIn ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class StreamPipeIn : public StreamPipe
{
    public:
        StreamPipeIn(num_e num, type_e type) : StreamPipe(num, type, IN) {}

        virtual void enable(void);
        //virtual void disable(void);
        //virtual bool enabled(void);

        virtual void isr(dir_e dir, bank_e bank) = 0;

        //virtual void stall(void);
        //virtual void unstall(void);
        //virtual bool stalled(void);

        //virtual bool peek(UsbPkt * & pkt);

        virtual bool send(UsbPkt * pkt) = 0;
        virtual bool canSend(void);

    protected:
        virtual bool give(UsbPkt * p) { return false; }
};

////////////////////////////////////////////////////////////////////////////////
// BulkPipe ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < EndPoint::num_e N, EndPoint::dir_e D >
class BulkPipe;

////////////////////////////////////////////////////////////////////////////////
// BulkPipe < OUT > ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < EndPoint::num_e N >
class BulkPipe < N, EndPoint::OUT > : public StreamPipeOut
{
    static_assert(N != N0, "Endpoint 0 reserved for Default Control Pipe.");

    public:
        BulkPipe(void) : StreamPipeOut(N, BULK) {}

        //virtual void enable(void);
        //virtual void disable(void);
        //virtual bool enabled(void);

        virtual void isr(dir_e dir, bank_e bank);

        //virtual void stall(void);
        //virtual void unstall(void);
        //virtual bool stalled(void);

        virtual bool recv(UsbPkt * & p) { return _pkt_queue.dequeue(p); }
};

////////////////////////////////////////////////////////////////////////////////
// BulkPipe < IN > /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < EndPoint::num_e N >
class BulkPipe < N, EndPoint::IN > : public StreamPipeIn
{
    static_assert(N != N0, "Endpoint 0 reserved for Default Control Pipe.");

    public:
        BulkPipe(void) : StreamPipeIn(N, BULK) {}

        //virtual void enable(void);
        //virtual void disable(void);
        //virtual bool enabled(void);

        virtual void isr(dir_e dir, bank_e bank);

        //virtual void stall(void);
        //virtual void unstall(void);
        //virtual bool stalled(void);

        virtual bool send(UsbPkt * p);
        //virtual bool canSend(void);
};

////////////////////////////////////////////////////////////////////////////////
// MessagePipe /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class MessagePipe : public EndPoint
{
    public:
        // Transfer Stage
        enum ts_e : uint8_t { SETUP, DATA, STATUS };

        MessagePipe(num_e num);

        virtual void enable(void);
        virtual void disable(void);
        //virtual bool enabled(void);

        virtual void isr(dir_e dir, bank_e bank) = 0;
        virtual void process(void) = 0;

        virtual void stall(void) { _bd[(IN << 1) | _tx_bank]->stall(); }
        virtual void unstall(void) { _bd[(IN << 1) | _tx_bank]->unstall(); EndPoint::unstall(); }
        //virtual bool stalled(void);

    protected:
        virtual bool give(UsbPkt * p) { return false; }

        BD * const _bd[4];

        UsbPkt * const _setup_pkt = UsbPkt::acquire();
        UsbPkt * const _status_pkt = UsbPkt::acquire();

        Queue < UsbPkt *, 8 > _rx_queue;
        Queue < UsbPkt *, 8 > _tx_queue;

        ts_e volatile _state = STATUS;
        uint8_t volatile _rx_bank = EVEN;
        uint8_t volatile _tx_bank = EVEN;
};

////////////////////////////////////////////////////////////////////////////////
// ControlPipe /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < EndPoint::num_e N >
class ControlPipe;

////////////////////////////////////////////////////////////////////////////////
// DefaultPipe /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class Usb;

template < >
class ControlPipe < EndPoint::N0 > : public MessagePipe
{
    public:
        // Standard Request Codes
        enum src_e : uint8_t
        {
            // Standard Codes
            GET_STATUS,
            CLEAR_FEATURE,
            SET_FEATURE = 0x03,
            SET_ADDRESS = 0x05,
            GET_DESCRIPTOR,
            SET_DESCRIPTOR,
            GET_CONFIGURATION,
            SET_CONFIGURATION,
            GET_INTERFACE,
            SET_INTERFACE,
            SYNCH_FRAME,
        };

        // Mass Storage Request Codes
        enum msrc_e : uint8_t
        {
            ADSC,
            GET_REQUEST = 0xFC,
            PUT_REQUEST,

            // Bulk-Only Mass Storage
            GET_MAX_LUN,
            BOMSR,  // Bulk-Only Mass Storage Reset
        };

        // Request Type
        enum rt_e : uint8_t { STANDARD, CLASS, VENDOR, RESERVED };

        // Request Recipient
        enum rr_e : uint8_t { DEVICE, INTERFACE, ENDPOINT, OTHER };

        enum feature_e : uint8_t { ENDPOINT_HALT, DEVICE_REMOTE_WAKEUP, TEST_MODE };

        struct req_t
        {
            uint8_t bmRequestType;
            uint8_t bRequest;
            uint16_t wValue;
            uint16_t wIndex;
            uint16_t wLength;

            ////////////////////////////////////////////////////////////////////

            dir_e dir(void) const { return (dir_e)(bmRequestType >> 7); }
            rt_e type(void) const { return (rt_e)((bmRequestType >> 5) & 0x03); }
            uint8_t recipient(void) const { return bmRequestType & 0x1F; }

        } __attribute__ ((packed));

        ////////////////////////////////////////////////////////////////////////

        ControlPipe(Usb & usb) : MessagePipe(N0), _usb(usb) {}

        //virtual void enable(void);
        //virtual void disable(void);
        //virtual bool enabled(void);

        virtual void isr(dir_e dir, bank_e bank);
        virtual void process(void);

        //virtual void stall(void);
        //virtual void unstall(void);
        //virtual bool stalled(void);

    private:
        Usb & _usb;
        req_t _req;
        uint16_t _dlen;
        uint16_t _doff;
        alignas(4) uint8_t _data[1024];
        bool volatile _set_address = false;

        using state_action_t = void (ControlPipe::*)(void);
        state_action_t const _state_actions[3] =
        {
            &ControlPipe::setup,
            &ControlPipe::data,
            nullptr
        };

        using std_req_t = bool (ControlPipe::*)(void);
        std_req_t const _std_reqs[13] =
        {
            &ControlPipe::getStatus,
            &ControlPipe::clearFeature,
            nullptr,
            &ControlPipe::setFeature,
            nullptr,
            &ControlPipe::setAddress,
            &ControlPipe::getDescriptor,
            &ControlPipe::setDescriptor,
            &ControlPipe::getConfiguration,
            &ControlPipe::setConfiguration,
            &ControlPipe::getInterface,
            &ControlPipe::setInterface,
            &ControlPipe::synchFrame
        };

        void setup(void);
        void data(void);

        bool isFreeBD(dir_e dir, bank_e bank) { return _bd[(dir << 1) | bank]->addr == nullptr; }
        bool send(UsbPkt * p);
        void tx(UsbPkt * p);

        // Standard Requests
        bool getStatus(void);
        bool clearFeature(void);
        bool setFeature(void);
        bool setAddress(void);
        bool getDescriptor(void);
        bool setDescriptor(void);
        bool getConfiguration(void);
        bool setConfiguration(void);
        bool getInterface(void);
        bool setInterface(void);
        bool synchFrame(void);

        // Mass Storage Bulk-Only Requests
        bool getMaxLUN(void);
        bool bomsr(void);  // Bulk-Only Mass Storage Reset

        // Helpers to verify and return request endpoint
        EndPoint * reqEP(void);  // Checks for any endpoint type
        EndPoint * reqEP(type_e type);  // Checks for specific endpoint type
        EndPoint * req2ep(dir_e & ep_dir);
};

////////////////////////////////////////////////////////////////////////////////
// BulkOnlyIface ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class BulkOnlyIface
{
    public:
        enum status_e : uint8_t { PASSED, FAILED, PHASE_ERROR };
        enum state_e : uint8_t { COMMAND, DATA, STATUS };

        struct CBW
        {
            uint32_t dCBWSignature;
            uint32_t dCBWTag;
            uint32_t dCBWDataTransferLength;
            uint8_t bmCBWFlags;
            uint8_t bCBWLUN : 4;
            uint8_t rsvd1   : 4;
            uint8_t bCBWCBLength : 5;
            uint8_t rsvd2        : 3;
            uint8_t CBWCB[16];

        } __attribute__ ((packed));

        struct CSW
        {
            uint32_t const dCSWSignature = _s_status_signature;
            uint32_t dCSWTag;
            uint32_t dCSWDataResidue;
            uint8_t bCSWStatus;

        } __attribute__ ((packed));

        ////////////////////////////////////////////////////////////////////////

        BulkOnlyIface(void) = default;

        void process(void);
        void reset(void) { _state = COMMAND; }
        void enable(void);
        void disable(void);

    private:
        state_e _state = COMMAND;
        status_e _status = PASSED;
        EndPoint::dir_e _data_dir;
        uint32_t _tag = 0;
        uint32_t _transfer_length = 0;
        uint32_t _transferred = 0;

        using BulkOutEP = BulkPipe < BULK_OUT_EP_NUM, EndPoint::OUT >;
        using BulkInEP  = BulkPipe < BULK_IN_EP_NUM, EndPoint::IN >;

        BulkOutEP _ep_out;
        BulkInEP _ep_in;

        Scsi _scsi;

        using state_action_t = void (BulkOnlyIface::*)(void);
        state_action_t const _state_actions[3] =
        {
            &BulkOnlyIface::command,
            &BulkOnlyIface::data,
            &BulkOnlyIface::status
        };

        static constexpr uint32_t const _s_command_signature = 0x43425355;
        static constexpr uint32_t const _s_status_signature = 0x53425355;

        void command(void);
        void data(void);
        void status(void);

        int dataOut(void);
        int dataIn(void);
};

////////////////////////////////////////////////////////////////////////////////
// Usb /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
extern "C" void usb_isr(void);

class Usb : public Module
{
    using DefaultPipe = ControlPipe < EndPoint::N0 >;

    friend void usb_isr(void);
    friend DefaultPipe;

    public:
        static Usb & acquire(void) { static Usb usb; return usb; }
        void process(void);
        bool active(void) const { return ((msecs() - _sof_ts) < _s_inactive); }

    private:
        Usb(void);

        void setAddress(uint16_t address);

        DefaultPipe _ep0{*this};
        BulkOnlyIface _iface;

        // Device States
        enum ds_e : uint8_t
        {
            DETACHED,
            ATTACHED,
            POWERED,  // Expect Reset
            DEFAULT,  // Expect Address
            ADDRESS,  // Expect Get Descriptors and Set Configuration
            CONFIGURED,
        };

        // Can Suspend from POWERED to CONFIGURED

        ds_e _state = ATTACHED;
        bool volatile _suspended = false;
        bool _remote_wakeup = false;
        //static constexpr bool const _s_self_powered = true;
        static constexpr bool const _s_self_powered = false;

        uint32_t volatile _sof_ts = msecs();
        static constexpr uint32_t const _s_inactive = 1000;

        // Interrupt handling
        static Usb * _s_usb; // Pointer to instance for ISR
        void isr(void);   // Called with above from ISR

        void usbrst(void);
        void error(void);
        void softok(void);
        void tokdne(void);
        void sleep(void);
        void resume(void);
        void stall(void);

        using usb_isr_handler_t = void (Usb::*)(void);
        usb_isr_handler_t const _isr_handlers[8] =
        {
            &Usb::usbrst,
            &Usb::error,
            &Usb::softok,
            &Usb::tokdne,
            &Usb::sleep,
            &Usb::resume,
            nullptr,
            &Usb::stall
        };

        reg8 _base_reg = (reg8)0x40072000;
        //reg8 _perid        = _base_reg;
        //reg8 _idcomp       = _base_reg + 0x0004;
        //reg8 _rev          = _base_reg + 0x0008;
        reg8 _addinfo      = _base_reg + 0x000C;
        //reg8 _otgistat     = _base_reg + 0x0010;
        //reg8 _otgicr       = _base_reg + 0x0014;
        //reg8 _otgstat      = _base_reg + 0x0018;
        //reg8 _otgctl       = _base_reg + 0x001C;
        reg8 _istat        = _base_reg + 0x0080;
        reg8 _inten        = _base_reg + 0x0084;
        reg8 _errstat      = _base_reg + 0x0088;
        reg8 _erren        = _base_reg + 0x008C;
        reg8 _stat         = _base_reg + 0x0090;
        reg8 _ctl          = _base_reg + 0x0094;
        reg8 _addr         = _base_reg + 0x0098;
        reg8 _bdtpage1     = _base_reg + 0x009C;
        //reg8 _frmnuml      = _base_reg + 0x00A0;
        //reg8 _frmnumh      = _base_reg + 0x00A4;
        //reg8 _token        = _base_reg + 0x00A8;
        //reg8 _softhld      = _base_reg + 0x00AC;
        reg8 _bdtpage2     = _base_reg + 0x00B0;
        reg8 _bdtpage3     = _base_reg + 0x00B4;
        //reg8 _endpt0       = _base_reg + 0x00C0;
        //reg8 _endpt1       = _base_reg + 0x00C4;
        //reg8 _endpt2       = _base_reg + 0x00C8;
        //reg8 _endpt3       = _base_reg + 0x00CC;
        //reg8 _endpt4       = _base_reg + 0x00D0;
        //reg8 _endpt5       = _base_reg + 0x00D4;
        //reg8 _endpt6       = _base_reg + 0x00D8;
        //reg8 _endpt7       = _base_reg + 0x00DC;
        //reg8 _endpt8       = _base_reg + 0x00E0;
        //reg8 _endpt9       = _base_reg + 0x00E4;
        //reg8 _endpt10      = _base_reg + 0x00E8;
        //reg8 _endpt11      = _base_reg + 0x00EC;
        //reg8 _endpt12      = _base_reg + 0x00F0;
        //reg8 _endpt13      = _base_reg + 0x00F4;
        //reg8 _endpt14      = _base_reg + 0x00F8;
        //reg8 _endpt15      = _base_reg + 0x00FC;
        reg8 _usbctrl      = _base_reg + 0x0100;
        //reg8 _observe      = _base_reg + 0x0104;
        reg8 _control      = _base_reg + 0x0108;
        reg8 _usbtrc0      = _base_reg + 0x010C;
        //reg8 _usbfrmadjust = _base_reg + 0x0114;
};

////////////////////////////////////////////////////////////////////////////////
// UsbPacket ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < uint16_t S, uint8_t N >
uint32_t volatile UsbPacket < S, N >::_s_available = UsbPacket < S, N >::_s_max_available;

template < uint16_t S, uint8_t N >
UsbPacket < S, N > UsbPacket < S, N >::_s_packets[N];

template < uint16_t S, uint8_t N >
EndPoint * UsbPacket < S, N >::_s_eps[EP_NUM_CNT] = {};

template < uint16_t S, uint8_t N >
uint32_t volatile UsbPacket < S, N >::_s_ep_need[EP_NUM_CNT] = {};

template < uint16_t S, uint8_t N >
uint32_t volatile UsbPacket < S, N >::_s_need = 0;

template < uint16_t S, uint8_t N >
UsbPacket < S, N > * UsbPacket < S, N >::acquire(void)
{
    if (available())
        return _acquire();

    return nullptr;
}

template < uint16_t S, uint8_t N >
UsbPacket < S, N > * UsbPacket < S, N >::acquire(EndPoint & ep)
{
    if (available())
        return _acquire();

    if ((ep._type == EndPoint::CONTROL) || (ep._dir == EndPoint::OUT))
    {
        if (_s_eps[ep._num] == nullptr)
            _s_eps[ep._num] = &ep;

        _s_ep_need[ep._num]++;
        _s_need++;
    }

    return nullptr;
}

template < uint16_t S, uint8_t N >
UsbPacket < S, N > * UsbPacket < S, N >::_acquire(void)
{
    uint8_t index = __builtin_ctz(_s_available);
    _s_available &= ~(1 << index);

    _s_packets[index].count = 0;

    return &_s_packets[index];
}

template < uint16_t S, uint8_t N >
void UsbPacket < S, N >::release(UsbPacket * p)
{
    if (p == nullptr)
        return;

    uint8_t index = p - &_s_packets[0];
    if (index >= N) return;

    if (!_give(p))
        _s_available |= (1 << index);
}

template < uint16_t S, uint8_t N >
bool UsbPacket < S, N >::_give(UsbPacket * p)
{
    if (_s_need == 0)
        return false;

    for (uint8_t i = 0; i < EP_NUM_CNT; i++)
    {
        if ((_s_ep_need[i] != 0) && (_s_eps[i] != nullptr))
        {
            if (_s_eps[i]->give(p))
            {
                _s_ep_need[i]--;
                _s_need--;
                return true;
            }

            // If the EndPoint doesn't need this one, assume
            // it doesn't need any more.
            _s_need -= _s_ep_need[i];
            _s_ep_need[i] = 0;
        }
    }

    return false;
}

template < uint16_t S, uint8_t N >
uint16_t UsbPacket < S, N >::set(uint8_t const * data, uint16_t dlen)
{
    uint16_t cpy = dlen;
    if (cpy > S) cpy = S;
    memcpy(buffer, data, cpy); count = cpy;
    return cpy;
}

////////////////////////////////////////////////////////////////////////////////
// BulkPipe < OUT > ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < EndPoint::num_e N >
void BulkPipe < N, EndPoint::OUT >::isr(dir_e dir, bank_e bank)
{
    // This shouldn't happen if corresponding ENDPTn register
    // is set up correctly
    //if (dir != _dir) { stall(); return; }

    BD * bd = _bd[bank];

    UsbPkt * p = (UsbPkt *)bd->addr;
    p->count = bd->count();

    // Will be able to queue packet since queue size is >= number of
    // available packets.
    (void)_pkt_queue.enqueue(p);

    p = UsbPkt::acquire(*this);

    // If can't get a new packet to put in buffer descriptor, keep ownership
    // and don't allocate new buffer.
    if (p == nullptr)
    {
        bd->clear();
        return;
    }

    bd->set(p->buffer, p->size, _dataX);

    _dataX ^= 1;
    _bank ^= 1;
}

////////////////////////////////////////////////////////////////////////////////
// BulkPipe < IN > /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < EndPoint::num_e N >
void BulkPipe < N, EndPoint::IN >::isr(dir_e dir, bank_e bank)
{
    // This shouldn't happen if corresponding ENDPTn register
    // is set up correctly
    //if (dir != _dir) { stall(); return; }

    // Get buffer descriptor for finished send
    BD * bd = _bd[bank];

    UsbPkt * p = (UsbPkt *)bd->addr;
    UsbPkt::release(p);

    bd->clear();

    // See if there are any packets in queue
    if (_pkt_queue.isEmpty())
        return;

    (void)_pkt_queue.dequeue(p);

    // Get buffer descriptor for next send
    bd = _bd[_bank];

    bd->set(p->buffer, p->count, _dataX);

    _dataX ^= 1;
    _bank ^= 1;
}

template < EndPoint::num_e N >
bool BulkPipe < N, EndPoint::IN >::send(UsbPkt * p)
{
    if (p == nullptr)
        return false;

    BD * bd = _bd[_bank];

    if (bd->addr != nullptr)
        return _pkt_queue.enqueue(p);

    bd->set(p->buffer, p->count, _dataX);

    _dataX ^= 1;
    _bank ^= 1;

    return true;
}

#endif
