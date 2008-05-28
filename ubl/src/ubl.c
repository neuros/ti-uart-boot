/* --------------------------------------------------------------------------
    FILE        : ubl.c 				                             	 	        
    PURPOSE     : Main User Boot Loader file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Dec-18-2006  
 
    HISTORY
        v1.0 completion 							 						      
 	        Daniel Allred - Jan-22-2006
 	    v1.1
 	        DJA - Feb-1-2007 - Added dummy entry point make NAND UBL work
 	                           with the CCS flashing utility from SDI. This
 	                           fakeentry is located at 0x20 at runtime and
 	                           simply redirects to the true entry point, boot().
 	                                                                      
 ----------------------------------------------------------------------------- */

#include "ubl.h"
#include "dm644x.h"
#include "uart.h"
#include "util.h"

#ifdef UBL_NOR
#include "nor.h"
#endif

#ifdef UBL_NAND
#include "nand.h"
#endif

Uint32 gEntryPoint;
BootMode gBootMode;

void selfcopy()
{
	// Self copy setup 
	extern Uint32 __selfcopysrc, __selfcopydest, __selfcopydestend;
		
	//Enable ITCM
	asm(" MRC p15, 0, r0, c9, c1, 1");
	asm(" MOV r0, #0x1");
	asm(" MCR p15, 0, r0, c9, c1, 1");
		
	//Enable DTCM
	asm(" MRC p15, 0, r0, c9, c1, 0");
	asm(" MOV r0, #0x8000");
	asm(" ORR r0, r0, #0x1");
	asm(" MCR p15, 0, r0, c9, c1, 0");
	
	VUint32* src = &(__selfcopysrc);
	VUint32* dest = &(__selfcopydest);
	VUint32* destend = &(__selfcopydestend);

	// Copy the words
	while (dest < destend)
	{
		*dest = *src;
		dest++;
		src++;
	}
	
	//Jump to the normal entry point 
	boot();	
}

void fake_entry()
{
    boot();
}

void boot()
{   
	asm(" MRS	r0, cpsr");
	asm(" BIC	r0, r0, #0x1F");    // CLEAR MODES
	asm(" ORR	r0, r0, #0x13");    // SET SUPERVISOR mode
	asm(" ORR   r0, r0, #0xC0");    // Disable FIQ and IRQ
	asm(" MSR	cpsr, r0");       
	
	// Set the IVT to low memory, leave MMU & caches disabled 
	asm(" MRC	p15, 0, r1, c1, c0, 0");
 	asm(" BIC	r0,r0,#0x00002000");
	asm(" MCR	p15, 0, r1, c1, c0, 0");
	
	// Stack setup (__topstack symbol defined in linker script)
	extern Uint32 __topstack;
	register Uint32* stackpointer asm ("sp");	
	stackpointer = &(__topstack);
	
    // Call to main code
    main();
    
    // Jump to entry point
	APPEntry = (void*) gEntryPoint;
    (*APPEntry)();	
}

Int32 main(void)
{
	// Read boot mode 
	gBootMode = (BootMode) ( ( (SYSTEM->BOOTCFG) & 0xC0) >> 6);
	
	if (gBootMode != NON_SECURE_UART)
    {
        // Turn on the UART since it's not on by default in NOR 
        // or NAND boot modes
        waitloop(1000);
        LPSCTransition(LPSC_UART0,PSC_ENABLE);
    }
    else
    {
        // Wait until the RBL is done using the UART. 
        while((UART0->LSR & 0x40) == 0 );
    }

	// Platform Initialization
	DM644xInit();

	// Set RAM pointer to beginning of RAM space
	set_current_mem_loc(0);

	// Send some information to host
    UARTSendData((Uint8 *) "TI UBL Version: ",FALSE);
    UARTSendData((Uint8 *) UBL_VERSION_STRING,FALSE);
    UARTSendData((Uint8 *) ", Flash type: ", FALSE);
    UARTSendData((Uint8 *) UBL_FLASH_TYPE, FALSE);
	UARTSendData((Uint8 *) "\r\nBooting PSP Boot Loader\r\nPSPBootMode = ",FALSE);
	
	/* Select Boot Mode */
	switch(gBootMode)
	{
#ifdef UBL_NAND
		case NON_SECURE_NAND:
		{
			//Report Bootmode to host
			UARTSendData((Uint8 *) "NAND\r\n",FALSE);

			// copy binary or S-record of application from NAND to DDRAM, and decode if needed
			if (NAND_Copy() != E_PASS)
			{
				UARTSendData((Uint8 *) "NAND Boot failed.\r\n", FALSE);
				goto UARTBOOT;
			}
			else
			{
				UARTSendData((Uint8 *) "NAND Boot success.\r\n", FALSE);
			}
			break;
		}
#endif		
#ifdef UBL_NOR
		case NON_SECURE_NOR:
		{
			//Report Bootmode to host
			UARTSendData((Uint8 *) "NOR \r\n", FALSE);

			// Copy binary or S-record of application from NOR to DDRAM, then decode
			if (NOR_Copy() != E_PASS)
			{
				UARTSendData((Uint8 *) "NOR Boot failed.\r\n", FALSE);
				goto UARTBOOT;
			}
			else
			{
				UARTSendData((Uint8 *) "NOR Boot success.\r\n", FALSE);
			}
			break;
		}
#endif		
		case NON_SECURE_UART:
		{
			//Report Bootmode to host
			UARTSendData((Uint8 *) "UART\r\n", FALSE);
            goto UARTBOOT;
			break;
		}
		default:
		{
UARTBOOT:	UART_Boot();
			break;
		}
	}
		
	UARTSendData((Uint8*)"   DONE", TRUE);
	
	waitloop(10000);

	// Disabling UART timeout timer
    while((UART0->LSR & 0x40) == 0 );
	TIMER0->TCR = 0x00000000;

	return E_PASS;    
}


