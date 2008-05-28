/* --------------------------------------------------------------------------
    FILE        : nand.c 				                             	 	        
    PURPOSE     : NAND driver file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Jan-22-2007
 
    HISTORY
 	     v1.0 completion 							 						      
 	          Daniel Allred - Jan-22-2007
	     v1.1 modify for neuros
	          Terry Qiu tqiu@neuros.com.cn -May-28-2008
 ----------------------------------------------------------------------------- */

#ifdef UBL_NAND

#include "ubl.h"
#include "dm644x.h"
#include "uart.h"
#include "nand.h"

static Uint8 gNandTx[MAX_PAGE_SIZE] __attribute__((section(".ddrram2")));
static Uint8 gNandRx[MAX_PAGE_SIZE] __attribute__((section(".ddrram2")));

// Symbol from linker script
extern Uint32 __NANDFlash;

// structure for holding details about the NAND device itself
volatile NAND_INFO gNandInfo;

// Table of ROM supported NAND devices
const NAND_DEVICE_INFO gNandDevInfo[] = 
{ // devID, numBlocks,  pagesPerBlock,  bytesPerPage
    {0x6E,  256,        16,             256+8},     /* 1 MB */
    {0x68,  256,        16,             256+8},     /* 1 MB */
    {0xEC,  256,        16,             256+8},     /* 1 MB */
    {0xE8,  256,        16,             256+8},     /* 1 MB */
    {0xEA,  512,        16,             256+8},     /* 2 MB */
    {0xE3,  512,        16,             512+16},	/* 4 MB */
    {0xE5,  512,        16,             512+16},	/* 4 MB */
    {0xE6,  1024,	    16,             512+16},	/* 8 MB */
    {0x39,  1024,   	16,             512+16},	/* 8 MB */
    {0x6B,  1024,       16,             512+16},	/* 8 MB */
    {0x73,  1024,       32,             512+16},	/* 16 MB */
    {0x33,  1024,       32,             512+16},	/* 16 MB */
    {0x75,  2048,       32,             512+16},	/* 32 MB */
    {0x35,  2048,       32,             512+16},	/* 32 MB */
    {0x43,  1024,       32,             512+16},    /* 16 MB 0x1243 */
    {0x45,  2048,       32,             512+16},    /* 32 MB 0x1245 */
    {0x53,  1024,       32,             512+16},    /* 16 MB 0x1253 */
    {0x55,  2048,       32,             512+16},    /* 32 MB 0x1255 */
    {0x36,  4096,       32,             512+16},	/* 64 MB */
    {0x46,  4096,       32,             512+16},    /* 64 MB 0x1346 */
    {0x56,  4096,       32,             512+16},    /* 64 MB 0x1356 */
    {0x76,  4096,       32,             512+16},	/* 64 MB */
    {0x74,  8192,       32,             512+16},    /* 128 MB 0x1374 */
    {0x79,  8192,       32,             512+16},	/* 128 MB */
    {0x71,  16384,      32,             512+16},	/* 256 MB */
    {0xF1,  1024,       64,             2048+64},   /* 128 MB - Big Block */
    {0xA1,  1024,       64,             2048+64},	/* 128 MB - Big Block */
    {0xAA,  2048,       64,             2048+64},	/* 256 MB - Big Block */
    {0xDA,  2048,       64,             2048+64},	/* 256 MB - Big Block */
    {0xDC,  4096,       64,             2048+64},	/* 512 MB - Big Block */
    {0xAC,  4096,       64,             2048+64},   /* 512 MB - Big Block */
    {0xB1,  1024,       64,             2048+64},   /* 128 MB - Big Block 0x22B1 */
    {0xC1,  1024,       64,             2048+64},   /* 128 MB - Big Block 0x22C1 */
    {0x00,	0,          0,              0}	        /* Dummy null entry to indicate end of table*/
};

VUint8 *flash_make_addr (Uint32 baseAddr, Uint32 offset)
{
	return ((VUint8 *) ( baseAddr + offset ));
}

void flash_write_data(PNAND_INFO pNandInfo, Uint32 offset, Uint32 data)
{
	volatile FLASHPtr addr;
	FLASHData dataword;
	dataword.l = data;

	addr.cp = flash_make_addr (pNandInfo->flashBase, offset);
	switch (pNandInfo->busWidth)
	{
	    case BUS_8BIT:
            *addr.cp = dataword.c;
            break;
        case BUS_16BIT:
            *addr.wp = dataword.w;
            break;
	}
}

void flash_write_cmd (PNAND_INFO pNandInfo, Uint32 cmd)
{
	flash_write_data(pNandInfo, NAND_CLE_OFFSET, cmd);
}

void flash_write_addr (PNAND_INFO pNandInfo, Uint32 addr)
{
	flash_write_data(pNandInfo, NAND_ALE_OFFSET, addr);
}

void flash_write_bytes(PNAND_INFO pNandInfo, void* pSrc, Uint32 numBytes)
{
    volatile FLASHPtr destAddr, srcAddr;
	Uint32 i;
	
	srcAddr.cp = (VUint8*) pSrc;
	destAddr.cp = flash_make_addr (pNandInfo->flashBase, NAND_DATA_OFFSET );
	switch (pNandInfo->busWidth)
	{
    case BUS_8BIT:
        for(i=0;i<( numBytes );i++)
	        *destAddr.cp = *srcAddr.cp++;
        break;
    case BUS_16BIT:
        for(i=0;i<( numBytes >> 1);i++)
	        *destAddr.wp = *srcAddr.wp++;
        break;
    }

}

void flash_write_addr_cycles(PNAND_INFO pNandInfo, Uint32 block, Uint32 page)
{
    flash_write_addr_bytes(pNandInfo, pNandInfo->numColAddrBytes, 0x00000000);
    flash_write_row_addr_bytes(pNandInfo, block, page);
}

void flash_write_addr_bytes(PNAND_INFO pNandInfo, Uint32 numAddrBytes, Uint32 addr)
{    
    Uint32 i;
    for (i=0; i<numAddrBytes; i++)
    {
        flash_write_addr(pNandInfo, ( (addr >> (8*i) ) & 0xff) );
	}
}

void flash_write_row_addr_bytes(PNAND_INFO pNandInfo, Uint32 block, Uint32 page)
{
    Uint32 row_addr;
	row_addr = (block << (pNandInfo->blkShift - pNandInfo->pageShift)) | page;
	flash_write_addr_bytes(pNandInfo, pNandInfo->numRowAddrBytes, row_addr);
}

Uint32 flash_read_data (PNAND_INFO pNandInfo)
{
	volatile FLASHPtr addr;
	FLASHData cmdword;
	cmdword.l = 0x0;

	addr.cp = flash_make_addr (pNandInfo->flashBase, NAND_DATA_OFFSET );
	switch (gNandInfo.busWidth)
	{
	    case BUS_8BIT:
            cmdword.c = *addr.cp;
            break;
        case BUS_16BIT:
            cmdword.w = *addr.wp;
            break;
	}
	return cmdword.l;
}

void flash_read_bytes(PNAND_INFO pNandInfo, void* pDest, Uint32 numBytes)
{
    volatile FLASHPtr destAddr, srcAddr;
	Uint32 i;
	
	destAddr.cp = (VUint8*) pDest;
	srcAddr.cp = flash_make_addr (pNandInfo->flashBase, NAND_DATA_OFFSET );
	switch (pNandInfo->busWidth)
	{
    case BUS_8BIT:
        for(i=0;i<( numBytes );i++)
	        *destAddr.cp++ = *srcAddr.cp;
        break;
    case BUS_16BIT:
        for(i=0;i<( numBytes >> 1);i++)
	        *destAddr.wp++ = *srcAddr.wp;
        break;
    }
}

void flash_swap_data(PNAND_INFO pNandInfo, Uint32* data)
{
    Uint32 i,temp = *data;
    volatile FLASHPtr  dataAddr, tempAddr;
    
    dataAddr.cp = flash_make_addr((Uint32) data, 3);
    tempAddr.cp = flash_make_addr((Uint32) &temp,0);
        
    switch (gNandInfo.busWidth)
	{
    case BUS_8BIT:
        for(i=0; i<4; i++)
	        *dataAddr.cp-- = *tempAddr.cp++;
        break;
    case BUS_16BIT:
        for(i=0; i<2; i++)
	        *dataAddr.wp-- = *tempAddr.wp++;
        break;
    }
}

// Poll bit of NANDFSR to indicate ready
Uint32 NAND_WaitForRdy(Uint32 timeout) {
	VUint32 cnt;
	cnt = timeout;

	waitloop(200);

	while( !(AEMIF->NANDFSR & NAND_NANDFSR_READY) && ((cnt--) > 0) )

    if(cnt == 0)
	{
		UARTSendData((Uint8 *)"NANDWaitForRdy() Timeout!\n", FALSE);
		return E_FAIL;
	}

    return E_PASS;
}


// Wait for the status to be ready in NAND register
//      There were some problems reported in DM320 with Ready/Busy pin
//      not working with all NANDs. So this check has also been added.
Uint32 NAND_WaitForStatus(Uint32 timeout) {
	VUint32 cnt;
	Uint32 status;
	cnt = timeout;

    do
    {
	    flash_write_cmd((PNAND_INFO)&gNandInfo,NAND_STATUS);
	    status = flash_read_data((PNAND_INFO)&gNandInfo) & (NAND_STATUS_ERROR | NAND_STATUS_BUSY);
        cnt--;
  	}
  	while((cnt>0) && !status);

	if(cnt == 0)
	{
		UARTSendData((Uint8 *)"NANDWaitForStatus() Timeout!\n", FALSE);
		return E_FAIL;
	}

	return E_PASS;
}

// Read the current ECC calculation and restart process
Uint32 NAND_ECCReadAndRestart (PNAND_INFO pNandInfo)
{
    Uint32 retval;
    // Read and mask appropriate (based on CSn space flash is in) ECC regsiter
    retval = ((Uint32*)(&(AEMIF->NANDF1ECC)))[pNandInfo->CSOffset] & pNandInfo->ECCMask;
    // Write appropriate bit to start ECC calcualtions 
    AEMIF->NANDFCR |= (1<<(8 + (pNandInfo->CSOffset)));   
    return retval;
}

// Initialze NAND interface and find the details of the NAND used
Uint32 NAND_Init()
{
    Uint32 width, *CSRegs;
	UARTSendData((Uint8 *) "Initializing NAND flash...\r\n", FALSE);
	
    // Set NAND flash base address
    gNandInfo.flashBase = (Uint32) &(__NANDFlash);
    
    //Get the CSOffset (can be 0 through 3 - corresponds with CS2 through CS5)
    gNandInfo.CSOffset = (gNandInfo.flashBase >> 25) - 1;
    
    // Setting the nand_width = 0(8 bit NAND) or 1(16 bit NAND). AEMIF CS2 bus Width
	//   is given by the BOOTCFG(bit no.5)
    width = ( ( (SYSTEM->BOOTCFG) & 0x20) >> 5);
    gNandInfo.busWidth = (width)?BUS_16BIT:BUS_8BIT;

    // Setup AEMIF registers for NAND    
    CSRegs = (Uint32*) &(AEMIF->AB1CR);
    CSRegs[gNandInfo.CSOffset] = 0x3FFFFFFC | width;        // Set correct ABxCR reg
    AEMIF->NANDFCR |= (0x1 << (gNandInfo.CSOffset));        // NAND enable for CSx
    NAND_ECCReadAndRestart((PNAND_INFO)&gNandInfo); 
                         
	// Send reset command to NAND
	flash_write_cmd( (PNAND_INFO)&gNandInfo, NAND_RESET );

	if ( NAND_WaitForRdy(NAND_TIMEOUT) != E_PASS )
        return E_FAIL;
		
	return NAND_GetDetails();
}

// Get details of the NAND flash used from the id and the table of NAND devices
Uint32 NAND_GetDetails()
{
	Uint32 deviceID,i,j;
	
	// Issue device read ID command
    flash_write_cmd( (PNAND_INFO)&gNandInfo, NAND_RDID);
    flash_write_addr( (PNAND_INFO)&gNandInfo, NAND_RDIDADD);
    
	// Read ID bytes
	j        = flash_read_data( (PNAND_INFO)&gNandInfo ) & 0xFF;
	deviceID = flash_read_data( (PNAND_INFO)&gNandInfo ) & 0xFF;
	j        = flash_read_data( (PNAND_INFO)&gNandInfo ) & 0xFF;
	j        = flash_read_data( (PNAND_INFO)&gNandInfo ) & 0xFF;

	i=0;
	while (gNandDevInfo[i].devID != 0x00)
	{
		if(deviceID == gNandDevInfo[i].devID)
		{
			gNandInfo.devID             = (Uint8) gNandDevInfo[i].devID;
			gNandInfo.pagesPerBlock     = gNandDevInfo[i].pagesPerBlock;
			gNandInfo.numBlocks         = gNandDevInfo[i].numBlocks;
			gNandInfo.bytesPerPage      = NANDFLASH_PAGESIZE(gNandDevInfo[i].bytesPerPage);
			gNandInfo.spareBytesPerPage = gNandDevInfo[i].bytesPerPage - gNandInfo.bytesPerPage;
					
			// Assign the big_block flag
			gNandInfo.bigBlock = (gNandInfo.bytesPerPage == 2048)?TRUE:FALSE;
			
			// Setup address shift values	
			j = 0;
			while( (gNandInfo.pagesPerBlock >> j) > 1)
			{
				j++;
			}
			gNandInfo.blkShift = j;        
			gNandInfo.pageShift = (gNandInfo.bigBlock)?16:8;
			gNandInfo.blkShift += gNandInfo.pageShift;
			
			// Set number of column address bytes needed
			gNandInfo.numColAddrBytes = gNandInfo.pageShift >> 3;
			
			j = 0;
			while( (gNandInfo.numBlocks >> j) > 1)
			{
				j++;
			}
						
			// Set number of row address bytes needed
			if ( (gNandInfo.blkShift + j) <= 24 )
			{
			    gNandInfo.numRowAddrBytes = 3 - gNandInfo.numColAddrBytes;
			}
			else if ((gNandInfo.blkShift + j) <= 32)
			{
			    gNandInfo.numRowAddrBytes = 4 - gNandInfo.numColAddrBytes;
			}
			else
			{
			    gNandInfo.numRowAddrBytes = 5 - gNandInfo.numColAddrBytes;
			}
			
			// Set the ECC bit mask
			if (gNandInfo.bytesPerPage < 512)
			    gNandInfo.ECCMask = 0x07FF07FF;
			else
			    gNandInfo.ECCMask = 0x0FFF0FFF;
			    		
			return E_PASS;
		}
		i++;
	}
	// No match was found for the device ID
	return E_FAIL;
}

// Routine to read a page from NAND
Uint32 NAND_ReadPage(Uint32 block, Uint32 page, Uint8 *dest) {
	Uint32 eccValue[4];
	Uint32 spareValue[4],tempSpareValue;
	Uint8 numReads,i;
	
	//Setup numReads
	numReads = (gNandInfo.bytesPerPage >> 9);
	if (numReads == 0) numReads++;

    // Write read command
    flash_write_cmd((PNAND_INFO)&gNandInfo,NAND_LO_PAGE);
	
	// Write address bytes
	flash_write_addr_cycles((PNAND_INFO)&gNandInfo, block, page);

    // Additional confirm command for big_block devices
	if(gNandInfo.bigBlock)	
		flash_write_cmd((PNAND_INFO)&gNandInfo, NAND_READ_30H);

	// Wait for data to be available
	if(NAND_WaitForRdy(NAND_TIMEOUT) != E_PASS)
		return E_FAIL;

	// Starting the ECC in the NANDFCR register for CS2(bit no.8)
	NAND_ECCReadAndRestart((PNAND_INFO)&gNandInfo);

    // Read the page data
    for (i=0; i<numReads; i++)
    {
        // Actually read bytes
	    flash_read_bytes((PNAND_INFO)&gNandInfo, (void*)(dest), gNandInfo.bytesPerPage);	    
	    // Get the ECC Value
	    eccValue[i] = NAND_ECCReadAndRestart((PNAND_INFO)&gNandInfo);
	    //Increment pointer
	    dest += gNandInfo.bytesPerPage;
	}
		
	// Read the stored ECC value(s)
	for (i=0; i<numReads; i++)
	{
	    if (gNandInfo.bytesPerPage == 256)
            flash_read_bytes((PNAND_INFO)&gNandInfo, (void*)(spareValue), 8);
        else
            flash_read_bytes((PNAND_INFO)&gNandInfo, (void*)(spareValue),16);

        if (gNandInfo.bigBlock)
        {
            flash_swap_data((PNAND_INFO)&gNandInfo, (Uint32*)(spareValue+2));
            tempSpareValue = spareValue[2];
        }
        else
        {
            flash_swap_data((PNAND_INFO)&gNandInfo, (Uint32*)(spareValue));
            tempSpareValue = spareValue[0];
        }
	if(tempSpareValue = 0xffffffff)	tempSpareValue = ~tempSpareValue;
	if(eccValue[i] = 0xffffffff) eccValue[i] = ~eccValue[i];
	    // Verify ECC values
		if(eccValue[i] != tempSpareValue)
        {
            UARTSendData((Uint8 *)"NAND ECC failure!\r\n", FALSE);
            return E_FAIL;
        }
    }
    
    // Read remainder of spare bytes
	//flash_read_bytes( (PNAND_INFO)&gNandInfo, (void*)(dest), (gNandInfo.spareBytesPerPage - (4*numReads)) );

    // Return status check result
	return NAND_WaitForStatus(NAND_TIMEOUT);
	
}

// Generic routine to write a page of data to NAND
Uint32 NAND_WritePage(Uint32 block, Uint32 page, Uint8 *src) {
	Uint32 eccValue[4];
	Uint32 tempSpareValue[4];
	Uint8 numWrites,i;
	
	//Setup numReads
	numWrites = (gNandInfo.bytesPerPage >> 9);
	if (numWrites == 0) numWrites++;

	// Write program command
	flash_write_cmd((PNAND_INFO)&gNandInfo, NAND_PGRM_START);
	
	// Write address bytes
	flash_write_addr_cycles((PNAND_INFO)&gNandInfo,block,page);
	
	// Starting the ECC in the NANDFCR register for CS2(bit no.8)
	NAND_ECCReadAndRestart((PNAND_INFO)&gNandInfo);
	
	// Write data
	for (i=0; i<numWrites; i++)
	{
	    // Write data to page
	    flash_write_bytes((PNAND_INFO)&gNandInfo, (void*) src, gNandInfo.bytesPerPage);
	    
	    // Read the ECC value
	    eccValue[i] = NAND_ECCReadAndRestart((PNAND_INFO)&gNandInfo);
	    
	    // Increment the pointer
	    src += gNandInfo.bytesPerPage;
	}
	
	// Write spare bytes
	for (i=0; i<numWrites; i++)
	{
        flash_swap_data((PNAND_INFO)&gNandInfo, &(eccValue[i]));
        
        // Place the ECC values where the ROM read routine expects them
        if (gNandInfo.bigBlock)
        {
            tempSpareValue[0] = 0xFFFFFFFF;
            tempSpareValue[1] = 0xFFFFFFFF;
            tempSpareValue[2] = eccValue[i];
            tempSpareValue[3] = 0xFFFFFFFF;
        }
        else
        {
            tempSpareValue[0] = eccValue[i];
            tempSpareValue[1] = 0xFFFFFFFF;
            tempSpareValue[2] = 0xFFFFFFFF;
            tempSpareValue[3] = 0xFFFFFFFF;
        }
        if (gNandInfo.bytesPerPage == 256)
            flash_write_bytes((PNAND_INFO)&gNandInfo, (void*)(tempSpareValue), 8);
        else
            flash_write_bytes((PNAND_INFO)&gNandInfo, (void*)(tempSpareValue),16);
    }
    			
    // Write program end command
    flash_write_cmd((PNAND_INFO)&gNandInfo, NAND_PGRM_END);
	
	// Wait for the device to be ready
	if (NAND_WaitForRdy(NAND_TIMEOUT) != E_PASS)
		return E_FAIL;

    // Return status check result	
	return NAND_WaitForStatus(NAND_TIMEOUT);
}

// Verify data written by reading and comparing byte for byte
Uint32 NAND_VerifyPage(Uint32 block, Uint32 page, Uint8* src, Uint8* dest)
{
    Uint32 i;

    if (NAND_ReadPage(block, page, dest) != E_PASS)
        return E_FAIL;
    
    for (i=0; i< gNandInfo.bytesPerPage; i++)
    {
        // Check for data read errors
        if (src[i] != dest[i])
        {
            UARTSendData("Data mismatch! Verification failed.", FALSE);
            return E_FAIL;
        }
    }
	return E_PASS;
}

// NAND Flash erase block function
Uint32 NAND_EraseBlocks(Uint32 startBlkNum, Uint32 blkCnt)
{	
	Uint32 i;
		
	// Do bounds checking
	if ( (startBlkNum + blkCnt - 1) >= gNandInfo.numBlocks )
		return E_FAIL;
	
	// Output info about what we are doing
	UARTSendData((Uint8 *)"Erasing blocks 0x", FALSE);
	UARTSendInt(startBlkNum);
	UARTSendData((Uint8 *)" through 0x", FALSE);
	UARTSendInt(startBlkNum + blkCnt - 1);
	UARTSendData((Uint8 *)".\r\n", FALSE);

	for (i = 0; i < blkCnt; i++)
	{
		// Start erase command
		flash_write_cmd((PNAND_INFO)&gNandInfo, NAND_BERASEC1);

		// Write the row addr bytes only
		flash_write_row_addr_bytes((PNAND_INFO)&gNandInfo, (startBlkNum+i), 0);
		
		// Confirm erase command
		flash_write_cmd((PNAND_INFO)&gNandInfo, NAND_BERASEC2);

		// Wait for the device to be ready
		if (NAND_WaitForRdy(NAND_TIMEOUT) != E_PASS)
			return E_FAIL;

		// verify the op succeeded by reading status from flash
        if (NAND_WaitForStatus(NAND_TIMEOUT) != E_PASS)
			return E_FAIL;
	}

	return E_PASS;
}

// NAND Flash unprotect command
Uint32 NAND_UnProtectBlocks(Uint32 startBlkNum, Uint32 blkCnt)
{
	Uint32 endBlkNum;
	endBlkNum = startBlkNum + blkCnt - 1;

	// Do bounds checking
	if (endBlkNum >= gNandInfo.numBlocks)
		return E_FAIL;

	// Output info about what we are doing
	UARTSendData((Uint8 *)"Unprotecting blocks 0x", FALSE);
	UARTSendInt(startBlkNum);
	UARTSendData((Uint8 *)" through 0x", FALSE);
	UARTSendInt(endBlkNum);
	UARTSendData((Uint8 *)".\n", FALSE);

	flash_write_cmd((PNAND_INFO)&gNandInfo, NAND_UNLOCK_START);
	flash_write_row_addr_bytes((PNAND_INFO)&gNandInfo, startBlkNum, 0);
	
	flash_write_cmd((PNAND_INFO)&gNandInfo, NAND_UNLOCK_END);
	flash_write_row_addr_bytes((PNAND_INFO)&gNandInfo, endBlkNum, 0);
		
	return E_PASS;
}

// NAND Flash protect command
void NAND_ProtectBlocks(void)
{
	UARTSendData((Uint8 *)"Protecting the entire NAND flash.\n", FALSE);
	flash_write_cmd((PNAND_INFO)&gNandInfo, NAND_LOCK);
}


// Generic function to write a UBL or Application header and the associated data
Uint32 NAND_WriteHeaderAndData(NAND_BOOT *nandBoot, Uint8 *srcBuf) {
	Uint32     endBlockNum;
	Uint32     *ptr;
	Uint32     blockNum;
	Uint32     count;
	Uint32     countMask;
	Uint32     numBlks;
	
	// Get total number of blocks needed
	numBlks = 0;
	while ( (numBlks * gNandInfo.pagesPerBlock)  < (nandBoot->numPage + 1) )
	{
		numBlks++;
	}
	UARTSendData((Uint8 *)"Number of blocks needed for header and data: 0x", FALSE);
	UARTSendInt(numBlks);
	UARTSendData((Uint8 *)"\r\n", FALSE);

	// Check whether writing UBL or APP (based on destination block)
	blockNum = nandBoot->block;
	if (blockNum == START_UBL_BLOCK_NUM)
	{
		endBlockNum = END_UBL_BLOCK_NUM;
	}
	else if (blockNum == START_APP_BLOCK_NUM)
	{
		endBlockNum = END_APP_BLOCK_NUM;
	}
	else
	{
		return E_FAIL; /* Block number is out of range */
	}

NAND_WRITE_RETRY:
	if (blockNum > endBlockNum)
	{
		return E_FAIL;
	}
	UARTSendData((Uint8 *)"Attempting to start in block number 0x", FALSE);
	UARTSendInt(blockNum);
	UARTSendData((Uint8 *)".\n", FALSE);

	// Unprotect all needed blocks of the Flash 
	if (NAND_UnProtectBlocks(blockNum,numBlks) != E_PASS)
	{
		blockNum++;
		UARTSendData((Uint8 *)"Unprotect failed\n", FALSE);
		goto NAND_WRITE_RETRY;
	}

	// Erase the block where the header goes and the data starts
	if (NAND_EraseBlocks(blockNum,numBlks) != E_PASS)
	{
		blockNum++;
		UARTSendData((Uint8 *)"Erase failed\n", FALSE);
		goto NAND_WRITE_RETRY;
	}
		
	// Setup header to be written
	ptr = (Uint32 *) gNandTx;
	ptr[0] = nandBoot->magicNum;
	ptr[1] = nandBoot->entryPoint;
	ptr[2] = nandBoot->numPage;
	ptr[3] = blockNum;	//always start data in current block
	ptr[4] = 1;			//always start data in page 1 (this header goes in page 0)
	ptr[5] = nandBoot->ldAddress;

	// Write the header to page 0 of the current blockNum
	UARTSendData((Uint8 *)"Writing header...\n", FALSE);
	if (NAND_WritePage(blockNum, 0, gNandTx) != E_PASS)
		return E_FAIL;
		
	waitloop(200);

	// Verify the page just written
	if (NAND_VerifyPage(blockNum, 0, gNandTx, gNandRx) != E_PASS)
		return E_FAIL;

	count = 1; 

	// The following assumes power of 2 page_cnt -  *should* always be valid 
	countMask = (Uint32)gNandInfo.pagesPerBlock - 1;
	UARTSendData((Uint8 *)"Writing data...\n", FALSE);
	do {
		// Write the UBL or APP data on a per page basis
		if (NAND_WritePage(blockNum, (count & countMask), srcBuf) != E_PASS)
			return E_FAIL;

		waitloop(200);
		
		// Verify the page just written
		if (NAND_VerifyPage(blockNum, (count & countMask), srcBuf, gNandRx) != E_PASS)
		    return E_FAIL;
		
		count++;
		srcBuf += gNandInfo.bytesPerPage;
		if (!(count & countMask))
		{
			blockNum++;
		}
	} while (count <= nandBoot->numPage);

	NAND_ProtectBlocks();

	return E_PASS;
}

#endif
