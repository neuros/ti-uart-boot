/* --------------------------------------------------------------------------
    FILE        : nand.c 				                             	 	        
    PURPOSE     : NAND driver file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : 04-Jun-2007
 
    HISTORY
 	     v1.0 completion 							 						      
 	          Daniel Allred - Jan-22-2007
 	     v1.11 - DJA - 07-Mar-2007
 	          Fixed bug(s) for writing and reading Big BLock (2K) NAND devices.
 	     v1.12 - DJA - 14-Mar-2007
 	          Fixed bug for writing 256/512 block devices caused by v1.11 update
 	          (Thanks to Ivan Tonchev)
         v1.13 - DJA - 04-Jun-2007
              Hefty modifications to NAND code, reducing code path to one, 
			  regardless of page size.  Also adding ECC correction code.
			  Removing static allocation of temp page data.
 ----------------------------------------------------------------------------- */

#ifdef UBL_NAND

#include "ubl.h"
#include "dm644x.h"
#include "uart.h"
#include "nand.h"
#include "util.h"

// Symbol from linker script
extern Uint32 __NANDFlash;

// structure for holding details about the NAND device itself
volatile NAND_INFO gNandInfo;

// Global variables for page buffers 
static Uint8* gNandTx;
static Uint8* gNandRx;

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

// ***************************************
// Generic Low-level NAND access functions
// ***************************************
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

// **********************
// Status Check functions
// **********************

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

// ****************************************************
// Read the current ECC calculation and restart process
// ****************************************************
Uint32 NAND_ECCReadAndRestart (PNAND_INFO pNandInfo)
{
    VUint32 retval,temp;

	// Flush data writes (by reading CS3 data region)
	temp = *((VUint32*)(((VUint8*)pNandInfo->flashBase) + 0x02000000));

    // Read and mask appropriate (based on CSn space flash is in) ECC regsiter
    retval = ((Uint32*)(&(AEMIF->NANDF1ECC)))[pNandInfo->CSOffset] & pNandInfo->ECCMask;
    
	// Write appropriate bit to start ECC calcualtions 
    AEMIF->NANDFCR |= (1<<(8 + (pNandInfo->CSOffset)));   

	// Flush NANDFCR write (by reading another CFG register)
	temp = AEMIF->ERCSR;

    return retval;
}

// ***************************************************************
// Use old (write) and new (read) ECCs to correct single-bit error
// ***************************************************************
Uint32 NAND_ECCCorrection(PNAND_INFO pNandInfo, Uint32 ECCold, Uint32 ECCnew, Uint8 *data)
{
	Uint16 ECCxorVal, byteAddr, bitAddr;

	if (!pNandInfo->ECCEnable)
		return E_FAIL;

	ECCxorVal = (Uint16) ((ECCold & 0xFFFF0000) >> 16) ^  // write ECCo
	                     ((ECCold & 0x0000FFFF) >> 0 ) ^  // write ECCe
				         ((ECCnew & 0xFFFF0000) >> 16) ^  // read ECCo 
				         ((ECCnew & 0x0000FFFF) >> 0 );   // read ECCe

	if ( ECCxorVal == (0x0000FFFF & pNandInfo->ECCMask) )
	{
		// Single Bit error - can be corrected
        ECCxorVal = (Uint16) ((ECCold & 0xFFFF0000) >> 16) ^ ((ECCnew & 0xFFFF0000) >> 16);
        byteAddr = (ECCxorVal >> 3);
        bitAddr = (ECCxorVal & 0x7);
        data[byteAddr] ^= (0x1 << bitAddr);
        return E_PASS;
	}
	else
	{
        // Multiple Bit error - nothing we can do
        return E_FAIL;
	}
}

// *******************
// NAND Init Functions
// *******************

// Initialze NAND interface and find the details of the NAND used
Uint32 NAND_Init()
{
    Uint32 width, *CSRegs;
	UARTSendData((Uint8 *) "Initializing NAND flash...\r\n", FALSE);
	
	// Alloc mem for temp pages
	gNandTx = (Uint8 *) ubl_alloc_mem(MAX_PAGE_SIZE);
	gNandRx = (Uint8 *) ubl_alloc_mem(MAX_PAGE_SIZE);
	
	// Set NAND flash base address
    gNandInfo.flashBase = (Uint32) &(__NANDFlash);
    
    //Get the CSOffset (can be 0 through 3 - corresponds with CS2 through CS5)
    gNandInfo.CSOffset = (gNandInfo.flashBase >> 25) - 1;
    
    // Setting the nand_width = 0(8 bit NAND) or 1(16 bit NAND). AEMIF CS2 bus Width
	//   is given by the BOOTCFG(bit no.5)
    width = ( ( (SYSTEM->BOOTCFG) & 0x00000020) >> 5);
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
	Uint32 manfID,deviceID,i,j;
	
	// Issue device read ID command
    flash_write_cmd( (PNAND_INFO)&gNandInfo, NAND_RDID);
    flash_write_addr( (PNAND_INFO)&gNandInfo, NAND_RDIDADD);
    
	// Read ID bytes
	manfID   = flash_read_data( (PNAND_INFO)&gNandInfo ) & 0xFF;
	deviceID = flash_read_data( (PNAND_INFO)&gNandInfo ) & 0xFF;
	j        = flash_read_data( (PNAND_INFO)&gNandInfo ) & 0xFF;
	j        = flash_read_data( (PNAND_INFO)&gNandInfo ) & 0xFF;

	i=0;
	while (gNandDevInfo[i].devID != 0x00)
	{
		if(deviceID == gNandDevInfo[i].devID)
		{
		    gNandInfo.manfID = (Uint8) manfID;
			UARTSendData( (Uint8*)"Manufacturer ID  = 0x", FALSE);
			UARTSendInt(gNandInfo.manfID);
			UARTSendData( (Uint8*)"\r\n",FALSE);
		
			gNandInfo.devID = (Uint8) gNandDevInfo[i].devID;
			UARTSendData( (Uint8*)"Device ID        = 0x", FALSE);
			UARTSendInt(gNandInfo.devID);
			UARTSendData( (Uint8*)"\r\n",FALSE);
			
			gNandInfo.pagesPerBlock = gNandDevInfo[i].pagesPerBlock;
			UARTSendData( (Uint8*)"Pages Per Block  = 0x", FALSE);
			UARTSendInt( gNandInfo.pagesPerBlock );
			UARTSendData( (Uint8*)"\r\n",FALSE);
	
			gNandInfo.numBlocks = gNandDevInfo[i].numBlocks;
			UARTSendData( (Uint8*)"Number of Blocks = 0x", FALSE);
			UARTSendInt( gNandInfo.numBlocks );
			UARTSendData( (Uint8*)"\r\n",FALSE);
			
			gNandInfo.bytesPerPage = NANDFLASH_PAGESIZE(gNandDevInfo[i].bytesPerPage);
			UARTSendData( (Uint8*)"Bytes Per Page   = 0x", FALSE);
			UARTSendInt( gNandInfo.bytesPerPage );
			UARTSendData( (Uint8*)"\r\n",FALSE);
			
			break;
		}
		i++;
	}
	if (gNandDevInfo[i].devID == 0x00) return E_FAIL;

	// Assign the big_block flag
	gNandInfo.bigBlock = (gNandInfo.bytesPerPage>MAX_BYTES_PER_OP)?TRUE:FALSE;
	
	// Assign the bytes per operation value
	gNandInfo.bytesPerOp = (gNandInfo.bytesPerPage>MAX_BYTES_PER_OP)?MAX_BYTES_PER_OP:gNandInfo.bytesPerPage;
	
	// Assign the number of operations per page value
	gNandInfo.numOpsPerPage = (gNandInfo.bytesPerOp < MAX_BYTES_PER_OP)?1:(gNandInfo.bytesPerPage >> MAX_BYTES_PER_OP_SHIFT);
	
	// Assign the number of spare bytes per operation
	gNandInfo.spareBytesPerOp = gNandInfo.bytesPerOp >> SPAREBYTES_PER_OP_SHIFT;
	
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
	gNandInfo.ECCMask = 0x00000000;
	for (j = 0; (((gNandInfo.bytesPerOp*8)>>j) > 0x1); j++)
	{
	    gNandInfo.ECCMask |= (0x00010001<<j);
	}
	
	gNandInfo.ECCOffset = (gNandInfo.bigBlock)?2:0;

	gNandInfo.ECCEnable = TRUE;
				    		
	return E_PASS;
}


// *******************
// NAND Read Functions
// *******************

// Routine to read a page from NAND
Uint32 NAND_ReadPage(Uint32 block, Uint32 page, Uint8 *dest) {
	Uint32 eccValue[4];
	Uint32 spareValue[4];
	Uint8 i;
	
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

	// Starting the ECC in the NANDFCR register
	NAND_ECCReadAndRestart((PNAND_INFO)&gNandInfo);

    // Read the page data
    for (i=0; i < gNandInfo.numOpsPerPage; i++)
    {
        // Actually read bytes
		flash_read_bytes((PNAND_INFO)&gNandInfo, (void*)(dest), gNandInfo.bytesPerOp);
	    
	    // Get the ECC Value
	    eccValue[i] = NAND_ECCReadAndRestart((PNAND_INFO)&gNandInfo);
	    	    
	    //Increment pointer
	    dest += gNandInfo.bytesPerOp;
	}

	// Reset the page pointer
    dest -= gNandInfo.bytesPerPage;

    // Check ECCs
	for (i=0; i<gNandInfo.numOpsPerPage; i++)
	{
		flash_read_bytes((PNAND_INFO)&gNandInfo, (void*)(spareValue), gNandInfo.spareBytesPerOp);
		flash_swap_data((PNAND_INFO)&gNandInfo, (Uint32*)(spareValue+gNandInfo.ECCOffset));

		// Verify ECC values
		if(eccValue[i] != spareValue[gNandInfo.ECCOffset])
		{
			if (NAND_ECCCorrection( (PNAND_INFO)&gNandInfo,
					                spareValue[gNandInfo.ECCOffset],
						            eccValue[i],
							        dest+(i*gNandInfo.bytesPerOp) ) != E_PASS)
			{
				UARTSendData( (Uint8 *) "NAND ECC failure!\r\n", FALSE);
				return E_FAIL;
			}
		}
	}
	
    // Return status check result
	return NAND_WaitForStatus(NAND_TIMEOUT);
}

// ********************
// NAND Write Functions
// ********************

// Generic routine to write a page of data to NAND
Uint32 NAND_WritePage(Uint32 block, Uint32 page, Uint8 *src) {
	Uint32 eccValue[4];
	Uint32 spareValue[4];
	Uint8 i;

	// Make sure the NAND page pointer is at start of page
    flash_write_cmd((PNAND_INFO)&gNandInfo,NAND_LO_PAGE);

	// Write program command
	flash_write_cmd((PNAND_INFO)&gNandInfo, NAND_PGRM_START);
	
	// Write address bytes
	flash_write_addr_cycles((PNAND_INFO)&gNandInfo,block,page);
	
	// Starting the ECC in the NANDFCR register for CS2(bit no.8)
	NAND_ECCReadAndRestart((PNAND_INFO)&gNandInfo);
	
	// Write data
	for (i=0; i<gNandInfo.numOpsPerPage; i++)
	{
	    flash_write_bytes((PNAND_INFO)&gNandInfo, (void*) src, gNandInfo.bytesPerOp);    
	    	    	    
	    // Read the ECC value
	    eccValue[i] = NAND_ECCReadAndRestart((PNAND_INFO)&gNandInfo);	    

	    // Increment the pointer
	    src += gNandInfo.bytesPerOp;
	}

	// Write spare bytes
    spareValue[0] = 0xFFFFFFFF;
    spareValue[1] = 0xFFFFFFFF;
    spareValue[2] = 0xFFFFFFFF;
    spareValue[3] = 0xFFFFFFFF;
    for (i=0; i<gNandInfo.numOpsPerPage; i++)
    {
        // Swap the bytes for how the ROM needs them
        flash_swap_data((PNAND_INFO)&gNandInfo, &(eccValue[i]));
        
		// Place the ECC values where the ROM read routine expects them    
        spareValue[gNandInfo.ECCOffset] = eccValue[i];
        
		// Actually write the Spare Bytes               
        flash_write_bytes((PNAND_INFO)&gNandInfo, (void*)(spareValue), gNandInfo.spareBytesPerOp);
    }
    			
    // Write program end command
    flash_write_cmd((PNAND_INFO)&gNandInfo, NAND_PGRM_END);
	
	// Wait for the device to be ready
	if (NAND_WaitForRdy(NAND_TIMEOUT) != E_PASS)
		return E_FAIL;

    // Return status check result	
	return NAND_WaitForStatus(NAND_TIMEOUT);
}

// **********************************************************
// Verify data written by reading and comparing byte for byte
// **********************************************************
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

// *******************************
// NAND Flash erase block function
// *******************************
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


// *******************************************************************
// NAND Protect and Unprotect commands (not all devices support these)
// *******************************************************************

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
