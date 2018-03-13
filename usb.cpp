#include "usb.h"
#include "module.h"
#include "armv7m.h"

////////////////////////////////////////////////////////////////////////////////
// Buffer Descriptor Table (BDT) ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
__attribute__ ((section(".usbdescriptortable"), used))
static BD bdt[EP_NUM_CNT * 4] = {};

////////////////////////////////////////////////////////////////////////////////
// EndPoint ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
reg8 EndPoint::_s_endpt0 = (reg8)0x400720C0;
EndPoint * EndPoint::_s_eps[16][2] = {};

EndPoint::EndPoint(num_e num, type_e type, dir_e dir)
    : _num(num), _type(type), _dir(dir), _endptN(_s_endpt0 + (num * 4))
{
    if (_s_eps[_num][_dir] != nullptr)
        return;

    if (_type == CONTROL)
        _s_eps[_num][OUT] = _s_eps[_num][IN] = this;
    else
        _s_eps[_num][_dir] = this;
}

EndPoint::~EndPoint(void)
{
    if (_type == CONTROL)
        _s_eps[_num][OUT] = _s_eps[_num][IN] = nullptr;
    else
        _s_eps[_num][_dir] = nullptr;
}

bool EndPoint::enabled(void)
{
    if (_dir == IN)
        return *_endptN & USB_ENDPTn_EPTXEN;
    else
        return *_endptN & USB_ENDPTn_EPRXEN;
}

void EndPoint::reset(void)
{
    for (uint8_t i = N0; i <= N15; i++)
    {
        EndPoint * ep = _s_eps[i][OUT];
        if (ep != nullptr)
        {
            ep->disable();
            if (ep->_type == EndPoint::CONTROL)
                continue;
        }

        ep = _s_eps[i][IN];
        if (ep != nullptr)
            ep->disable();
    }
}

////////////////////////////////////////////////////////////////////////////////
// UsbDesc /////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
UsbDesc::String const UsbDesc::String::strs[UsbDesc::String::ICNT] =
{
    String(nullptr),
    String(UsbDesc::String::manufacturer_str),
    String(UsbDesc::String::product_str),
    String(UsbDesc::String::serial_number_str)
};

////////////////////////////////////////////////////////////////////////////////
// StreamPipe //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
StreamPipe::StreamPipe(num_e num, type_e type, dir_e dir)
    : EndPoint(num, type, dir),
      _bd{ &bdt[(num << 2) | (dir << 1) | EVEN],
           &bdt[(num << 2) | (dir << 1) |  ODD] }
{
    // Empty
}

void StreamPipe::disable(void)
{
    if ((_dir == OUT) && (*_endptN & USB_ENDPTn_EPTXEN))
        *_endptN &= ~USB_ENDPTn_EPRXEN;
    else if ((_dir == IN) && (*_endptN & USB_ENDPTn_EPRXEN))
        *_endptN &= ~USB_ENDPTn_EPTXEN;
    else
        *_endptN = 0;

    // XXX If two endpoints are using the same endpoint number and EPSTALL
    // may be necessary to figure out which is causing the stall.

    UsbPkt * p;
    while (_pkt_queue.dequeue(p))
        UsbPkt::release(p);

    for (uint8_t i = 0; i < 2; i++)
    {
        if (_bd[i]->addr != nullptr)
            UsbPkt::release((UsbPkt *)_bd[i]->addr);

        _bd[i]->clear();
    }
}

////////////////////////////////////////////////////////////////////////////////
// StreamPipeOut ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void StreamPipeOut::enable(void)
{
    if (enabled()) return;

    UsbPkt * p = UsbPkt::acquire(*this);
    if (p != nullptr)
    {
        _bd[_bank]->set(p->buffer, p->size, DATA0);

        p = UsbPkt::acquire(*this);
        if (p != nullptr)
            _bd[_bank ^ 1]->set(p->buffer, p->size, DATA1);
    }

    _dataX = DATA0;

    *_endptN |= USB_ENDPTn_EPHSHK | USB_ENDPTn_EPRXEN;
}

bool StreamPipeOut::give(UsbPkt * p)
{
    if (_bd[_bank]->addr != nullptr)
        return false;

    _bd[_bank]->set(p->buffer, p->size, _dataX);

    _dataX ^= 1;
    _bank ^= 1;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// StreamPipeIn ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void StreamPipeIn::enable(void)
{
    if (enabled()) return;

    _dataX = DATA0;

    *_endptN |= USB_ENDPTn_EPHSHK | USB_ENDPTn_EPTXEN;
}

bool StreamPipeIn::canSend(void)
{
    return (_bd[0]->addr == nullptr)
        || (_bd[1]->addr == nullptr) || !_pkt_queue.isFull();
}

////////////////////////////////////////////////////////////////////////////////
// MessagePipe /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
MessagePipe::MessagePipe(num_e num)
    : EndPoint(num, CONTROL),
      _bd{ &bdt[(num << 2) | (OUT << 1) | EVEN],
           &bdt[(num << 2) | (OUT << 1) |  ODD],
           &bdt[(num << 2) | (IN  << 1) | EVEN],
           &bdt[(num << 2) | (IN  << 1) |  ODD] }
{
    // Empty
}

void MessagePipe::enable(void)
{
    if (enabled()) return;

    _bd[_rx_bank]->set(_setup_pkt->buffer, _setup_pkt->size, DATA0);
    _state = STATUS;
    _dataX = DATA0;

    *_endptN = USB_ENDPTn_EPHSHK | USB_ENDPTn_EPTXEN | USB_ENDPTn_EPRXEN;
}

void MessagePipe::disable(void)
{
    auto pkt_release = [&](UsbPkt * pkt) -> void
    {
        if ((pkt != _setup_pkt) && (pkt != _status_pkt))
            UsbPkt::release(pkt);
    };

    *_endptN = 0;

    UsbPkt * p;
    while (_rx_queue.dequeue(p))
        pkt_release(p);

    while (_tx_queue.dequeue(p))
        pkt_release(p);

    for (uint8_t i = 0; i < 4; i++)
    {
        if (_bd[i]->addr != nullptr)
            pkt_release((UsbPkt *)_bd[i]->addr);

        _bd[i]->clear();
    }
}

////////////////////////////////////////////////////////////////////////////////
// DefaultPipe /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ControlPipe < EndPoint::N0 >::isr(dir_e dir, bank_e bank)
{
    BD * bd = _bd[(dir << 1) | bank];

    UsbPkt * p = (UsbPkt *)bd->addr;
    uint16_t count = bd->count();
    PID::pid_e pid = bd->pid();

    bd->clear();

    switch (pid)
    {
        case PID::SETUP:
            if (count != sizeof(req_t))
            {
                stall();
                return;
            }

            memcpy(&_req, p->buffer, sizeof(req_t));
            _state = SETUP;
            *_usb._ctl &= ~USB_CTL_TXSUSPENDTOKENBUSY;
            break;

        case PID::OUT:
            // Host-to-device Status transaction
            break;

        case PID::IN:
            if ((_state == STATUS) && _set_address)
            {
                _set_address = false;
                _usb.setAddress(_req.wValue);
            }

            if (p != _status_pkt)
                UsbPkt::release(p);

            if (_tx_queue.dequeue(p))
                tx(p);
            break;

        default:
            stall();
            break;
    }
}

void ControlPipe < EndPoint::N0 >::process(void)
{
    // Not sure how a Setup token can get through if stalled since the MCU
    // is going to return a STALL handshake regardless of what the token is.
    // Assume that a STALL has been sent and the host is trying to send a
    // Setup token as it shall to clear the stall condition.
    if (stalled()) unstall();

    if (_state_actions[_state] != nullptr)
        MFC(_state_actions[_state])();
}

void ControlPipe < EndPoint::N0 >::setup(void)
{
    // USB 2.0 - 9.2.7 Request Error
    // When a request is received by a device that is not defined for the device,
    // is inappropriate for the current setting of the device, or has values that
    // are not compatible with the request, then a Request Error exists. The device
    // deals with the Request Error by returning a STALL PID in response to the next
    // Data stage transaction or in the Status stage of the message. It is preferred
    // that the STALL PID be returned at the next Data stage transaction, as this
    // avoids unnecessary bus activity.

    // Stages
    //  Setup -> Status
    //  Setup -> Data -> Status

    // 0 = Host-to-Device (OUT)
    // 1 = Device-to-Host (IN)
    dir_e dir = _req.dir();

    // 0 = Standard
    // 1 = Class
    // 2 = Vendor
    // 3 = Reserved
    rt_e type = _req.type();

    // 0 = Device
    // 1 = Interface
    // 2 = Endpoint
    // 3 = Other
    // 4..31 = Reserved
    uint8_t recipient = _req.recipient();

    uint8_t code = _req.bRequest;

    if ((type == RESERVED) || (type == VENDOR) || (recipient >= OTHER))
    {
        stall();
        return;
    }

    bool success = false;

    if ((type == STANDARD) && (code <= SYNCH_FRAME) && (_std_reqs[code] != nullptr))
    {
        success = MFC(_std_reqs[code])();
    }
    else if (type == CLASS)
    {
        if (code == BOMSR)
            success = bomsr();
        else if (code == GET_MAX_LUN)
            success = getMaxLUN();
    }

    if (!success)
    {
        stall();
        return;
    }

    // Not supporting SetDescriptor so the only OUT with data is SETUP
    if (dir == OUT)
    {
        send(_status_pkt);
        return;
    }

    _doff = 0;

    // Only send host the amount of data requested
    if (_dlen > _req.wLength)
        _dlen = _req.wLength;

    UsbPkt * p = UsbPkt::acquire();
    if (p == nullptr)
    {
        // If there aren't any packets available just go to DATA state
        // and wait for one there.
        _state = DATA;
        return;
    }

    uint16_t cpy = p->set(_data, _dlen);

    if (!send(p))
    {
        UsbPkt::release(p);

        _state = DATA;
        return;
    }

    _doff += cpy;
}

void ControlPipe < EndPoint::N0 >::data(void)
{
    UsbPkt * p = UsbPkt::acquire();
    if (p == nullptr)
        return;

    bool sent = false;

    // If data sent is divisible by max packet size, have to send a zero
    // length packet to signal end of transaction.
    if ((_doff == _dlen) && ((_dlen % p->size) == 0))
    {
        p->count = 0;
        sent = send(p);
    }
    else
    {
        uint16_t cpy = p->set(_data + _doff, _dlen - _doff);

        if ((sent = send(p)))
            _doff += cpy;
    }

    if (!sent)
        UsbPkt::release(p);
}

bool ControlPipe < EndPoint::N0 >::send(UsbPkt * p)
{
    //if (p == nullptr)
    //    return false;

    if (!isFreeBD(IN, (bank_e)_tx_bank))
        return _tx_queue.enqueue(p);

    tx(p);

    return true;
}

void ControlPipe < EndPoint::N0 >::tx(UsbPkt * p)
{
    //if (p == nullptr)
    //    return;

    switch (_state)
    {
        case SETUP:
            if ((_req.dir() == OUT) && ((p != _status_pkt) || (p->count != 0)))
                return;

            if (p->count < p->size)
                _state = STATUS;
            else
                _state = DATA;
            break;

        case DATA:
            if (p->count < p->size)
                _state = STATUS;
            break;

        case STATUS:
            return;
    }

    BD * bd = _bd[(IN << 1) | _tx_bank];

    _tx_bank ^= 1;
    _dataX ^= 1;

    bd->set(p->buffer, p->count, _dataX);

    if (_state == DATA)
        return;

    // Above packet is either a zero-length Status packet or last Data IN packet.

    _rx_bank ^= 1;
    bd = _bd[(OUT << 1) | _rx_bank];

    if (_req.dir() == OUT)
    {
        // If Data direction is OUT, i.e host-to-device then the packet sent in
        // is for the Status stage.  Expect next OUT to start a new Setup stage.

        _dataX = 0;  // DATA0 for next Setup stage
        bd->set(_setup_pkt->buffer, _setup_pkt->size, _dataX);
    }
    else
    {
        // If Data direction is IN, i.e  device-to-host then the packet sent in
        // is the last of the Data stage.  Expect next OUT to be Status from host.

        _dataX = 1;  // DATA1 for host-to-device Status stage
        bd->set(_status_pkt->buffer, _status_pkt->size, _dataX);

        // Also the packet after the host sends the status packet will start a
        // new Setup stage.

        _rx_bank ^= 1;
        bd = _bd[(OUT << 1) | _rx_bank];

        _dataX = 0;
        bd->set(_setup_pkt->buffer, _setup_pkt->size, _dataX);
    }
}

EndPoint * ControlPipe < EndPoint::N0 >::reqEP(void)
{
    dir_e ep_dir;
    EndPoint * ep = req2ep(ep_dir);

    if ((ep == nullptr) || ((ep->type() != CONTROL) && (ep->dir() != ep_dir)))
        return nullptr;

    return ep;
}

EndPoint * ControlPipe < EndPoint::N0 >::reqEP(type_e type)
{
    dir_e ep_dir;
    EndPoint * ep = req2ep(ep_dir);

    if ((ep == nullptr) || (ep->type() != type) || (ep->dir() != ep_dir))
        return nullptr;

    return ep;
}

EndPoint * ControlPipe < EndPoint::N0 >::req2ep(dir_e & ep_dir)
{
    num_e ep_num = (num_e)(_req.wIndex & 0x000F);
    ep_dir = (dir_e)((_req.wIndex & 0x0080) >> 7);

    if (ep_num >= EP_NUM_CNT)
        return nullptr;

    return EndPoint::get(ep_num, ep_dir);
}

bool ControlPipe < EndPoint::N0 >::getStatus(void)
{
    uint8_t recipient = _req.recipient();

    // Request Error
    if ((_req.dir() != IN)
            || ((recipient != DEVICE) && (recipient != INTERFACE) && (recipient != ENDPOINT))
            || ((_usb._state == Usb::ADDRESS)
                && ((recipient == INTERFACE) || ((recipient == ENDPOINT) && (_req.wIndex != 0)))))
    {
        return false;
    }

    // Unspecified device behavior
    if ((_usb._state == Usb::DEFAULT)
            || (_req.wValue != 0) || (_req.wLength != 2)
            || ((recipient == DEVICE) && (_req.wIndex != 0)))
    {
        return false;
    }

    uint8_t status;

    if (recipient == DEVICE)
    {
        status = ((uint8_t)_usb._remote_wakeup << 1) | _usb._s_self_powered;
    }
    else if (recipient == INTERFACE)
    {
        if (_req.wIndex != UsbDesc::Interface::iface_number)
            return false;

        // For some reason this is valid and zero is returned
        status = 0;
    }
    else if (recipient == ENDPOINT)
    {
        EndPoint * ep = reqEP();
        if (ep == nullptr)
            return false;

        status = (uint8_t)ep->stalled();
    }

    _data[0] = status;
    _data[1] = 0;
    _dlen = 2;

    return true;
}

bool ControlPipe < EndPoint::N0 >::clearFeature(void)
{
    uint8_t recipient = _req.recipient();

    // Request Error
    // Note: There aren't any Standard Feature Selectors for interfaces
    if ((_req.dir() != OUT)
            || ((recipient != DEVICE) && (recipient != ENDPOINT))
            || ((_usb._state == Usb::ADDRESS)
                && ((recipient == INTERFACE) || ((recipient == ENDPOINT) && (_req.wIndex != 0)))))
    {
        return false;
    }

    // Unspecified device behavior
    if ((_usb._state == Usb::DEFAULT) || (_req.wLength != 0))
        return false;

    if (recipient == DEVICE)
    {
        if (_req.wValue != DEVICE_REMOTE_WAKEUP)
            return false;

        _usb._remote_wakeup = false;
    }
    else  // ENDPOINT
    {
        EndPoint * ep = reqEP();
        if ((ep == nullptr) || (ep->num() == EndPoint::N0) || (_req.wValue != ENDPOINT_HALT))
            return false;

        ep->unstall();
    }

    return true;
}

bool ControlPipe < EndPoint::N0 >::setFeature(void)
{
    uint8_t recipient = _req.recipient();

    // Request Error
    // There aren't any Standard Feature Selectors for interfaces
    if ((_req.dir() != OUT)
            || ((recipient != DEVICE) && (recipient != ENDPOINT))
            || ((_usb._state == Usb::ADDRESS)
                && ((recipient == INTERFACE) || ((recipient == ENDPOINT) && (_req.wIndex != 0)))))
    {
        return false;
    }

    // Unspecified device behavior
    if ((_usb._state == Usb::DEFAULT) || (_req.wLength != 0))
    {
        // Must be able to accept TEST_MODE in Default state, but since this is
        // not a high speed device it's not valid.
        return false;
    }

    if (recipient == DEVICE)
    {
        // Test mode only supported for high speed devices and since this
        // is a full speed device return Request Error
        if (_req.wValue != DEVICE_REMOTE_WAKEUP)
            return false;

        _usb._remote_wakeup = true;
    }
    else  // ENDPOINT
    {
        // It's not required or recommended that the Halt feature be
        // implemented for the Default Control Pipe.

        EndPoint * ep = reqEP();
        if ((ep == nullptr) || (ep->num() == EndPoint::N0) || (_req.wValue != ENDPOINT_HALT))
            return false;

        ep->stall();
    }

    return true;
}

bool ControlPipe < EndPoint::N0 >::setAddress(void)
{
    // Request Error
    if ((_req.dir() != OUT) || (_req.recipient() != DEVICE))
        return false;

    // The device behavior for receiving an invalid value is "unspecified"
    // but the MCU register is only set up to hold 7 bits of address with
    // the MSb in the register being a Low Speed Enable bit.
    if (_req.wValue > 127)
        return false;

    // Unspecified device behavior
    if ((_usb._state == Usb::CONFIGURED) || (_req.wIndex != 0) || (_req.wLength != 0))
        return false;

    // The USB device does not change its device address until the
    // Status stage of this request is successfully completed.

    if ((_usb._state == Usb::ADDRESS)
            || ((_usb._state == Usb::DEFAULT) && (_req.wValue != 0)))
    {
        _set_address = true;
    }

    return true;
}

bool ControlPipe < EndPoint::N0 >::getDescriptor(void)
{
    uint8_t type = _req.wValue >> 8;
    uint8_t index = _req.wValue & 0x00FF;

    // Request Error
    if ((_req.dir() != IN) || (_req.recipient() != DEVICE)
            || ((type != UsbDesc::CONFIGURATION) && (type != UsbDesc::STRING)
                && ((type != UsbDesc::DEVICE) || (index != 0))))
    {
        return false;
    }

    if (type == UsbDesc::DEVICE)
    {
        static UsbDesc::Device dev;
        _dlen = dev.bLength;
        memcpy(_data, &dev, _dlen);
    }
    else if (type == UsbDesc::STRING)
    {
        if ((index >= UsbDesc::String::ICNT)
                || ((_req.wIndex != 0) && (_req.wIndex != UsbDesc::String::lang_id)))
            return false;

        _dlen = UsbDesc::String::strs[index].bLength;
        memcpy(_data, &UsbDesc::String::strs[index], _dlen);
    }
    else  // CONFIGURATION
    {
        if (index != (UsbDesc::Configuration::conf_value - 1))
            return false;

        static UsbDesc::BulkOnlyConfiguration boc;
        _dlen = boc.conf.wTotalLength;
        memcpy(_data, &boc, _dlen);
    }

    return true;
}

bool ControlPipe < EndPoint::N0 >::setDescriptor(void)
{
    // Unspecified device behavior
    if (_usb._state == Usb::DEFAULT)
        return false;

    // This is an optional request - don't support it
    return false;
}

bool ControlPipe < EndPoint::N0 >::getConfiguration(void)
{
    // Request Error
    if ((_req.dir() != IN) || (_req.recipient() != DEVICE))
        return false;

    // Unspecified device behavior
    if ((_usb._state == Usb::DEFAULT)
            || (_req.wValue != 0) || (_req.wIndex != 0) || (_req.wLength != 1))
    {
        return false;
    }

    // Value of zero must be returned if in Address state
    uint8_t conf_value = 0;

    if (_usb._state == Usb::CONFIGURED)
        conf_value = UsbDesc::Configuration::conf_value;

    _data[0] = conf_value;
    _dlen = 1;

    return true;
}

bool ControlPipe < EndPoint::N0 >::setConfiguration(void)
{
    // Configuring a device or changing an alternate setting causes all of the
    // status and configuration values associated with endpoints in the affected
    // interfaces to be set to their default values. This includes setting the
    // data toggle of any endpoint using data toggles to the value DATA0.

    // Request Error
    if ((_req.dir() != OUT) || (_req.recipient() != DEVICE))
        return false;

    // Unspecified device behavior
    if ((_usb._state == Usb::DEFAULT)
            || (((_req.wValue & 0xFF00) != 0) || (_req.wIndex != 0) || (_req.wLength != 0)))
    {
        return false;
    }

    uint8_t conf_value = _req.wValue & 0x00FF;

    if (_usb._state == Usb::ADDRESS)
    {
        if (conf_value == 0)
            return true;
        else if (conf_value != UsbDesc::Configuration::conf_value)
            return false;

        _usb._state = Usb::CONFIGURED;
        _usb._iface.enable();
    }
    else if (_usb._state == Usb::CONFIGURED)
    {
        if (conf_value == 0)
            _usb._state = Usb::ADDRESS;
        else if (conf_value != UsbDesc::Configuration::conf_value)
            return false;

        _usb._iface.disable();
        if (_usb._state == Usb::CONFIGURED)
            _usb._iface.enable();
    }

    return true;
}

bool ControlPipe < EndPoint::N0 >::getInterface(void)
{
    // Request Error
    if ((_req.dir() != IN) || (_req.recipient() != INTERFACE) || (_usb._state == Usb::ADDRESS))
        return false;

    // Unspecified device behavior
    if ((_usb._state == Usb::DEFAULT) || (_req.wValue != 0) || (_req.wLength != 1))
        return false;

    if (_req.wIndex != UsbDesc::Interface::iface_number)
        return false;

    _data[0] = UsbDesc::Interface::iface_alt;
    _dlen = 1;

    return true;
}

bool ControlPipe < EndPoint::N0 >::setInterface(void)
{
    // Request Error
    if ((_req.dir() != OUT) || (_req.recipient() != INTERFACE) || (_usb._state == Usb::ADDRESS))
        return false;

    // Unspecified device behavior
    if ((_usb._state == Usb::DEFAULT) || (_req.wLength != 0))
        return false;

    if ((_req.wValue != UsbDesc::Interface::iface_alt)
            || (_req.wIndex != UsbDesc::Interface::iface_number))
    {
        return false;
    }

    // Only one interface and one alternate setting so don't need to do anything.

    return true;
}

bool ControlPipe < EndPoint::N0 >::synchFrame(void)
{
    // This is only for Implicit Feedback Isochronous endpoints

    // Request Error
    if ((_req.dir() != IN) || (_req.recipient() != ENDPOINT) || (_usb._state == Usb::ADDRESS))
        return false;

    // Unspecified device behavior
    if ((_usb._state == Usb::DEFAULT) || (_req.wValue != 0) || (_req.wLength != 2))
        return false;

    if (reqEP(ISOCHRONOUS) == nullptr)
        return false;

    // See Usb 2.0 specification 5.12.4.3 Implicit Feedback

    return false;
}

bool ControlPipe < EndPoint::N0 >::getMaxLUN(void)
{
    // Request Error
    if ((_req.dir() != IN) || (_req.recipient() != INTERFACE))
        return false;

    // Unspecified device behavior
    if ((_req.wValue != 0) || (_req.wLength != 1))
        return false;

    if (_req.wIndex != UsbDesc::Interface::iface_number)
        return false;

    _data[0] = MAX_LUN;
    _dlen = 1;

    return true;
}

bool ControlPipe < EndPoint::N0 >::bomsr(void)
{
    // Request Error
    if ((_req.dir() != OUT) || (_req.recipient() != INTERFACE))
        return false;

    // Unspecified device behavior
    if ((_req.wValue != 0) || (_req.wLength != 0))
        return false;

    if (_req.wIndex != UsbDesc::Interface::iface_number)
        return false;

    // Universal Serial Bus / Mass Storage Class / Bulk-Only Transport - Section 3.1
    // This request is used to reset the mass storage device and its associated interface.
    //
    // This class-specific request shall ready the device for the next CBW from the host.
    //
    // The host shall send this request via the default pipe to the device. The device shall
    // preserve the value of its bulk data toggle bits and endpoint STALL conditions despite
    // the Bulk-Only Mass Storage Reset.
    //
    // The device shall NAK the status stage of the device request until the Bulk-Only Mass
    // Storage Reset is complete.

    _usb._iface.reset();

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// BulkOnlyIface ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void BulkOnlyIface::process(void)
{
    if (_state_actions[_state] != nullptr)
        MFC(_state_actions[_state])();
}

void BulkOnlyIface::enable(void)
{
    if (!_ep_out.enabled())
        _ep_out.enable();

    if (!_ep_in.enabled())
        _ep_in.enable();

    _state = COMMAND;
}

void BulkOnlyIface::disable(void)
{
    if (_ep_out.enabled())
        _ep_out.disable();

    if (_ep_in.enabled())
        _ep_in.disable();
}

void BulkOnlyIface::command(void)
{
    UsbPkt * p;
    if (!_ep_out.recv(p))
        return;

    CBW * cbw = (CBW *)p->buffer;

    if ((p->count != sizeof(CBW)) || (cbw->dCBWSignature != _s_command_signature)
            || (cbw->bCBWCBLength == 0) || (cbw->bCBWCBLength > sizeof(cbw->CBWCB)))
    {
        _ep_in.stall();  // Host expects stall on Bulk-In
        UsbPkt::release(p);
        return;
    }

    _data_dir = (EndPoint::dir_e)(cbw->bmCBWFlags >> 7);
    _tag = cbw->dCBWTag;
    _transfer_length = cbw->dCBWDataTransferLength;
    _transferred = 0;

    int ret = _scsi.request(cbw->CBWCB, cbw->bCBWCBLength);

    if ((ret >= SCSI_PASSED) && (_transfer_length != 0))
        _state = DATA;
    else
        _state = STATUS;

    if (_state == STATUS)
    {
        if (ret == SCSI_FAILED)
            _status = FAILED;
        else if (ret == SCSI_ERROR)
            _status = PHASE_ERROR;
        else
            _status = PASSED;
    }

    UsbPkt::release(p);
}

void BulkOnlyIface::data(void)
{
    int ret;

    if (_data_dir == EndPoint::OUT)
        ret = dataOut();
    else
        ret = dataIn();

    if (ret < SCSI_PASSED)
    {
        _state = STATUS;
        _status = (ret == SCSI_FAILED) ? FAILED : PHASE_ERROR;
    }
    else
    {
        // Sanity check
        if ((uint32_t)ret > (_transfer_length - _transferred))
            ret = (int)(_transfer_length - _transferred);

        _transferred += (uint32_t)ret;

        if ((_transferred == _transfer_length) || _scsi.done())
        {
            _state = STATUS;
            _status = PASSED;
        }
    }
}

int BulkOnlyIface::dataOut(void)
{
    UsbPkt * p;
    if (!_ep_out.recv(p))
        return SCSI_PASSED;

    uint16_t count;
    if ((_transfer_length - _transferred) < p->count)
        count = _transfer_length - _transferred;
    else
        count = p->count;

    int ret = _scsi.write(p->buffer, count);

    UsbPkt::release(p);

    return ret;
}

int BulkOnlyIface::dataIn(void)
{
    if (!_ep_in.canSend())
        return SCSI_PASSED;

    UsbPkt * p = UsbPkt::acquire();
    if (p == nullptr)
        return SCSI_PASSED;

    uint16_t size;
    if ((_transfer_length - _transferred) < p->size)
        size = _transfer_length - _transferred;
    else
        size = p->size;

    int ret = _scsi.read(p->buffer, size);

    if (ret < SCSI_PASSED)
    {
        UsbPkt::release(p);
        return ret;
    }

    p->count = (uint16_t)ret;
    (void)_ep_in.send(p);  // Check done at beginning

    return ret;
}

void BulkOnlyIface::status(void)
{
    if (!_ep_in.canSend())
        return;

    UsbPkt * p = UsbPkt::acquire();
    if (p == nullptr)
        return;

    static CSW csw;

    csw.dCSWTag = _tag;
    csw.dCSWDataResidue = _transfer_length - _transferred;
    csw.bCSWStatus = _status;

    p->set((uint8_t const *)&csw, sizeof(csw));
    (void)_ep_in.send(p);  // Check done at beginning

    _state = COMMAND;
}

////////////////////////////////////////////////////////////////////////////////
// Usb /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void usb_isr(void)
{
    Usb::_s_usb->isr();
}

Usb * Usb::_s_usb = nullptr;

Usb::Usb(void)
{
    if (_s_usb == nullptr)
        _s_usb = this;

    Module::enable(MOD_USBOTG);

    // Reset USB module
    //*_usbtrc0 = USB_USBTRC0_USBRESET;
    // Register always reads as zero and documentation suggests waiting
    // 2 USB clock cycles.
    //delay_usecs(1);

    // Set Buffer Descriptor Table base address
    *_bdtpage1 = USB_BDTPAGE1(bdt);
    *_bdtpage2 = USB_BDTPAGE2(bdt);
    *_bdtpage3 = USB_BDTPAGE3(bdt);

    // Clear ISR flags
    *_istat = 0xFF;
    *_errstat = 0xFF;

    // Enable USB
    *_ctl = USB_CTL_USBENSOFEN;

    // Take USB transceiver out of the suspended state
    // and disable weak pulldowns - both are set on reset.
    //*_usbctrl &= ~(USB_USBCTRL_SUSP | USB_USBCTRL_PDE);
    *_usbctrl = 0;

    // Enable Reset interrupt
    *_inten = USB_INTEN_USBRSTEN;

    NVIC::setPriority(IRQ_USBOTG, 112);
    NVIC::enable(IRQ_USBOTG);

    _state = POWERED;  // XXX Or should it be ATTACHED?

    // Enable D+ pullup for full speed device mode
    // Enabled after enabling interrupt since doing this will signal to host
    // that a device is attached and it will proceed to reset and enumeration.
    *_control = USB_CONTROL_DPPULLUPNONOTG;
}

void Usb::setAddress(uint16_t address)
{
    if (address > 127)
        return;

    if ((_state == DEFAULT) && (address != 0))
    {
        _state = ADDRESS;
        *_addr = (uint8_t)address;
    }
    else if (_state == ADDRESS)
    {
        if (address == 0)
            _state = DEFAULT;

        *_addr = (uint8_t)address;
    }
}

void Usb::isr(void)
{
    uint8_t istat = *_istat;

    do {
        MFC(_isr_handlers[__builtin_ctz(istat)])();
        istat &= istat - 1;
    } while (istat);

    *_istat = 0xFF;
}

void Usb::process(void)
{
    NVIC::disable(IRQ_USBOTG);

    _ep0.process();
    _iface.process();

    NVIC::enable(IRQ_USBOTG);
}

void Usb::usbrst(void)
{
    EndPoint::reset();
    _ep0.enable();

    // Clear any pending interrupts
    *_errstat = 0xFF;
    *_istat = 0xFF;

    // Enable error interrupts
    *_erren =
        USB_ERREN_PIDERREN |
        USB_ERREN_CRC5EOFEN |
        USB_ERREN_CRC16EN |
        USB_ERREN_DFN8EN |
        USB_ERREN_BTOERREN |
        USB_ERREN_DMAERREN |
        USB_ERREN_BTSERREN |
        0;

    // Enable status interrupts
    *_inten =
        USB_INTEN_USBRSTEN |
        USB_INTEN_ERROREN |
        USB_INTEN_SOFTOKEN |
        USB_INTEN_TOKDNEEN |
        USB_INTEN_SLEEPEN |
        USB_INTEN_STALLEN |
        0;

    *_addr = 0;

    // Reset wakes device from SUSPENDED state
    _suspended = false;

    _state = DEFAULT;
}

void Usb::error(void)
{
    // Each bit is set as soon as the error conditions is detected.
    // Therefore, the interrupt does not typically correspond with the
    // end of a token being processed.

    uint8_t error = *_errstat;

    if (error & USB_ERRSTAT_PIDERR) { }
    if (error & USB_ERRSTAT_CRC5EOF) { }
    if (error & USB_ERRSTAT_CRC16) { }
    if (error & USB_ERRSTAT_DFN8) { }
    if (error & USB_ERRSTAT_BTOERR) { }
    if (error & USB_ERRSTAT_DMAERR) { }
    if (error & USB_ERRSTAT_BTSERR) { }

    *_errstat = 0xFF;
}

void Usb::softok(void)
{
    // Sent out once every 1.00 ms += 0x0005 ms for full speed.
    //
    // Full-speed devices that have no particular need for bus timing
    // information may ignore the SOF packet.
    //
    // If device is in the SUSPEND state, operation is resumed

    _suspended = false;
}

void Usb::tokdne(void)
{
    using num_e = EndPoint::num_e;
    using dir_e = EndPoint::dir_e;
    using bank_e = EndPoint::bank_e;

    uint8_t stat = *_stat;

    num_e ep_num = (num_e)USB_STAT_ENDP(stat);
    dir_e ep_dir = (stat & USB_STAT_TX) ? EndPoint::IN : EndPoint::OUT;
    bank_e bd_bank = (stat & USB_STAT_ODD) ? EndPoint::ODD : EndPoint::EVEN;

    EndPoint * ep = EndPoint::get(ep_num, ep_dir);
    if (ep != nullptr)
        ep->isr(ep_dir, bd_bank);
}

void Usb::sleep(void)
{
    // Set if an idle of 3 ms detected.
    // Puts device in SUSPENDED state.
    _suspended = true;

    // USB 2.0 - 7.1.7.7 Resume
    //
    // A device with remote wakeup capability may not generate resume signaling
    // unless the bus has been continuously in the Idle state for 5 ms (TWTRSM).
    // This allows the hubs to get into their Suspend state and prepare for
    // propagating resume signaling. The remote wakeup device must hold the resume
    // signaling for at least 1 ms but for no more than 15 ms (TDRSMUP). At the end
    // of this period, the device stops driving the bus (puts its drivers into the
    // high-impedance state and does not drive the bus to the J state).
    //
    // For remote wakeup to be enabled, a Set Feature request with Feature Selector
    // DEVICE_REMOTE_WAKEUP would have to be processed.
    //
    // The following should be used for resume / remote wakeup
    // delay_ms(2);  // Already 3 ms have elapsed
    // *_s_ctl |= USB_CTL_RESUME;
    // delay(5);  // Between 1 ms and 15 ms
    // *_s_ctl &= ~USB_CTL_RESUME;
    //
    // Enable RESUME interrupt only if remote wakeup is enabled
    // *_s_inten |= USB_INTEN_RESUMEEN;
}

void Usb::resume(void)
{
    _suspended = false;

    // Disable RESUME interrupt only necessary if remote wakeup is enabled
    // *_s_inten &= ~USB_INTEN_RESUMEEN;
}

void Usb::stall(void)
{
    // Asserted when a STALL handshake is sent by the SIE.
    //
    // The actual transmission of data across the physical USB takes places as
    // a serial bit stream. A Serial Interface Engine (SIE), whether implemented
    // as part of the host or a USB device, handles the serialization and
    // deserialization of USB transmissions. On the host, this SIE is part of the
    // Host Controller.
}
