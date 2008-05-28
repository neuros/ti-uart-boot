/* --------------------------------------------------------------------------
    FILE        : nand.h
    PURPOSE     : NAND header file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Jan-22-2007

    HISTORY
        v1.00 completion
 	        Daniel Allred - Jan-22-2007
 ----------------------------------------------------------------------------- */


#ifndef _NAND_H_
#define _NAND_H_

#include "ubl.h"
#include "tistdtypes.h"
#include "dm644x.h"
#include "nand.h"

// -------------- Constant define and macros --------------------

// BUS width defines
#define BUS_8BIT    0x01
#define BUS_16BIT   0x02
#define BUS_32BIT   0x04

// NAND flash addresses
#define NAND_DATA_OFFSET    (0x00)
#define NAND_ALE_OFFSET     (0x0B)
#define NAND_CLE_OFFSET     (0x10)

// NAND timeout 
#define NAND_TIMEOUT    10240

// NAND flash commands
#define NAND_LO_PAGE        0x00
#define NAND_HI_PAGE        0x01
#define NAND_LOCK           0x2A
#define NAND_UNLOCK_START   0x23
#define NAND_UNLOCK_END     0x24
#define NAND_READ_30H       0x30
#define NAND_EXTRA_PAGE     0x50
#define	NAND_RDID           0x90
#define NAND_RDIDADD        0x00
#define	NAND_RESET          0xFF
#define	NAND_PGRM_START     0x80
#define NAND_PGRM_END       0x10
#define NAND_RDY            0x40
#define	NAND_PGM_FAIL       0x01
#define	NAND_BERASEC1       0x60
#define	NAND_BERASEC2       0xD0
#define	NAND_STATUS         0x70

// Flags to indicate what is being written to NAND
#define NAND_UBL_WRITE		0
#define NAND_APP_WRITE		1

// Defines for which blocks are valid for writing UBL and APP data
#define START_UBL_BLOCK_NUM     1
#define END_UBL_BLOCK_NUM       5
#define START_APP_BLOCK_NUM     6
#define END_APP_BLOCK_NUM       50

// Status Output
#define NAND_NANDFSR_READY		(0x01)
#define NAND_STATUS_WRITEREADY 	(0xC0)
#define NAND_STATUS_ERROR	 	(0x01)
#define NAND_STATUS_BUSY		(0x40)

#define UNKNOWN_NAND		    (0xFF)			// Unknown device id
#define MAX_PAGE_SIZE	        (2112)          // Including Spare Area

// Macro gets the page size in bytes without the spare bytes 
#define NANDFLASH_PAGESIZE(x) ( ( x >> 8 ) << 8 )


// ----------------- NAND device and information structures --------------------
// NAND_DEVICE_INFO structure
typedef struct _NAND_DEV_STRUCT_ {
    Uint8   devID;              // DeviceID
    Uint16  numBlocks;          // number of blocks in device
    Uint8   pagesPerBlock;      // page count per block
    Uint16  bytesPerPage;       // byte count per page (include spare)
} NAND_DEVICE_INFO, *PNAND_DEVICE_INFO;

// NAND_INFO structure 
typedef struct _NAND_MEDIA_STRUCT_ {
    Uint32  flashBase;          // Base address of CS memory space where NAND is connected
	Uint8   busWidth;           // NAND width 1->16 bits 0->8 bits
	Uint8   devID;              // NAND_DevTable index
	Uint16  numBlocks;          // block count per device
	Uint8   pagesPerBlock;      // page count per block
	Uint16  bytesPerPage;       // Number of bytes in a page
	Uint8   numColAddrBytes;    // Number of Column address cycles
	Uint8   numRowAddrBytes;    // Number of Row address cycles
	Uint32  ECCMask;            // Mask for ECC register
	Bool    bigBlock;			// TRUE - Big block device, FALSE - small block device
	Uint8   spareBytesPerPage;  // Number of bytes in spare byte area of each page   	
	Uint8   blkShift;			// Number of bits by which block address is to be shifted
	Uint8   pageShift;			// Number of bits by which page address is to be shifted
	Uint8   CSOffset;           // 0 for CS2 space, 1 for CS3 space, 2 for CS4 space, 3 for CS5 space
} NAND_INFO, *PNAND_INFO;

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

// ---------------- Prototypes of functions for NAND flash --------------------

// Generic NAND flash functions
VUint8 *flash_make_addr (Uint32 baseAddr, Uint32 offset);
void flash_write_addr (PNAND_INFO pNandInfo, Uint32 addr);
void flash_write_cmd (PNAND_INFO pNandInfo, Uint32 cmd);
void flash_write_bytes (PNAND_INFO pNandInfo, void *pSrc, Uint32 numBytes);
void flash_write_addr_cycles(PNAND_INFO pNandInfo, Uint32 block, Uint32 page);
void flash_write_addr_bytes(PNAND_INFO pNandInfo, Uint32 numAddrBytes, Uint32 addr);
void flash_write_row_addr_bytes(PNAND_INFO pNandInfo, Uint32 block, Uint32 page);
void flash_write_data(PNAND_INFO pNandInfo, Uint32 offset, Uint32 data);
Uint32 flash_read_data (PNAND_INFO pNandInfo);
void flash_read_bytes(PNAND_INFO pNandInfo, void *pDest, Uint32 numBytes);
void flash_swap_data(PNAND_INFO pNandInfo, Uint32* data);

//Initialize the NAND registers and structures
Uint32 NAND_Init();
Uint32 NAND_GetDetails();

// Page read and write functions
Uint32 NAND_ReadPage(Uint32 block, Uint32 page, Uint8 *dest);
Uint32 NAND_WritePage(Uint32 block, Uint32 page, Uint8 *src);
Uint32 NAND_VerifyPage(Uint32 block, Uint32 page, Uint8 *src, Uint8* dest);

// Copy Application code from NAND to RAM (found in nandboot.c)
Uint32 NAND_Copy();

// Used to write NAND UBL or APP header and data to NAND
Uint32 NAND_WriteHeaderAndData(NAND_BOOT *nandBoot,Uint8 *srcBuf);

// Used to erase an entire NAND block
Uint32 NAND_EraseBlocks(Uint32 startBlkNum, Uint32 blkCount);

// Unprotect blocks encompassing specfied addresses */
Uint32 NAND_UnProtectBlocks(Uint32 startBlkNum,Uint32 endBlkNum);

// Protect blocks
void NAND_ProtectBlocks(void);

// Wait for ready signal seen at NANDFSCR
Uint32 NAND_WaitForRdy(Uint32 timeout);

// Wait for status result from device to read good */
Uint32 NAND_WaitForStatus(Uint32 timeout);

// Read ECC data and restart the ECC calculation
Uint32 NAND_ECCReadAndRestart (PNAND_INFO pNandInfo);

#endif //_NAND_H_
