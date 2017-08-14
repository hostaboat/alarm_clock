#ifndef _FILE_H_
#define _FILE_H_

#include "disk.h"
#include "types.h"
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// File Info ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct FileInfo
{
    uint32_t address;
    uint32_t size;

    void set(uint32_t address, uint32_t size) { this->address = address; this->size = size; }
};

////////////////////////////////////////////////////////////////////////////////
// File ////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
enum fst_e  // File System Type
{
    FST_NONE,
    FST_FAT32,
};

class File
{
    public:
        virtual int read(uint8_t * buf, int amt) = 0;
        virtual int read(uint8_t const ** p, uint8_t n) = 0;
        virtual bool rewind(void) = 0;

        virtual bool eof(void) { return _remaining == 0; }
        virtual uint32_t remaining(void) { return _remaining; }
        virtual uint32_t size(void) { return _info.size; }
        virtual void close(void) { _taken = false; }

        virtual bool valid(void) { return _taken; }
        virtual fst_e fs(void) { return _fst; }

        virtual void setInfo(FileInfo const & info) { _info = info; }

    protected:
        File(fst_e fst) : _fst(fst) {}
        File(fst_e fst, FileInfo const & info) : _fst(fst), _info(info) {}

        fst_e _fst;
        FileInfo _info;
        uint32_t _remaining;
        bool _taken = false;
};

// DD = DevDisk
template < class DD, fst_e FST = FST_NONE >
class TFile : public File
{
    public:
        virtual int read(uint8_t * buf, int amt) = 0;
        virtual int read(uint8_t const ** p, uint8_t n) = 0;
        virtual bool rewind(void) = 0;

        virtual void close(void) { _dd = nullptr; File::close(); }
        virtual bool valid(void) { return File::valid() && (_dd != nullptr) && _dd->valid(); }

    protected:
        TFile(void) : File(FST) {}
        TFile(FileInfo const & info) : File(FST, info) {}

        DD * _dd = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
// File System /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template < class DD, fst_e FST = FST_NONE >
class FileSystem
{
    public:
        virtual int getFiles(FileInfo * infos, uint16_t num_infos, char const * const * exts) = 0;
        virtual File * open(FileInfo const & info) = 0;
        virtual bool valid(void) { return _dd.valid(); }

    protected:
        DD & _dd = DD::acquire();
};

////////////////////////////////////////////////////////////////////////////////
// Fat32 ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define FAT32_SECTOR_SIZE  SD_READ_BLK_LEN

#define BOOT_RECORD_SIGNATURE    0xAA55

#define MBR_EXE_CODE_LEN    446
#define NUM_PARTITIONS        4

#define SHORT_NAME_LEN   11
#define LONG_NAME_LEN_1  10
#define LONG_NAME_LEN_2  12
#define LONG_NAME_LEN_3   4

#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

#define LAST_LONG_ENTRY  0x40
#define DIR_ENTRY_FREE   0xE5
#define DIR_ENTRY_LAST   0x00

#define PTYPE_FAT12            0x01
#define PTYPE_FAT16_LT_32MB    0x04
#define PTYPE_EXT_DOS          0x05
#define PTYPE_FAT16_GT_32MB    0x06
#define PTYPE_FAT32            0x0B
#define PTYPE_FAT32_LBA13h     0x0C
#define PTYPE_FAT16_LBA13h     0x0E
#define PTYPE_EXT_DOS_LBA13h   0x0F

#define CLUSTER_MAX_FAT12    4085
#define CLUSTER_MAX_FAT16   65525

#define FAT32_CLUSTER_EOC   0x0FFFFFF8   // End of Chain
#define FAT32_CLUSTER_BAD   0x0FFFFFF7
#define FAT32_CLUSTER_MASK  0x0FFFFFFF
#define FAT32_CLUSTER_ERROR 0xFFFFFFFF

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

} __attribute__((packed));

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

} __attribute__((packed));

struct DirEntry
{
    union
    {
        // 32 bytes
        struct
        {
            char name[SHORT_NAME_LEN];     // Short 8.3 name
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

        };

        // 32 bytes
        struct
        {
            uint8_t order_num;                 // The order of this entry in the
                                               //   sequence of long dir entries
                                               //   associated with short dir entry.
                                               // If masked with 0x40 (LAST_LONG_ENTRY),
                                               //   it's the last in the set.
            char long_name1[LONG_NAME_LEN_1];  // Characters 1-5 of the long name (UTF-16LE)
            uint8_t long_attrs;                // Must be ATTR_LONG_NAME
            uint8_t type;                      // If zero, directory entry is a
                                               //   sub-component of a long name
            uint8_t cksum;                     // Checksum of name in the short
                                               //   dir entry at end of long dir set
            char long_name2[LONG_NAME_LEN_2];  // Characters 6-11 of the long name
            uint16_t first_cluster;            // MUST be ZERO.  Has no meaning in this context
            char long_name3[LONG_NAME_LEN_3];  // Characters 12-13 of the long name

        };

        // For access to common attributes field
        struct
        {
            uint8_t dummy[SHORT_NAME_LEN];
            uint8_t attrs;
        };
    };

    uint32_t cluster(void) const
    {
        return (((uint32_t)first_cluster_high << 16) & 0xFFFF0000)
            | ((uint32_t)first_cluster_low & 0x0000FFFF);
    }

} __attribute__((packed));

union sector_u
{
    uint8_t    a8[FAT32_SECTOR_SIZE];
    uint16_t  a16[FAT32_SECTOR_SIZE / 2];
    uint32_t  a32[FAT32_SECTOR_SIZE / 4];
    DirEntry  dir[FAT32_SECTOR_SIZE / 32];

    bool isBoot(void) { return a16[255] == BOOT_RECORD_SIGNATURE; }
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

} __attribute__((packed));

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
        virtual bool rewind(void);

        Fat32File(Fat32File const &) = delete;
        Fat32File & operator=(Fat32File const &) = delete;

    protected:
        Fat32File(FileInfo const & info) : TFile < DD, FST_FAT32 > (info) { this->_taken = true; rewind(); }
        Fat32File(void) {}

        virtual void setInfo(FileInfo const & info) { this->_info = info; this->_taken = true; rewind(); }

    private:
        bool nextSector(void);

        uint32_t _cluster = 0;
        uint32_t _sofc = 0;  // sector of cluster

        uint32_t _ds = 0; // data sector
        uint32_t _dsb_off = 0;  // data buffer offset
        sector_u _dsb;  // data buffer

        uint32_t _ts = 0; // table sector
        sector_u _tsb;  // table buffer
};

template < class DD >
bool Fat32File < DD >::nextSector(void)
{
    auto get_sector = [&](uint32_t ds) -> bool
    {
        _ds = ds;
        if (!this->_dd->read(_ds, _dsb.a8))
            this->close();
        return this->valid();
    };

    _dsb_off = 0;
    if (++_sofc != Fat32 < DD >::_s_sectors_per_cluster)
        return get_sector(_ds + 1);

    _sofc = 0;
    if (Fat32 < DD >::nextCluster(*this->_dd, _cluster, _ts, _tsb))
        return get_sector(Fat32 < DD >::dataSector(_cluster));

    this->close();  // Failure
    return this->valid();
}

template < class DD >
int Fat32File < DD >::read(uint8_t * buf, int amt)
{
    if (!this->valid() || (buf == nullptr) || (amt < 0))
        return -1;

    int n = 0;
    while (n != amt)
    {
        if (this->eof())
            return n;

        if ((_dsb_off == sizeof(_dsb.a8)) && !nextSector())
            return -1;

        uint32_t cpy = sizeof(_dsb.a8) - _dsb_off;
        if (cpy > (uint32_t)(amt - n)) cpy = amt - n;
        if (cpy > this->_remaining) cpy = this->_remaining;

        // Worried about getting an unaligned pointer and not sure if just
        // checking the first few bits is sufficient.
        //memcpy(buf + n, _dsb.a8 + _dsb_off, cpy);
        for (uint32_t i = 0; i < cpy; i++)
            buf[n+i] = _dsb.a8[_dsb_off+i];
        n += cpy; _dsb_off += cpy; this->_remaining -= cpy;
    }

    return n;
}

template < class DD >
int Fat32File < DD >::read(uint8_t const ** p, uint8_t n)
{
    if (!this->valid() || (p == nullptr) || (n > sizeof(_dsb.a8)))
        return -1;

    if (this->eof())
        return 0;

    if ((_dsb_off == sizeof(_dsb.a8)) && !nextSector())
        return -1;

    if ((_dsb_off + n) > sizeof(_dsb.a8))
        return -1;

    if (n > this->_remaining)
        n = this->_remaining;

    *p = &_dsb.a8[_dsb_off];
    _dsb_off += n; this->_remaining -= n;

    return n;
}

template < class DD >
bool Fat32File < DD >::rewind(void)
{
    if (!this->valid())
        return false;

    this->_remaining = this->_info.size;
    _cluster = this->_info.address;
    _sofc = _dsb_off = 0;

    uint32_t ts = Fat32 < DD >::tableSector(_cluster);
    uint32_t ds = Fat32 < DD >::dataSector(_cluster);

    if (((ts != _ts) && !this->_dd->read(ts, _tsb.a8))
            || ((ds != _ds) && !this->_dd->read(ds, _dsb.a8)))
    {
        this->close();
    }

    _ts = ts;
    _ds = ds;

    return this->valid();
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

        virtual int getFiles(FileInfo * infos, uint16_t num_infos, char const * const * exts);
        virtual File * open(FileInfo const & info);

        Fat32(Fat32 const &) = delete;
        Fat32 & operator=(Fat32 const &) = delete;

    private:
        Fat32(void);

        int getFiles(uint32_t cluster, uint8_t level, FileInfo * infos,
                uint16_t num_infos, uint16_t info_index, char const * const * exts);

        static constexpr uint8_t const _s_dir_max_level = 5;

        uint32_t _volume_sector_start = 0;
        uint32_t _root_cluster = 0;
        bool _valid = true;

        static uint32_t tableSector(uint32_t cluster);
        static uint32_t dataSector(uint32_t cluster);
        static bool nextCluster(DD & dd, uint32_t & cluster, uint32_t & table_sector, sector_u & table_sector_buffer);

        static uint32_t _s_table_sector_start;
        static uint32_t _s_data_sector_start;
        static uint32_t _s_sectors_per_cluster;
        static uint32_t _s_cluster_to_sector;

        static constexpr uint8_t const _s_num_files = 2;
        static Fat32File < DD > _s_files[_s_num_files];
};

template < class DD > uint32_t Fat32 < DD >::_s_table_sector_start = 0;
template < class DD > uint32_t Fat32 < DD >::_s_data_sector_start = 0;
template < class DD > uint32_t Fat32 < DD >::_s_sectors_per_cluster = 0;
template < class DD > uint32_t Fat32 < DD >::_s_cluster_to_sector = 0;
template < class DD > Fat32File < DD > Fat32 < DD >::_s_files[_s_num_files] = {};

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

    static sector_u s;

    if (!this->_dd.read(0, s.a8) || !s.isBoot())
    {
        _valid = false;
        return;
    }

    PartInfo const * p = (PartInfo const *)&s.a8[MBR_EXE_CODE_LEN];

    if (!getFatVolume(p, _volume_sector_start))
    {
        _valid = false;
        return;
    }

    // Don't need previous information so reuse sector enum
    if (!this->_dd.read(_volume_sector_start, s.a8) || !s.isBoot())
    {
        _valid = false;
        return;
    }

    FatInfo const * f = (FatInfo const *)s.a8;

    if (!isFat32(f) || !isValidFat32(f))
    {
        _valid = false;
        return;
    }

    _s_table_sector_start = _volume_sector_start + f->reserved_sectors;
    _s_data_sector_start = _s_table_sector_start + (f->num_FAT_sectors32 * f->num_FATs);
    _s_sectors_per_cluster = f->sectors_per_cluster;
    _s_cluster_to_sector = __builtin_ctz(_s_sectors_per_cluster);
    _root_cluster = f->root_cluster;
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
    return _s_data_sector_start + ((cluster - 2) << _s_cluster_to_sector);
}

template < class DD >
bool Fat32 < DD >::nextCluster(DD & dd, uint32_t & cluster, uint32_t & table_sector, sector_u & table_sector_buffer)
{
    uint32_t new_table_sector = tableSector(cluster);

    // Need to get a new sector in the FAT table
    if (table_sector != new_table_sector)
    {
        table_sector = new_table_sector;

        if (!dd.read(table_sector, table_sector_buffer.a8))
            return false;
    }

    cluster = table_sector_buffer.a32[cluster & ((FAT32_SECTOR_SIZE / 4) - 1)] & FAT32_CLUSTER_MASK;

    return true;
}

template < class DD >
File * Fat32 < DD >::open(FileInfo const & info)
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

    Fat32File < DD > & f = _s_files[i];

    f._dd = &this->_dd;

    f.setInfo(info);
    if (!f.valid())
        return nullptr;

    return &f;
}

template < class DD >
int Fat32 < DD >::getFiles(FileInfo * infos, uint16_t num_infos, char const * const * exts)
{
    if (!valid() || (infos == nullptr) || (exts == nullptr))
        return -1;

    int file_cnt = getFiles(_root_cluster, 0, infos, num_infos, 0, exts);

    if (file_cnt != -1)
        return file_cnt;

    _valid = false;

    return -1;
}

template < class DD >
int Fat32 < DD >::getFiles(uint32_t cluster, uint8_t level, FileInfo * infos,
        uint16_t num_infos, uint16_t info_index, char const * const * exts)
{
    if (level == _s_dir_max_level)
        return info_index;

    static sector_u data_sectors[_s_dir_max_level];
    static sector_u table_sectors[_s_dir_max_level];

    uint32_t data_sector = dataSector(cluster);
    uint32_t table_sector = 0;

    if (!this->_dd.read(data_sector, data_sectors[level].a8))
        return -1;

    DirEntry const * entry = (DirEntry const *)data_sectors[level].dir;
    uint32_t off = 0, soff = 0;

    while (entry->name[0] != DIR_ENTRY_LAST)
    {
        if (((entry->attrs & ATTR_LONG_NAME) != ATTR_LONG_NAME)
                && (entry->name[0] != 0x2E)  // Skip dot files
                && (entry->name[0] != DIR_ENTRY_FREE))
        {
            if ((entry->attrs & ATTR_DIRECTORY) && !(entry->attrs & (ATTR_SYSTEM | ATTR_HIDDEN)))
            {
                int file_cnt = getFiles(entry->cluster(), level+1, infos, num_infos, info_index, exts);
                if (file_cnt == -1)
                    return -1;
                info_index = file_cnt;
            }
            else if (!(entry->attrs & (ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)))
            {
                char const * ext = entry->name + 8;

                int i;
                for (i = 0; exts[i] != nullptr; i++)
                {
                    if (strncmp(ext, exts[i], 3) == 0)
                        break;
                }

                if (exts[i] != nullptr)
                    infos[info_index++].set(entry->cluster(), entry->file_size);
            }

            if (info_index == num_infos)
                break;
        }

        off += sizeof(DirEntry);
        if ((off % FAT32_SECTOR_SIZE) != 0)
        {
            entry++;
            continue;
        }

        if ((++soff % _s_sectors_per_cluster) != 0)
        {
            data_sector++;
        }
        else
        {
            if (!nextCluster(this->_dd, cluster, table_sector, table_sectors[level]))
                return -1;

            data_sector = dataSector(cluster);
        }

        if (!this->_dd.read(data_sector, data_sectors[level].a8))
            return -1;

        entry = (DirEntry const *)data_sectors[level].dir;
    }

    return info_index;
}

////////////////////////////////////////////////////////////////////////////////
// Templates ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
using TFs = Fat32 < TDisk >;

#if 0
template
    < template < uint16_t, pin_t, template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
    class DD, uint16_t RBL, pin_t CS, template < pin_t, pin_t, pin_t > class SPI, pin_t MOSI, pin_t MISO, pin_t SCK >
class TEST
{
    private:
        DD < RBL, CS, SPI, MOSI, MISO, SCK > d;
};

using TTest = TEST < DevDisk, 512, PIN_SD_CS, SPI0, PIN_MOSI, PIN_MISO, PIN_SCK >;
#endif

#endif
