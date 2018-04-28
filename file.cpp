#include "file.h"
#include "utility.h"
#include "types.h"

////////////////////////////////////////////////////////////////////////////////
// FileInfo ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
FileInfo::FileInfo(ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    set(type, address, size, parent);
}

FileInfo::FileInfo(chr_t * name, ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    set(name, type, address, size, parent);
}

FileInfo::FileInfo(chr_t * name, uint16_t nlen, ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    set(name, nlen, type, address, size, parent);
}

FileInfo::FileInfo(wchr_t * name, ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    set(name, type, address, size, parent);
}

FileInfo::FileInfo(wchr_t * name, uint16_t nlen, ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    set(name, nlen, type, address, size, parent);
}

FileInfo::FileInfo(String < NS > const & name, ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    set(name, type, address, size, parent);
}

FileInfo & FileInfo::operator =(FileInfo const & rval)
{
    if (this == &rval)
        return *this;

    set(rval._name, rval._type, rval._addr, rval._size, rval._parent);

    return *this;
}

void FileInfo::set(ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    _type = type;
    _addr = address;
    _size = size;
    _parent = parent;
}

void FileInfo::set(chr_t * name, ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    set(type, address, size, parent);
    _name.set(name);
}

void FileInfo::set(chr_t * name, uint16_t nlen, ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    set(type, address, size, parent);
    _name.set(name, nlen);
}

void FileInfo::set(wchr_t * name, ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    set(type, address, size, parent);
    _name.set(name);
}

void FileInfo::set(wchr_t * name, uint16_t nlen, ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    set(type, address, size, parent);
    _name.set(name, nlen);
}

void FileInfo::set(String < NS > const & name, ft_e type, uint32_t address, uint32_t size, uint32_t parent)
{
    set(type, address, size, parent);
    _name = name;
}

FileInfo & FileInfo::operator +=(uint32_t amt)
{
    if (amt > (UINT32_MAX - _size))
        amt = UINT32_MAX - _size;

    _size += amt;

    return *this;
}

int FileInfo::serialize(uint8_t * buf, uint16_t blen) const
{
    if (blen < _s_min_size) 
        return -1;

    int bytes = 0;

    auto copy = [&](void const * src, int amt) -> void
    {
        memcpy(buf + bytes, src, amt);
        bytes += amt;
    };

    copy(&_type, sizeof(_type));
    copy(&_addr, sizeof(_addr));
    copy(&_size, sizeof(_size));
    copy(&_parent, sizeof(_parent));

    uint16_t name_len = _name.len();
    if (name_len > (blen - _s_min_size))
        name_len = blen - _s_min_size;  // Return -1 ?

    copy(&name_len, sizeof(name_len));

    if (name_len != 0)
        copy(_name.str(), name_len);

    return bytes;
}       
        
int FileInfo::deserialize(uint8_t const * buf, uint16_t blen)
{       
    if (blen < _s_min_size)
        return -1;

    int bytes = 0;

    auto copy = [&](void * dst, int amt) -> void
    {
        memcpy(dst, buf + bytes, amt);
        bytes += amt;
    };

    ft_e type;
    copy(&type, sizeof(type));

    switch (type)
    {
        case FT_DIR:
        case FT_REG:
            break;

        default:
            return -1;
    }

    _type = type;

    copy(&_addr, sizeof(_addr));
    copy(&_size, sizeof(_size));
    copy(&_parent, sizeof(_parent));

    uint16_t name_len;
    copy(&name_len, sizeof(name_len));

    if (name_len > (blen - _s_min_size))
        name_len = blen - _s_min_size;  // Return -1 ?

    if (name_len == 0)
        _name = '\0';
    else
        _name.set((chr_t *)(buf + bytes), name_len);

    bytes += name_len;

    return bytes;
}
