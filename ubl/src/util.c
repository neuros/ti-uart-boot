/* --------------------------------------------------------------------------
    FILE        : util.c 				                             	 	        
    PURPOSE     : Utility and misc. file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Jan-22-2007  
 
    HISTORY
 	     v1.00 completion 							 						      
 	          Daniel Allred - Jan-22-2007                                              
 ----------------------------------------------------------------------------- */
#include "ubl.h"
#include "dm644x.h"
#include "uart.h"
#include "util.h"

// Memory allocation stuff
static VUint32 current_mem_loc;
VUint8 DDRMem[0] __attribute__((section (".ddrram")));;

// DDR Memory allocation routines (for storing large data)
Uint32 get_current_mem_loc()
{
	return current_mem_loc;
}

void set_current_mem_loc(Uint32 value)
{
	current_mem_loc = value;
}

void *ubl_alloc_mem (Uint32 size)
{
	void *cPtr;
	Uint32 size_temp;

	// Ensure word boundaries
	size_temp = ((size + 4) >> 2 ) << 2;
	
	if((current_mem_loc + size_temp) > (MAX_IMAGE_SIZE))
	{
		return 0;
	}

	cPtr = (void *) (DDRMem + current_mem_loc);
	current_mem_loc += size_temp;

	return cPtr;
}


// S-record Decode stuff
Uint32 GetHexData(Uint8 *src, Uint32 numBytes, Uint8* seq)
{
	Uint32 i;
	Uint8 temp[2];
	Uint32 checksum = 0;

	for(i=0;i<numBytes;i++)
	{
		/* Converting ascii to Hex*/
		temp[0] = *src++ - 48;
		temp[1] = *src++ - 48;
		if(temp[0] > 22)/* To support lower case a,b,c,d,e,f*/
		   temp[0] = temp[0]-39;
		else if(temp[0]>9)/* To support Upper case A,B,C,D,E,F*/
		   temp[0] = temp[0]-7;
		if(temp[1] > 22)/* To support lower case a,b,c,d,e,f*/
		   temp[1] = temp[1]-39;
		else if(temp[1]>9)/* To support Upper case A,B,C,D,E,F*/
		   temp[1] = temp[1]-7;
	
		seq[i] = (temp[0] << 4) | temp[1];
		checksum += seq[i];

	}
	return checksum;
}

Uint32 GetHexAddr(Uint8 *src, Uint32* addr)
{
	Uint32 checksum;
	checksum = GetHexData(src,4,(Uint8 *)addr);
	*addr = ENDIAN_SWAP(*addr);
	return checksum;
}

Uint32 SRecDecode(Uint8 *srecAddr, Uint32 srecByteCnt, Uint32 *binAddr, Uint32 *binByteCnt)
{
	VUint32		index;
	Uint8		cnt;
	Uint32		dstAddr;
	Uint32		totalCnt;
	Uint32		checksum;

	totalCnt = 0;
	index = 0;

	// Look for S0 (starting/title) record
	if((srecAddr[index] != 'S')||(srecAddr[index+1] != '0'))
	{
		return E_FAIL;
	}
	
	index += 2;
	// Read the length of S0 record & Ignore that many bytes
	GetHexData (&srecAddr[index], 1, &cnt);
	index += (cnt * 2) + 2;     // Ignore the count and checksum

	while(index <= srecByteCnt)
	{
	    // Check for S3 (data) record
		if(( srecAddr[index] == 'S' )&&( srecAddr[index+1] == '3'))
		{
			index += 2;
		
			checksum = GetHexData (srecAddr+index, 1, &cnt);
			index += 2;

			checksum += GetHexAddr(srecAddr+index, &dstAddr);
			index += 8;
			
			// Eliminate the Address 4 Bytes and checksum
			cnt -=5;	

			// Read cnt bytes to the to the determined destination address
			checksum += GetHexData (srecAddr+index, cnt, (Uint8 *)dstAddr);
			index += (cnt << 1);
			dstAddr += cnt;
			totalCnt += cnt;

			/* Skip Checksum */
			GetHexData (srecAddr+index, 1, &cnt);
			index += 2;
			if ( cnt != (Uint8)((~checksum) & 0xFF) )
			{
				UARTSendData((Uint8 *) "S-record decode checksum failure.\r\n", FALSE);
				return E_FAIL;
			}
		}
		
		// Check for S7 (terminating) record
		if ( ( srecAddr[index] == 'S' ) && ( srecAddr[index + 1] == '7' ) )
		{
			index += 2;

			GetHexData (&srecAddr[index], 1, (Uint8 *)&cnt);
			index += 2;

			if (cnt == 5)
			{
				GetHexAddr (&srecAddr[index], binAddr);
				index +=8;

				// Skip over the checksum
				index += 2;
				// We are at the end of the S-record, so we can exit while loop
				break;
			}
			else 
			{
				return E_FAIL;
			}
		}

		// Ignore all spaces and returns
		if( ( srecAddr[index] == ' '  ) ||
			( srecAddr[index] == '\n' ) ||
			( srecAddr[index] == ','  ) ||
			( srecAddr[index] == '\r' ) )
		{
			/**** Do nothing ****/
			index ++;
		}

	}	//end while
	*binByteCnt = totalCnt;
	return E_PASS;
}


// Simple wait loop - comes in handy.
void waitloop(Uint32 loopcnt)
{
	for (loopcnt = 0; loopcnt<1000; loopcnt++)
	{
		asm("   NOP");
	}
}
