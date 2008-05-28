/* --------------------------------------------------------------------------
    FILE        : nor.c 				                             	 	        
    PURPOSE     : NOR driver file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Jan-22-2007
 
    HISTORY
 	     v1.00 completion 							 						      
 	          Daniel Allred - Jan-22-2007                                            
 ----------------------------------------------------------------------------- */

#ifdef UBL_NOR

#include "ubl.h"
#include "dm644x.h"
#include "uart.h"
#include "nor.h"

//External and global static variables
extern Uint32 __NORFlash;
volatile NOR_INFO gNorInfo;

// ----------------- Bus Width Agnostic commands -------------------
VUint8 *flash_make_addr (Uint32 blkAddr, Uint32 offset)
{
	return ((VUint8 *) ( blkAddr + (offset * gNorInfo.maxTotalWidth)));
}

void flash_make_cmd (Uint8 cmd, void *cmdbuf)
{
	Int32 i;
	Uint8 *cp = (Uint8 *) cmdbuf;

	for (i = gNorInfo.busWidth; i > 0; i--)
		*cp++ = (i & (gNorInfo.chipOperatingWidth - 1)) ? 0x00 : cmd;
}

void flash_write_cmd (Uint32 blkAddr, Uint32 offset, Uint8 cmd)
{
	volatile FLASHPtr addr;
	FLASHData cmdword;

	addr.cp = flash_make_addr (blkAddr, offset);
	flash_make_cmd ( cmd, &cmdword);
	switch (gNorInfo.busWidth)
	{
	    case BUS_8BIT:
            *addr.cp = cmdword.c;
            break;
        case BUS_16BIT:
            *addr.wp = cmdword.w;
            break;
	}
}

void flash_write_data(Uint32 address, Uint32 data)
{
	volatile FLASHPtr pAddr;
	FLASHData dataword;
	dataword.l = data;

	pAddr.cp = (VUint8*) address;
	
	switch (gNorInfo.busWidth)
	{
	    case BUS_8BIT:
            *pAddr.cp = dataword.c;
            break;
        case BUS_16BIT:
            *pAddr.wp = dataword.w;
            break;
	}
}

void flash_write_databuffer(Uint32* address, void* data, Uint32 numBytes)
{
    volatile FLASHPtr pAddr, pData;
    VUint8* endAddress;
		
	pData.cp = (VUint8*) data;
	pAddr.cp = (VUint8*) *address;
	endAddress =(VUint8*)((*address)+numBytes);
	while (pAddr.cp < endAddress)
	{
	    switch (gNorInfo.busWidth)
	    {
	        case BUS_8BIT:
                *pAddr.cp++ = *pData.cp++;
                break;
            case BUS_16BIT:
                *pAddr.wp++ = *pData.wp++;
                break;
	    }
    }
    
    // Put last data written at start of data buffer - For AMD verification
    switch (gNorInfo.busWidth)
    {
        case BUS_8BIT:
            *address = (Uint32)(endAddress-1);
            break;
        case BUS_16BIT:
            *address = (Uint32)(endAddress-2);
            break;
    }

}

Uint32 flash_verify_databuffer(Uint32 address, void* data, Uint32 numBytes)
{
    volatile FLASHPtr pAddr, pData;
    VUint8* endAddress;
		
	pData.cp = (VUint8*) data;
	pAddr.cp = (VUint8*) address;
	endAddress =(VUint8*)(address+numBytes);
	while (pAddr.cp < endAddress)
	{
	    switch (gNorInfo.busWidth)
	    {
	        case BUS_8BIT:
                if ( (*pAddr.cp++) != (*pData.cp++) )
                    return E_FAIL;
                break;
            case BUS_16BIT:
                if ( (*pAddr.wp++) != (*pData.wp++) )
                    return E_FAIL;
                break;
	    }
    }
    return E_PASS;
}

Uint32 flash_read_data(Uint32 address, Uint32 offset)
{
    volatile FLASHPtr pAddr;
	FLASHData dataword;
	dataword.l = 0x00000000;

	pAddr.cp = flash_make_addr(address, offset);
	
	switch (gNorInfo.busWidth)
	{
	    case BUS_8BIT:
            dataword.c = *pAddr.cp;
            break;
            
        case BUS_16BIT:
            dataword.w = *pAddr.wp;
            break;
	}
	return dataword.l;
}

FLASHData flash_read_CFI_bytes (Uint32 blkAddr, Uint32 offset, Uint8 numBytes)
{
    Int32 i;
	FLASHData readword;
	Uint8* pReadword = &readword.c;
	
	for (i = 0; i < numBytes; i++)
	{
	    *pReadword++ = *(flash_make_addr (blkAddr, offset+i));
    }
	
	return readword;
}

Bool flash_data_isequal (Uint32 blkAddr, Uint32 offset, Uint32 val)
{
	FLASHData testword_a, testword_b;
	Bool retval = FALSE;

	testword_a.l = val;
	testword_b.l = flash_read_data(blkAddr, offset);
	
	switch (gNorInfo.busWidth)
	{
	    case BUS_8BIT:
            retval = (testword_a.c == testword_b.c);
            break;
        case BUS_16BIT:
            retval = (testword_a.w == testword_b.w);
            break;
	}
    return retval;
}

Bool flash_CFI_isequal (Uint32 blkAddr, Uint32 offset, Uint8 val)
{
    volatile FLASHPtr addr;
	FLASHData testword;
	
	Bool retval = TRUE;

	addr.cp = flash_make_addr (blkAddr, offset);
	flash_make_cmd ( val, &testword);
	switch (gNorInfo.busWidth)
	{
	    case BUS_8BIT:
            retval = (testword.c == *addr.cp);
            break;
        case BUS_16BIT:
            retval = (testword.w == *addr.wp);
            break;
	}
    return retval;
}

Bool flash_issetall (Uint32 blkAddr, Uint32 offset, Uint8 mask)
{
    volatile FLASHPtr addr;
	FLASHData maskword;
	maskword.l = 0x00000000;
	
	Bool retval = TRUE;

	addr.cp = flash_make_addr (blkAddr, offset);
	flash_make_cmd ( mask, &maskword);
	switch (gNorInfo.busWidth)
	{
	    case BUS_8BIT:
            retval = ((maskword.c & *addr.cp) == maskword.c);
            break;
        case BUS_16BIT:
            retval = ((maskword.w & *addr.wp) == maskword.w);
            break;
	}
    return retval;
}

Bool flash_issetsome (Uint32 blkAddr, Uint32 offset, Uint8 mask)
{
    volatile FLASHPtr addr;
	FLASHData maskword;
	
	Bool retval = TRUE;

	addr.cp = flash_make_addr (blkAddr, offset);
	flash_make_cmd ( mask, &maskword);
	switch (gNorInfo.busWidth)
	{
	    case BUS_8BIT:
            retval = (maskword.c & *addr.cp);
            break;
        case BUS_16BIT:
            retval = (maskword.w & *addr.wp);
            break;
	}
    return retval;
}

//Initialize the AEMIF subsystem and settings
Uint32 NOR_Init()
{
    Uint8 width = ( ( (SYSTEM->BOOTCFG) >> 5) & 0x1 );

    // Select ASYNC EMIF Address Lines
    SYSTEM->PINMUX[0] = 0xC1F;

    // Program Asynchronous Wait Cycles Configuration Control Register
    AEMIF->AWCCR |= 0x0 ;

    // Program Asynchronous Bank3-5 Register
    AEMIF->AB1CR = 0x3FFFFFFC | width;
    AEMIF->AB2CR = 0x3FFFFFFC | width;
    AEMIF->AB3CR = 0x3FFFFFFC | width;
    AEMIF->AB4CR = 0x3FFFFFFC | width;
    
    /*AEMIF->AB1CR = 0
        | ( 0 << 31 ) // selectStrobe      = 0;
        | ( 0 << 30 ) // extWait           = 0;
        | ( 0 << 26 ) // writeSetup        = 0;    //   0 ns
        | ( 3 << 20 ) // writeStrobe       = 3;    //  35 ns
        | ( 0 << 17 ) // writeHold         = 0;    //   0 ns
        | ( 3 << 13 ) // readSetup         = 3;    //  30 ns
        | ( 10<< 7 )  // readStrobe        = 10;   // 120 ns
        | ( 0 << 4 )  // readHold          = 0;    //   0 ns
        | ( 3 << 2 )  // turnAround        = 3;    //  ?? ns ( MAX TIMEOUT )
        | ( 1 << 0 )  // asyncSize         = 1;    // 16-bit bus
        ;*/
                
    //Init the FlashInfo structure
    gNorInfo.flashBase = (Uint32) &(__NORFlash);
    
    // Set width to 8 or 16
    gNorInfo.busWidth = (width)?BUS_16BIT:BUS_8BIT;
    
    // Perform CFI Query
    if (QueryCFI(gNorInfo.flashBase) == E_PASS)
    {
        // Below is specifically needed to check for AMD flash on DVEVM (rev. D or earlier)
        // since it's top address line is not connected (don't ask me why)
        if (gNorInfo.numberRegions == 1)
        {
            if ( QueryCFI( gNorInfo.flashBase+(gNorInfo.flashSize>>1) ) == E_PASS )     
	        {
	            gNorInfo.flashSize >>= 1;
	            gNorInfo.numberBlocks[0] >>= 1;
	        }
        }
    }
    else
    {
        UARTSendData("CFI query failed.\r\n", FALSE);
        return E_FAIL;
    }
    
    // Setup function pointers
    
    UARTSendData("NOR Initialization:\r\n", FALSE);
    
    UARTSendData("\tCommand Set: ", FALSE);    
    switch (gNorInfo.commandSet)
    {
        case AMD_BASIC_CMDSET:
        case AMD_EXT_CMDSET:
            Flash_Erase          = &AMD_Erase;
            Flash_BufferWrite    = &AMD_BufferWrite;
            Flash_Write          = &AMD_Write;
            Flash_ID             = &AMD_ID;
            UARTSendData("AMD\r\n", FALSE);
            break;
        case INTEL_BASIC_CMDSET:
        case INTEL_EXT_CMDSET:
            Flash_Erase          = &Intel_Erase;
            Flash_BufferWrite    = &Intel_BufferWrite;
            Flash_Write          = &Intel_Write;
            Flash_ID             = &Intel_ID;
            UARTSendData("Intel\r\n", FALSE);
            break;
        default:
            Flash_Write          = &Unsupported_Write;
            Flash_BufferWrite    = &Unsupported_BufferWrite;
            Flash_Erase          = &Unsupported_Erase;
            Flash_ID             = &Unsupported_ID;
            UARTSendData("Unknown\r\n", FALSE);
            break;
    }
    
    if ( (*Flash_ID)(gNorInfo.flashBase) != E_PASS)
    {
        UARTSendData("NOR ID failed.\r\n", FALSE);
        return E_FAIL;
    }
        
    UARTSendData("\tManufacturer: ", FALSE);
    switch(gNorInfo.manfID)
    {
        case AMD:
            UARTSendData("AMD", FALSE);
            break;
        case FUJITSU:
            UARTSendData("FUJITSU", FALSE);
            break;
        case INTEL:
            UARTSendData("INTEL", FALSE);
            break;
        case MICRON:
            UARTSendData("MICRON", FALSE);
            break;
        case SAMSUNG:
            UARTSendData("SAMSUNG", FALSE);
            break;
        case SHARP:
            UARTSendData("SHARP", FALSE);
            break;
        default:
            UARTSendData("Unknown", FALSE);
            break;
    }
    UARTSendData("\r\n", FALSE);
    UARTSendData("\tSize (in bytes): 0x", FALSE);
    UARTSendInt( gNorInfo.flashSize );
    UARTSendData("\r\n", FALSE);
    
    return E_PASS;    
}

// Query the chip to check for CFI table and data
Uint32 QueryCFI( Uint32 baseAddress )
{                
    Int32 i;
    Uint32 blkVal; 
    
    // Six possible NOR Flash Configurations of DM644x
    //  1) Bus in x8 mode, x8 only device
    //  2) Bus in x8 mode, single x8/x16 flash operating in x8 mode
    //  3) Bus in x16 mode, single x8/x16 or x16-only flash operating in x16 mode
    //  4) Bus in x16 mode, two x8 flash operating in parallel.
    //  5) Bus in x16 mode, two x8/x16 flash, each in x8 mode, operating in parallel 
    //  6) Bus in x16 mode, single x16/x32 flash operating in x16 mode
	
	for (gNorInfo.chipOperatingWidth = BUS_8BIT; gNorInfo.chipOperatingWidth <= gNorInfo.busWidth;  gNorInfo.chipOperatingWidth <<= 1)
    {
        for (gNorInfo.maxTotalWidth = gNorInfo.busWidth; gNorInfo.maxTotalWidth <= (gNorInfo.busWidth*2); gNorInfo.maxTotalWidth <<= 1)
        {
            // Specify number of devices
            gNorInfo.numberDevices = 0;
            while ( gNorInfo.numberDevices * gNorInfo.chipOperatingWidth < gNorInfo.busWidth)
                gNorInfo.numberDevices++;
                                    
            // Enter the CFI Query mode
            flash_write_cmd (baseAddress, 0, CFI_EXIT_CMD);
            flash_write_cmd (baseAddress, CFI_QRY_CMD_ADDR, CFI_QRY_CMD);
            
            // Check for Query QRY values
            if ( flash_CFI_isequal ( baseAddress, CFI_Q, 'Q') && 
			     flash_CFI_isequal ( baseAddress, CFI_R, 'R') && 
			     flash_CFI_isequal ( baseAddress, CFI_Y, 'Y') )
			{               
			    gNorInfo.commandSet = (CMDSET) (flash_read_CFI_bytes(baseAddress,CFI_CMDSET,2).w);
	            gNorInfo.flashSize = 0x1 << flash_read_CFI_bytes(baseAddress,CFI_DEVICESIZE,1).c * gNorInfo.numberDevices;
                gNorInfo.numberRegions = flash_read_CFI_bytes(baseAddress,CFI_NUMBLKREGIONS,1).c;
                gNorInfo.bufferSize = 0x1 << flash_read_CFI_bytes(baseAddress,CFI_WRITESIZE,2).w * gNorInfo.numberDevices;
                
                // Get info on sector sizes in each erase region of device
                for (i = 0;i < gNorInfo.numberRegions; i++)
                {    
                    blkVal = flash_read_CFI_bytes(baseAddress,(CFI_BLKREGIONS+i*CFI_BLKREGIONSIZE),4).l;
                    gNorInfo.numberBlocks[i] = (blkVal&0x0000FFFF) + 1;
                    gNorInfo.blockSize[i]    = ((blkVal&0xFFFF0000) ? ( ((blkVal>>16)&0xFFFF) * 256) : 128) * gNorInfo.numberDevices;
                }
                
                // Exit CFI mode 
                flash_write_cmd (baseAddress, 0, CFI_EXIT_CMD);
			    
			    return E_PASS;
            }
        }        
    }
    
    flash_write_cmd (baseAddress, 0, CFI_EXIT_CMD);   
    return E_FAIL;
}


// -------------------------------------------------------------------------
// Manufacturer Specific Commands
// -------------------------------------------------------------------------

// ------------------------  Default Empty  ---------------------------
Uint32 Unsupported_Write( Uint32 address, VUint32 data)
{
    return E_FAIL;
}
Uint32 Unsupported_BufferWrite(Uint32 address, VUint8 data[], Uint32 length )
{
    return E_FAIL;
}
Uint32 Unsupported_Erase(Uint32 address)
{
    return E_FAIL;
}

Uint32 Unsupported_ID(Uint32 address)
{
    return E_FAIL;
}


// -------------------- Begin of Intel specific commands -----------------------

//ID flash
Uint32 Intel_ID( Uint32 baseAddress )
{
    // Intel Exit back to read array mode
    Intel_Soft_Reset_Flash();
    
    // Write ID command
    flash_write_cmd(baseAddress, 0, INTEL_ID_CMD);
        
    //Read Manufacturer's ID
    gNorInfo.manfID = (MANFID) flash_read_data(baseAddress, INTEL_MANFID_ADDR);
    
    // Read Device ID
    gNorInfo.devID1 = (Uint16) (MANFID) flash_read_data(baseAddress, INTEL_DEVID_ADDR);
    gNorInfo.devID2 = 0x0000;
        
    // Intel Exit back to read array mode
    Intel_Soft_Reset_Flash(); 
    
    return E_PASS;
}

// Reset back to Read array mode
void Intel_Soft_Reset_Flash()
{
    // Intel Exit back to read array mode
    flash_write_cmd(gNorInfo.flashBase,0,INTEL_RESET);
}

// Clear status register
void Intel_Clear_Status()
{
    // Intel clear status
    flash_write_cmd(gNorInfo.flashBase,0,INTEL_CLEARSTATUS_CMD);
}

// Remove block write protection
Uint32 Intel_Clear_Lock(VUint32 blkAddr)
{

	// Write the Clear Lock Command
    flash_write_cmd(blkAddr,0,INTEL_LOCK_CMD0);

    flash_write_cmd(blkAddr,0,INTEL_UNLOCK_BLOCK_CMD);

    // Check Status
	return Intel_Lock_Status_Check();
}

// Write-protect a block
Uint32 Intel_Set_Lock(VUint32 blkAddr)
{
	// Write the Set Lock Command	
    flash_write_cmd(blkAddr,0,INTEL_LOCK_CMD0);            
	
    flash_write_cmd(blkAddr,0,INTEL_LOCK_BLOCK_CMD);

	// Check Status
	return Intel_Lock_Status_Check();
}

void Intel_Wait_For_Status_Complete()
{
    while ( !flash_issetall(gNorInfo.flashBase, 0, BIT7) );
}

Uint32 Intel_Lock_Status_Check()
{
    Uint32 retval = E_PASS;
    //Uint8 status;

    Intel_Wait_For_Status_Complete();

    //status = flash_read_uint16((Uint32)gNorInfo.flashBase,0);
    //if ( status & BIT5 )
    if (flash_issetsome(gNorInfo.flashBase, 0, (BIT5 | BIT3)))
    {
        retval = E_FAIL;
		/*if ( status & BIT4 )
        {
			UARTSendData("Command Sequence Error\r\n", FALSE);
		}
		else
		{
			UARTSendData("Clear Lock Error\r\n", FALSE);
		}*/
	}
	/*if ( status & BIT3 )
	{
		retval = E_FAIL;
		//UARTSendData("Voltage Range Error\n", FALSE);
    }*/
	
	// Clear status
	Intel_Clear_Status();
	
	// Put chip back into read array mode.
	Intel_Soft_Reset_Flash();
	
	// Set Timings back to Optimum for Read
	return retval;
}

// Erase Block
Uint32 Intel_Erase(VUint32 blkAddr)
{
	Uint32 retval = E_PASS;
	
	// Clear Lock Bits
	retval |= Intel_Clear_Lock(blkAddr);
	
	// Send Erase commands
	flash_write_cmd(blkAddr,0,INTEL_ERASE_CMD0);
	flash_write_cmd(blkAddr,0,INTEL_ERASE_CMD1);
	
	// Wait until Erase operation complete
	Intel_Wait_For_Status_Complete();
    
    // Verify successful erase                       
    if ( flash_issetsome(gNorInfo.flashBase, 0, BIT5) )
        retval = E_FAIL;
    
	// Put back into Read Array mode.
	Intel_Soft_Reset_Flash();
	
	return retval;
}

// Write data
Uint32 Intel_Write( Uint32 address, VUint32 data )
{
    Uint32 retval = E_PASS;
	
	// Send Write command
	flash_write_cmd(address,0,INTEL_WRITE_CMD);
	flash_write_data(address, data);
                  
    // Wait until Write operation complete
    Intel_Wait_For_Status_Complete();
	                          
    // Verify successful program
    if ( flash_issetsome(gNorInfo.flashBase, 0, (BIT4|BIT3)) )
    {
        //UARTSendData("Write Op Failed.\r\n", FALSE);
        retval = E_FAIL;
    }
    
    // Lock the block
    //retval |= Intel_Set_Lock(blkAddr);
    
    // Put back into Read Array mode.
	Intel_Soft_Reset_Flash();
                          
    return retval;
}

// Buffer write data
Uint32 Intel_BufferWrite(Uint32 address, VUint8 data[], Uint32 numBytes )
{
    Uint32 startAddress = address;
	Uint32 retval = E_PASS;
	Uint32 timeoutCnt = 0, shift;

	// Send Write_Buff_Load command   
    do {
        flash_write_cmd(address,0,INTEL_WRT_BUF_LOAD_CMD);
        timeoutCnt++;
    }while( (!flash_issetall(gNorInfo.flashBase, 0, BIT7)) && (timeoutCnt < 0x00010000) );
    
    if (timeoutCnt >= 0x10000)
    {
        //    UARTSendData("Write Op Failed.\r\n", FALSE);
        retval = E_TIMEOUT;
    }
    else
    {
        //Establish correct shift value
	    shift = 0;
	    while ((gNorInfo.busWidth >> shift) > 1)
    	    shift++;
    
        // Write Length (either numBytes or numBytes/2)	    
        flash_write_cmd(startAddress, 0, (numBytes >> shift) - 1);
        
        // Write buffer length
        //flash_write_data(startAddress, (length - 1));
        
        // Write buffer data
        flash_write_databuffer(&address,(void*)data,numBytes);
                
        // Send write buffer confirm command
        flash_write_cmd(startAddress,0,INTEL_WRT_BUF_CONF_CMD);
        
        // Check status
        Intel_Wait_For_Status_Complete();
        // Verify program was successful
        
        //if ( flash_read_uint8(gNorInfo.flashBase,0) & BIT4 )
        if ( flash_issetsome(gNorInfo.flashBase, 0, BIT4) )
        {
        //    UARTSendData("Write Buffer Op Failed.\r\n", FALSE);
            retval = E_FAIL;
        }
        
        // Put back into Read Array mode.
    	Intel_Soft_Reset_Flash();
    }
                          
    return retval;
}
// -------------------- End of Intel specific commands ----------------------


// -------------------- Begin of AMD specific commands -----------------------
// Identify the Manufacturer and Device ID 
Uint32 AMD_ID( Uint32 baseAddress )
{
    // Exit back to read array mode
    AMD_Soft_Reset_Flash();

    // Write ID commands
    AMD_Prefix_Commands();
    flash_write_cmd(baseAddress, AMD_CMD2_ADDR, AMD_ID_CMD);

    // Read manufacturer's ID
    gNorInfo.manfID = (MANFID) flash_read_data(baseAddress, AMD_MANFID_ADDR);
    
    // Read device ID
    gNorInfo.devID1 = (Uint16) flash_read_data(baseAddress, AMD_DEVID_ADDR0);
    
    // Read additional ID bytes if needed
    if ( (gNorInfo.devID1 & 0xFF ) == AMD_ID_MULTI )
        gNorInfo.devID2 = flash_read_CFI_bytes(baseAddress, AMD_DEVID_ADDR1, 2).w;
    else
        gNorInfo.devID2 = 0x0000;
        
    // Exit back to read array mode
    AMD_Soft_Reset_Flash();
    
    return E_PASS;
}



void AMD_Soft_Reset_Flash()
{
	// Reset Flash to be in Read Array Mode
	flash_write_cmd(gNorInfo.flashBase,AMD_CMD2_ADDR,AMD_RESET);                  
}

// AMD Prefix Commands
void AMD_Prefix_Commands()
{
    flash_write_cmd(gNorInfo.flashBase, AMD_CMD0_ADDR, AMD_CMD0);
    flash_write_cmd(gNorInfo.flashBase, AMD_CMD1_ADDR, AMD_CMD1);
}

// Erase Block
Uint32 AMD_Erase(Uint32 blkAddr)
{
    Uint32 retval = E_PASS;

    // Send commands
	AMD_Prefix_Commands();
    flash_write_cmd(gNorInfo.flashBase, AMD_CMD2_ADDR, AMD_BLK_ERASE_SETUP_CMD);
    AMD_Prefix_Commands();
    flash_write_cmd(blkAddr, AMD_CMD2_ADDR, AMD_BLK_ERASE_CMD);
	
	// Poll DQ7 and DQ15 for status
    while ( !flash_issetall(blkAddr, 0, BIT7) );
    
    // Check data 
    if ( !flash_data_isequal(blkAddr, 0, AMD_BLK_ERASE_DONE) )
        retval = E_FAIL;
	
	/* Flash Mode: Read Array */
    AMD_Soft_Reset_Flash();
    
    return retval;
}

// AMD Flash Write
Uint32 AMD_Write( Uint32 address, VUint32 data )
{
	Uint32 retval = E_PASS;
	
	// Send Commands
	AMD_Prefix_Commands();
    flash_write_cmd(gNorInfo.flashBase, AMD_CMD2_ADDR, AMD_PROG_CMD);
    flash_write_data(address, data);

	// Wait for ready.
	while(TRUE)
	{
	    if ( (flash_read_data(address, 0 ) & (BIT7 | BIT15) ) == (data & (BIT7 | BIT15) ) )
	    {
			break;
	    }
		else
		{
		    if(flash_issetall(address, 0, BIT5))
			{
				if ( (flash_read_data(address, 0 ) & (BIT7 | BIT15) ) != (data & (BIT7 | BIT15) ) )
				{
				    UARTSendData("Timeout ocurred.\r\n",FALSE);
					retval = E_FAIL;
				}
			    break;				
			}
		}
	}
	
    // Return Read Mode
	AMD_Soft_Reset_Flash();
	
	// Verify the data.
	if ( (retval == E_PASS) && ( flash_read_data(address, 0) != data) )
	    retval = E_FAIL;
	
	return retval;
}

// AMD flash buffered write
Uint32 AMD_BufferWrite(Uint32 address, VUint8 data[], Uint32 numBytes )
{
    Uint32 startAddress = address;
	Uint32 blkAddress, blkSize;
	Uint32 data_temp;
	Uint32 retval = E_PASS;
	Uint32 shift;
	
	// Get block base address and size
	DiscoverBlockInfo(address, &blkSize, &blkAddress);
			
	// Write the Write Buffer Load command
    AMD_Prefix_Commands();
    flash_write_cmd(blkAddress, 0, AMD_WRT_BUF_LOAD_CMD);
        
    //Establish correct shift value
	shift = 0;
	while ((gNorInfo.busWidth >> shift) > 1)
	    shift++;
    
    // Write Length (either numBytes or numBytes/2)	    
    flash_write_cmd(blkAddress, 0, (numBytes >> shift) - 1);
	
	// Write Data
	flash_write_databuffer(&address,(void*)data, numBytes);
		
    // Program Buffer to Flash Confirm Write
    flash_write_cmd(blkAddress, 0, AMD_WRT_BUF_CONF_CMD);                  
    
    // Read last data item                  
    data_temp = flash_read_data((Uint32) (data + (address - startAddress)), 0);
        
	while(TRUE)
	{
		//temp1 = flash_read_data(address, 0 );   
		if( (flash_read_data(address, 0 ) & (BIT7 | BIT15)) == (data_temp & (BIT7 | BIT15) ) )
		{
			break;
		}
		else
		{
		    // Timeout has occurred
			if(flash_issetall(address, 0, BIT5))
			{
				if( (flash_read_data(address, 0 ) & (BIT7 | BIT15)) != (data_temp & (BIT7 | BIT15) ) )
				{
				    UARTSendData("Timeout ocurred.\r\n",FALSE);
					retval = E_FAIL;
				}
				break;
			}
			// Abort has occurred
			if(flash_issetall(address, 0, BIT1))
			{
				if( (flash_read_data(address, 0 ) & (BIT7 | BIT15)) != (data_temp & (BIT7 | BIT15) ) )
				{
				    UARTSendData("Abort ocurred.\r\n",FALSE);
					retval = E_FAIL;
					AMD_Write_Buf_Abort_Reset_Flash ();
				}
				break;
			}
		}
	}
	
	// Put chip back into read array mode.
	AMD_Soft_Reset_Flash();
	if (retval == E_PASS)
	    retval = flash_verify_databuffer(startAddress,(void*)data, numBytes);
	return retval;
}

// AMD Write Buf Abort Reset Flash
void AMD_Write_Buf_Abort_Reset_Flash()
{
	// Reset Flash to be in Read Array Mode
    AMD_Prefix_Commands();
    AMD_Soft_Reset_Flash();
}
//--------------------- End of AMD specific commands ------------------------


// Get info on block address and sizes
Uint32 DiscoverBlockInfo(Uint32 address,Uint32* blockSize, Uint32* blockAddr)
{
    Int32 i;
    Uint32 currRegionAddr, nextRegionAddr;
        
    currRegionAddr = (Uint32) gNorInfo.flashBase;
    if ((address < currRegionAddr) || (address >= (currRegionAddr+gNorInfo.flashSize)))
    {
        return E_FAIL;
    }
    
    for(i=0; i< (gNorInfo.numberRegions); i++)
    {
        nextRegionAddr = currRegionAddr + (gNorInfo.blockSize[i] * gNorInfo.numberBlocks[i]);
        if ( (currRegionAddr <= address) && (nextRegionAddr > address) )
        {
            *blockSize = gNorInfo.blockSize[i];
            *blockAddr = address & (~((*blockSize) - 1));
            break;
        }
        currRegionAddr = nextRegionAddr;
    }
    return E_PASS;
}

// --------------------------- NOR API Functions -----------------------------

//Global Erase NOR Flash
Uint32 NOR_GlobalErase()
{
    return NOR_Erase( (VUint32) gNorInfo.flashBase, (VUint32) gNorInfo.flashSize );
}

// Erase Flash Block
Uint32 NOR_Erase(VUint32 start_address, VUint32 size)
{
	VUint32 addr  = start_address;
	VUint32 range = start_address + size;
	Uint32 blockSize, blockAddr;
	
	UARTSendData((Uint8 *)"Erasing the NOR Flash\r\n", FALSE);
	
   	while (addr < range)
  	{
	    if (DiscoverBlockInfo(addr, &blockSize, &blockAddr) != E_PASS)
	    {
	        UARTSendData((Uint8 *)"Address out of range", FALSE);
	        return E_FAIL;
	    }
		
		//Increment to the next block
	    if ( (*Flash_Erase)(blockAddr) != E_PASS)
	    {
	        UARTSendData("Erase failure at block address 0x",FALSE);
	        UARTSendInt(blockAddr);
	        UARTSendData("\r\n", FALSE);
	        return E_FAIL;
	    }
	    addr = blockAddr + blockSize;
	    
	    // Show status messages
	    UARTSendData((Uint8 *)"Erased through 0x", FALSE);
		UARTSendInt(addr);
		UARTSendData((Uint8 *)"\r\n", FALSE);
  	}

	UARTSendData((Uint8 *)"Erase Completed\r\n", FALSE);

  	return(E_PASS);
}

// NOR_WriteBytes
Uint32 NOR_WriteBytes( Uint32 writeAddress,
					   Uint32 numBytes,
					   Uint32 readAddress)
{
	Uint32      blockSize, blockAddr;
	Int32		i;
	Uint32      retval = E_PASS;

	UARTSendData((Uint8 *)"Writing the NOR Flash\r\n", FALSE);

	// Make numBytes even if needed
	if (numBytes & 0x00000001)
		numBytes++;
		
    if (DiscoverBlockInfo(writeAddress, &blockSize, &blockAddr) != E_PASS)
    {
        UARTSendData((Uint8 *)"Address out of range", FALSE);
        return E_FAIL;
    }

	while (numBytes > 0)
  	{
        if( (numBytes < gNorInfo.bufferSize) || (writeAddress & (gNorInfo.bufferSize-1) ))
		{
			if ((*Flash_Write)(writeAddress, flash_read_data(readAddress,0) ) != E_PASS)
			{
			    UARTSendData("\r\nNormal Write Failed.\r\n", FALSE);
			    retval = E_FAIL;
			}
			else
			{
			    numBytes     -= gNorInfo.busWidth;
			    writeAddress += gNorInfo.busWidth;
			    readAddress  += gNorInfo.busWidth;
			}
		}
		else
		{
		    // Try to use buffered writes
			if((*Flash_BufferWrite)(writeAddress, (VUint8 *)readAddress, gNorInfo.bufferSize) == E_PASS)
			{
				numBytes -= gNorInfo.bufferSize;
				writeAddress += gNorInfo.bufferSize;
			    readAddress  += gNorInfo.bufferSize;
			}
			else
			{
			    // Try normal writes as a backup
			    for(i = 0; i<(gNorInfo.bufferSize>>1); i++)
				{
                    if ((*Flash_Write)(writeAddress, flash_read_data(readAddress,0) ) != E_PASS)
					{
						UARTSendData("\r\nNormal write also failed\r\n", FALSE);
						retval = E_FAIL;
						break;
					}
			    	else
			        {
			            numBytes     -= gNorInfo.busWidth;
			            writeAddress += gNorInfo.busWidth;
			            readAddress  += gNorInfo.busWidth;
			        }
				}
			}
		}

        // Output status info on the write operation
        if (retval == E_PASS)
        {    
	        if  ( ((writeAddress & (~((blockSize>>4)-1))) == writeAddress) || (numBytes == 0) )
	        {
	            UARTSendData((Uint8*) "NOR Write OK through 0x", FALSE);
		        UARTSendInt(writeAddress);
		        UARTSendData((Uint8*)"\r\n", FALSE);
        		
	            if (DiscoverBlockInfo(writeAddress, &blockSize, &blockAddr) != E_PASS)
                {
                    UARTSendData((Uint8 *)"Address out of range", FALSE);
                    return E_FAIL;
                }
	        }
	    }
	    else
	    {
		    UARTSendData((Uint8*) "NOR Write Failed...Aborting!\r\n", FALSE);
		    return E_FAIL;
		}
  	}
  	return retval;
}

#endif
