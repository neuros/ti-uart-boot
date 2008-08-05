/* --------------------------------------------------------------------------
    FILE        : nandboot.c 				                             	 	        
    PURPOSE     : NAND user boot loader file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Jan-22-2007
 
    HISTORY
 	     v1.00 completion 							 						      
 	          Daniel Allred - Jan-22-2007   
 	     v1.10 
 	          DJA - Feb-1-2007 - Added support for the magic number written by the
 	                             SDI CCS flashing tool.  They always write a the 
 	                             application as a binary image and use the 
 	                             UBL_MAGIC_DMA magic number in the application
 	                             header.
 ----------------------------------------------------------------------------- */

#ifdef UBL_NAND

#include "ubl.h"
#include "nand.h"
#include "uart.h"
#include "util.h"

// Structure with info about the NAND flash device
extern NAND_INFO gNandInfo;

// Entrypoint for application we are decoding out of flash
extern Uint32 gEntryPoint;

// structure for holding details about UBL stored in NAND
volatile NAND_BOOT	gNandBoot;

// Function to find out where the application is and copy to RAM
Uint32 NAND_Copy() {
	Uint32 count,blockNum;
	Uint32 i;
	Uint32 magicNum;
	Uint8 *rxBuf;		// RAM receive buffer
	Uint32 entryPoint2,temp;
	Uint32 block,page;
	Uint32 readError = E_FAIL;
	Bool failedOnceAlready = FALSE;

    // Maximum application size, in S-record form, is 16 MB
	rxBuf = (Uint8*)ubl_alloc_mem((MAX_IMAGE_SIZE>>1));
	blockNum = START_APP_BLOCK_NUM;

	UARTSendData((Uint8 *)"Starting NAND Copy...\r\n", FALSE);
	
	// NAND Initialization
	if (NAND_Init() != E_PASS)
		return E_FAIL;
    
NAND_startAgain:
	if (blockNum > END_APP_BLOCK_NUM)
		return E_FAIL;  /* NAND boot failed and return fail to main */

	// Read data about Application starting at START_APP_BLOCK_NUM, Page 0
	// and possibly going until block END_APP_BLOCK_NUM, Page 0
	for(count=blockNum; count <= END_APP_BLOCK_NUM; count++)
	{		
		if(NAND_ReadPage(count,0,rxBuf) != E_PASS)
			continue;

		magicNum = ((Uint32 *)rxBuf)[0];

		/* Valid magic number found */
		if((magicNum & 0xFFFFFF00) == MAGIC_NUMBER_VALID)
		{
			UARTSendData((Uint8 *) "Valid MagicNum found.\r\n", FALSE);
			blockNum = count;
			break;
		}

	}

	// Never found valid header in any page 0 of any of searched blocks
	if (count > END_APP_BLOCK_NUM)
	{
		return E_FAIL;
	}

	// Fill in NandBoot header
	gNandBoot.entryPoint = *(((Uint32 *)(&rxBuf[4])));/* The first "long" is entry point for Application */
	gNandBoot.numPage = *(((Uint32 *)(&rxBuf[8])));	 /* The second "long" is the number of pages */
	gNandBoot.block = *(((Uint32 *)(&rxBuf[12])));	 /* The third "long" is the block where Application is stored in NAND */
	gNandBoot.page = *(((Uint32 *)(&rxBuf[16])));	 /* The fourth "long" is the page number where Application is stored in NAND */
	gNandBoot.ldAddress = *(((Uint32 *)(&rxBuf[20])));	 /* The fifth "long" is the Application load address */

	// If the application is already in binary format, then our 
	// received buffer can point to the specified load address
	// instead of the temp location used for storing an S-record
	// Checking for the UBL_MAGIC_DMA guarantees correct usage with the 
	// Spectrum Digital CCS flashing tool, flashwriter_nand.out
	if ((magicNum == UBL_MAGIC_BIN_IMG) || (magicNum == UBL_MAGIC_DMA))
	{
	    // Set the copy location to final run location
		rxBuf = (Uint8 *)gNandBoot.ldAddress;
		// Free temp memory rxBuf used to point to
		set_current_mem_loc(get_current_mem_loc() - (MAX_IMAGE_SIZE>>1));
	}

NAND_retry:
	/* initialize block and page number to be used for read */
	block = gNandBoot.block;
	page = gNandBoot.page;

    // Perform the actual copying of the application from NAND to RAM
	for(i=0;i<gNandBoot.numPage;i++) {
	    // if page goes beyond max number of pages increment block number and reset page number
		if(page >= gNandInfo.pagesPerBlock) {
			page = 0;
			block++;
		}
NAND_retry_read:
		readError = NAND_ReadPage(block,page++,(&rxBuf[i*gNandInfo.bytesPerPage]));	/* Copy the data */

		// We attempt to read the page data twice.  If we fail twice then we go look for next block
		if(readError != E_PASS) {		
			page--;
			if(failedOnceAlready) {	
				block++;
				failedOnceAlready = FALSE;
			}
			else {
				failedOnceAlready = TRUE;
			}
			goto NAND_retry_read;
		}
	}

	// Application was read correctly, so set entrypoint
	gEntryPoint = gNandBoot.entryPoint;

	/* Data is already copied to RAM, just set the entry point */
	if(magicNum == UBL_MAGIC_SAFE)
	{
		// Or do the decode of the S-record 
		if(SRecDecode( (Uint8 *)rxBuf, 
		               gNandBoot.numPage * gNandInfo.bytesPerPage,
					   (Uint32 *) &entryPoint2,
		               (Uint32 *) &temp ) != E_PASS)
		{
		    UARTSendData("S-record decode failure.", FALSE);
			return E_FAIL;
		}
		
		if (gEntryPoint != entryPoint2)
		{
			UARTSendData("WARNING: S-record entrypoint does not match header entrypoint.\r\n", FALSE);
			UARTSendData("WARNING: Using header entrypoint - results may be unexpected.\r\n", FALSE);
		}
	}
	
	return E_PASS;
}

#endif
