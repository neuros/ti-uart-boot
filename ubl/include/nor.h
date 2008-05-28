/* --------------------------------------------------------------------------
    FILE        : nor.h
    PURPOSE     : NOR driver header file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Jan-22-2007

    HISTORY
 	    v1.00 completion
 	        Daniel Allred - Jan-22-2007
 ----------------------------------------------------------------------------- */

 
#ifndef _NOR_H_
#define _NOR_H_

#include "tistdtypes.h"
#include "dm644x.h"

/************* Constants and Macros **************/
#define BUS_8BIT    0x01
#define BUS_16BIT   0x02
#define BUS_32BIT   0x04

/**************** DEFINES for AMD Basic Command Set **************/
#define AMD_CMD0                    0xAA        // AMD CMD PREFIX 0
#define AMD_CMD1                    0x55        // AMD CMD PREFIX 1
#define AMD_CMD0_ADDR               0x555       // AMD CMD0 Offset 
#define AMD_CMD1_ADDR               0x2AA       // AMD CMD1 Offset 
#define AMD_CMD2_ADDR       	    0x555       // AMD CMD2 Offset 
#define AMD_ID_CMD                  0x90        // AMD ID CMD
#define AMD_MANFID_ADDR             0x00        // Manufacturer ID offset
#define AMD_DEVID_ADDR0             0x01        // First device ID offset
#define AMD_DEVID_ADDR1             0x0E        // Offset for 2nd byte of 3 byte ID 
#define AMD_DEVID_ADDR2             0x0F        // Offset for 3rd byte of 3 byte ID 
#define AMD_ID_MULTI                0x7E        // First-byte ID value for 3-byte ID
#define AMD_RESET                   0xF0        // AMD Device Reset Command
#define AMD_BLK_ERASE_SETUP_CMD     0x80        // Block erase setup
#define AMD_BLK_ERASE_CMD	        0x30        // Block erase confirm
#define AMD_BLK_ERASE_DONE	        0xFFFF      // Block erase check value
#define AMD_PROG_CMD                0xA0        // AMD simple Write command
#define AMD_WRT_BUF_LOAD_CMD        0x25        // AMD write buffer load command
#define AMD_WRT_BUF_CONF_CMD        0x29        // AMD write buffer confirm command

/**************** DEFINES for Intel Basic Command Set **************/
#define INTEL_ID_CMD            0x90        // Intel ID CMD
#define INTEL_MANFID_ADDR       0x00        // Manufacturer ID offset
#define INTEL_DEVID_ADDR        0x01        // Device ID offset
#define INTEL_RESET             0xFF        // Intel Device Reset Command
#define INTEL_ERASE_CMD0        0x20        // Intel Erase command
#define INTEL_ERASE_CMD1        0xD0        // Intel Erase command
#define INTEL_WRITE_CMD         0x40        // Intel simple write command
#define INTEL_WRT_BUF_LOAD_CMD  0xE8        // Intel write buffer load command
#define INTEL_WRT_BUF_CONF_CMD  0xD0        // Intel write buffer confirm command
#define INTEL_LOCK_CMD0         0x60        // Intel lock mode command
#define INTEL_LOCK_BLOCK_CMD    0x01        // Intel lock command
#define INTEL_UNLOCK_BLOCK_CMD  0xD0        // Intel unlock command
#define INTEL_CLEARSTATUS_CMD   0x50        // Intel clear status command


/**************** DEFINES for CFI Commands and Table **************/

// CFI Entry and Exit commands
#define CFI_QRY_CMD             0x98U
#define CFI_EXIT_CMD            0xF0U

// CFI address locations
#define CFI_QRY_CMD_ADDR        0x55U

// CFI Table Offsets in Bytes
#define CFI_Q                   0x10
#define CFI_R                   0x11
#define CFI_Y                   0x12
#define CFI_CMDSET              0x13
#define CFI_CMDSETADDR          0x15
#define CFI_ALTCMDSET           0x17
#define CFI_ALTCMDSETADDR       0x19
#define CFI_MINVCC              0x1B
#define CFI_MAXVCC              0x1C
#define CFI_MINVPP              0x1D
#define CFI_MAXVPP              0x1E
#define CFI_TYPBYTEPGMTIME      0x1F
#define CFI_TYPBUFFERPGMTIME    0x20
#define CFI_TYPBLOCKERASETIME   0x21
#define CFI_TYPCHIPERASETIME    0x22
#define CFI_MAXBYTEPGMTIME      0x23
#define CFI_MAXBUFFERPGMTIME    0x24
#define CFI_MAXBLOCKERASETIME   0x25
#define CFI_MAXCHIPERASETIME    0x26
#define CFI_DEVICESIZE          0x27
#define CFI_INTERFACE           0x28
#define CFI_WRITESIZE           0x2A
#define CFI_NUMBLKREGIONS       0x2C
#define CFI_BLKREGIONS          0x2D
#define CFI_BLKREGIONSIZE       0x04

// Maximum number of block regions supported
#define CFI_MAXREGIONS          0x06

/*********************** Enumerated types *************************/
// Supported Flash Manufacturers
enum FlashManufacturerID {
    UNKNOWN_ID = 0x00,
    AMD = 0x01,
    FUJITSU = 0x04,
    INTEL = 0x89,
    MICRON = 0x2C,
    SAMSUNG = 0xEC,
    SHARP = 0xB0
};
typedef enum FlashManufacturerID MANFID;

// Supported CFI command sets
enum FlashCommandSet {
    UNKNOWN_CMDSET = 0x0000,
    INTEL_EXT_CMDSET = 0x0001,
    AMD_BASIC_CMDSET = 0x0002,
    INTEL_BASIC_CMDSET = 0x0003,
    AMD_EXT_CMDSET = 0x0004,
    MITSU_BASIC_CMDSET = 0x0100,
    MITSU_EXT_CMDSET = 0x0101
    
};
typedef enum FlashCommandSet CMDSET;

/*************************** Structs *********************************/
// Struct to hold discovered flash parameters
typedef struct _NOR_MEDIA_STRUCT_ {
   Uint32       flashBase;                          // 32-bit address of flash start
   Uint8        busWidth;                           // 8-bit or 16-bit bus width
   Uint8        chipOperatingWidth;                 // The operating width of each chip
   Uint8        maxTotalWidth;                      // Maximum extent of width of all chips combined - determines offset shifts
   Uint32       flashSize;                          // Size of NOR flash regions in bytes (numberDevices * size of one device)
   Uint32       bufferSize;                         // Size of write buffer
   CMDSET       commandSet;                         // command set id (see CFI documentation)
   Uint8        numberDevices;                      // Number of deives used in parallel
   Uint8        numberRegions;                      // Number of regions of contiguous regions of same block size
   Uint32       numberBlocks[CFI_MAXREGIONS];    // Number of blocks in a region
   Uint32       blockSize[CFI_MAXREGIONS];       // Size of the blocks in a region
   MANFID       manfID;                             // Manufacturer's ID
   Uint16       devID1;                             // Device ID
   Uint16       devID2;                             // Used for AMD 3-byte ID devices
} NOR_INFO, *PNOR_INFO;

typedef union {
	Uint8 c;
	Uint16 w;
	Uint32 l;
} FLASHData;

typedef union {
	VUint8 *cp;
	VUint16 *wp;
	VUint32 *lp;
} FLASHPtr;


/*************************** Function Prototypes *************************/

// Global NOR commands
Uint32 NOR_Init ();
Uint32 NOR_Copy(void);
Uint32 NOR_WriteBytes(Uint32 writeAddress, Uint32 numBytes, Uint32 readAddress);
Uint32 NOR_GlobalErase();
Uint32 NOR_Erase(Uint32 start_address, Uint32 size);
Uint32 DiscoverBlockInfo(Uint32 address,Uint32* blockSize, Uint32* blockAddr);

// Flash Identification  and Discovery
Uint32 QueryCFI( Uint32 baseAddress );

// Generic functions to access flash data and CFI tables
VUint8 *flash_make_addr (Uint32 blkAddr, Uint32 offset);
void flash_make_cmd (Uint8 cmd, void *cmdbuf);
void flash_write_cmd (Uint32 blkAddr, Uint32 offset, Uint8 cmd);
void flash_write_data(Uint32 address, Uint32 data);
void flash_write_databuffer(Uint32* address, void* data, Uint32 numBytes);
Uint32 flash_verify_databuffer(Uint32 address, void* data, Uint32 numBytes);
Uint32 flash_read_data(Uint32 address, Uint32 offset);
FLASHData flash_read_CFI_bytes (Uint32 blkAddr, Uint32 offset, Uint8 numBytes);
Bool flash_isequal (Uint32 blkAddr, Uint32 offset, Uint8 val);
Bool flash_issetall (Uint32 blkAddr, Uint32 offset, Uint8 mask);
Bool flash_issetsome (Uint32 blkAddr, Uint32 offset, Uint8 mask);

// Generic commands that will point to either AMD or Intel command set
Uint32 (* Flash_Write)(Uint32, VUint32);
Uint32 (* Flash_BufferWrite)( Uint32, VUint8[], Uint32);
Uint32 (* Flash_Erase)(Uint32);
Uint32 (* Flash_ID)(Uint32);

// Empty commands for when neither command set is used
Uint32 Unsupported_Erase( Uint32 );
Uint32 Unsupported_Write( Uint32, VUint32 );
Uint32 Unsupported_BufferWrite( Uint32, VUint8[], Uint32 );
Uint32 Unsupported_ID( Uint32 );

//Intel pointer-mapped commands
Uint32 Intel_Erase( VUint32 blkAddr);
Uint32 Intel_Write( Uint32 address, VUint32 data );
Uint32 Intel_BufferWrite( Uint32 address, VUint8 data[], Uint32 numBytes );
Uint32 Intel_ID( Uint32 );

//AMD pointer-mapped commands
Uint32 AMD_Erase(Uint32 blkAddr);
Uint32 AMD_Write( Uint32 address, VUint32 data );
Uint32 AMD_BufferWrite(Uint32 address, VUint8 data[], Uint32 numBytes );
Uint32 AMD_ID( Uint32 );

// Misc. Intel commands
Uint32 Intel_Clear_Lock(VUint32 blkAddr);
Uint32 Intel_Set_Lock(VUint32 blkAddr);
Uint32 Intel_Lock_Status_Check();
void Intel_Soft_Reset_Flash();
void Intel_Clear_Status();
void Intel_Wait_For_Status_Complete();

// Misc. AMD commands
void AMD_Soft_Reset_Flash();
void AMD_Prefix_Commands();
void AMD_Write_Buf_Abort_Reset_Flash();

#endif //_NOR_H_
