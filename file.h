#ifndef _FILE_H_
#define _FILE_H_

#include "disk.h"
#include "utility.h"
#include "types.h"
#include "rtc.h"
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// FileInfo ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class FileInfo : public Serializable
{
    public:
        // File Type
        enum ft_e : uint16_t { FT_DIR = 1, FT_REG };

        static constexpr uint16_t const NS = 255;

        FileInfo(void) = default;

        FileInfo(ft_e type, uint32_t address, uint32_t size, uint32_t parent);
        FileInfo(chr_t * name, ft_e type, uint32_t address, uint32_t size, uint32_t parent);
        FileInfo(chr_t * name, uint16_t nlen, ft_e type, uint32_t address, uint32_t size, uint32_t parent);
        FileInfo(wchr_t * name, ft_e type, uint32_t address, uint32_t size, uint32_t parent);
        FileInfo(wchr_t * name, uint16_t nlen, ft_e type, uint32_t address, uint32_t size, uint32_t parent);
        FileInfo(String < NS > const & name, ft_e type, uint32_t address, uint32_t size, uint32_t parent);

        FileInfo(FileInfo const & copy) { *this = copy; }
        FileInfo & operator =(FileInfo const & rval);
        FileInfo & operator =(uint32_t size) { _size = size; return *this; }
        FileInfo & operator +=(uint32_t amt);

        void set(ft_e type, uint32_t address, uint32_t size, uint32_t parent);
        void set(chr_t * name, ft_e type, uint32_t address, uint32_t size, uint32_t parent);
        void set(chr_t * name, uint16_t nlen, ft_e type, uint32_t address, uint32_t size, uint32_t parent);
        void set(wchr_t * name, ft_e type, uint32_t address, uint32_t size, uint32_t parent);
        void set(wchr_t * name, uint16_t nlen, ft_e type, uint32_t address, uint32_t size, uint32_t parent);
        void set(String < NS > const & name, ft_e type, uint32_t address, uint32_t size, uint32_t parent);

        // Used for sorting
        friend bool operator ==(FileInfo const & lhs, FileInfo const & rhs) { return lhs._name == rhs._name; }
        friend bool operator >(FileInfo const & lhs, FileInfo const & rhs) { return lhs._name > rhs._name; }
        friend bool operator >=(FileInfo const & lhs, FileInfo const & rhs) { return (lhs > rhs) || (lhs == rhs); }
        friend bool operator !=(FileInfo const & lhs, FileInfo const & rhs) { return !(lhs == rhs); }
        friend bool operator <(FileInfo const & lhs, FileInfo const & rhs) { return !(lhs >= rhs); }
        friend bool operator <=(FileInfo const & lhs, FileInfo const & rhs) { return !(lhs > rhs); }

        static bool isDir(ft_e t) { return t == FT_DIR; }
        static bool isFile(ft_e t) { return t == FT_REG; }

        ft_e type(void) const { return _type; }
        bool isDir(void) const { return _type == FT_DIR; }
        bool isFile(void) const { return _type == FT_REG; }
        uint32_t address(void) const { return _addr; }
        uint32_t size(void) const { return isFile() ? _size : UINT32_MAX; }
        uint32_t parent(void) const { return _parent; }
        String < NS > const & name(void) const { return _name; }

        virtual int serialize(uint8_t * buf, uint16_t blen) const;
        virtual int deserialize(uint8_t const * buf, uint16_t blen);

    private:
        uint32_t _addr = 0;
        uint32_t _size = 0;
        uint32_t _parent = 0;
        ft_e _type = FT_DIR;
        String < NS > _name;

        static constexpr uint16_t const _s_min_size =                         // Name length 
            sizeof(_addr) + sizeof(_size) + sizeof(_parent) + sizeof(_type) + sizeof(uint16_t);
};

static constexpr uint16_t const NS = FileInfo::NS;

////////////////////////////////////////////////////////////////////////////////
// File ////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
enum fst_e  // File System Type
{
    FST_FAT32,
};

enum open_e : uint8_t
{
    O_READ   = 0x01,
    O_WRITE  = 0x02,
    O_RDWR   = 0x03,
    O_CREATE = 0x04,
    O_TRUNC  = 0x08,
};

class File
{
    public:
        virtual int read(uint8_t * buf, int amt) = 0;
        virtual int read(uint8_t const ** p, uint8_t n) = 0;
        virtual int read(FileInfo & info) = 0;
        virtual int write(uint8_t const * buf, int amt, bool flush = true) = 0;
        virtual int write(FileInfo const & info) = 0;
        virtual bool flush(void) = 0;
        virtual bool rewind(void) = 0;

        virtual bool eof(void) const { return _offset >= _info.size(); }
        virtual uint32_t remaining(void) const { return eof() ? 0 : _info.size() - _offset; }
        virtual uint32_t size(void) const { return _info.size(); }
        virtual uint32_t address(void) const { return _info.address(); }
        virtual uint32_t parent(void) const { return _info.parent(); }
        virtual void close(void) { _taken = false; }

        virtual bool canRead(void) const { return _oflags & O_READ; }
        virtual bool canWrite(void) const { return _oflags & O_WRITE; }

        virtual bool isDir(void) const { return _info.isDir(); }
        virtual bool isReg(void) const { return _info.isFile(); }

        virtual bool valid(void) { return _taken; }
        virtual fst_e fs(void) { return _fst; }

        virtual void set(FileInfo const & info, uint8_t oflags = O_READ) {
            _info = info;
            _oflags = oflags;
        }

    protected:
        File(fst_e fst) : _fst(fst) {}
        File(fst_e fst, FileInfo const & info, uint8_t oflags = O_READ)
            : _fst(fst), _info(info), _oflags(oflags) {}

        fst_e _fst;
        FileInfo _info;
        uint32_t _offset = 0;
        uint8_t _oflags = 0;
        bool _taken = false;
};

// DD = DevDisk
template < class DD, fst_e FST >
class TFile : public File
{
    public:
        virtual void close(void) { _dd = nullptr; File::close(); }
        virtual bool valid(void) { return File::valid() && (_dd != nullptr) && _dd->valid(); }

    protected:
        TFile(DD * dd = nullptr) : File(FST), _dd(dd) {}
        TFile(FileInfo const & info, uint8_t oflags = O_READ, DD * dd = nullptr)
            : File(FST, info, oflags), _dd(dd) {}

        DD * _dd = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
// File System /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < class DD, fst_e FST >
class FileSystem
{
    public:
        virtual File * open(uint32_t file_index, uint8_t oflags = O_READ) = 0;
        virtual File * open(FileInfo const & info, uint8_t oflags = O_READ) = 0;

        virtual File * open(chr_t const * name, uint8_t oflags = O_READ) = 0;
        virtual File * open(String < NS > const & name, uint8_t oflags = O_READ) = 0;

        virtual bool valid(void) { return _dd.valid(); }
        virtual bool busy(void) { return _dd.busy(); }
        virtual int sort(char const * const * exts = nullptr) = 0;

    protected:
        class FileSort
        {
            public:
                FileSort(FileSystem & fs);
                int sort(FileInfo const & dir, char const * const * exts);
                int retrieve(uint32_t file_index, FileInfo & info);

            private:
                static constexpr uint16_t const _s_block_size = SD_BLOCK_LEN;

                class WriteList
                {
                    public:
                        WriteList(void) = default;

                        void init(FileInfo * infos, uint16_t num_infos);

                        bool push(FileInfo const & info);
                        void sort(void);
                        int flush(uint32_t space, uint32_t offset);

                        int count(void) const { return _count; }
                        int size(void) const { return _size; }
                        void clear(void) { _count = 0; }

                        bool isFull(void) const { return _count == _size; }
                        bool isEmpty(void) const { return _count == 0; }

                    private:
                        FileInfo * _infos = nullptr;
                        uint16_t _count = 0;
                        uint16_t _size = 0;
                        bool _sorted = false;

                        DD & _dd = DD::acquire();
                };

                class ReadList
                {
                    public:
                        ReadList(void) = default;

                        bool init(FileInfo * infos, uint16_t num_infos,
                                uint32_t expected, uint32_t space, uint32_t offset);

                        bool shift(WriteList & wlist);
                        bool fill(void);

                        int count(void) const { return _count; }
                        int size(void) const { return _size; }
                        int remaining(void) const { return _remaining; }

                        bool isFull(void) const { return (_count == _size) || (_remaining == 0); }
                        bool isEmpty(void) const { return _count == 0; }

                        friend bool operator ==(ReadList const & lhs, ReadList const & rhs) {
                            return lhs._infos[lhs._head] == rhs._infos[rhs._head];
                        }

                        friend bool operator !=(ReadList const & lhs, ReadList const & rhs) {
                            return !(lhs == rhs);
                        }

                        friend bool operator >(ReadList const & lhs, ReadList const & rhs) {
                            return lhs._infos[lhs._head] > rhs._infos[rhs._head];
                        }

                        friend bool operator >=(ReadList const & lhs, ReadList const & rhs) {
                            return (lhs > rhs) || (lhs == rhs);
                        }

                        friend bool operator <(ReadList const & lhs, ReadList const & rhs) {
                            return !(lhs >= rhs);
                        }

                        friend bool operator <=(ReadList const & lhs, ReadList const & rhs) {
                            return !(lhs > rhs);
                        }

                    private:
                        uint16_t _count = 0;
                        uint16_t _size = 0;
                        uint16_t _head = 0;
                        uint16_t _tail = 0;
                        uint32_t _remaining = 0;

                        FileInfo * _infos = nullptr;
                        uint32_t _block = 0;
                        uint32_t _offset = 0;
                        uint16_t _boff = 0;
                        uint8_t _buffer[_s_block_size];

                        DD & _dd = DD::acquire();
                };

                bool read(uint32_t space, uint32_t offset, FileInfo & info);
                bool write(uint32_t space, uint32_t offset, FileInfo const & info);
                bool flush(void);

                bool sort(FileInfo const & dir, char const * const * exts, int sorted_items);

                static uint32_t diskBlock(uint32_t space, uint32_t offset) {
                    return space + (offset / _s_infos_per_block);
                }

                static uint16_t blockOffset(uint32_t offset) {
                    return (offset * _s_info_size) % _s_block_size;
                }

                uint32_t _pp_space[2];
                uint32_t _sorted_space;
                uint32_t _final_space;

                //static constexpr uint16_t const _s_info_size = 64;
                static constexpr uint16_t const _s_info_size = 128;
                static constexpr uint16_t const _s_infos_per_block = _s_block_size / _s_info_size;
                static constexpr uint16_t const _s_num_infos = 64;
                static constexpr uint16_t const _s_write_list_size = _s_num_infos / 4;
                static constexpr uint16_t const _s_num_read_lists = 4;
                static constexpr uint16_t const _s_read_list_size =
                    (_s_num_infos - _s_write_list_size) / _s_num_read_lists;

                static uint8_t _s_rbuffer[_s_block_size];
                static uint8_t _s_wbuffer[_s_block_size];
                uint32_t _rcached = 0;
                uint32_t _wcached = 0;
                uint32_t _files = 0;

                FileSystem & _fs;
        };

        DD & _dd = DD::acquire();
        FileSort _fs{*this};
};

////////////////////////////////////////////////////////////////////////////////
// FileSort ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < class DD, fst_e FST >
uint8_t FileSystem < DD, FST >::FileSort::_s_wbuffer[_s_block_size];

template < class DD, fst_e FST >
uint8_t FileSystem < DD, FST >::FileSort::_s_rbuffer[_s_block_size];

template < class DD, fst_e FST >
FileSystem < DD, FST >::FileSort::FileSort(FileSystem < DD, FST > & fs)
    : _fs(fs)
{
    //  Capacity     Files   Sort Space
    //  -------------------------------
    //      1 GB       256        64 KB
    //      2 GB       512       128 KB
    //      4 GB      1024       256 KB
    //      8 GB      2048       512 KB
    //     16 GB      4096         1 MB
    //     32 GB      8192         2 MB
    //     64 GB     16384         4 MB
    //    128 GB     32768         8 MB
    //    256 GB     65536        16 MB
    //    512 GB    131072        32 MB
    //   1024 GB    262144        64 MB
    //   2048 GB    524288       128 MB

    uint32_t capacity = _fs._dd.capacity();  // In kilobytes
    uint32_t bytes = capacity / 4;
    uint32_t space = _fs._dd.reserve(bytes);
    uint32_t blocks = bytes / _s_block_size;

    auto next = [&](void) -> void { space += blocks / 4; };

    _pp_space[0] = space; next();
    _pp_space[1] = space; next();
    _sorted_space = space; next();
    _final_space = space;
}

template < class DD, fst_e FST >
int FileSystem < DD, FST >::FileSort::sort(FileInfo const & dir, char const * const * exts)
{
    _files = _rcached = _wcached = 0;

    if (!dir.isDir() || !sort(dir, exts, 0))
        return -1;

    _rcached = _wcached = 0;

    return _files;
}

template < class DD, fst_e FST >
int FileSystem < DD, FST >::FileSort::retrieve(uint32_t file_index, FileInfo & info)
{
    if (file_index >= _files)
        return -1;

    uint32_t block = diskBlock(_final_space, file_index);
    if (block != _rcached)
    {
        int err;
        if ((err = _fs._dd.read(block, _s_rbuffer)) <= 0)
            return err;

        _rcached = block;
    }

    return info.deserialize(_s_rbuffer + blockOffset(file_index), _s_info_size);
}

template < class DD, fst_e FST >
bool FileSystem < DD, FST >::FileSort::read(uint32_t space, uint32_t offset, FileInfo & info)
{
    uint32_t block = diskBlock(space, offset);
    if (block != _rcached)
    {
        if (_fs._dd.read(block, _s_rbuffer) < 0)
            return false;

        _rcached = block;
    }

    if (info.deserialize(_s_rbuffer + blockOffset(offset), _s_info_size) < 0)
        return false;

    return true;
}

template < class DD, fst_e FST >
bool FileSystem < DD, FST >::FileSort::write(uint32_t space, uint32_t offset, FileInfo const & info)
{
    uint32_t block = diskBlock(space, offset);
    if (block != _wcached)
    {
        if (!flush() || (_fs._dd.read(block, _s_wbuffer) < 0))
            return false;

        _wcached = block;
    }

    if (info.serialize(_s_wbuffer + blockOffset(offset), _s_info_size) < 0)
        return false;

    return true;
}

template < class DD, fst_e FST >
bool FileSystem < DD, FST >::FileSort::flush(void)
{
    if (_wcached == 0)
        return true;

    if (_fs._dd.write(_wcached, _s_wbuffer) < 0)
        return false;

    _wcached = 0;

    return true;
}

template < class DD, fst_e FST >
bool FileSystem < DD, FST >::FileSort::sort(FileInfo const & dir, char const * const * exts, int sorted_items)
{
    static FileInfo infos[_s_num_infos];
    static FileInfo info;

    auto recurse = [&](uint32_t dir_items) -> bool
    {
        for (uint32_t i = 0; i < dir_items; i++)
        {
            if (!read(_sorted_space, sorted_items + i, info))
                return false;

            if (info.isDir())
            {
                _rcached = 0;

                if (!sort(info, exts, sorted_items + dir_items))
                    return false;
            }
            else
            {
                if (!write(_final_space, _files++, info))
                    return false;
            }
        }

        return flush();
    };

    // Phase 1 /////////////////////////////////////////////////////////////////

    static WriteList wlist;

    File * dp = _fs.open(dir);

    // File may be corrupt - just return true
    if (dp == nullptr)
        return true;

    uint8_t pp_toggle = 0;
    uint32_t ping_lists = 0;
    uint32_t pong_lists = 0;
    uint32_t dir_items = 0;
    uint32_t written_items = 0;

    wlist.init(infos, _s_num_infos);

    auto write_sorted = [&](uint32_t space) -> bool
    {
        if (wlist.isEmpty())
            return true;

        uint32_t offset = (space == _sorted_space) ? sorted_items : written_items;

        wlist.sort();

        int n = wlist.flush(space, offset);
        if (n < 0)
            return false;

        if (space != _sorted_space)
        {
            written_items += n;
            ping_lists++;
        }

        return true;
    };

    int err;

    while ((err = dp->read(info)) > 0)
    {
        String < NS > const & name = info.name();

        if (name[0] == '.')
            continue;

        if ((info.type() == FileInfo::FT_REG) && (exts != nullptr))
        {
            uint8_t i = 0;
            for (; exts[i] != nullptr; i++)
            {
                int index = name.rfind((chr_t *)exts[i]);

                if ((index > 0) && (name[index-1] == '.') && ((name.len() - (uint16_t)strlen(exts[i])) == index))
                    break;
            }

            if (exts[i] == nullptr)
                continue;
        }

        if (wlist.isFull() && !write_sorted(_pp_space[pp_toggle]))
        {
            err = -1;
            break;
        }

        dir_items++;
        wlist.push(info);
    }

    dp->close();

    if (err < 0)
        return false;

    if (!wlist.isEmpty())
    {
        if (ping_lists == 0)  // All items in memory
        {
            if (!write_sorted(_sorted_space))
                return false;

            return recurse(dir_items);
        }
        else
        {
            if (!write_sorted(_pp_space[pp_toggle]))
                return false;
        }
    }

    if (ping_lists == 0)
        return true;

    // Phase 2 /////////////////////////////////////////////////////////////////

    static ReadList rlists[_s_num_read_lists];

    uint32_t num_items = dir_items;
    uint32_t max_items = _s_num_infos;
    uint32_t write_space;

    auto write_init = [&](void) -> void
    {
        if (ping_lists <= _s_num_read_lists)
            write_space = _sorted_space;
        else
            write_space = _pp_space[pp_toggle ^ 1];

        written_items = 0;
    };

    auto write_list = [&](void) -> bool
    {
        if (wlist.isEmpty())
            return true;

        uint32_t offset = written_items;

        if (write_space == _sorted_space)
            offset += sorted_items;

        int n = wlist.flush(write_space, offset);
        if (n < 0)
            return false;

        written_items += n;

        return true;
    };

    write_init();
    wlist.init(infos + (_s_num_read_lists * _s_read_list_size), _s_write_list_size);

    while (ping_lists > 0)
    {
        if (ping_lists == 1)
        {
            if (!rlists[0].init(infos, _s_read_list_size, num_items, _pp_space[pp_toggle], dir_items - num_items))
                return false;

            while (!rlists[0].isEmpty())
            {
                if (!rlists[0].shift(wlist) || (wlist.isFull() && !write_list()))
                    return false;
            }

            pong_lists++;
            ping_lists--;
        }
        else
        {
            MinHeap < ReadList * , _s_num_read_lists > min_heap;
            int i = 0;

            for (; (i < _s_num_read_lists) && (num_items != 0); i++)
            {
                uint32_t offset = dir_items - num_items;
                FileInfo * items = infos + (i * _s_read_list_size);

                uint32_t expected = (num_items > max_items) ? max_items : num_items;
                num_items -= expected;

                if (!rlists[i].init(items, _s_read_list_size, expected, _pp_space[pp_toggle], offset))
                    return false;

                if (!min_heap.insert(&rlists[i]))
                    return false;
            }

            ReadList * rlist;

            while (min_heap.peek(rlist))
            {
                if (!rlist->shift(wlist) || (wlist.isFull() && !write_list()))
                    return false;

                if (rlist->isEmpty())
                    min_heap.remove(rlist);
                else
                    min_heap.heapify();
            }

            pong_lists++;
            ping_lists -= i;
        }

        if ((ping_lists == 0) && (pong_lists > 1))
        {
            ping_lists = pong_lists;
            pong_lists = 0;
            pp_toggle ^= 1;
            num_items = dir_items;
            max_items *= _s_num_read_lists;

            if (!write_list())
                return false;

            write_init();
        }
    }

    if (!write_list())
        return false;

    return recurse(dir_items);
}

////////////////////////////////////////////////////////////////////////////////
// WriteList ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < class DD, fst_e FST >
void FileSystem < DD, FST >::FileSort::WriteList::init(FileInfo * infos, uint16_t num_infos)
{
    _infos = infos;
    _size = num_infos;
    _count = 0;
    _sorted = false;
}

template < class DD, fst_e FST >
bool FileSystem < DD, FST >::FileSort::WriteList::push(FileInfo const & info)
{
    if (isFull())
        return false;

    _infos[_count++] = info;
    _sorted = false;

    return true;
}

template < class DD, fst_e FST >
void FileSystem < DD, FST >::FileSort::WriteList::sort(void)
{
    if (_sorted)
        return;

    //quicksort(_infos, 0, _count - 1);
    shellsort(_infos, _count);

    _sorted = true;
}

template < class DD, fst_e FST >
int FileSystem < DD, FST >::FileSort::WriteList::flush(uint32_t space, uint32_t offset)
{
    static uint8_t buffer[_s_block_size];

    if (isEmpty())
        return 0;

    int s = 0;

    auto serialize = [&](uint16_t boff) -> bool
    {
        while ((boff < _s_block_size) && (s < _count))
        {
            if (_infos[s++].serialize(buffer + boff, _s_info_size) < 0)
                return false;

            boff += _s_info_size;
        }

        return true;
    };

    uint32_t block = diskBlock(space, offset);
    uint16_t boff = blockOffset(offset);
    uint32_t num_blocks = (boff + (_count * _s_info_size)) / _s_block_size;

    // If byte offset is within block, need to read it in to preserve data
    // before offset.
    if ((boff != 0) || (num_blocks == 0))
    {
        if (_dd.read(block, buffer) < 0)
            return -1;

        if (num_blocks == 0)
        {
            if (!serialize(boff) || (_dd.write(block, buffer) < 0))
                return -1;

            clear();

            return s;
        }
    }

    dd_desc_t dd = _dd.open(block, num_blocks, DD_WRITE);

    if (!dd)
        return -1;

    int err;

    // Serialize and write to disk
    for (uint32_t i = 0; i < num_blocks; i++)
    {
        if (!serialize((s == 0) ? boff : 0))
        {
            err = -1;
            break;
        }

        while ((err = _dd.write(dd, buffer, _s_block_size)) == 0);

        if (err < 0)
            break;
    }

    _dd.close(dd);

    if (err < 0)
        return -1;

    // If number to write isn't block aligned, have to read last block in
    // before writing to preserve data after last written item.
    if (s < _count)
    {
        if (_dd.read(block + num_blocks, buffer) < 0)
            return -1;

        if (!serialize(0) || (_dd.write(block + num_blocks, buffer) < 0))
            return -1;
    }

    clear();

    return s;
}

////////////////////////////////////////////////////////////////////////////////
// ReadList ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < class DD, fst_e FST >
bool FileSystem < DD, FST >::FileSort::ReadList::init(
        FileInfo * infos, uint16_t num_infos, uint32_t expected, uint32_t space, uint32_t offset)
{
    _infos = infos;
    _size = num_infos;
    _remaining = expected;
    _offset = offset;
    _block = diskBlock(space, offset);
    _boff = blockOffset(offset);

    _count = _head = _tail = 0;

    if ((_boff != 0) && (_dd.read(_block, _buffer) < 0))
        return false;

    return fill();
}

template < class DD, fst_e FST >
bool FileSystem < DD, FST >::FileSort::ReadList::shift(WriteList & el)
{
    if (isEmpty() || !el.push(_infos[_head]))
        return false;

    if (++_head == _size)
        _head = 0;

    _count--;

    // Only read if empty to minimize disk reads
    if (isEmpty() && !fill())
        return false;

    return true;
}

template < class DD, fst_e FST >
bool FileSystem < DD, FST >::FileSort::ReadList::fill(void)
{
    if (isFull())
        return true;

    auto deserialize = [&](void) -> void
    {
        while ((_boff < _s_block_size) && !isFull())
        {
            _infos[_tail].deserialize(_buffer + _boff, _s_info_size);

            if (++_tail == _size)
                _tail = 0;

            _count++;
            _remaining--;

            _boff += _s_info_size;
            _offset++;
        }

        if (_boff == _s_block_size)
            _boff = 0;

        if (_offset == _s_infos_per_block)
        {
            _block++;
            _offset = 0;
        }
    };

    if (_boff > 0)
    {
        deserialize();

        if (isFull())
            return true;
    }

    uint32_t max_fill = ((uint16_t)(_size - _count) < _remaining) ? _size - _count : _remaining;
    uint32_t num_blocks = ceiling(max_fill, _s_infos_per_block);
    dd_desc_t dd = _dd.open(_block, num_blocks, DD_READ);

    if (!dd)
        return false;

    int err;

    do
    {
        while ((err = _dd.read(dd, _buffer, _s_block_size)) == 0);

        if (err < 0)
            break;

        deserialize();

    } while (--num_blocks != 0);

    _dd.close(dd);

    return err > 0;
}

////////////////////////////////////////////////////////////////////////////////
// Fat32 ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define FAT32_SECTOR_SIZE  SD_BLOCK_LEN

#define BOOT_RECORD_SIGNATURE    0xAA55

#define MBR_EXE_CODE_LEN    446
#define NUM_PARTITIONS        4

#define CLUSTER_MAX_FAT12    4085
#define CLUSTER_MAX_FAT16   65525

#define FAT32_CLUSTER_EOC   0x0FFFFFF8   // End of Chain
#define FAT32_CLUSTER_BAD   0x0FFFFFF7
#define FAT32_CLUSTER_MASK  0x0FFFFFFF
#define FAT32_CLUSTER_FREE  0x00000000

// Partition types
enum pt_e : uint8_t
{
    PT_FAT12          = 0x01,
    PT_FAT16_LT_32MB  = 0x04,
    PT_EXT_DOS        = 0x05,
    PT_FAT16_GT_32MB  = 0x06,
    PT_FAT32          = 0x0B,
    PT_FAT32_LBA13h   = 0x0C,
    PT_FAT16_LBA13h   = 0x0E,
    PT_EXT_DOS_LBA13h = 0x0F,
};

// 16 bytes
struct PartInfo
{
    uint8_t current_state;
    uint8_t partition_head_start;
    uint16_t partition_cylinder_start;
    uint8_t partition_type;
    uint8_t partition_head_end;
    uint16_t partition_cylinder_end;
    uint32_t sector_offset;
    uint32_t num_sectors;

} __attribute__ ((packed));

struct FatInfo
{
    uint8_t jump_code[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_sector;          // Valid values though assuming 512
                                        //   512, 1024, 2048, 4096
    uint8_t sectors_per_cluster;        // Must be power of 2
                                        //   1, 2, 4, 8, 16, 32, 64, 128
    uint16_t reserved_sectors;          // MUST not be 0
    uint8_t num_FATs;                   // Should always be 2
    uint16_t num_root_dir_entries;      // Only valid for FAT12 and FAT16.
                                        //   FAT32 MUST be 0
    uint16_t num_sectors16;             // Valid for FAT12 and FAT16.
                                        //   For FAT32 MUST be 0
    uint8_t media;
    uint16_t num_FAT_sectors16;         // For FAT32 must be 0
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t num_hidden_sectors;
    uint32_t num_sectors32;             // Total count of sectors on the volume
                                        //   FAT32 only

    union
    {
        // FAT12 and FAT16
        struct
        {
            uint8_t drive_num;
            uint8_t reserved1;
            uint8_t boot_sig;
            uint32_t volume_id;
            uint8_t volume_label[11];
            uint8_t file_sys_type[8];
        };

        // FAT32
        struct
        {
            uint32_t num_FAT_sectors32;      // Number of sectors per FAT
            uint16_t ext_flags;
            uint16_t fs_version;
            uint32_t root_cluster;           // Set to the cluster number
                                             //   of the first cluster of the
                                             //   root directory
            uint16_t fs_info_sector;         // Sector number of the FSINFO
                                             //   structure in the reserved area
            uint16_t backup_boot_sector;     // Sector number of backup copy of
                                             //   boot record in reserved area
            uint8_t reserved[12];            // Not used and not to be confused
                                             //   with the reserved area
            uint8_t drive_num_32;
            uint8_t reserved2;
            uint8_t boot_sig_32;             // Extended boot signature 0x29 indicates
                                             //   presence of next 3 fields
            uint32_t volume_id_32;
            uint8_t volume_label_32[11];
            uint8_t file_sys_type_32[8];
        };
    };

} __attribute__ ((packed));

// 512 bytes
struct FsInfo
{
    uint32_t lead_sig;            // 0x41615252 - validates that this
                                  //   is an FSInfo sector
    uint8_t reserved1[480];
    uint32_t struct_sig;          // 0x61417272 - another signature
    uint32_t free_count;          // Last known free cluster on the volume.
                                  //   0xFFFFFFFF == Unknown.  Not reliable.
    uint32_t next_free;           // Cluster number driver should start
                                  //   looking for free clusters - A hint.
    uint8_t reserved2[12];
    uint32_t trail_sig;           // 0xAA55 - like the boot record signature

} __attribute__ ((packed));

struct FatDirEntry
{
    enum attr_e
    {
        ATTR_READ_ONLY = 0x01,
        ATTR_HIDDEN    = 0x02,
        ATTR_SYSTEM    = 0x04,
        ATTR_VOLUME_ID = 0x08,
        ATTR_DIRECTORY = 0x10,
        ATTR_ARCHIVE   = 0x20,
    };

    static constexpr uint8_t const ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID;
    static constexpr uint8_t const SN_PRE_LEN = 8;
    static constexpr uint8_t const SN_EXT_LEN = 3;
    static constexpr uint8_t const SHORT_NAME_LEN = SN_PRE_LEN + SN_EXT_LEN;
    static constexpr uint8_t const LN_LEN_1 = 5;
    static constexpr uint8_t const LN_LEN_2 = 6;
    static constexpr uint8_t const LN_LEN_3 = 2;
    static constexpr uint8_t const LONG_NAME_LEN = LN_LEN_1 + LN_LEN_2 + LN_LEN_3;
    static constexpr uint8_t const LAST_LONG_ENTRY = 0x40;
    static constexpr uint8_t const DIR_ENTRY_LAST  = 0x00;
    static constexpr uint8_t const DIR_ENTRY_FREE    = 0xE5;
    static constexpr uint8_t const FREE_REPLACE_CHAR = 0x05;

    union
    {
        // 32 bytes
        struct
        {
            chr_t name[SHORT_NAME_LEN];    // Short 8.3 name
            uint8_t short_attrs;           // File attributes
            uint8_t reserved;              // Reserved for Windows NT
            uint8_t ctime_tenth;           // File creation time stamp
                                           //   a tenth of a second
            uint16_t ctime;                // File creation time
            uint16_t cdate;                // File creation date
            uint16_t adate;                // Last access date
            uint16_t first_cluster_high;   // High word of this entry's first
                                           //   cluster number (0 for FAT12 and FAT16)
            uint16_t wtime;                // Last write time
            uint16_t wdate;                // Last write date
            uint16_t first_cluster_low;    // Low word of this entry's first cluster number
            uint32_t file_size;            // This file's size in bytes

        } __attribute__ ((packed));

        // 32 bytes
        struct
        {
            uint8_t order_num;       // The order of this entry in the sequence of long dir entries
                                     //   associated with short dir entry. If masked with 0x40
                                     //   (LAST_LONG_ENTRY), it's the last in the set.
            wchr_t ln1[LN_LEN_1];    // Characters 1-5 of the long name (UTF-16LE)
            uint8_t long_attrs;      // Must be ATTR_LONG_NAME
            uint8_t type;            // If zero, directory entry is a sub-component of a long name
            uint8_t chksum;          // Checksum of name in the short dir entry at end of long dir set
            wchr_t ln2[LN_LEN_2];    // Characters 6-11 of the long name
            uint16_t first_cluster;  // MUST be ZERO.  Has no meaning in this context
            wchr_t ln3[LN_LEN_3];    // Characters 12-13 of the long name

        } __attribute__ ((packed));

        // For access to common attributes field
        struct
        {
            chr_t dummy[SHORT_NAME_LEN];
            uint8_t attrs;

        } __attribute__ ((packed));

    } __attribute__ ((packed));

    static uint16_t time(uint16_t hour, uint16_t minute, uint16_t second)
    {
        if (hour > 23)
            hour = 23;

        if (minute > 59)
            minute = 59;

        if (second > 59)
            second = 59;

        return (hour << 11) | (minute << 5) | (second / 2);
    }

    static uint16_t date(uint16_t year, uint16_t month, uint16_t day)
    {
        if (year <= 1980)
        {
            year = 0;
        }
        else
        {
            year -= 1980;
            if (year > 127)
                year = 127;
        }

        if (month == 0)
            month = 1;
        else if (month > 12)
            month = 12;

        uint8_t max_day = Rtc::clockMaxDay(month, year);

        if (day == 0)
            day = 1;
        else if (day > max_day)
            day = max_day;

        return (year << 9) | (month << 5) | day;
    }

    uint32_t cluster(void) const { return (((uint32_t)first_cluster_high << 16) | (uint32_t)first_cluster_low); }
    void cluster(uint32_t c) { first_cluster_high = c >> 16; first_cluster_low = c & 0xFFFF; }

    bool isLast(void) const { return name[0] == DIR_ENTRY_LAST; }
    bool isFree(void) const { return name[0] == DIR_ENTRY_FREE; }
    void setLast(void) { name[0] = DIR_ENTRY_LAST; }
    void setFree(void) { name[0] = DIR_ENTRY_FREE; }

    bool isLongName(void) const { return !isFree() && (attrs & ATTR_LONG_NAME) == ATTR_LONG_NAME; }
    bool isLnLast(void) const { return isLongName() && (order_num & LAST_LONG_ENTRY); }
    uint8_t lnOrd(void) const { return isLongName() ? order_num & 0x3F : 0; }

    bool isShortName(void) const { return !isFree() && !isLongName(); }

    bool isReadOnly(void) const { return isShortName() && (attrs & ATTR_READ_ONLY); }
    bool isHidden(void) const { return isShortName() && (attrs & ATTR_HIDDEN); }
    bool isSystem(void) const { return isShortName() && (attrs & ATTR_SYSTEM); }
    bool isVolumeID(void) const { return isShortName() && (attrs & ATTR_VOLUME_ID); }
    bool isFile(void) const { return isShortName() && !isVolumeID() && !(attrs & ATTR_DIRECTORY); }
    bool isDirectory(void) const { return isShortName() && (attrs & ATTR_DIRECTORY); }

} __attribute__ ((packed));

union sector_u
{
    uint8_t      a8[FAT32_SECTOR_SIZE];
    uint16_t    a16[FAT32_SECTOR_SIZE / 2];
    uint32_t    a32[FAT32_SECTOR_SIZE / 4];
    FatDirEntry dir[FAT32_SECTOR_SIZE / 32];

    bool isBoot(void) { return a16[255] == BOOT_RECORD_SIGNATURE; }
};

////////////////////////////////////////////////////////////////////////////////
// Fat32 File //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < class DD > class Fat32;

template < class DD >
class Fat32File : public TFile < DD, FST_FAT32 >
{
    friend class Fat32 < DD >;

    public:
        virtual int read(uint8_t * buf, int amt);
        virtual int read(uint8_t const ** p, uint8_t n);
        virtual int read(FileInfo & info);
        virtual int write(uint8_t const * buf, int amt, bool flush = true);
        virtual int write(FileInfo const & info);
        virtual bool flush(void);
        virtual bool rewind(void);

        Fat32File(Fat32File const &) = delete;
        Fat32File & operator=(Fat32File const &) = delete;

    protected:
        Fat32File(DD * dd = nullptr) : TFile < DD, FST_FAT32 > (dd) {}
        Fat32File(FileInfo const & info, DD * dd = nullptr) : TFile < DD, FST_FAT32 > (info, dd) {
            this->_taken = true; rewind();
        }

        virtual void set(FileInfo const & info, uint8_t oflags = O_READ);

    private:
        bool read(void) { return this->_dd->read(_ds, _dsb.a8) > 0; }
        bool write(void) { return (_dsb_off == 0) ? true : this->_dd->write(_ds, _dsb.a8) > 0; }
        uint8_t checksum(chr_t const * name);
        bool readNext(void);
        int readEntry(FatDirEntry const * & entry);
        int _read(uint8_t * buf, int amt);
        int _read(uint8_t const ** p, uint8_t n);
        int _write(uint8_t const * buf, int amt, bool flush = true);
        bool update(void);

        uint32_t _cluster = 0;
        uint32_t _sofc = 0;  // sector of cluster

        uint32_t _ds = 0;       // data sector
        uint32_t _dsb_off = 0;  // data buffer offset
        sector_u _dsb;          // data buffer

        bool _rewind = false;
        bool _flush = false;

        static constexpr uint8_t const LNL  = FatDirEntry::LONG_NAME_LEN;
        static constexpr uint8_t const LNL1 = FatDirEntry::LN_LEN_1;
        static constexpr uint8_t const LNL2 = FatDirEntry::LN_LEN_2;
        static constexpr uint8_t const LNL3 = FatDirEntry::LN_LEN_3;
        static constexpr uint8_t const SNL  = FatDirEntry::SHORT_NAME_LEN;
        static constexpr uint8_t const SNL1 = FatDirEntry::SN_PRE_LEN;
        static constexpr uint8_t const SNL2 = FatDirEntry::SN_EXT_LEN;
};

template < class DD >
void Fat32File < DD >::set(FileInfo const & info, uint8_t oflags)
{
    File::set(info, oflags);

    this->_taken = true;
    _flush = _rewind = false;
    _ds = 0;

    if (oflags & O_TRUNC)
        Fat32 < DD >::update(*this);

    (void)rewind();
}

template < class DD >
bool Fat32File < DD >::rewind(void)
{
    if (!this->valid())
        return false;

    this->_offset = _sofc = _dsb_off = 0;
    _cluster = this->_info.address();

    if (this->_dd->busy())
    {
        _rewind = true;
        return true;
    }

    uint32_t ds = Fat32 < DD >::dataSector(_cluster);

    if ((ds != _ds) && (this->_dd->read(ds, _dsb.a8) != sizeof(_dsb.a8)))
        this->close();

    _ds = ds;
    _rewind = false;

    return this->valid();
}

template < class DD >
uint8_t Fat32File < DD >::checksum(chr_t const * name)
{
    uint8_t sum = name[0];

    for (uint8_t i = 1; i < SNL; i++)
        sum = (sum << 7) + (sum >> 1) + name[i];

    return sum;
}

template < class DD >
bool Fat32File < DD >::readNext(void)
{
    auto next_sector = [&](uint32_t ds) -> bool
    {
        _ds = ds;

        if (!read())
            this->close();

        return this->valid();
    };

    _dsb_off = 0;

    if (++_sofc != Fat32 < DD >::_s_sectors_per_cluster)
        return next_sector(_ds + 1);

    _sofc = 0;

    if (Fat32 < DD >::nextCluster(*this->_dd, _cluster))
        return next_sector(Fat32 < DD >::dataSector(_cluster));

    this->close();
    return this->valid();
}

template < class DD >
int Fat32File < DD >::read(uint8_t * buf, int amt)
{
    if (!this->isReg() || !this->canRead())
        return -1;

    return _read(buf, amt);
}

template < class DD >
int Fat32File < DD >::_read(uint8_t * buf, int amt)
{
    if (!this->valid() || (buf == nullptr) || (amt < 0) || (_rewind && !rewind()))
        return -1;

    if ((amt == 0) || this->_dd->busy() || _rewind)
        return 0;

    int n = 0;
    while (n != amt)
    {
        if (this->eof())
            return n;

        if ((_dsb_off == sizeof(_dsb.a8)) && !readNext())
            return -1;

        uint32_t cpy = sizeof(_dsb.a8) - _dsb_off;

        if (cpy > (uint32_t)(amt - n))
            cpy = amt - n;

        if (cpy > this->remaining())
            cpy = this->remaining();

        memcpy(buf + n, _dsb.a8 + _dsb_off, cpy);

        n += cpy; _dsb_off += cpy; this->_offset += cpy;
    }

    return n;
}

template < class DD >
int Fat32File < DD >::read(uint8_t const ** p, uint8_t n)
{
    if (!this->isReg() || !this->canRead())
        return -1;

    return _read(p, n);
}

template < class DD >
int Fat32File < DD >::readEntry(FatDirEntry const * & entry)
{
    static constexpr uint8_t const dir_size = sizeof(FatDirEntry);

    if ((_dsb_off == sizeof(_dsb.a8)) && !readNext())
        return -1;

    entry = (FatDirEntry const *)&_dsb.a8[_dsb_off];

    if (entry->isLast())
        return 0;

    _dsb_off += dir_size;
    this->_offset += dir_size;

    return dir_size;
}

template < class DD >
int Fat32File < DD >::read(FileInfo & info)
{
    if (!this->isDir() || !this->canRead())
        return -1;

    static String < NS > name;

    FatDirEntry const * entry;
    uint8_t ln_ord;
    int ret, chksum;
    bool valid;

    auto init = [&](void) -> void
    {
        name.clear();
        ln_ord = ret = 0;
        chksum = -1;
        valid = true;
    };

    auto free_entry = [&](bool flush = false) -> void
    {
        FatDirEntry * bad = (FatDirEntry *)entry;

        bad->setFree();

        if (flush || (_dsb_off == sizeof(_dsb.a8)))
            (void)this->_dd->write(_ds, _dsb.a8);
    };

    init();

    while ((ret = readEntry(entry)) > 0)
    {
        if (entry->isLongName())
        {
            if (!valid)
            {
                free_entry();
                continue;
            }

            if (entry->isLnLast())
            {
                ln_ord = entry->lnOrd();
                chksum = entry->chksum;
            }
            else if ((entry->chksum != chksum) || (entry->lnOrd() != ln_ord) || (ln_ord == 0))
            {
                valid = false;
                free_entry();
                continue;
            }

            String < LNL > ln(entry->ln1, LNL1);

            if (ln.len() == LNL1)
                ln.postpend(entry->ln2, LNL2);

            if (ln.len() == (LNL1 + LNL2))
                ln.postpend(entry->ln3, LNL3);

            name.prepend(ln.str(), ln.len());
            ln_ord--;
        }
        else if (entry->isShortName() && !entry->isVolumeID())
        {
            uint32_t cluster = entry->cluster();
            uint32_t ds = Fat32 < DD >::dataSector(cluster);
            uint32_t size = entry->file_size;
            uint32_t sectors = ceiling(size, FAT32_SECTOR_SIZE);

            if (((ds + sectors) > this->_dd->blocks()) || ((ds + sectors) < ds))
                valid = false;

            if (!valid || ((chksum != -1) && (chksum != checksum(entry->name))))
            {
                free_entry(true);
                init();
                continue;
            }

            if (name.len() == 0)
            {
                name.set(entry->name, SNL1);
                name.rstrip();
                name += '.';
                name.postpend(&entry->name[SNL1], SNL2);
                name.rstrip();

                if (name[0] == FatDirEntry::FREE_REPLACE_CHAR)
                    name[0] = FatDirEntry::DIR_ENTRY_FREE;
            }

            FileInfo::ft_e type = entry->isDirectory() ? FileInfo::FT_DIR : FileInfo::FT_REG;

            info.set(name, type, cluster, size, this->address());

            return sizeof(FileInfo);
        }
    }

    return ret;
}

template < class DD >
int Fat32File < DD >::_read(uint8_t const ** p, uint8_t n)
{
    if (!this->valid() || (p == nullptr) || (n > sizeof(_dsb.a8)) || (_rewind && !rewind()))
        return -1;

    if ((n == 0) || this->eof() || this->_dd->busy() || _rewind)
        return 0;

    if (((_dsb_off == sizeof(_dsb.a8)) && !readNext()) || ((_dsb_off + n) > sizeof(_dsb.a8)))
        return -1;

    if (n > this->remaining())
        n = this->remaining();

    *p = &_dsb.a8[_dsb_off];
    _dsb_off += n; this->_offset += n;

    return n;
}

template < class DD >
int Fat32File < DD >::write(uint8_t const * buf, int amt, bool flush)
{
    if (!this->isReg() || !this->canWrite())
        return -1;

    return _write(buf, amt, flush);
}

template < class DD >
bool Fat32File < DD >::update(void)
{
    if (this->_offset <= this->size())
        return true;

    uint32_t size = this->size();

    this->_info = this->_offset;

    if (!Fat32 < DD >::update(*this))
    {
        this->_info = size;
        return false;
    }

    return true;
}

template < class DD >
bool Fat32File < DD >::flush(void)
{
    if (!this->isReg() || !this->canWrite())
        return false;

    if (_flush && (!write() || !update()))
        return false;
    else if (_flush)
        _flush = false;

    return true;
}

template < class DD >
int Fat32File < DD >::write(FileInfo const & info)
{
    if (!this->isDir() || !this->canWrite() || !rewind())
        return -1;

    if (this->_dd->busy() || _rewind)
        return 0;

    String < NS > const & name = info.name();

    if ((name.len() == 0) || (name[name.len() - 1] == ' '))
        return -1;

    auto is_legal = [](chr_t c, bool ln = false) -> bool
    {
        chr_t const sn_legal[] =
        {
            '$', '%', '\'', '-', '_', '@', '~', '`',
            '!', '(', ')', '{', '}', '^', '#', '&'
        };

        chr_t const ln_legal[] = { '.', '+', ',', ';', '=', '[', ']', ' ' };

        if ((c > 127)
                || ((c >= '0') && (c <= '9'))
                || ((c >= 'A') && (c <= 'Z'))
                || ((c >= 'a') && (c <= 'z')))
        {
            return true;
        }

        for (uint8_t i = 0; i < sizeof(sn_legal); i++)
        {
            if (c == sn_legal[i])
                return true;
        }

        if (!ln)
            return false;

        for (uint8_t i = 0; i < sizeof(ln_legal); i++)
        {
            if (c == ln_legal[i])
                return true;
        }

        return false;
    };

    for (uint16_t i = 0; i < name.len(); i++)
    {
        if (!is_legal(name[i], true))
            return -1;
    }

    static FileInfo cinfo;

    while (read(cinfo) > 0)
    {
        if (cinfo.name() == name)
            return -1;
    }

    if (!rewind())
        return -1;

    chr_t short_name[SNL];
    bool lossy = false;
    uint16_t i = 0, j = 0;

    memset(short_name, ' ', SNL);

    int ext = name.rfind('.');
    if (ext == (name.len() - 1))
        ext = -1;

    for (; (i < name.len()) && (j < SNL1); i++)
    {
        chr_t c = name[i];

        if (((j == 0) && (c == '.')) || (c == ' '))
            continue;

        if ((c == '.') && (i == ext))
            break;

        if ((c >= 'a') && (c <= 'z'))
        {
            short_name[j] = toupper(c);
        }
        else if (!is_legal(c))
        {
            short_name[j] = '_';
            lossy = true;
        }
        else
        {
            short_name[j] = c;
        }

        j++;
    }

    if ((i != j) || ((ext != -1) && (i != ext)) || (i != name.len()))
        lossy = true;

    if (short_name[0] == FatDirEntry::DIR_ENTRY_FREE)
        short_name[0] = FatDirEntry::FREE_REPLACE_CHAR;

    uint16_t base = j;

    if (ext != -1)
    {
        for (i = ext + 1, j = SNL1; (i < name.len()) && (j < SNL); i++, j++)
        {
            if (!is_legal(name[i]))
                return -1;

            short_name[j] = toupper(name[i]);
        }

        if (i != name.len())
            lossy = true;
    }

    auto find = [&](void) -> bool
    {
        FatDirEntry const * entry;

        while (readEntry(entry) > 0)
        {
            if (entry->isShortName() && (memcmp(entry->name, short_name, SNL) == 0))
                return true;
        }

        return false;
    };

    auto create = [&](void) -> bool
    {
        uint8_t num_lns = ceiling(name.len(), LNL);
        uint16_t nlen = name.len();
        uint8_t ln_empty = LNL - (name.len() % LNL);
        uint8_t chksum = checksum(short_name);

        // Long name entries
        for (uint8_t i = 0; i < num_lns; i++)
        {
            auto cpy_name = [&](wchr_t * lnN, uint8_t len) -> void
            {
                uint8_t ln_len = len;
                uint8_t j = 0;

                if (ln_empty >= len)
                {
                    ln_empty -= len;
                    len = 0;
                }
                else if (ln_empty != 0)
                {
                    len -= ln_empty;
                    ln_empty = 0;
                }

                for (; j < len; j++)
                    lnN[j] = name[(nlen-len)+j];

                if ((j != 0) && (j < ln_len))
                    lnN[j++] = 0;

                while (j < ln_len)
                    lnN[j++] = 0xFFFF;

                nlen -= len;
            };

            FatDirEntry entry = {};

            cpy_name(entry.ln3, LNL3);
            cpy_name(entry.ln2, LNL2);
            cpy_name(entry.ln1, LNL1);

            entry.order_num = num_lns - i;
            if (i == 0)
                entry.order_num |= FatDirEntry::LAST_LONG_ENTRY;

            entry.attrs = FatDirEntry::ATTR_LONG_NAME;
            entry.chksum = chksum;

            if (_write((uint8_t const *)&entry, sizeof(FatDirEntry)) < 0)
                return false;
        }

        // Short name entry
        FatDirEntry entry = {};

        memcpy(entry.name, short_name, SNL);

        if (info.isDir())
        {
            entry.attrs = FatDirEntry::ATTR_DIRECTORY;
            entry.file_size = 0;
        }
        else
        {
            entry.file_size = info.size();
        }

        entry.cluster(info.address());

        Rtc & rtc = Rtc::acquire();

        entry.ctime = entry.wtime =
            FatDirEntry::time(rtc.clockHour(), rtc.clockMinute(), rtc.clockSecond());

        entry.cdate = entry.adate = entry.wdate =
            FatDirEntry::date(rtc.clockYear(), rtc.clockMonth(), rtc.clockDay());

        return (_write((uint8_t const *)&entry, sizeof(FatDirEntry)) > 0);
    };

    if (!lossy)
    {
        if (find() || !create())
            return -1;

        return sizeof(FileInfo);
    }

    // Based on Linux kernel code in fs/fat/namei_vfat.c : vfat_create_shortname()
    // "Windows only tries a few cases before using random values for part of the base."

    if (base > (SNL1 - 2))
        base = SNL1 - 2;

    short_name[base] = '~';

    for (i = 1; i < 10; i++)
    {
        short_name[base+1] = i + '0';

        if (!find())
            return create() ? sizeof(FileInfo) : -1;

        if (!rewind())
            return -1;
    }

    uint32_t us = usecs();
    uint8_t tail = (us >> 16) & 0x07;

    if (base > (SNL1 - 6))
        base = SNL1 - 6;

    short_name[base+4] = '~';
    short_name[base+5] = tail + '1';

    static constexpr uint32_t const timeout = 1000;
    uint32_t ms = msecs();

    while ((msecs() - ms) < timeout)
    {
        auto hex_chr = [](uint8_t n) -> chr_t
        {
            if (n < 10)
                return n + '0';
            else if (n < 16)
                return (n - 10) + 'A';

            return 'F';
        };

        uint16_t n = us & UINT16_MAX;

        for (i = 3; i >= 0; i--)
        {
            short_name[base+i] = hex_chr(n & 0x0F);
            n >>= 4;
        }

        if (!find())
            return create() ? sizeof(FileInfo) : -1;

        if (!rewind())
            return -1;

        us -= 11;
    }

    return -1;
}

template < class DD >
int Fat32File < DD >::_write(uint8_t const * buf, int amt, bool flush)
{
    if (!this->valid() || (buf == nullptr) || (amt < 0) || (_rewind && !rewind()))
        return -1;

    if ((amt == 0) || this->_dd->busy() || _rewind)
        return 0;

    auto write_next = [&](void) -> bool
    {
        auto next_sector = [&](uint32_t ds) -> bool
        {
            _ds = ds;

            if (!read())
                this->close();

            return this->valid();
        };

        if (!write())
        {
            this->close();
            return this->valid();
        }

        _dsb_off = 0;

        if (++_sofc != Fat32 < DD >::_s_sectors_per_cluster)
            return next_sector(_ds + 1);

        _sofc = 0;

        if (Fat32 < DD >::newCluster(*this->_dd, _cluster))
            return next_sector(Fat32 < DD >::dataSector(_cluster));

        this->close();
        return this->valid();
    };

    int n = 0;
    while (n != amt)
    {
        uint32_t cpy = sizeof(_dsb.a8) - _dsb_off;

        if (cpy > (uint32_t)(amt - n))
            cpy = amt - n;

        memcpy(_dsb.a8 + _dsb_off, buf + n, cpy);

        n += cpy; _dsb_off += cpy; this->_offset += cpy;

        if ((_dsb_off == sizeof(_dsb.a8)) && !write_next())
            return -1;
    }

    if (flush && (!write() || !update()))
    {
        this->close();
        return -1;
    }
    else if (!flush && (_dsb_off != 0))
    {
        _flush = true;
    }

    return n;
}

////////////////////////////////////////////////////////////////////////////////
// Fat32 File System ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < class DD >
class Fat32 : public FileSystem < DD, FST_FAT32 >
{
    friend class Fat32File < DD >;

    public:
        static Fat32 & acquire(void) { static Fat32 fat32; return fat32; }
        bool valid(void) { return FileSystem < DD, FST_FAT32 >::valid() && _valid; }

        virtual File * open(uint32_t file_index, uint8_t oflags = O_READ);
        virtual File * open(FileInfo const & info, uint8_t oflags = O_READ);
        virtual File * open(chr_t const * name, uint8_t oflags = O_READ);
        virtual File * open(String < NS > const & name, uint8_t oflags = O_READ);

        virtual int sort(char const * const * exts = nullptr);

        Fat32(Fat32 const &) = delete;
        Fat32 & operator=(Fat32 const &) = delete;

    private:
        Fat32(void);
        File * open(Fat32File < DD > & dir, String < NS > const & name, uint8_t oflags);

        static bool read(DD & dd, uint32_t sector, uint8_t (&buf)[SD_BLOCK_LEN]) {
            return dd.read(sector, buf) == SD_BLOCK_LEN;
        }

        static bool write(DD & dd, uint32_t sector, uint8_t (&buf)[SD_BLOCK_LEN]) {
            return dd.write(sector, buf) == SD_BLOCK_LEN;
        }

        static constexpr uint8_t const _s_dir_max_level = 5;

        uint32_t _volume_sector_start = 0;
        bool _valid = true;

        static uint32_t tsEntry(uint32_t cluster) { return cluster & ((FAT32_SECTOR_SIZE / 4) - 1); }

        static bool isEOC(uint32_t cluster) { return (cluster & FAT32_CLUSTER_MASK) >= FAT32_CLUSTER_EOC; }
        static bool isBad(uint32_t cluster) { return (cluster & FAT32_CLUSTER_MASK) == FAT32_CLUSTER_BAD; }
        static bool isFree(uint32_t cluster) { return (cluster & FAT32_CLUSTER_MASK) == FAT32_CLUSTER_FREE; }
        static bool isUsed(uint32_t cluster) { return !isEOC(cluster) && !isBad(cluster) && !isFree(cluster); }

        // Value needs to be >= EOC so just add mask
        static void markEOC(uint32_t & cluster) { cluster |= FAT32_CLUSTER_MASK; };

        static void markFree(uint32_t & cluster) {
            cluster &= ~FAT32_CLUSTER_MASK; cluster |= FAT32_CLUSTER_FREE;
        };

        static void markUsed(uint32_t & cluster, uint32_t next_cluster) {
            cluster &= ~FAT32_CLUSTER_MASK; cluster |= next_cluster;
        };

        static uint32_t tableSector(uint32_t cluster);
        static uint32_t dataSector(uint32_t cluster);
        static bool nextCluster(DD & dd, uint32_t & cluster);
        static bool findFree(DD & dd, uint32_t & cluster);
        static bool newCluster(DD & dd, uint32_t & cluster);
        static bool update(Fat32File < DD > & file);

        static uint32_t _s_table_sector_start;
        static uint32_t _s_num_fat_sectors;
        static uint32_t _s_data_sector_start;
        static uint32_t _s_sectors_per_cluster;
        static uint32_t _s_cluster_to_sector;
        static uint32_t _s_root_cluster;

        static constexpr uint8_t const _s_num_files = 4;
        static Fat32File < DD > _s_files[_s_num_files];

        static FileInfo _s_root_dir;
        static String < NS > _s_name;
        static constexpr chr_t const * _s_sort_name = (chr_t const *)"songlist.txt";

        static sector_u _s_tsb;
        static uint32_t _s_ts;
        static sector_u _s_dsb;
};

template < class DD > uint32_t Fat32 < DD >::_s_table_sector_start = 0;
template < class DD > uint32_t Fat32 < DD >::_s_num_fat_sectors = 0;
template < class DD > uint32_t Fat32 < DD >::_s_data_sector_start = 0;
template < class DD > uint32_t Fat32 < DD >::_s_sectors_per_cluster = 0;
template < class DD > uint32_t Fat32 < DD >::_s_cluster_to_sector = 0;
template < class DD > uint32_t Fat32 < DD >::_s_root_cluster = 0;
template < class DD > Fat32File < DD > Fat32 < DD >::_s_files[_s_num_files] = {};
template < class DD > FileInfo Fat32 < DD >::_s_root_dir;
template < class DD > String < NS > Fat32 < DD >::_s_name;
template < class DD > sector_u Fat32 < DD >::_s_tsb;
template < class DD > uint32_t Fat32 < DD >::_s_ts = 0;
template < class DD > sector_u Fat32 < DD >::_s_dsb;

template < class DD >
Fat32 < DD >::Fat32(void)
{
    if (!valid())
        return;

    ////////////////////////////////////////////////////////////////////////////
    auto getFatVolume = [](PartInfo const * p, uint32_t & sector_start) -> bool
    {
        for (uint8_t i = 0; i < NUM_PARTITIONS; i++, p++)
        {
            if ((p->partition_type == PT_FAT32) || (p->partition_type == PT_FAT32_LBA13h))
            {
                sector_start = p->sector_offset;
                return true;
            }
        }

        return false;
    };

    // Taken from:
    //   Microsoft Extensible Firmware Initiative
    //   FAT32 File System Specification
    //   FAT: General Overview of On-Disk Format
    //     Version 1.03, December 6, 2000
    //     Microsoft Corporation
    auto isFat32 = [](FatInfo const * f) -> bool
    {
        if ((f->num_root_dir_entries != 0) || (f->num_sectors16 != 0)
                || (f->num_FAT_sectors16 != 0) || (f->num_FAT_sectors32 == 0))
        {
            return false;
        }

        uint32_t num_fat_sectors = f->num_FATs * f->num_FAT_sectors32;
        uint32_t num_data_sectors = f->num_sectors32 - (f->reserved_sectors + num_fat_sectors);
        uint32_t cluster_count = num_data_sectors / f->sectors_per_cluster;

        if (cluster_count < CLUSTER_MAX_FAT12)  // FAT12
            return false;
        else if (cluster_count < CLUSTER_MAX_FAT16)  // FAT16
            return false;

        return true;  // FAT32
    };

    auto isValidFat32 = [](FatInfo const * f) -> bool
    {
        return
            ((f->bytes_per_sector == FAT32_SECTOR_SIZE) &&   // Only supporting 512 byte sectors
        //   (f->num_FATS == 2) &&                           // Should be 2
             (f->reserved_sectors != 0));                    // Must not be 0
    };
    ////////////////////////////////////////////////////////////////////////////

    if (!read(this->_dd, 0, _s_dsb.a8) || !_s_dsb.isBoot())
    {
        _valid = false;
        return;
    }

    PartInfo const * p = (PartInfo const *)&_s_dsb.a8[MBR_EXE_CODE_LEN];

    if (!getFatVolume(p, _volume_sector_start))
    {
        _valid = false;
        return;
    }

    // Don't need previous information so reuse sector enum
    if (!read(this->_dd, _volume_sector_start, _s_dsb.a8) || !_s_dsb.isBoot())
    {
        _valid = false;
        return;
    }

    FatInfo const * f = (FatInfo const *)_s_dsb.a8;

    if (!isFat32(f) || !isValidFat32(f))
    {
        _valid = false;
        return;
    }

    _s_table_sector_start = _volume_sector_start + f->reserved_sectors;
    _s_num_fat_sectors = f->num_FAT_sectors32;
    _s_data_sector_start = _s_table_sector_start + (f->num_FAT_sectors32 * f->num_FATs);
    _s_sectors_per_cluster = f->sectors_per_cluster;
    _s_cluster_to_sector = __builtin_ctz(_s_sectors_per_cluster);
    _s_root_cluster = f->root_cluster;

    _s_root_dir.set(FileInfo::FT_DIR, _s_root_cluster, 0, 0);
}

// Each entry in FAT table is 32 bits or 4 bytes so byte offset would be cluster
// number * 4. Each FAT sector is assumed to be 512 bytes so the sector for the
// above calculated byte offset would be the byte offset / 512.
// So sector = (cluster * 4) / 512 = cluster / 128 = cluster >> 7
template < class DD >
uint32_t Fat32 < DD >::tableSector(uint32_t cluster)
{
    return _s_table_sector_start + (cluster >> 7);
}

// Since the sectors per cluster are powers of two can use bit shift
template < class DD >
uint32_t Fat32 < DD >::dataSector(uint32_t cluster)
{
    if (cluster < 2)
        return UINT32_MAX;
    else
        return _s_data_sector_start + ((cluster - 2) << _s_cluster_to_sector);
}

template < class DD >
bool Fat32 < DD >::nextCluster(DD & dd, uint32_t & cluster)
{
    if (!isUsed(cluster))
        return false;

    uint32_t ts = tableSector(cluster);

    if (_s_ts != ts)
    {
        if (!read(dd, ts, _s_tsb.a8))
            return false;

        _s_ts = ts;
    }

    cluster = _s_tsb.a32[tsEntry(cluster)] & FAT32_CLUSTER_MASK;

    return isUsed(cluster);
}

template < class DD >
bool Fat32 < DD >::findFree(DD & dd, uint32_t & cluster)
{
    uint32_t const entries_per_sector = FAT32_SECTOR_SIZE / 4;

    cluster = 0;

    uint32_t sectors = _s_num_fat_sectors;
    uint32_t ts = tableSector(cluster);

    while (0 != sectors--)
    {
        if (!read(dd, ts, _s_tsb.a8))
            return false;

        _s_ts = ts++;

        for (uint32_t i = 0; i < entries_per_sector; i++, cluster++)
        {
            if ((cluster > _s_root_cluster) && isFree(_s_tsb.a32[i]))
            {
                markEOC(_s_tsb.a32[i]);
                return write(dd, _s_ts, _s_tsb.a8);
            }
        }
    }

    return false;
}

template < class DD >
bool Fat32 < DD >::newCluster(DD & dd, uint32_t & cluster)
{
    if (!isUsed(cluster))
        return false;

    uint32_t cl = cluster;

    if (nextCluster(dd, cluster))
        return true;

    if (!isEOC(cluster))
        return false;

    cluster = 0;

    uint32_t const entries_per_sector = FAT32_SECTOR_SIZE / 4;

    uint32_t sectors = _s_num_fat_sectors;
    uint32_t ts = tableSector(cluster);

    while (0 != sectors--)
    {
        if (!read(dd, ts, _s_tsb.a8))
            return false;

        _s_ts = ts++;

        for (uint32_t i = 0; i < entries_per_sector; i++, cluster++)
        {
            if ((cluster > _s_root_cluster) && isFree(_s_tsb.a32[i]))
            {
                markEOC(_s_tsb.a32[i]);

                ts = tableSector(cl);
                if (ts != _s_ts)
                {
                    if (!write(dd, _s_ts, _s_tsb.a8) || !read(dd, ts, _s_tsb.a8))
                        return false;

                    _s_ts = ts;
                }

                markUsed(_s_tsb.a32[tsEntry(cl)], cluster);

                return write(dd, _s_ts, _s_tsb.a8);
            }
        }
    }

    return false;
}

template < class DD >
File * Fat32 < DD >::open(uint32_t file_index, uint8_t oflags)
{
    static FileInfo info;

    if (this->_fs.retrieve(file_index, info) <= 0)
        return nullptr;

    return open(info, oflags);
}

template < class DD >
File * Fat32 < DD >::open(FileInfo const & info, uint8_t oflags)
{
    if (!valid())
        return nullptr;

    uint8_t i = 0;
    for (; i < _s_num_files; i++)
    {
        if (!_s_files[i]._taken)
            break;
    }

    if (i == _s_num_files)
        return nullptr;

    _s_files[i]._dd = &this->_dd;

    _s_files[i].set(info, oflags);
    if (!_s_files[i].valid())
        return nullptr;

    return &_s_files[i];
}

template < class DD >
File * Fat32 < DD >::open(chr_t const * name, uint8_t oflags)
{
    if (name == nullptr)
        return nullptr;

    if (name[0] == '/')
        _s_name.set(&name[1], strlen((char const *)name) - 1);
    else
        _s_name.set(name);

    return open(_s_name, oflags);
}

template < class DD >
File * Fat32 < DD >::open(String < NS > const & name, uint8_t oflags)
{
    static Fat32File < DD > root(&this->_dd);

    root.set(_s_root_dir, O_RDWR);

    if (!root.valid())
        return nullptr;

    int index = name.find('/');
    if (index == 0)
        _s_name = &name[1];
    else
        _s_name = name;

    return open(root, _s_name, oflags);
}

template < class DD >
File * Fat32 < DD >::open(Fat32File < DD > & dir, String < NS > const & name, uint8_t oflags)
{
    static FileInfo info;

    auto find = [&](String < NS > const & sname) -> int
    {
        int err;

        while (((err = dir.read(info)) > 0) && (info.name() != sname));

        return err;
    };

    auto truncate = [&](uint32_t cluster) -> bool
    {
        auto write_ts = [&](void) -> bool
        {
            return (this->_dd.write(_s_ts, _s_tsb.a8) > 0);
        };

        auto next_ts = [&](uint32_t ts) -> bool
        {
            if (_s_ts == ts)
                return true;
            else if (!write_ts())
                return false;

            return (this->_dd.read(ts, _s_tsb.a8) > 0);
        };

        uint32_t cnum = 0;
        uint32_t ts =  tableSector(cluster);

        if (this->_dd.read(ts, _s_tsb.a8) < 0)
            return false;

        _s_ts = ts;

        while (true)
        {
            uint32_t & cl = _s_tsb.a32[tsEntry(cluster)];

            if (isEOC(cl))
            {
                if (cnum != 0)
                    markFree(cl);

                break;
            }
            else if (isFree(cl) || isBad(cl))
            {
                if (cnum == 0)
                    markEOC(cl);

                break;
            }

            cluster = cl & FAT32_CLUSTER_MASK;
            ts = tableSector(cluster);

            (0 == cnum++) ? markEOC(cl) : markFree(cl);

            if (!next_ts(ts))
                return false;

            _s_ts = ts;
        }

        return write_ts();
    };

    int index = name.find('/');

    if ((index == -1) || (index == (name.len() - 1)))
    {
        bool is_dir = index != -1;

        if (is_dir)
            _s_name.set(name.str(), name.len() - 1);
        else
            _s_name = name;

        int err = find(_s_name);
        if (err < 0)
            return nullptr;

        if (err == 0)
        {
            if (!(oflags & O_CREATE))
                return nullptr;

            uint32_t cluster;
            if (!findFree(this->_dd, cluster))
                return nullptr;

            info.set(_s_name, is_dir ? FileInfo::FT_DIR : FileInfo::FT_REG, cluster, 0, dir.address());

            if (dir.write(info) < 0)
                return nullptr;
        }
        else if (oflags & O_TRUNC)
        {
            if (is_dir)
                return nullptr;

            if (!truncate(info.address()))
                return nullptr;

            info = 0;
        }

        return open(info, oflags);
    }

    static String < NS > sub;

    sub.set(name.str(), index);

    int err = find(sub);

    // Not found or found a file, but looking for a directory
    if ((err <= 0) || info.isFile())
        return nullptr;

    dir.set(info, O_RDWR);
    _s_name = &name[index+1];

    return open(dir, _s_name, oflags);
}

template < class DD >
bool Fat32 < DD >::update(Fat32File < DD > & file)
{
    if (file._dd == nullptr)
        return false;

    uint32_t cluster = file.parent();
    uint32_t ds = dataSector(cluster);

    do
    {
        if (!read(*file._dd, ds, _s_dsb.a8))
            return false;

        for (uint8_t i = 0; i < sizeof(_s_dsb.a8) / sizeof(FatDirEntry); i++)
        {
            FatDirEntry & entry = _s_dsb.dir[i];

            if (entry.isLast())
                return false;

            if (entry.isShortName() && (entry.cluster() == file.address()))
            {
                Rtc & rtc = Rtc::acquire();

                entry.file_size = file.size();
                entry.wtime = FatDirEntry::time(rtc.clockHour(), rtc.clockMinute(), rtc.clockSecond());
                entry.adate = entry.wdate = FatDirEntry::date(rtc.clockYear(), rtc.clockMonth(), rtc.clockDay());

                return write(*file._dd, ds, _s_dsb.a8);
            }
        }

        if (++ds == _s_sectors_per_cluster)
        {
            if (!nextCluster(*file._dd, cluster))
                return false;

            ds = dataSector(cluster);
        }

    } while (true);

    return false;
}

template < class DD >
int Fat32 < DD >::sort(char const * const * exts)
{
    int num_files = this->_fs.sort(_s_root_dir, exts);

    if (num_files <= 0)
        return num_files;

    File * file = open(_s_sort_name, O_WRITE | O_CREATE | O_TRUNC);
    if (file == nullptr)
        return -1;

    uint8_t c;
    int i = 0, err;
    for (; i < num_files; i++)
    {
        FileInfo info;

        if ((err = this->_fs.retrieve(i, info)) <= 0)
            break;

        chr_t const * fileno = itoa(i + 1);
        if ((err = file->write(fileno, strlen((char const *)fileno), false)) <= 0)
            break;

        c = ' ';
        if ((err = file->write(&c, 1, false)) <= 0)
            break;

        if ((err = file->write((uint8_t *)info.name().str(), info.name().len(), false)) <= 0)
            break;

        c = '\n';
        if ((err = file->write(&c, 1, false)) <= 0)
            break;
    }

    if ((err > 0) && !file->flush())
        return -1;

    file->close();

    if (i != num_files)
        return err;

    return num_files;
}

////////////////////////////////////////////////////////////////////////////////
// Templates ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
using TFs = Fat32 < TDisk >;

#endif
