#ifndef _SCSI_H_
#define _SCSI_H_

#include "types.h"
#include "disk.h"
#include <string.h>

#define SCSI_PASSED   0
#define SCSI_FAILED  -1
#define SCSI_ERROR   -2

// Direct Access Block Device Operation Codes
enum op_code_e : uint8_t
{
/*     D - Direct Access Block Device (SBC-4)            Device Column key
       .Z - Host Managed Zoned Block Device (ZBC)        ---------------------
       . T - Sequential Access Device (SSC-5)            M = Mandatory
       .  P - Processor Device (SPC-2)                   O = Optional
       .  .R - C/DVD Device (MMC-6)                      V = Vendor specific
       .  . O - Optical Memory Block Device (SBC)        Z = Obsolete -- with
       .  .  M - Media Changer Device (SMC-3)                [std] identifying
       .  .  .A - Storage Array Device (SCC-2)               last standard
       .  .  . E - SCSI Enclosure Services device (SES-3)
       .  .  .  B - Simplified Direct-Access (Reduced Block) device (RBC)
       .  .  .  .K - Optical Card Reader/Writer device (OCRW)
       .  .  .  . V - Automation/Device Interface device (ADC-4)
       .  .  .  .  F - Object-based Storage Device (OSD-2)
       .  .  .  .  .
   OP  DZTPROMAEBKVF    Description */
/* 00  MMMMMMMMMMMMM */ TEST_UNIT_READY = 0x00,
// 01    M              REWIND
// 01  Z   ZZZ          REZERO UNIT,
// 02  V VVV V
/* 03  MMMMMMMMMOMMM */ REQUEST_SENSE = 0x03,
// 04  MO  OO           FORMAT UNIT
// 04    O              FORMAT MEDIUM
// 04                   FORMAT
// 05  V MVV V          READ BLOCK LIMITS
// 06  V VVV V
// 07  O V  OV          REASSIGN BLOCKS
// 07        O          INITIALIZE ELEMENT STATUS
// 08  Z M  OV          READ(6)
// 08     O             RECEIVE
// 08                   GET MESSAGE(6)
// 09  V VVV V
// 0A  Z O  OV          WRITE(6)
// 0A     M             SEND(6)
// 0A                   SEND MESSAGE(6)
// 0A                   PRINT
// 0B  Z   OZV          SEEK(6)
// 0B    O              SET CAPACITY
// 0B                   SLEW AND PRINT
// 0C  V VVV V
// 0D  V VVV V
// 0E  V VVV V
// 0F  V OVV V          READ REVERSE(6)
// 10  V MVV            WRITE FILEMARKS(6)
// 10                   SYNCHRONIZE BUFFER
// 11  V MVV            SPACE(6)
/* 12  MMMMMMMMMMMMM */ INQUIRY = 0x12,
// 13  V  VV
// 13    O              VERIFY(6)
// 14  V OVV            RECOVER BUFFERED DATA
/* 15  O M  OOOO OO  */ MODE_SELECT_6 = 0x15,
// 16  Z ZZ OOOZ O      RESERVE(6)
// 16        Z          RESERVE ELEMENT(6)
// 17  Z ZZ OOOZ O      RELEASE(6)
// 17        Z          RELEASE ELEMENT(6)
// 18  Z ZZZO    Z      COPY
// 19  V MVV            ERASE(6)
/* 1A  O M  OOOO OO  */ MODE_SENSE_6 = 0x1A,
// 1B  OM  OO O MO O    START STOP UNIT
// 1B    O        M     LOAD UNLOAD
// 1B                   SCAN
// 1B                   STOP PRINT
// 1B        O          OPEN/CLOSE IMPORT/EXPORT ELEMENT
// 1C  O OO OOOM OOO    RECEIVE DIAGNOSTIC RESULTS
// 1D  MOMM MMOM MMM    SEND DIAGNOSTIC
// 1E  O O OOO   O O    PREVENT ALLOW MEDIUM REMOVAL
// 1F
// 20  V   VV    V
// 21  V   VV    V
// 22  V   VV    V
// 23  V    V    V
/* 23      O         */ READ_FORMAT_CAPACITIES = 0x23,  // For Windows
// 24  V   V            SET WINDOW
/* 25  M    M   M    */ READ_CAPACITY_10 = 0x25,
// 25      O            READ CAPACITY
// 25            M      READ CARD CAPACITY
// 25                   GET WINDOW
// 26  V   V
// 27  V   V
/* 28  M   OM   MM   */ READ_10 = 0x28,
// 28                   GET MESSAGE(10)
// 29  V   VO           READ GENERATION
/* 2A  O   OM   MO   */ WRITE_10 = 0x2A,
// 2A                   SEND(10)
// 2A                   SEND MESSAGE(10)
// 2B  Z   OO    O      SEEK(10)
// 2B    M              LOCATE(10)
// 2B        O          POSITION TO ELEMENT
// 2C  V   OO           ERASE(10)
// 2D       O           READ UPDATED BLOCK
// 2D  V
// 2E  O   OO   MO      WRITE AND VERIFY(10)
// 2F  O   OO           VERIFY(10)
// 30  Z   ZZ           SEARCH DATA HIGH(10)
// 31  Z   ZZ           SEARCH DATA EQUAL(10)
// 31                   OBJECT POSITION
// 32  Z   ZZ           SEARCH DATA LOW(10)
// 33  Z   ZO           SET LIMITS(10)
// 34  O    O    O      PRE-FETCH(10)
// 34    M              READ POSITION
// 34                   GET DATA BUFFER STATUS
// 35  O   OO   MO      SYNCHRONIZE CACHE(10)
// 36  Z    O    O      LOCK UNLOCK CACHE(10)
// 37  O    O           READ DEFECT DATA(10)
// 37        O          INITIALIZE ELEMENT STATUS WITH RANGE
// 38       O    O      MEDIUM SCAN
// 39  Z ZZZO    Z      COMPARE
// 3A  Z ZZZO    Z      COPY AND VERIFY
// 3B  OOOOOOOOOMOOO    WRITE BUFFER
// 3C  OOOOOOOOO OOO    READ BUFFER(10)
// 3D       O           UPDATE BLOCK
// 3E  Z    Z           READ LONG(10)
// 3F  O    O           WRITE LONG(10)
// 40  Z ZZZOZ          CHANGE DEFINITION
// 41  O                WRITE SAME(10)
// 42  O                UNMAP
// 42      O            READ SUB-CHANNEL
// 43      O            READ TOC/PMA/ATIP
// 44    M        M     REPORT DENSITY SUPPORT
// 44                   READ HEADER
// 45      O            PLAY AUDIO(10)
// 46      M            GET CONFIGURATION
// 47      O            PLAY AUDIO MSF
// 48  OO       O       SANITIZE
// 49
// 4A      M            GET EVENT STATUS NOTIFICATION
// 4B      O            PAUSE/RESUME
// 4C  OOOO OOOO OOO    LOG SELECT
// 4D  OMOO OOOO OMO    LOG SENSE
// 4E      O            STOP PLAY/SCAN
// 4F
// 50  Z                XDWRITE(10)
// 51  O                XPWRITE(10)
// 51      O            READ DISC INFORMATION
// 52  Z                XDREAD(10)
// 52      O            READ TRACK INFORMATION
// 53  O                XDWRITEREAD(10)
// 53      O            RESERVE TRACK
// 54      O            SEND OPC INFORMATION
/* 55  OMO MOOOOMOMO */ MODE_SELECT_10 = 0x55,
// 56  Z ZZ OOOZ        RESERVE(10)
// 56        Z          RESERVE ELEMENT(10)
// 57  Z ZZ OOOZ        RELEASE(10)
// 57        Z          RELEASE ELEMENT(10)
// 58      O            REPAIR TRACK
// 59
/* 5A  OMO MOOOOMOMO */ MODE_SENSE_10 = 0x5A,
// 5B      O            CLOSE TRACK/SESSION
// 5C      O            READ BUFFER CAPACITY
// 5D      O            SEND CUE SHEET
// 5E  OOMO OOOO   M    PERSISTENT RESERVE IN
// 5F  OOMO OOOO   M    PERSISTENT RESERVE OUT
// 7E  O O O OOOO O     extended CDB
// 7F  O           M    variable length CDB (more than 16 bytes)
// 80  Z                XDWRITE EXTENDED(16)
// 80    M              WRITE FILEMARKS(16)
// 81  Z                REBUILD(16)
// 81    O              READ REVERSE(16)
// 82  Z                REGENERATE(16)
// 82    O              ALLOW OVERWRITE
// 83  O OO O    OO     Third-party Copy OUT
// 84  O OO O    OO     Third-party Copy IN
// 85  OO       O       ATA PASS-THROUGH(16)
// 86  Z ZZ ZZZZZZZ     ACCESS CONTROL IN
// 87  Z ZZ ZZZZZZZ     ACCESS CONTROL OUT
// 88  MMO  O   O       READ(16)
// 89  O                COMPARE AND WRITE
// 8A  OMO  O   O       WRITE(16)
// 8B  O                ORWRITE
// 8C  O O  OO  O M     READ ATTRIBUTE
// 8D  O O  OO  O O     WRITE ATTRIBUTE
// 8E  O    O   O       WRITE AND VERIFY(16)
// 8F  OOO  O   O       VERIFY(16)
// 90  O    O   O       PRE-FETCH(16)
// 91  OM   O   O       SYNCHRONIZE CACHE(16)
// 91    O              SPACE(16)
// 92  Z    O           LOCK UNLOCK CACHE(16)
// 92    M              LOCATE(16)
// 93  OM               WRITE SAME(16)
// 93    M              ERASE(16)
// 94  OM               ZBC OUT
// 95  OM               ZBC IN
// 96
// 97
// 98
// 99
// 9A  OO               WRITE STREAM(16)
// 9B  OOO   OOO  O     READ BUFFER(16)
// 9C  O                WRITE ATOMIC(16)
// 9D                   SERVICE ACTION BIDIRECTIONAL
// 9E  OM               SERVICE ACTION IN(16)
// 9F             M     SERVICE ACTION OUT(16)
/* A0  MMMO OMMM OMO */ REPORT_LUNS = 0xA0,
// A1      O            BLANK
// A1  OO       O       ATA PASS-THROUGH(12)
// A2  OOO O      O     SECURITY PROTOCOL IN
// A3  OMO  OOMOOOM     MAINTENANCE IN
// A3      O            SEND KEY
// A4  OOO  OOOOOOO     MAINTENANCE OUT
// A4      O            REPORT KEY
// A5    Z  OM          MOVE MEDIUM
// A5      O            PLAY AUDIO(12)
// A6        O          EXCHANGE MEDIUM
// A6      O            LOAD/UNLOAD C/DVD
// A7  Z Z  O           MOVE MEDIUM ATTACHED
// A7      O            SET READ AHEAD
// A8  O   OO           READ(12)
// A8                   GET MESSAGE(12)
// A9             O     SERVICE ACTION OUT(12)
// AA  O   OO           WRITE(12)
// AA                   SEND MESSAGE(12)
// AB      O      O     SERVICE ACTION IN(12)
// AC       O           ERASE(12)
// AC      O            GET PERFORMANCE
// AD      O            READ DVD STRUCTURE
// AE  O    O           WRITE AND VERIFY(12)
// AF  O    O           VERIFY(12)
// B0      ZZ           SEARCH DATA HIGH(12)
// B1      ZZ           SEARCH DATA EQUAL(12)
// B2      ZZ           SEARCH DATA LOW(12)
// B3  Z   ZO           SET LIMITS(12)
// B4  Z Z ZO           READ ELEMENT STATUS ATTACHED
// B5  OOO O      O     SECURITY PROTOCOL OUT
// B5        O          REQUEST VOLUME ELEMENT ADDRESS
// B6        O          SEND VOLUME TAG
// B6      O            SET STREAMING
// B7  OO   O           READ DEFECT DATA(12)
// B8    Z ZOM          READ ELEMENT STATUS
// B9      O            READ CD MSF
// BA  O    OOMO        REDUNDANCY GROUP (IN)
// BA      O            SCAN
// BB  O    OOOO        REDUNDANCY GROUP (OUT)
// BB      O            SET CD SPEED
// BC  O    OOMO        SPARE (IN)
// BD  O    OOOO        SPARE (OUT)
// BD      O            MECHANISM STATUS
// BE  O    OOMO        VOLUME SET (IN)
// BE      O            READ CD
// BF  O    OOOO        VOLUME SET (OUT)
// BF      O            SEND DVD STRUCTURE
};

// Sense Key
enum sk_e : uint8_t
{
    NO_SENSE,
    RECOVERED_ERROR,
    NOT_READY,
    MEDIUM_ERROR,
    HARDWARE_ERROR,
    ILLEGAL_REQUEST,
    UNIT_ATTENTION,
    DATA_PROTECT,
    BLANK_CHECK,
    VENDOR_SPECIFIC,
    COPY_ABORTED,
    ABORTED_COMMAND,
    VOLUME_OVERFLOW = 0x0D,
    MISCOMPARE,
    COMPLETED,
};

// Peripheral Device Type
enum pdt_e : uint8_t                  // Doc.  Description
{                                     // ----------------------------------------------------------
    DIRECT_ACCESS_BLOCK,              // SBC   Direct-access block device (e.g. magnetic disk)
    SEQUENTIAL_ACCESS,                // SSC   Sequential-access device (e.g. magnetic tape)
    PRINTER,                          // SSC   Printer device
    PROCESSOR,                        // SPC   Processor device
    WRITE_ONCE,                       // SBC   Write-once device (e.g. some optical disks)
    CD_DVD,                           // MMC   CD/DVD device
    SCANNER,                          // SCSI  Scanner device
    OPTICAL_MEMORY,                   // SBC   Optical memory device (e.g. some optical disks)
    MEDIUM_CHANGER,                   // SMC   Media/Medium changer device (e.g. juekboxes)
    COMMUNICATIONS,                   // SCSI  Communications device
    STORAGE_ARRAY_CONTROLLER = 0x0C,  // SSC   Storage array controller device (e.g. RAID)
    ENCLOSURE_SERVICES,               // SES   Enclosure services device
    SIMPLIFIED_DIRECT_ACCESS,         // RBC   Simplified direct-access device (e.g. magnetic disk)
    OPTICAL_CARD_READER_WRITER,       // OCRW  Optical card reader/writer device
    OBJECT_BASED_STORAGE = 0x11,      // OSD   Object-based Storage Device
    AUTOMATION_DRIVE_INTERFACE,       // ADC   Automation/Drive Interface
    HOST_MANAGED_ZONED_BLOCK = 0x14,  // ZBC   Host managed zoned block device
    WELL_KNOWN_LOGICAL_UNIT = 0x1E,   // SPC   Well known logical unit
    UNKNOWN_OR_NO_DEVICE_TYPE = 0x1F  // ----  Unknown or no device type
};

class Scsi
{
    public:
        Scsi(void);

        int request(uint8_t * req, uint8_t rlen);
        int read(uint8_t * buf, uint16_t blen);
        int write(uint8_t * data, uint16_t dlen);

        bool done(void) { return _transferred == _transfer_length; }

    private:
        // Client Requests
        int testUnitReady(uint8_t * req);
        int requestSense(uint8_t * req);
        int inquiry(uint8_t * req);
        int modeSelect(uint8_t * req);
        int modeSense(uint8_t * req);
        int readFormatCapacities(uint8_t * req);
        int readCapacity10(uint8_t * req);
        int read10(uint8_t * req);
        int write10(uint8_t * req);
        int reportLuns(uint8_t * req);

        // Client Data-In Buffer
        int paramRead(uint8_t * buf, uint16_t blen);
        int dataRead(uint8_t * buf, uint16_t blen);

        // Client Data-Out Buffer
        int paramWrite(uint8_t * data, uint16_t dlen);
        int dataWrite(uint8_t * data, uint16_t dlen);

        TDisk & _dd = TDisk::acquire();
        uint32_t const _num_blocks = _dd.blocks();
        static constexpr uint16_t const _s_block_size = SD_READ_BLK_LEN;

        op_code_e _op_code;

        uint32_t _lba = 0;
        uint16_t _transfer_length = 0;
        uint16_t _transferred = 0;
        uint8_t _dbuf[_s_block_size];
        uint16_t _doff = 0;

        // Sense Key Specific Data
        uint8_t _sks[3];

        // In milliseconds.  A value of zero means disabled.
        static constexpr uint8_t const _s_recovery_time_limit = 0;
        static constexpr uint8_t const _s_read_retry_count = 10;
        static constexpr uint8_t const _s_write_retry_count = 10;

        static constexpr char const * const _s_vendor_id = "KTW     ";
        static constexpr char const * const _s_prod_id = "Boogie Box      ";
        static constexpr char const * const _s_prod_rev = "1.1 ";

        ////////////////////////////////////////////////////////////////////////
        // Inquiry /////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////

        // Standard Inquiry
        struct StandardInquiry
        {
            // Byte 0
            uint8_t pdt : 5;  // Peripheral Device Type
            uint8_t pq  : 3;  // Peripheral Qualifier - set this to 000b

            // Byte 1
            uint8_t r1      : 6;  // reserved
            uint8_t lu_cong : 1;  // Logical Unit Conglomerate
            uint8_t rmb     : 1;  // Removable Medium Bit

            // Byte 2
            uint8_t version;

            // Byte 3
            uint8_t rdf      : 4;  // Response Data Format - shall be set to 0x02
            uint8_t hi_sup   : 1;  // Historical Support
            uint8_t norm_aca : 1;  // Normal ACA Supported - support NACA bit in CDB CONTROL byte
            uint8_t r2       : 1;  // reserved
            uint8_t r3       : 1;  // reserved

            // Byte 4
            uint8_t alen;  // Length of bytes following this field - set to 31

            // Byte 5
            uint8_t protect : 1;  // Support of protection information
            uint8_t r4      : 2;  // reserved
            uint8_t pc3     : 1;  // Third-Party Copy
            uint8_t tpgs    : 2;  // Target Port Group Support
            uint8_t o1      : 1;  // obsolete
            uint8_t sccs    : 1;  // SCC Supported

            // Byte 6
            uint8_t o2       : 1;  // obsolete
            uint8_t r5       : 1;  // reserved
            uint8_t r6       : 1;  // reserved
            uint8_t o3       : 1;  // obsolete
            uint8_t multi_p  : 1;  // Multiple SCSI Port
            uint8_t vs1      : 1;  // vendor specific
            uint8_t enc_serv : 1;  // Enclosure Services
            uint8_t o4       : 1;  // obsolete

            // Byte 7
            uint8_t vs2     : 1;  // vendor specific
            uint8_t cmd_que : 1;  // Shall be set to 1
            uint8_t r7      : 1;  // reserved
            uint8_t o5      : 1;  // obsolete
            uint8_t o6      : 1;  // obsolete
            uint8_t r8      : 1;  // reserved
            uint8_t o7      : 1;  // obsolete

            // Bytes 8-15
            uint8_t vendor_id[8];

            // Bytes 16-31
            uint8_t prod_id[16];

            // Bytes 32-35
            uint8_t prod_rev[4];

            enum version_e : uint8_t { NO_CLAIM, SPC_0 = 0x03, SPC_2, SPC_3, SPC_4, SPC_5 };

            StandardInquiry(void)
                : pdt(DIRECT_ACCESS_BLOCK),
                  rmb(1),     // Removable Media
                  version(SPC_2),
                  rdf(0x02),  // Must be set to 0x02
                  alen(0x1F)  // 31 additional bytes
            {}

        } __attribute__ ((packed));

        // Vital Product Data //////////////////////////////////////////////////
        enum vpd_e : uint8_t
        {
            SUPPORTED_PAGES,
            UNIT_SERIAL_NUMBER = 0x80,
            BLOCK_DEVICE_CHARACTERISTICS = 0xB1,
        };

        struct VPD
        {
            union
            {
                struct
                {
                    uint8_t pdt : 4;  // Peripheral Device Type
                    uint8_t pq  : 4;  // Peripheral Qualifier
                };

                uint8_t const peripheral_device_type = DIRECT_ACCESS_BLOCK;

            } __attribute__ ((packed));

            uint8_t const page_code;
            uint16_t const page_length;

            VPD(uint8_t pc, uint16_t pl) : page_code(pc), page_length(htons(pl)) {}
            virtual uint8_t size(void) const { return ntohs(page_length) + sizeof(VPD); }

        } __attribute__ ((packed));

        // Supported VPD Pages
        struct SupportedVPDPages : public VPD
        {
            static constexpr uint8_t const num_vpd_pages = 3;

            uint8_t const pages[num_vpd_pages] =
            {
                SUPPORTED_PAGES,
                UNIT_SERIAL_NUMBER,
                BLOCK_DEVICE_CHARACTERISTICS,
            };

            SupportedVPDPages(void) : VPD(SUPPORTED_PAGES, num_vpd_pages) {}

        } __attribute__ ((packed));

        // Unit Serial Number
        struct UnitSerialNumber : public VPD
        {
            static constexpr uint8_t const serial_number_length = 16;

            // XXX Should be in sync with USB serial number
            uint8_t const product_serial_number[serial_number_length] =
            {
                '0', '0', '0', '0',
                '0', '0', '0', '0',
                '0', '0', '0', '0',
                '0', '0', '0', '1',
            };

            UnitSerialNumber(void) : VPD(UNIT_SERIAL_NUMBER, serial_number_length) {}

        } __attribute__ ((packed));

        // Block Device Characteristics
        struct BlockDeviceCharacteristics : public VPD
        {
            uint16_t const medium_rotation_rate = htons(0x0001);  // Non-rotating medium
            uint8_t const product_type = SECURE_DIGITAL_CARD;

            union
            {
                struct
                {
                    uint8_t nominal_form_factor : 4;  // 5h Less than 1.8 inch
                    uint8_t wacereq : 2;  // 00b Not specified
                    uint8_t wabereq : 2;  // 00b Not specified
                };

                uint8_t const flags0 = 0x05;

            } __attribute__ ((packed));

            union
            {
                struct
                {
                    uint8_t vbuls : 1;  // 0b  No unmapped LBAs while processing VERIFY
                    uint8_t fuab  : 1;  // 0b  SBC-2 behavior for SYNCHRONIZE CACHE
                    uint8_t bocs  : 1;  // 0b  Background operation control not supported
                    uint8_t rbwz  : 1;  // 0b  Reassign blocks - write vendor specific data
                    uint8_t zoned : 2;  // 00b Not reported
                    uint8_t r0    : 2;
                };

                uint8_t flags1;

            } __attribute__ ((packed));

            uint8_t const r1[3] = {};
            uint32_t depopulation_time = 0;

            uint8_t const r2[48] = {};

            ////////////////////////////////////////////////////////////////////

            // Product Type
            enum pt_e : uint8_t
            {
                NOT_INDICATED,
                CFAST,
                COMPACT_FLASH,
                MEMORY_STICK,
                MULTIMEDIA_CARD,
                SECURE_DIGITAL_CARD,
                XQD,
                UNIVERSAL_FLASH_STORAGE,
            };

            BlockDeviceCharacteristics(void) : VPD(BLOCK_DEVICE_CHARACTERISTICS, 0x003C) {}

        } __attribute__ ((packed));

        StandardInquiry _si;
        SupportedVPDPages _vpd_sp;
        UnitSerialNumber _vpd_us;
        BlockDeviceCharacteristics _vpd_bdc;


        ////////////////////////////////////////////////////////////////////////
        // Sense Data //////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////

        bool _have_sense = false;

        // ILLEGAL REQUEST
        template < uint8_t ASC, uint8_t ASCQ >
        void illegalRequest(bool in_cdb, bool info_valid, uint32_t info, uint16_t fp, bool bpv, uint8_t bp = 0);

        template < uint8_t ASC, uint8_t ASCQ >
        void illegalRequest(bool in_cdb, uint16_t field_pointer) {
            return illegalRequest < ASC, ASCQ > (in_cdb, false, 0, field_pointer, false);
        }

        template < uint8_t ASC, uint8_t ASCQ >
        void illegalRequest(bool in_cdb, uint16_t field_pointer, uint8_t bit_pointer) {
            return illegalRequest < ASC, ASCQ > (in_cdb, false, 0, field_pointer, true, bit_pointer);
        }

        void invalidCommand(void) {
            illegalRequest < 0x20, 0x00 > (false, false, 0, 0, false);
        }

        void invalidBitField(uint16_t field_pointer, uint8_t bit_pointer) {
            return illegalRequest < 0x24, 0x00 > (true, field_pointer, bit_pointer);
        }

        void invalidField(uint16_t field_pointer) {
            return illegalRequest < 0x24, 0x00 > (true, field_pointer);
        }

        void invalidLBA(uint16_t field_pointer, uint32_t lba) {
            return illegalRequest < 0x21, 0x00 > (true, true, lba, field_pointer, false);
        }

        void noSaveSupport(void) {
            return illegalRequest < 0x39, 0x00 > (true, false, 0, 0, false);
        }

        void errorParamListLen(void) {
            return illegalRequest < 0x1A, 0x00 > (true, false, 0, 0, false);
        }

        // HARDWARE ERROR, MEDIUM ERROR, RECOVERED ERROR
        template < sk_e SK, uint8_t ASC, uint8_t ASCQ >
        void hmrError(uint16_t retry_count, bool info_valid, uint32_t info = 0);

        // HARDWARE ERROR
        template < uint8_t ASC, uint8_t ASCQ >
        void hardwareError(uint16_t retry_count, uint32_t info) {
            hmrError < HARDWARE_ERROR, ASC, ASCQ > (retry_count, true, info);
        }

        template < uint8_t ASC, uint8_t ASCQ >
        void hardwareError(uint16_t retry_count) {
            hmrError < HARDWARE_ERROR, ASC, ASCQ > (retry_count, false);
        }

        // MEDIUM ERROR
        template < uint8_t ASC, uint8_t ASCQ >
        void mediumError(uint16_t retry_count, uint32_t info) {
            hmrError < MEDIUM_ERROR, ASC, ASCQ > (retry_count, true, info);
        }

        template < uint8_t ASC, uint8_t ASCQ >
        void mediumError(uint16_t retry_count) {
            hmrError < MEDIUM_ERROR, ASC, ASCQ > (retry_count, false);
        }

        void diskError(uint8_t retry_count, uint32_t lba) {
            // UNRECOVERED READ ERROR - Not sure what to put for a write error
            mediumError < 0x11, 0x00 > (retry_count, lba);
        }

        void readError(uint32_t lba) { diskError(_s_read_retry_count, lba); }
        void writeError(uint32_t lba) { diskError(_s_write_retry_count, lba); }

        // RECOVERED ERROR
        template < uint8_t ASC, uint8_t ASCQ >
        void recoveredError(uint16_t retry_count, uint32_t info) {
            hmrError < RECOVERED_ERROR, ASC, ASCQ > (retry_count, true, info);
        }

        template < uint8_t ASC, uint8_t ASCQ >
        void recoveredError(uint16_t retry_count) {
            hmrError < RECOVERED_ERROR, ASC, ASCQ > (retry_count, false);
        }

        // NO SENSE, NOT READY
        template < sk_e SK, uint8_t ASC, uint8_t ASCQ >
        void noSense_notReady(uint16_t progress_indication);

        void noSense(void) {
            noSense_notReady < NO_SENSE, 0x00, 0x00 > (0);
        }

        template < uint8_t ASC, uint8_t ASCQ >
        void notReady(uint16_t progress_indication) {
            noSense_notReady < NOT_READY, ASC, ASCQ > (progress_indication);
        };

        template < uint8_t ASC, uint8_t ASCQ >
        void noSense(uint16_t progress_indication) {
            noSense_notReady < NO_SENSE, ASC, ASCQ > (progress_indication);
        }

        // COPY ABORTED
        template < uint8_t ASC, uint8_t ASCQ >
        void copyAborted(bool in_sd, uint16_t fp, bool bpv, uint8_t bp = 0);

        template < uint8_t ASC, uint8_t ASCQ >
        void copyAborted(bool in_sd, uint16_t field_pointer) {
            copyAborted < ASC, ASCQ > (in_sd, field_pointer, false);
        }

        template < uint8_t ASC, uint8_t ASCQ >
        void copyAborted(bool in_sd, uint16_t field_pointer, uint8_t bit_pointer) {
            copyAborted < ASC, ASCQ > (in_sd, field_pointer, true, bit_pointer);
        }

        // UNIT ATTENTION
        template < uint8_t ASC, uint8_t ASCQ >
        void unitAttention(bool overflow);

        // Add Sense Data
        template < sk_e SK, uint8_t ASC, uint8_t ASCQ >
        void addSense(bool info_valid, uint32_t info, uint32_t cs_info, uint8_t fruc);

        template < sk_e SK, uint8_t ASC, uint8_t ASCQ >
        void addSense(void) { addSense < SK, ASC, ASCQ > (false, 0, 0, 0); }

        template < sk_e SK, uint8_t ASC, uint8_t ASCQ >
        void addSense(uint32_t info) { addSense < SK, ASC, ASCQ > (true, info, 0, 0); }


        ////////////////////////////////////////////////////////////////////////
        // Mode Pages //////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////

        // MODE SELECT/SENSE(6) Header
        struct ModePage6
        {
            uint8_t mode_data_length;
            uint8_t const medium_type = 0x00;

            union
            {
                struct
                {
                    uint8_t r0     : 4;
                    uint8_t dpofua : 1;  // Disable Page Out & Force Unit Access support
                    uint8_t r1     : 2;
                    uint8_t wp     : 1;  // Write-protected
                };

                uint8_t const device_specific_parameter = 0;

            } __attribute__ ((packed));

            uint8_t block_descriptor_length;

        } __attribute__ ((packed));

        // MODE SELECT/SENSE(10) Header
        struct ModePage10
        {
            uint16_t mode_data_length;
            uint8_t const medium_type = 0x00;

            union
            {
                struct
                {
                    uint8_t r0     : 4;
                    uint8_t dpofua : 1;
                    uint8_t r1     : 2;
                    uint8_t wp     : 1;
                };

                uint8_t const device_specific_parameter = 0;

            } __attribute__ ((packed));

            union
            {
                struct
                {
                    uint8_t long_lba : 1;
                    uint8_t r2       : 7;
                };

                uint8_t const llba = 0;  // Don't ever return a long lba

            } __attribute__ ((packed));

            uint8_t const r3 = 0;   
            uint16_t block_descriptor_length;

        } __attribute__ ((packed));

        // Mode Page Short LBA Block Descriptor
        struct ModePageShortBD
        {
            uint32_t const number_of_logical_blocks;

            union
            {
                struct
                {
                    uint32_t r0  : 8;
                    uint32_t lbl : 24;
                };

                uint32_t const logical_block_length;

            } __attribute__ ((packed));

            ////////////////////////////////////////////////////////////////////

            ModePageShortBD(uint32_t num_blocks, uint32_t block_length)
                : number_of_logical_blocks(htonl(num_blocks)),
                  logical_block_length(htonl(block_length)) {}

        } __attribute__ ((packed));

        // Mode Page Long LBA Block Descriptor
        struct ModePageLongBD
        {
            //uint64_t number_of_logical_blocks;
            uint32_t const u0 = 0;  // unused
            uint32_t const number_of_logical_blocks;
            uint32_t const r0 = 0;
            uint32_t const logical_block_length;

            ModePageLongBD(uint32_t num_blocks, uint32_t block_length)
                : number_of_logical_blocks(htonl(num_blocks)),
                  logical_block_length(htonl(block_length)) {}

        } __attribute__ ((packed));

        // Mode Page Codes
        enum mpc_e : uint8_t
        {
            READ_WRITE_ERROR_RECOVERY = 0x01,
            FLEXIBLE_DISK = 0x05,
            CACHING = 0x08,
            INFORMATIONAL_EXCEPTIONS_CONTROL = 0x1C,
            ALL_SUPPORTED_PAGES = 0x3F
        };

        // Common header for mode pages
        struct ModePage
        {
            union
            {
                struct
                {
                    uint8_t page_code : 5;  // 01h
                    uint8_t spf       : 1;  // Set to 0b
                    uint8_t ps        : 1;  // Parameters Saveable - set to 0
                };

                uint8_t const hdr;

            } __attribute__ ((packed));

            uint8_t const page_length;

            ////////////////////////////////////////////////////////////////////

            ModePage(mpc_e page_code, uint8_t page_len)
                : hdr(page_code), page_length(page_len) {}

        } __attribute__ ((packed));

        // Read/Write Error Recovery
        struct ReadWriteErrorRecovery : public ModePage
        {
            union
            {
                // Set all of these to zero
                struct
                {
                    uint8_t ro0  : 1;
                    uint8_t dte  : 1;  // Data Terminate on Error
                    uint8_t per  : 1;  // Post Error
                    uint8_t ro1  : 1;
                    uint8_t rc   : 1;  // Read Continuous
                    uint8_t tb   : 1;  // Transfer Block
                    uint8_t arre : 1;  // Automatic Read Reassignment Enabled
                    uint8_t awre : 1;  // Automatic Write Reassignment Enabled
                };

                uint8_t const flags0 = 0;

            } __attribute__ ((packed));

            uint8_t const read_retry_count = _s_read_retry_count;
            uint8_t const ro2 = 0;
            uint8_t const ro3 = 0;
            uint8_t const ro4 = 0;

            union
            {
                // Set all of these to 0
                struct
                {
                    uint8_t mmc6   : 2;  // Restricted for MMC-6 - dont' use
                    uint8_t ro5    : 3;
                    uint8_t mwr    : 2;  // Misaligned Write Reporting
                    uint8_t lbpere : 1;  // Logical Block Provisioning Error Reporting Enabled
                };

                uint8_t const flags1 = 0;

            } __attribute__ ((packed));

            uint8_t const write_retry_count = _s_write_retry_count;
            uint8_t const ro6 = 0;
            uint16_t const recovery_time_limit = htons(_s_recovery_time_limit);

            /////////////////////////////////////////////////////////////////////

            ReadWriteErrorRecovery(void)
                : ModePage(READ_WRITE_ERROR_RECOVERY, 0x0A) {}

        } __attribute__ ((packed));

        // Flexible Disk
        struct FlexibleDisk : public ModePage
        {
            // In kbits/s
            uint16_t const transfer_rate = htons(0x03E8);  // 1 Mbit/s
            uint8_t const number_of_heads = 0;
            uint8_t const sectors_per_track = 0;
            uint16_t const data_bytes_per_sector = htons(_s_block_size);
            uint16_t const number_of_cylinders = 0;
            uint16_t const starting_cylinder_write_precompensation = 0;
            uint16_t const starting_cylinder_reduced_write_current = 0;
            uint16_t const device_step_rate = 0;
            uint8_t const device_step_pulse_width = 0;
            uint16_t const head_settle_delay = 0;
            uint8_t const motor_on_delay = 0;
            uint8_t const motor_off_delay = 0;

            union
            {
                struct
                {
                    uint8_t r0   : 5;
                    uint8_t mo   : 1;
                    uint8_t ssn  : 1;
                    uint8_t trdy : 1;
                };

                uint8_t const flags0 = 0;

            } __attribute__ ((packed));

            union
            {
                struct
                {
                    uint8_t spc : 4;
                    uint8_t r1  : 4;
                };

                uint8_t const flags1 = 0;

            } __attribute__ ((packed));

            uint8_t const write_compensation = 0;
            uint8_t const head_load_delay = 0;
            uint8_t const head_unload_delay = 0;

            union
            {
                struct
                {
                    uint8_t pin2  : 4;
                    uint8_t pin32 : 4;
                };

                uint8_t const pins0 = 0;

            } __attribute__ ((packed));

            union
            {
                struct
                {
                    uint8_t pin1 : 4;
                    uint8_t pin4 : 4;
                };

                uint8_t const pins1 = 0;

            } __attribute__ ((packed));

            uint16_t const medium_rotation_rate = 0;
            uint8_t const r2 = 0;
            uint8_t const r3 = 0;

            ////////////////////////////////////////////////////////////////////

            FlexibleDisk(void) : ModePage(FLEXIBLE_DISK, 0x1E) {}

        } __attribute__ ((packed));

        // Caching
        struct Caching : public ModePage
        {
            union
            {
                struct
                {
                    uint8_t rcd  : 1;
                    uint8_t mf   : 1;
                    uint8_t wce  : 1;
                    uint8_t size : 1;
                    uint8_t disc : 1;
                    uint8_t cap  : 1;
                    uint8_t abpf : 1;
                    uint8_t ic   : 1;
                };

                uint8_t const flags0 = 0;

            } __attribute__ ((packed));

            union
            {
                struct
                {
                    uint8_t write_retention_priority       : 4;
                    uint8_t demand_read_retention_priority : 4;
                };

                uint8_t const retention_priority = 0;

            } __attribute__ ((packed));

            uint16_t const disable_pre_fetch_transfer_length = 0;
            uint16_t const minimum_pre_fetch = 0;
            uint16_t const maximum_pre_fetch = 0;
            uint16_t const maximum_pre_fetch_ceiling = 0;

            union
            {
                struct
                {
                    uint8_t nv_dis    : 1;
                    uint8_t sync_prog : 2;
                    uint8_t vs        : 2;
                    uint8_t dra       : 1;
                    uint8_t lbcss     : 1;
                    uint8_t fsw       : 1;
                };

                uint8_t const flags2 = 0;

            } __attribute__ ((packed));

            uint8_t const number_of_cache_segments = 0;
            uint16_t const cache_segment_size = 0;
            uint32_t const ro0 = 0;

            ////////////////////////////////////////////////////////////////////

            Caching(void) : ModePage(CACHING, 0x12) {}

        } __attribute__ ((packed));

        // Informational Exceptions Control
        struct InformationalExceptionsControl : public ModePage
        {
            union
            {
                // Set all of these to zero
                struct
                {
                    uint8_t log_err  : 1;  // Log Error
                    uint8_t ebackerr : 1;  // Enable Background Error
                    uint8_t test     : 1;  // Test device failure prediction
                    uint8_t d_excpt  : 1;  // Disable Exception Control
                    uint8_t e_wasc   : 1;  // Enable Warning
                    uint8_t ebf      : 1;  // Enable Background Function
                    uint8_t ro0      : 1;
                    uint8_t perf     : 1;  // Performance
                };

                uint8_t const flags0 = 0;

            } __attribute__ ((packed));

            // Method of Reporting Informational Exceptions
            enum mrie_e : uint8_t
            {
                NO_INFO_REPORTING,        // No reporting of informational exception condition
                EST_UNIT_ATTN_COND = 2,   // Establish unit attention condition
                COND_GEN_REC_ERROR,       // Conditionally generate recovered error
                UNCOND_GEN_REC_ERROR,     // Unconditionally generate recovered error
                GEN_NO_SENSE,             // Generate no sense
                REPORT_ON_REQUEST,        // Only report informational exception condition on request
            };

            union
            {
                struct
                {
                    uint8_t mrie : 4;  // Method of Reporting Informational Exceptions
                    uint8_t ro1  : 4;
                };

                // Pcap capture sets this to 5 - Generate no sense
                uint8_t const method_of_reporting = NO_INFO_REPORTING;

            } __attribute__ ((packed));

            // Specifies period in 100 ms increments that the device server shall use for
            // reporting that an informational exception condition has occurred.
            uint32_t const interval_timer = 0;

            // Maximum number of times the device server may report an informational
            // exception condition to the application client.
            uint32_t const report_count = 0;

            ////////////////////////////////////////////////////////////////////

            InformationalExceptionsControl(void)
                : ModePage(INFORMATIONAL_EXCEPTIONS_CONTROL, 0x0A) {}

        } __attribute__ ((packed));

        ModePageShortBD _mpbd{_num_blocks, _s_block_size};
        ReadWriteErrorRecovery _mp_rwer;
        FlexibleDisk _mp_fd;
        Caching _mp_caching;
        InformationalExceptionsControl _mp_iec;

        // For returning all supported mode pages
        static constexpr uint8_t const _s_num_mode_pages = 4;
        ModePage * _mode_pages[_s_num_mode_pages] =
        {
            nullptr,  // _mp_rwer
            nullptr,  // _mp_fd
            &_mp_caching,
            &_mp_iec
        };
};

// ILLEGAL REQUEST
template < uint8_t ASC, uint8_t ASCQ >
void Scsi::illegalRequest(bool in_cdb, bool info_valid, uint32_t info, uint16_t fp, bool bpv, uint8_t bp)
{
    struct fp_s  // Field Pointer Sense Key Specific Information
    {
        uint8_t bit_pointer : 3;  // Point to first bit - left-most (MSb?) bit of field
        uint8_t bpv         : 1;  // Bit Pointer Valid - set to 1 Bit Pointer field is valid
        uint8_t rsvd        : 2;
        uint8_t cd          : 1;  // Set to 1 in CDB, 0 in parameters
        uint8_t sksv        : 1;  // Must be set to 1

        uint16_t field_pointer;   // Indicates which byte is in error indexed from 0

    } __attribute__ ((packed));

    memset(_sks, 0, sizeof(_sks));

    fp_s * fptr = (fp_s *)_sks;

    fptr->field_pointer = htons(fp);
    fptr->sksv = 1;
    fptr->cd = in_cdb;
    fptr->bpv = bpv;
    fptr->bit_pointer = bpv ? bp : 0;

    info_valid ? addSense < ILLEGAL_REQUEST, ASC, ASCQ > (info)
               : addSense < ILLEGAL_REQUEST, ASC, ASCQ > ();
}

// HARDWARE ERROR, MEDIUM ERROR, RECOVERED ERROR
template < sk_e SK, uint8_t ASC, uint8_t ASCQ >
void Scsi::hmrError(uint16_t retry_count, bool info_valid, uint32_t info)
{
    static_assert((SK == MEDIUM_ERROR) || (SK == HARDWARE_ERROR) || (SK == RECOVERED_ERROR),
            "Invalid Sense Key for Medium/Hardware/Recoverd Error");

    struct arc_s  // Actual Retry Count Sense Key Specific Information
    {
        uint8_t rsvd : 7;
        uint8_t sksv : 1;  // Must be set to 1

        uint16_t actual_retry_count;  // Per Read-Write Error Recovery mode page

    } __attribute__ ((packed));

    memset(_sks, 0, sizeof(_sks));

    arc_s * arc = (arc_s *)_sks;

    arc->sksv = 1;
    arc->actual_retry_count = htons(retry_count);

    info_valid ? addSense < SK, ASC, ASCQ > (info)
               : addSense < SK, ASC, ASCQ > ();
}

// NO SENSE, NOT READY
template < sk_e SK, uint8_t ASC, uint8_t ASCQ >
void Scsi::noSense_notReady(uint16_t progress_indication)
{
    static_assert((SK == NO_SENSE) || (SK == NOT_READY),
            "Invalid Sense Key for No Sense / Not Ready");

    struct pi_s
    {
        uint8_t rsvd : 7;
        uint8_t sksv : 1;  // Must be set to 1

        uint16_t progress_indication;  // A numerator where the denomenator is 65536

    } __attribute__ ((packed));

    memset(_sks, 0, sizeof(_sks));

    pi_s * pi = (pi_s *)_sks;

    pi->sksv = 1;
    pi->progress_indication = htons(progress_indication);

    addSense < SK, ASC, ASCQ > ();
}

// COPY ABORTED
template < uint8_t ASC, uint8_t ASCQ >
void Scsi::copyAborted(bool in_sd, uint16_t fp, bool bpv, uint8_t bp)
{
    struct sp_s  // Segment Pointer Sense Key Specific Information
    {
        uint8_t bit_pointer : 3;  // Point to first bit - left-most (MSb?) bit of field
        uint8_t bpv         : 1;  // Bit Pointer Valid - set to 1 Bit Pointer field is valid
        uint8_t rsvd        : 2;
        uint8_t sd          : 1;  // Indicates whether the field pointer is relative to the
                                  // start of the parameter list (0) or to the start of a
                                  // segment descriptor (1)
        uint8_t sksv        : 1;  // Must be set to 1

        uint16_t field_pointer;   // Indicates which byte is in error indexed from 0

    } __attribute__ ((packed));

    memset(_sks, 0, sizeof(_sks));

    sp_s * sp = (sp_s *)_sks;

    sp->field_pointer = htons(sp);
    sp->sksv = 1;
    sp->sd = in_sd;
    sp->bpv = bpv;
    sp->bit_pointer = bpv ? bp : 0;

    addSense < COPY_ABORTED, ASC, ASCQ > ();
}

// UNIT ATTENTION
template < uint8_t ASC, uint8_t ASCQ >
void Scsi::unitAttention(bool overflow)
{
    struct ua_s  // Unit Attention Sense Key Specific Information
    {
        uint8_t overflow : 1;  // Unit attention condition queue has overflowed
        uint8_t rsvd     : 6;
        uint8_t sksv     : 1;  // Must be set to 1

        uint16_t rsvd2;

    } __attribute__ ((packed));

    memset(_sks, 0, sizeof(_sks));
    ua_s * ua = (ua_s *)_sks;
    ua->overflow = overflow;

    addSense < UNIT_ATTENTION, ASC, ASCQ > ();
}

template < sk_e SK, uint8_t ASC, uint8_t ASCQ >
void Scsi::addSense(bool info_valid, uint32_t info, uint32_t cs_info, uint8_t fruc)
{
    // Fixed Format sense data
    struct ff_s
    {
        uint8_t response_code : 7;  // 0x70 or 0x71
        uint8_t valid : 1;  // for information field
        uint8_t r1;

        uint8_t sense_key : 4;
        uint8_t sdat_ovfl : 1;
        uint8_t ili : 1;
        uint8_t eom : 1;
        uint8_t file_mark : 1;

        uint32_t information;

        uint8_t additional_sense_length;

        uint32_t command_specific_information;

        uint8_t asc;
        uint8_t ascq;
        uint8_t field_replaceable_unit_code;

        uint8_t sks[3];  // Sense Key Specific information

    } __attribute__ ((packed));

    memset(_dbuf, 0, sizeof(ff_s));

    ff_s * ff = (ff_s *)_dbuf;

    ff->response_code = 0x70;
    if (info_valid) ff->valid = 1;
    ff->sense_key = SK;
    ff->information = htonl(info);
    ff->additional_sense_length = 10;
    ff->command_specific_information = htonl(cs_info);
    ff->asc = ASC;
    ff->ascq = ASCQ;
    ff->field_replaceable_unit_code = fruc;
    memcpy(ff->sks, &_sks, sizeof(ff->sks));

    _doff = _transferred = 0;
    _transfer_length = sizeof(ff_s);
    _have_sense = true;
}

#endif
