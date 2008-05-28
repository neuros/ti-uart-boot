/* --------------------------------------------------------------------------
    FILE        : uart.c 				                             	 	        
    PURPOSE     : UART driver file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Jan-22-2007
 
    HISTORY
 	     v1.00 completion 							 						      
 	          Daniel Allred - Jan-22-2007                                              
 ----------------------------------------------------------------------------- */
#include "dm644x.h"
#include "uart.h"
#include "util.h"
#include "ubl.h"

extern VUint32 gMagicFlag,gBootCmd;
extern VUint32 gDecodedByteCount,gSrecByteCount;

// Send specified number of bytes 
Uint32 UARTSendData(Uint8* seq, Bool includeNull)
{
	Uint32 status = 0;
    Int32 i,numBytes;
	Uint32 timerStatus = 1;
	
	numBytes = includeNull?(GetStringLen(seq)+1):(GetStringLen(seq));
	
	for(i=0;i<numBytes;i++) {
		/* Enable Timer one time */
		TIMER0Start();
		do{
			status = (UART0->LSR)&(0x20);
			timerStatus = TIMER0Status();
		} while (!status && timerStatus);

		if(timerStatus == 0)
			return E_TIMEOUT;
		
		// Send byte 
		(UART0->THR) = seq[i];
	}
	return E_PASS;
}

Uint32 UARTSendInt(Uint32 value)
{
	char seq[9];
	Uint32 i,shift,temp;

	for( i = 0; i < 8; i++)
	{
		shift = ((7-i)*4);
		temp = ((value>>shift) & (0x0000000F));
		if (temp > 9)
		{
			temp = temp + 7;
        }
		seq[i] = temp + 48;	
	}
	seq[8] = 0;
	return UARTSendData(seq, FALSE);
}

// Get string length by finding null terminating char
Int32 GetStringLen(Uint8* seq)
{
	Int32 i = 0;
	while ((seq[i] != 0) && (i<MAXSTRLEN)){ i++;}
	if (i == MAXSTRLEN)
		return -1;
	else
		return i;
}

// Receive data from UART 
Uint32 UARTRecvData(Uint32 numBytes, Uint8* seq)
{
	Uint32 i, status = 0;
	Uint32 timerStatus = 1;
	
	for(i=0;i<numBytes;i++) {
		
        // Enable timer one time
		TIMER0Start();
		do{
			status = (UART0->LSR)&(0x01);
			timerStatus = TIMER0Status();
		} while (!status && timerStatus);

		if(timerStatus == 0)
			return E_TIMEOUT;
		
		// Receive byte 
		seq[i] = (UART0->RBR) & 0xFF;
        
        // Check status for errors
        if( ( (UART0->LSR)&(0x1C) ) != 0 )
            return E_FAIL;

	}
	return E_PASS;
}

// More complex send / receive functions
Uint32 UARTCheckSequence(Uint8* seq, Bool includeNull)
{
    Int32 i, numBytes;
    Uint32  status = 0,timerStatus = 1;
    
    numBytes = includeNull?(GetStringLen(seq)+1):(GetStringLen(seq));
    
    for(i=0;i<numBytes;i++) {
        /* Enable Timer one time */
        TIMER0Start();
        do{
            status = (UART0->LSR)&(0x01);
            timerStatus = TIMER0Status();
        } while (!status && timerStatus);

        if(timerStatus == 0)
            return E_TIMEOUT;

        if( ( (UART0->RBR)&0xFF) != seq[i] )
            return E_FAIL;
    }
    return E_PASS;
}

Uint32 UARTGetHexData(Uint32 numBytes, Uint32* data) {
    
    Uint32 i,j;
    Uint32 temp[8];
    Uint32 timerStatus = 1, status = 0;
    Uint32 numLongs, numAsciiChar, shift;
    
    if(numBytes == 2) {
        numLongs = 1;
        numAsciiChar = 4;
        shift = 12;
    } else {
        numLongs = numBytes/4;
        numAsciiChar = 8;
        shift = 28;
    }

    for(i=0;i<numLongs;i++) {
        data[i] = 0;
        for(j=0;j<numAsciiChar;j++) {
            /* Enable Timer one time */
            TIMER0Start();
            do{
                status = (UART0->LSR)&(0x01);
                timerStatus = TIMER0Status();
            } while (!status && timerStatus);

            if(timerStatus == 0)
                return E_TIMEOUT;

            /* Converting ascii to Hex*/
            temp[j] = ((UART0->RBR)&0xFF)-48;
            if(temp[j] > 22)/* To support lower case a,b,c,d,e,f*/
               temp[j] = temp[j]-39;
            else if(temp[j]>9)/* To support Upper case A,B,C,D,E,F*/
               temp[j] = temp[j]-7;

            /* Checking for bit 1,2,3,4 for reception Error *///1E->1C
            if( ( (UART0->LSR)&(0x1C) ) != 0)
                return E_FAIL;

            data[i] |= (temp[j]<<(shift-(j*4)));
        }
    }
    return E_PASS;
}

Uint32 UARTGetCMD(Uint32* bootCmd)
{
    if(UARTCheckSequence((Uint8*)"    CMD", TRUE) != E_PASS)
    {
        return E_FAIL;
    }

    if(UARTGetHexData(4,bootCmd) != E_PASS)
    {
        return E_FAIL;
    }

    return E_PASS;
}

Uint32 UARTGetHeaderAndData(UART_ACK_HEADER* ackHeader)
{
    Uint32 error = E_FAIL;

    // Send ACK command
    error = UARTCheckSequence((Uint8*)"    ACK", TRUE);
    if(error != E_PASS)
    {
        return E_FAIL;
    }

    // Get the ACK header elements
    error =  UARTGetHexData( 4, (Uint32 *) &(ackHeader->magicNum)     );
    error |= UARTGetHexData( 4, (Uint32 *) &(ackHeader->appStartAddr) );
    error |= UARTGetHexData( 4, (Uint32 *) &(ackHeader->srecByteCnt)  );
    error |= UARTCheckSequence((Uint8*)"0000", FALSE);
    if(error != E_PASS)
    {
        return E_FAIL;
    }

    // Verify that the S-record's size is appropriate
    if((ackHeader->srecByteCnt == 0) || (ackHeader->srecByteCnt > MAX_IMAGE_SIZE))
    {
        UARTSendData((Uint8*)" BADCNT", TRUE);/*trailing /0 will come along*/
        return E_FAIL;
    }

    // Verify application start address is in RAM (lower 16bit of appStartAddr also used 
    // to hold UBL entry point if this header describes a UBL)
    if( (ackHeader->appStartAddr < RAM_START_ADDR) || (ackHeader->appStartAddr > RAM_END_ADDR) )
    {
        UARTSendData((Uint8*)"BADADDR", TRUE);/*trailing /0 will come along*/
        return E_FAIL;
    }

    // Allocate storage for S-record
    ackHeader->srecAddr = (Uint32) ubl_alloc_mem(ackHeader->srecByteCnt);

    // Send BEGIN command
    if ( UARTSendData((Uint8*)"  BEGIN", TRUE) != E_PASS )
        return E_FAIL;

    // Receive the data over UART
    if ( UARTRecvData(ackHeader->srecByteCnt, (Uint8*)(ackHeader->srecAddr)) != E_PASS )
    {
        UARTSendData((Uint8*)"\r\nUART Receive Error\r\n", FALSE);
        return E_FAIL;
    }

    // Return DONE when all data arrives
    if ( UARTSendData((Uint8*)"   DONE", TRUE) != E_PASS )
        return E_FAIL;

    // Now decode the S-record
    if ( SRecDecode(    (Uint8 *)(ackHeader->srecAddr),
                        ackHeader->srecByteCnt,
                        &(ackHeader->binAddr),
                        &(ackHeader->binByteCnt) ) != E_PASS )
    {
        UARTSendData((Uint8*)"\r\nS-record Decode Failed.\r\n", FALSE);
        return E_FAIL;
    }

    if ( UARTSendData((Uint8*)"   DONE", TRUE) != E_PASS )
        return E_FAIL;


    return E_PASS;
}   
