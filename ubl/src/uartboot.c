/* --------------------------------------------------------------------------
    FILE        : uartboot.c 				                             	 	        
    PURPOSE     : UART boot and interface file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Jan-22-2007  
 
    HISTORY
 	     v1.00 completion 							 						      
 	          Daniel Allred - Jan-22-2007                                              
 ----------------------------------------------------------------------------- */

#include "ubl.h"
#include "uart.h"
#include "util.h"
#include "dm644x.h"

#ifdef UBL_NOR
#include "nor.h"
#endif

#ifdef UBL_NAND
#include "nand.h"
#endif

extern Uint32 gEntryPoint;

#ifdef UBL_NAND
extern NAND_INFO gNandInfo;
#endif

#ifdef UBL_NOR
extern NOR_INFO gNorInfo;
#endif

void UART_Boot(void) {

#ifdef UBL_NAND
	NAND_BOOT          nandBoot;
#endif
#ifdef UBL_NOR
	NOR_BOOT           norBoot;
	Uint32             blkAddress, blkSize, baseAddress;
#endif	
	UART_ACK_HEADER    ackHeader;
	Uint32             dataAddr = 0,dataByteCnt=0;
	Uint32             bootCmd;

UART_tryAgain:
	// Initialize UART and TIMER
	//UARTInit();
	waitloop(100);
	UARTSendData((Uint8 *) "Starting UART Boot...\r\n", FALSE);

	// UBL Sends 'BOOTPSP/0'
	if (UARTSendData((Uint8*)"BOOTPSP", TRUE) != E_PASS)
		goto UART_tryAgain;

	// Get the BOOT command
	if(UARTGetCMD(&bootCmd) != E_PASS)
		goto UART_tryAgain;

	switch(bootCmd)
	{
		// Only used for doing simple boot of UART
		case UBL_MAGIC_SAFE:
		{
			if ( UARTSendData((Uint8*)"SENDAPP", TRUE) != E_PASS)
				goto UART_tryAgain;
			if (UARTGetHeaderAndData(&ackHeader) != E_PASS)
			{
				goto UART_tryAgain;
			}
			gEntryPoint = ackHeader.binAddr;
			break;
		}
#ifdef UBL_NOR
		// Flash the UBL to start of NOR and put header and s-record app in NOR
		case UBL_MAGIC_NOR_SREC_BURN:
		case UBL_MAGIC_NOR_BIN_BURN:
		{
			if ( UARTSendData((Uint8*)"SENDUBL", TRUE) != E_PASS)
				goto UART_tryAgain;
			// Get the UBL into binary form
			if (UARTGetHeaderAndData(&ackHeader) != E_PASS)
			{
				goto UART_tryAgain;
			}

			// Initialize the NOR Flash
			NOR_Init();

			// Erasing the Flash
			NOR_Erase(gNorInfo.flashBase, ackHeader.binByteCnt);

			// Write binary UBL to NOR flash
			NOR_WriteBytes(gNorInfo.flashBase, ackHeader.binByteCnt, ackHeader.binAddr);

			// Send SENDAPP command
			if ( UARTSendData((Uint8*)"SENDAPP", TRUE) != E_PASS)
				goto UART_tryAgain;

			// Get the application header and data
			if (UARTGetHeaderAndData(&ackHeader) != E_PASS)
			{
				goto UART_tryAgain;
			}

			// Determine whether to use binary or srec
			if (bootCmd == UBL_MAGIC_NOR_BIN_BURN) 
			{
				dataByteCnt = ackHeader.binByteCnt;
				dataAddr = ackHeader.binAddr;
			}
			else if (bootCmd == UBL_MAGIC_NOR_SREC_BURN) 
			{
				dataByteCnt = ackHeader.srecByteCnt;
				dataAddr = ackHeader.srecAddr;				
			}
	
			// Erase the NOR flash where header and data will go
			DiscoverBlockInfo( (gNorInfo.flashBase + UBL_IMAGE_SIZE), &blkSize, &blkAddress );
	        baseAddress =  (blkAddress + blkSize);
			NOR_Erase( baseAddress, (dataByteCnt + sizeof(norBoot)) );

			norBoot.magicNum = ackHeader.magicNum;			//MagicFlag for Application (binary or safe)
			norBoot.appSize = dataByteCnt;					//Bytes of application (either srec or binary)
			norBoot.entryPoint = ackHeader.appStartAddr;	//Value from ACK header
			norBoot.ldAddress = ackHeader.binAddr;			//Should be same as AppStartAddr

			// Write the NOR_BOOT header to the flash
			NOR_WriteBytes( baseAddress, sizeof(norBoot), (Uint32) &norBoot);

			// Write the application data to the flash
			NOR_WriteBytes((baseAddress + sizeof(norBoot)), dataByteCnt, dataAddr);

			// Set the entry point for code execution to the newly copied binary UBL
			gEntryPoint = gNorInfo.flashBase;
			break;
		}	
		case UBL_MAGIC_NOR_RESTORE:
		{
			// Get the APP (should be u-boot) into binary form
			if ( UARTSendData((Uint8*)"SENDAPP", TRUE) != E_PASS)
				goto UART_tryAgain;
			
			if ( UARTGetHeaderAndData(&ackHeader) != E_PASS )
				goto UART_tryAgain;

			// Initialize the NOR Flash
			if ( NOR_Init() != E_PASS )
				goto UART_tryAgain;
			
			// Erasing the Flash
			if ( NOR_Erase(gNorInfo.flashBase, ackHeader.binByteCnt) != E_PASS )
			    goto UART_tryAgain;

			// Write the actual application to the flash
			if ( NOR_WriteBytes(gNorInfo.flashBase, ackHeader.binByteCnt, ackHeader.binAddr) != E_PASS )
			    goto UART_tryAgain;

			// Set the entry point for code execution
			gEntryPoint = gNorInfo.flashBase;
			break;
		}
		case UBL_MAGIC_NOR_GLOBAL_ERASE:
		{
			// Initialize the NOR Flash
			NOR_Init();

			// Erasing the Flash
			if (NOR_GlobalErase() != E_PASS)
			{
				UARTSendData((Uint8 *)"\r\nErase failed.\r\n", FALSE);
			}
			else
			{
				UARTSendData((Uint8 *)"\r\nErase completed successfully.\r\n", FALSE);
			}

			// Set the entry point for code execution
			// Go to reset in this case since no code was downloaded
			gEntryPoint = 0x0; 

			break;
		}
#endif
#ifdef UBL_NAND
		case UBL_MAGIC_NAND_SREC_BURN:
		case UBL_MAGIC_NAND_BIN_BURN:
		{
			if ( UARTSendData((Uint8*)"SENDUBL", TRUE) != E_PASS)
				goto UART_tryAgain;

			// Get the UBL into binary form
			if (UARTGetHeaderAndData(&ackHeader) != E_PASS)
			{
				goto UART_tryAgain;
			}

			// Initialize the NAND Flash
			if (NAND_Init() != E_PASS)
			{
			    UARTSendData("NAND_Init() failed!", FALSE);
			    goto UART_tryAgain;
			}   

			// Get magicNum
			nandBoot.magicNum = ackHeader.magicNum;

			// Get entrypoint for UBL
			nandBoot.entryPoint = (Uint32) (0x0000FFFF & ackHeader.appStartAddr);

			// The UBL image is 14kBytes plus do some rounding
			nandBoot.numPage = 0;
			while ( (nandBoot.numPage * gNandInfo.bytesPerPage) < (0x3800))
			{
				nandBoot.numPage++;
			}

			// The page is always page 0 for the UBL header, so we use page 1 for data
			nandBoot.page = 1;
			// The block is always block is always 1 (to start with) for the UBL header
			nandBoot.block = START_UBL_BLOCK_NUM;
			// This field doesn't matter for the UBL header
			nandBoot.ldAddress = 0;
	
			// Write header to page 0 of block 1(or up to block 5)
			// Write the UBL to the same block, starting at page 1 (since blocks are 16k)
			UARTSendData((Uint8 *) "Writing UBL to NAND flash\r\n", FALSE);
			if (NAND_WriteHeaderAndData(&nandBoot, (Uint8 *) ackHeader.binAddr) != E_PASS)
			    goto UART_tryAgain;

			// Send SENDAPP command
			if (UARTSendData((Uint8*)"SENDAPP", TRUE) != E_PASS)
				goto UART_tryAgain;

			// Get the application header and data
			if (UARTGetHeaderAndData(&ackHeader) != E_PASS)
			{
				goto UART_tryAgain;
			}

			// Set parameters depending on whether binary or srecord
			if (bootCmd == UBL_MAGIC_NAND_SREC_BURN)
			{
				dataByteCnt = ackHeader.srecByteCnt;
				dataAddr = ackHeader.srecAddr;
			}
			else if (bootCmd == UBL_MAGIC_NAND_BIN_BURN)
			{
				dataByteCnt = ackHeader.binByteCnt;
				dataAddr = ackHeader.binAddr;
			}

			// Rely on the host applciation to send over the right magic number (safe or bin)
			nandBoot.magicNum = ackHeader.magicNum;

			// Use the entrypoint received in ACK header
			nandBoot.entryPoint = ackHeader.appStartAddr;

			// The APP s-record image is dataByteCnt bytes plus do some rounding
			nandBoot.numPage = 0;
			while ( (nandBoot.numPage * gNandInfo.bytesPerPage) < dataByteCnt )
			{
				nandBoot.numPage++;
			}

			// The page is always page 0 for the header, so we use page 1 for data
			nandBoot.page = 1;

			// The block is always 6 (to start with) for the APP header
			nandBoot.block = START_APP_BLOCK_NUM;

			// The load address is only important if this is a binary image
			nandBoot.ldAddress = ackHeader.binAddr;

			// Nand Burn of application data
			UARTSendData((Uint8 *) "Writing APP to NAND flash\r\n", FALSE);
			if (NAND_WriteHeaderAndData(&nandBoot, (Uint8 *) dataAddr) != E_PASS)
			    goto UART_tryAgain;

			// Set the entry point to nowhere, since there isn't an appropriate binary image to run */
			gEntryPoint = 0x0;
			break;
		}	/* end case UBL_MAGIC_NAND_SREC_BURN */
		case UBL_MAGIC_NAND_GLOBAL_ERASE:
		{
			// Initialize the NAND Flash
			if (NAND_Init() != E_PASS)
			{
			    UARTSendData("NAND_Init() failed!", FALSE);
			    goto UART_tryAgain;
			}

			// Unprotect the NAND Flash
			NAND_UnProtectBlocks(1,gNandInfo.numBlocks - 1);

			// Erase all the pages of the device
			if (NAND_EraseBlocks(1,(gNandInfo.numBlocks - 1)) != E_PASS)
			{
				UARTSendData((Uint8 *)"Erase failed.\r\n", FALSE);
				goto UART_tryAgain;
			}
			else
			{
				UARTSendData((Uint8 *)"Erase completed successfully.\r\n", FALSE);
			}
			            
			// Protect the device
			NAND_ProtectBlocks();

			// Set the entry point for code execution
			// Go to reset in this case since no code was downloaded 
			gEntryPoint = 0x0; 
			break;
		}
#endif
		default:
		{
			// Simple UART Boot
			if ( UARTSendData((Uint8*)"SENDAPP", TRUE) != E_PASS)
				goto UART_tryAgain;

			if (UARTGetHeaderAndData(&ackHeader) != E_PASS)
			{
				goto UART_tryAgain;
			}
			gEntryPoint = ackHeader.binAddr;
			break;
		}
	}	/* end switch statement */
}

