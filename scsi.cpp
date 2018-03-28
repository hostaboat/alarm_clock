#include "scsi.h"
#include "types.h"
#include <string.h>

Scsi::Scsi(void)
{
    memcpy(_si.vendor_id, _s_vendor_id, sizeof(_si.vendor_id));
    memcpy(_si.prod_id, _s_prod_id, sizeof(_si.prod_id));
    memcpy(_si.prod_rev, _s_prod_rev, sizeof(_si.prod_rev));
}

int Scsi::request(uint8_t * req, uint8_t rlen)
{
    enum type_e { FIXED_6, FIXED_10, FIXED_12, FIXED_16, VARIABLE, EXTENDED, VENDOR, RESERVED };

    auto cdb_type = [](uint8_t op_code) -> type_e
    {
        // Taken from SPC-5 4.2.5.1 Common CDB Fields / Operation Code
        if      (op_code <= 0x1F) return FIXED_6;
        else if (op_code <= 0x5F) return FIXED_10;
        else if (op_code <= 0x7D) return RESERVED;
        else if (op_code == 0x7E) return EXTENDED;
        else if (op_code == 0x7F) return VARIABLE;
        else if (op_code <= 0x9F) return FIXED_16;
        else if (op_code <= 0xBF) return FIXED_12;
        else                      return VENDOR;
    };

    auto cdb_length = [](type_e type) -> uint8_t
    {
        switch (type)
        {
            case FIXED_6:  return 6;
            case FIXED_10: return 10;
            case FIXED_12: return 12;
            case FIXED_16: return 16;
            default:       return 0;
        }
    };

    if ((req == nullptr) || (rlen == 0))
        return SCSI_ERROR;

    _op_code = (op_code_e)req[0];
    uint8_t clen = cdb_length(cdb_type(_op_code));

    // If length is 0, then command isn't supported.
    //
    // There doesn't seem to be an error code for Op Code / CDB length mismatch
    // so just use invalid command.
    //
    // However, Windows OS for some reason uses CDB length of 12 instead of 6 for
    // REQUEST SENSE (0x03), so just make sure there is an adequate amount of data.
    if ((clen == 0) || (rlen < clen))
    {
        invalidCommand();
        return SCSI_FAILED;
    }

    // NACA bit in Control field not supported
    // Control field is last byte in CDB
    if (req[clen - 1] & (1 << 2))
    {
        invalidBitField(clen - 1, 2);
        return SCSI_FAILED;
    }

    _doff = _transferred = 0;

    int status;

    switch (_op_code)
    {
        case TEST_UNIT_READY:
            status = testUnitReady(req);
            break;
        case REQUEST_SENSE:
            status = requestSense(req);
            break;
        case INQUIRY:
            status = inquiry(req);
            break;
        case MODE_SELECT_6:
        case MODE_SELECT_10:
            status = modeSelect(req);
            break;
        case MODE_SENSE_6:
        case MODE_SENSE_10:
            status = modeSense(req);
            break;
        case READ_FORMAT_CAPACITIES:
            status = readFormatCapacities(req);
            break;
        case READ_CAPACITY_10:
            status = readCapacity10(req);
            break;
        case READ_10:
            status = read10(req);
            break;
        case WRITE_10:
            status = write10(req);
            break;
        case REPORT_LUNS:
            status = reportLuns(req);
            break;
        default:  // Already checked above, but get rid of warning
            invalidCommand();
            return SCSI_FAILED;
    }

    if (status != 0)
        return status;

    return rlen;
}

int Scsi::testUnitReady(uint8_t * req)
{
    _transfer_length = 0;
    return 0;
}

int Scsi::requestSense(uint8_t * req)
{
    if (req[1] & (1 << 0))  // DESC - Descriptor format not supported
    {
        invalidBitField(1, 0);
        return SCSI_FAILED;
    }

    if (!_have_sense)
        noSense();

    uint8_t alloc_len = req[4];

    if (alloc_len < _transfer_length)
        _transfer_length = alloc_len;

    return 0;
}

int Scsi::inquiry(uint8_t * req)
{
    // Command Support Data - CmdDt - not supported
    if (req[1] & (1 << 1))
    {
        invalidBitField(1, 1); 
        return SCSI_FAILED;
    }

    bool evpd = req[1] & (1 << 0);  // Enable Vital Product Data
    uint8_t page_code = req[2];

    // If EVPD bit is not set, PAGE CODE must be 0
    if (!evpd && (page_code != 0x00))
    {
        invalidField(2);
        return SCSI_FAILED;
    }

    if (evpd)
    {
        VPD * vpd = nullptr;

        switch (page_code)
        {
            case SUPPORTED_PAGES:
                vpd = &_vpd_sp;
                break;
            case UNIT_SERIAL_NUMBER:
                vpd = &_vpd_us;
                break;
            case BLOCK_DEVICE_CHARACTERISTICS:
                vpd = &_vpd_bdc;
                break;
            default:
                break;
        }

        if (vpd == nullptr)
        {
            invalidField(2);
            return SCSI_FAILED;
        }

        memcpy(_dbuf, vpd, vpd->size());
        _transfer_length = vpd->size();
    }
    else
    {
        memcpy(_dbuf, &_si, sizeof(_si));
        _transfer_length = sizeof(_si);
    }

    uint16_t alloc_len = ntohs(*(uint16_t *)&req[3]);

    if (alloc_len < _transfer_length)
        _transfer_length = alloc_len;

    return 0;
}

int Scsi::modeSelect(uint8_t * req)
{
    // SP bit - Save pages
    if (req[1] & (1 << 0))
    {
        invalidBitField(1, 0);
        return SCSI_FAILED;
    }

    // PF bit - parameters are vendor specific
    if (req[1] & (1 << 4))
    {
        invalidBitField(1, 4);
        return SCSI_FAILED;
    }

    uint16_t param_list_len;

    if (_op_code == MODE_SELECT_6)
        param_list_len = req[4];
    else  // MODE_SELECT_10
        param_list_len = ntohs(*(uint16_t *)&req[7]);

    // RTD bit - Revert to defaults
    bool rtd = req[1] & (1 << 1);
    if (rtd && (param_list_len != 0))
    {
        invalidBitField(1, 1);
        return SCSI_FAILED;
    }

    if (param_list_len > sizeof(_dbuf))
    {
        errorParamListLen();
        return SCSI_FAILED;
    }

    _transfer_length = param_list_len;

    return 0;
}

int Scsi::modeSense(uint8_t * req)
{
    // DBD (Disable Block Descriptors) bit
    bool dbd = req[1] & (1 << 3);

    // Long LBA Accepted - only using short lba block descriptor
    //if ((_op_code == MODE_SENSE_10) && (req[1] & (1 << 4))) { }

    // Page Control values
    enum pc_e : uint8_t { CURRENT, CHANGEABLE, DEFAULT, SAVED };

    pc_e page_control = (pc_e)(req[2] >> 6);

    if (page_control == CHANGEABLE)
    {
        invalidBitField(2, 6);
        return SCSI_FAILED;
    }
    else if (page_control == SAVED)
    {
        noSaveSupport();
        return SCSI_FAILED;
    }

    // Only 0x00 subpage code
    if (req[3] != 0x00)
    {
        invalidField(3);
        return SCSI_FAILED;
    }

    uint16_t offset;

    if (_op_code == MODE_SENSE_6)
        offset = sizeof(ModePage6);
    else  // MODE_SENSE_10
        offset = sizeof(ModePage10);

    if (!dbd)
        offset += sizeof(ModePageShortBD);

    uint8_t page_code = req[2] & 0x3F;

    switch (page_code)
    {
        case READ_WRITE_ERROR_RECOVERY:
            memcpy(_dbuf + offset, &_mp_rwer, sizeof(_mp_rwer));
            offset += sizeof(_mp_rwer);
            break;

        case FLEXIBLE_DISK:
            memcpy(_dbuf + offset, &_mp_fd, sizeof(_mp_fd));
            offset += sizeof(_mp_fd);
            break;

        case CACHING:
            memcpy(_dbuf + offset, &_mp_caching, sizeof(_mp_caching));
            offset += sizeof(_mp_caching);
            break;

        case INFORMATIONAL_EXCEPTIONS_CONTROL:
            memcpy(_dbuf + offset, &_mp_iec, sizeof(_mp_iec));
            offset += sizeof(_mp_iec);
            break;

        case ALL_SUPPORTED_PAGES:
            for (uint8_t i = 0; i < _s_num_mode_pages; i++)
            {
                if (_mode_pages[i] == nullptr)
                    continue;

                ModePage * mp = _mode_pages[i];
                uint8_t mp_len = mp->page_length + sizeof(ModePage);

                memcpy(_dbuf + offset, mp, mp_len);
                offset += mp_len;
            }
            break;

        default:
            invalidBitField(2, 0);
            return SCSI_FAILED;
    }

    _transfer_length = offset;

    if (_op_code == MODE_SENSE_6)
    {
        ModePage6 mp6;

        mp6.mode_data_length = _transfer_length - 1;
        mp6.block_descriptor_length = dbd ? 0 : sizeof(ModePageShortBD);

        memcpy(_dbuf, &mp6, sizeof(mp6));

        offset = sizeof(mp6);
    }
    else  // MODE_SENSE_10
    {
        ModePage10 mp10;

        mp10.mode_data_length = htons(_transfer_length - 2);
        mp10.block_descriptor_length = dbd ? 0 : htons(sizeof(ModePageShortBD));

        memcpy(_dbuf, &mp10, sizeof(mp10));

        offset = sizeof(mp10);
    }

    if (!dbd)
        memcpy(_dbuf + offset, &_mpbd, sizeof(_mpbd));

    uint16_t alloc_len;

    if (_op_code == MODE_SENSE_6)
        alloc_len = req[4];
    else  // mode_sense_10
        alloc_len = ntohs(*(uint16_t *)&req[7]);

    if (alloc_len < _transfer_length)
        _transfer_length = alloc_len;

    return 0;
}

int Scsi::readFormatCapacities(uint8_t * req)
{
    // Windows wants to know about this even though the Inquiry
    // told it that this is a Direct Access Block Device and
    // not a CD/DVD device.  If CHECK SENSE is returned for this
    // command, i.e. that it failed, Windows does nothing and the
    // transaction hangs, though it should at least send a
    // REQUEST SENSE command.

    struct RFC
    {
        uint8_t const r0 = 0;
        uint8_t const r1 = 0;
        uint8_t const r2 = 0;
        uint8_t const capacity_list_length = sizeof(RFC) - 4;

        // Current/Maximum Capacity Descriptor
        uint32_t const number_of_blocks0;
        uint8_t const descriptor_type = FORMATTED;
        uint8_t const unused0 = 0;
        uint16_t const block_length0 = htons(_s_block_size);

        // Formattable Capacity Descriptor
        uint32_t const number_of_blocks1;
        uint8_t const format_type = 0;
        uint8_t const unused1 = 0;
        uint16_t const block_length1 = htons(_s_block_size);

        // Descriptor types
        enum rfc_e : uint8_t { RESERVED, UNFORMATTED, FORMATTED, NO_MEDIA };

        RFC(uint32_t num_blocks)
            : number_of_blocks0(htonl(num_blocks)),
              number_of_blocks1(htonl(num_blocks)) {}

    } __attribute__ ((packed));

    static RFC rfc(_num_blocks);

    memcpy(_dbuf, &rfc, sizeof(rfc));
    _transfer_length = sizeof(rfc);

    uint16_t alloc_len = ntohs(*(uint16_t *)&req[7]);

    if (alloc_len < _transfer_length)
        _transfer_length = alloc_len;

    return 0;
}

int Scsi::readCapacity10(uint8_t * req)
{
    uint32_t last_lba = htonl(_num_blocks - 1);
    uint32_t block_size = htonl(_s_block_size);

    memcpy(_dbuf, &last_lba, 4);
    memcpy(_dbuf + 4, &block_size, 4);

    _transfer_length = 8;

    return 0;
}

int Scsi::read10(uint8_t * req)
{
    // Don't care about DPO and FUA bits or GROUP NUMBER field

    // No read protection so RDPROTECT field must be zero
    if ((req[1] & (7 << 5)) != 0)
    {
        invalidBitField(1, 5);
        return SCSI_FAILED;
    }

    // Rebuild assist mode not supported so RARC bit must be zero
    if (req[1] & (1 << 2))
    {
        invalidBitField(1, 2);
        return SCSI_FAILED;
    }

    uint32_t lba = ntohl(*(uint32_t *)&req[2]);
    uint16_t num_blocks = ntohs(*(uint16_t *)&req[7]);
    uint32_t lba_end = lba + num_blocks;

    if ((lba_end > _num_blocks) || (lba_end <= lba))  // Overflow
    {
        invalidLBA((lba > _num_blocks) ? 2 : 7, _num_blocks - 1);
        return SCSI_FAILED;
    }

    _lba = lba;
    _transfer_length = num_blocks;

    _disk_desc = _dd.open(lba, num_blocks, DD_READ);

    return 0;
}

int Scsi::write10(uint8_t * req)
{
    // Don't care about DPO and FUA bits or GROUP NUMBER field

    // No write protection so WRPROTECT field must be zero
    if ((req[1] & (7 << 5)) != 0)
    {
        invalidBitField(1, 5);
        return SCSI_FAILED;
    }

    uint32_t lba = ntohl(*(uint32_t *)&req[2]);
    uint16_t num_blocks = ntohs(*(uint16_t *)&req[7]);
    uint32_t lba_end = lba + num_blocks;

    if ((lba_end > _num_blocks) || (lba_end <= lba))  // Overflow
    {
        invalidLBA((lba > _num_blocks) ? 2 : 7, _num_blocks - 1);
        return SCSI_FAILED;
    }

    _lba = lba;
    _transfer_length = num_blocks;

    _disk_desc = _dd.open(lba, num_blocks, DD_WRITE);

    return 0;
}

int Scsi::reportLuns(uint8_t * req)
{
    uint8_t report = req[2];  // SELECT REPORT

    switch (report)
    {
        case 0x00: case 0x01: case 0x02: case 0x10: case 0x11: case 0x12: break;
        default: invalidField(2); return SCSI_FAILED;
    }

    _transfer_length = 8;

    if ((report == 0x00) || (report == 0x02) || (report == 0x11))
        _transfer_length += 8;

    memset(_dbuf, 0, _transfer_length);

    // LUN LIST LENGTH = 8
    if (_transfer_length == 16)
    {
        uint32_t lun_list_len = htonl(8);
        memcpy(_dbuf, &lun_list_len, 4);
    }

    uint32_t alloc_len = ntohl(*(uint32_t *)&req[6]);

    if (alloc_len < _transfer_length)
        _transfer_length = alloc_len;

    return 0;
}

int Scsi::read(uint8_t * buf, uint16_t blen)
{
    switch (_op_code)
    {
        case REQUEST_SENSE:
        case INQUIRY:
        case MODE_SENSE_6:
        case MODE_SENSE_10:
        case READ_FORMAT_CAPACITIES:
        case READ_CAPACITY_10:
        case REPORT_LUNS:
            return paramRead(buf, blen);

        case READ_10:
            return dataRead(buf, blen);

        case TEST_UNIT_READY:
        case WRITE_10:
        default:
            break;
    }

    return SCSI_ERROR;
}

int Scsi::write(uint8_t * data, uint16_t dlen)
{
    switch (_op_code)
    {
        case MODE_SELECT_6:
        case MODE_SELECT_10:
            return paramWrite(data, dlen);

        case WRITE_10:
            return dataWrite(data, dlen);

        case TEST_UNIT_READY:
        case REQUEST_SENSE:
        case INQUIRY:
        case READ_FORMAT_CAPACITIES:
        case READ_CAPACITY_10:
        case READ_10:
        case REPORT_LUNS:
        default:
            break;
    }

    return SCSI_ERROR;
}

int Scsi::paramRead(uint8_t * buf, uint16_t blen)
{
    int cpy = _transfer_length - _transferred;

    if (blen < cpy)
        cpy = blen;

    memcpy(buf, _dbuf + _doff, cpy);

    _doff += cpy;
    _transferred += cpy;

    if (done() && (_op_code == REQUEST_SENSE))
        _have_sense = false;

    return cpy;
}

int Scsi::paramWrite(uint8_t * data, uint16_t dlen)
{
    int cpy = _transfer_length - _transferred;

    if (dlen < cpy)
        cpy = dlen;

    memcpy(_dbuf + _doff, data, cpy);

    _doff += cpy;
    _transferred += cpy;

    if (done())
    {
        switch (_op_code)
        {
            case MODE_SELECT_6:
            case MODE_SELECT_10:
                // Don't do anything
                break;

            default:
                break;
        }
    }

    return cpy;
}

void Scsi::dataUpdate(uint16_t n)
{
    _doff += n;

    if (_doff == _s_block_size)
    {
        _doff = 0;
        _transferred++;

        if (done())
        {
            _dd.close(_disk_desc);
            _disk_desc = 0;
        }
    }
}

int Scsi::dataRead(uint8_t * buf, uint16_t blen)
{
    auto read = [&](void) -> bool
    {
        bool ret;
        uint32_t tstart = msecs();
        uint8_t retries = 0;

        while (!(ret = _dd.read(_lba + _transferred, _dbuf)))
        {
            if (++retries == _s_read_retry_count)
                break;

            if ((_s_recovery_time_limit != 0) && ((msecs() - tstart) > _s_recovery_time_limit))
                break;
        }

        if (!ret)
            readError(_lba + _transferred);

        return ret;
    };

    auto readLBA = [&](void) -> bool
    {
        if (++_transferred != _transfer_length)
        {
            if (!read())
                return false;

            _doff = 0;
        }

        return true;
    };

    ////////////////////////////////////////////////////////////////////////////

    if (done()) return 0;

    if (_disk_desc != 0)
    {
        int n = _dd.read(_disk_desc, buf, blen);
        if (n >= 0)
        {
            dataUpdate(n);
            return n;
        }

        _dd.close(_disk_desc);
        _disk_desc = 0;

        if (!read())
            return SCSI_FAILED;
    }

    if ((_transferred == 0) && (_doff == 0) && !read())
        return SCSI_FAILED;

    bool success = true;
    uint16_t n = 0;

    while ((n != blen) && !done())
    {
        uint16_t cpy = sizeof(_dbuf) - _doff;
        if (cpy > (blen - n)) cpy = blen - n;

        memcpy(buf + n, _dbuf + _doff, cpy);
        n += cpy; _doff += cpy;

        if ((_doff == sizeof(_dbuf)) && !(success = readLBA()))
            break;
    }

    return success ? n : SCSI_FAILED;
}

int Scsi::dataWrite(uint8_t * data, uint16_t dlen)
{
    auto write = [&](void) -> bool
    {
        bool success;
        uint32_t tstart = msecs();
        uint8_t retries = 0;

        while (!(success = _dd.write(_lba + _transferred, _dbuf)))
        {
            if (_s_write_retry_count == retries++)
                break;

            if ((_s_recovery_time_limit != 0) && ((msecs() - tstart) > _s_recovery_time_limit))
                break;
        }

        if (!success)
            writeError(_lba + _transferred);

        return success;
    };

    auto writeLBA = [&](void) -> bool
    {
        if (_transferred != _transfer_length)
        {
            if (!write())
                return false;

            _doff = 0;
            _transferred++;
        }

        return true;
    };

    ////////////////////////////////////////////////////////////////////////////

    if (done()) return 0;

    if (_disk_desc != 0)
    {
        int n = _dd.write(_disk_desc, data, dlen);
        if (n >= 0)
        {
            dataUpdate(n);
            return n;
        }

        _dd.close(_disk_desc);
        _disk_desc = 0;

        return SCSI_FAILED;
    }

    bool success = true;
    uint16_t n = 0;

    while ((n != dlen) && !done())
    {
        uint16_t cpy = sizeof(_dbuf) - _doff;
        if (cpy > (dlen - n)) cpy = dlen - n;

        memcpy(_dbuf + _doff, data + n, cpy);
        n += cpy; _doff += cpy;

        if ((_doff == sizeof(_dbuf)) && !(success = writeLBA()))
            break;
    }

    return success ? n : SCSI_FAILED;
}
