/* --------------------------------------------------------------------------
    FILE        : norboot.c 				                             	 	        
    PURPOSE     : NOR user boot loader file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Jan-22-2007  
 
    HISTORY
 	     v1.00 completion 							 						      
 	          Daniel Allred - Jan-22-2007                                              
 ----------------------------------------------------------------------------- */

#ifdef UBL_NOR

#include "ubl.h"
#include "nor.h"
#include "util.h"
#include "uart.h"

extern Uint32 gEntryPoint;
extern NOR_INFO gNorInfo;

/* Function to find out where the Application is and copy to DRAM */
Uint32 NOR_Copy() {
	volatile NOR_BOOT	*hdr = 0;
	VUint32		*appStartAddr = 0;
	VUint32		count = 0;
	VUint32		*ramPtr = 0;
	Uint32      blkSize, blkAddress;

	UARTSendData((Uint8 *) "Starting NOR Copy...\r\n", FALSE);
	
	// Nor Initialization
	if (NOR_Init() != E_PASS)
	    return E_FAIL;
	    
	DiscoverBlockInfo( (gNorInfo.flashBase + UBL_IMAGE_SIZE), &blkSize, &blkAddress );
	
	hdr = (volatile NOR_BOOT *) (blkAddress + blkSize);

	/* Magic number found */
	if((hdr->magicNum & 0xFFFFFF00) != MAGIC_NUMBER_VALID)
	{
	 	return E_FAIL;/* Magic number not found */
	}

	/* Set the Start Address */
	appStartAddr = (Uint32 *)(((Uint8*)hdr) + sizeof(NOR_BOOT));

	if(hdr->magicNum == UBL_MAGIC_BIN_IMG)
	{
		ramPtr = (Uint32 *) hdr->ldAddress;

		/* Copy data to RAM */
		for(count = 0; count < ((hdr->appSize + 3)/4); count ++)
		{
			ramPtr[count] = appStartAddr[count];
		}
		gEntryPoint = hdr->entryPoint;
		/* Since our entry point is set, just return success */
		return E_PASS;
	}

	if(SRecDecode((Uint8 *)appStartAddr, hdr->appSize, (Uint32 *)&gEntryPoint, (Uint32 *)&count ) != E_PASS)
	{
		return E_FAIL;
	}
 	return E_PASS;
}

#endif
