/* --------------------------------------------------------------------------
    FILE        : dm644x.c 				                             	 	        
    PURPOSE     : Platform initialization file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DESC:       : System init routines for the user boot loader
    
    HISTORY
 	     v1.00 completion (with support for DM6441 and DM6441_LV)							 						      
 	          Daniel Allred - Jan-22-2007
         v1.11 - DJA - 07-Mar-2007
 	          Made all DDR and PLL global values as const variables.
 	          Added code to set 1394 MDSTAT, as per workaround from U-boot
 	          and code to init all power domains to known state.
         v1.13 - DJA - 04-Jun-2007
	          Fix for DDRPHYCR misread from user's guide.
         v1.14 - DJA - 13-Sep-2007
               Fix for DSP power domain initialzation - reflects U-boot fix
 ----------------------------------------------------------------------------- */

#include "dm644x.h"
#include "ubl.h"
#include "uart.h"

extern VUint32 DDRMem[0];
extern BootMode gBootMode;
extern Uint32 gEntryPoint;

// ---------------------------------------------------------------------------
// Global Memory Timing and PLL Settings
// ---------------------------------------------------------------------------
#if defined(DM6441_LV)
    // For Micron MT47H64M16BT-37E @ 135 MHz   
    const Uint8 DDR_NM = 0;
    const Uint8 DDR_CL = 3;
    const Uint8 DDR_IBANK = 3;
    const Uint8 DDR_PAGESIZE = 2;
    const Uint8 DDR_T_RFC = 17;
    const Uint8 DDR_T_RP = 2;
    const Uint8 DDR_T_RCD = 2;
    const Uint8 DDR_T_WR = 2;
    const Uint8 DDR_T_RAS = 5;
    const Uint8 DDR_T_RC = 7;
    const Uint8 DDR_T_RRD = 1;
    const Uint8 DDR_T_WTR = 1;
    const Uint8 DDR_T_XSNR = 18;
    const Uint8 DDR_T_XSRD = 199;
    const Uint8 DDR_T_RTP = 1;
    const Uint8 DDR_T_CKE = 2;
    const Uint16 DDR_RR = 1264;
    const Uint8 DDR_Board_Delay = 3;
    const Uint8 DDR_READ_Latency = 5;
    
    const Uint32 PLL2_Mult = 20;
    const Uint32 PLL2_Div1 = 10;
    const Uint32 PLL2_Div2 = 2;
#else
    // For Micron MT47H64M16BT-37E @ 162 MHz
    const Uint8 DDR_NM = 0;
    const Uint8 DDR_CL = 3;
    const Uint8 DDR_IBANK = 3;
    const Uint8 DDR_PAGESIZE = 2;
    const Uint8 DDR_T_RFC = 20;
    const Uint8 DDR_T_RP = 2;
    const Uint8 DDR_T_RCD = 2;
    const Uint8 DDR_T_WR = 2;
    const Uint8 DDR_T_RAS = 6;
    const Uint8 DDR_T_RC = 8;
    const Uint8 DDR_T_RRD = 2;
    const Uint8 DDR_T_WTR = 1;
    const Uint8 DDR_T_XSNR = 22;
    const Uint8 DDR_T_XSRD = 199;
    const Uint8 DDR_T_RTP = 1;
    const Uint8 DDR_T_CKE = 2;
    const Uint16 DDR_RR = 1053;
    const Uint8 DDR_Board_Delay = 3;
    const Uint8 DDR_READ_Latency = 5; 
    
    const Uint32 PLL2_Mult = 24;
    const Uint32 PLL2_Div1 = 12;
    const Uint32 PLL2_Div2 = 2;
    
    // 567 verison - only use this with older silicon/EVMs
    // For Micron MT47H64M16BT-37E @ 189 MHz
    /*const Uint8 DDR_NM = 0;
    const Uint8 DDR_CL = 4;
    const Uint8 DDR_IBANK = 3;
    const Uint8 DDR_PAGESIZE = 2;
    const Uint8 DDR_T_RFC = 24;
    const Uint8 DDR_T_RP = 2;
    const Uint8 DDR_T_RCD = 2;
    const Uint8 DDR_T_WR = 2;
    const Uint8 DDR_T_RAS = 7;
    const Uint8 DDR_T_RC = 10;
    const Uint8 DDR_T_RRD = 2;
    const Uint8 DDR_T_WTR = 1;
    const Uint8 DDR_T_XSNR = 25;
    const Uint8 DDR_T_XSRD = 199;
    const Uint8 DDR_T_RTP = 1;
    const Uint8 DDR_T_CKE = 2;
    const Uint16 DDR_RR = 1477;
    const Uint8 DDR_Board_Delay = 2;
    const Uint8 DDR_READ_Latency = 5;
    
    const Uint32 PLL2_Mult = 14;
    const Uint32 PLL2_Div1 = 7;
    const Uint32 PLL2_Div2 = 1;*/
#endif

// Set CPU clocks
#if defined(DM6441_LV)
    const Uint32 PLL1_Mult = 15;  // DSP=405 MHz
#elif defined(DM6441)
    const Uint32 PLL1_Mult = 19;  // DSP=513 MHz  
#else
    const Uint32 PLL1_Mult = 22;  // DSP=594 MHz
    // 567 version - only use this with older silicon/EVMs
    // Uint32 PLL1_Mult = 21;
#endif
    
// ---------------------------------------------------------
// End of global PLL and Memory settings
// ---------------------------------------------------------
    

void LPSCTransition(Uint8 module, Uint8 state)
{
    while (PSC->PTSTAT & 0x00000001);
	PSC->MDCTL[module] = ((PSC->MDCTL[module]) & (0xFFFFFFE0)) | (state);
	PSC->PTCMD |= 0x00000001;
	while ((PSC->PTSTAT) & 0x00000001);
	while (((PSC->MDSTAT[module]) & 0x1F) != state);	
}

//
// Initialize IOB01 board
//
#define REG(x)	(*((VUint32 *)x))
void IOB01_Init(void)
{
	SYSTEM->PINMUX[0] = 0x80000000;	// PINMUX0 - enable EMAC, AEAW[4:0]=0
	SYSTEM->PINMUX[1] = 0x00010081;	// PINMUX1 - Enable CLK0, I2C, UART0
	REG(0x1c67038) = 0xffffbc5f;	// Output GIOs = 45, 41, 40, 39, 37
	REG(0x1c6703c) = 0x000002a0;	// Set NAND_WE=1, SD_CAP#=1, HD_CAP#=1
}
	
void DM644xInit()
{
	// Mask all interrupts
	AINTC->INTCTL = 0x0;
	AINTC->EINT0 = 0x0;
	AINTC->EINT1 = 0x0;		
	
	/******************* IOB01 Hardware Setup ****************/
	IOB01_Init();

	/******************* UART Setup **************************/
	UARTInit();
	
	/******************* System PLL Setup ********************/
	PLL1Init();
	
	/******************* DDR PLL Setup ***********************/	
	PLL2Init();

	/******************* DDR2 Timing Setup *******************/
	DDR2Init();
			
	/******************* AEMIF Setup *************************/
	// Handled in NOR or NAND init
	//AEMIFInit();
    
    /******************* IRQ Vector Table Setup **************/
    IVTInit();
}

void PSCInit()
{
    Uint32 i;

	//***************************************
	// Do always on power domain transitions
	//***************************************
	while ((PSC->PTSTAT) & 0x00000001);

    for( i = LPSC_VPSS_MAST ; i < LPSC_1394 ; i++ )
        PSC->MDCTL[i] |= 0x03; // Enable

	// Do this for enabling a WDT initiated reset this is a workaround
	// for a chip bug.  Not required under normal situations 
	// Copied from U-boot boards/DaVinci/platform.S and convereted to C
	//      LDR R6, P1394
	//      MOV R10, #0x0	
	//      STR R10, [R6]        
    PSC->MDCTL[LPSC_1394] = 0x0;
    
    for( i = LPSC_USB ; i < LPSC_DSP ; i++ )
        PSC->MDCTL[i] |= 0x03; // Enable

    // Set EMURSTIE to 1 on the following
    PSC->MDCTL[LPSC_VPSS_SLV]   |= 0x0203;
    PSC->MDCTL[LPSC_EMAC0]      |= 0x0203;
    PSC->MDCTL[LPSC_EMAC1]      |= 0x0203;
    PSC->MDCTL[LPSC_MDIO]       |= 0x0203;
    PSC->MDCTL[LPSC_USB]        |= 0x0203;
    PSC->MDCTL[LPSC_ATA]        |= 0x0203;
    PSC->MDCTL[LPSC_VLYNQ]      |= 0x0203;
    PSC->MDCTL[LPSC_HPI]        |= 0x0203;
    PSC->MDCTL[LPSC_DDR2]       |= 0x0203;
    PSC->MDCTL[LPSC_AEMIF]      |= 0x0203;
    PSC->MDCTL[LPSC_MMCSD]      |= 0x0203;
    PSC->MDCTL[LPSC_MEMSTK]     |= 0x0203;
    PSC->MDCTL[LPSC_ASP]        |= 0x0203;
    PSC->MDCTL[LPSC_GPIO]       |= 0x0203;
    PSC->MDCTL[LPSC_IMCOP]      |= 0x0203;

    // Do Always-On Power Domain Transitions
    PSC->PTCMD |= 0x00000001;
    while ((PSC->PTSTAT) & 0x00000001);
	
	// Clear EMURSTIE to 0 on the following
	PSC->MDCTL[LPSC_VPSS_SLV]   &= 0x0003;
    PSC->MDCTL[LPSC_EMAC0]      &= 0x0003;
    PSC->MDCTL[LPSC_EMAC1]      &= 0x0003;
    PSC->MDCTL[LPSC_MDIO]       &= 0x0003;
    PSC->MDCTL[LPSC_USB]        &= 0x0003;
    PSC->MDCTL[LPSC_ATA]        &= 0x0003;
    PSC->MDCTL[LPSC_VLYNQ]      &= 0x0003;
    PSC->MDCTL[LPSC_HPI]        &= 0x0003;
    PSC->MDCTL[LPSC_DDR2]       &= 0x0003;
    PSC->MDCTL[LPSC_AEMIF]      &= 0x0003;
    PSC->MDCTL[LPSC_MMCSD]      &= 0x0003;
    PSC->MDCTL[LPSC_MEMSTK]     &= 0x0003;
    PSC->MDCTL[LPSC_ASP]        &= 0x0003;
    PSC->MDCTL[LPSC_GPIO]       &= 0x0003;
    PSC->MDCTL[LPSC_IMCOP]      &= 0x0003;    

	//***************************************
	// Do DSP power domain transition
	//***************************************
	if ((PSC->PDSTAT1 & 0x1F) == 0)
	{
		// Set PSC force mode
		PSC->GBLCTL |= 0x1;		// May not be necessary 
		
		// Set NEXT bit to on
		PSC->PDCTL1 |= 0x1;
		
		// Clear external power indicator
		PSC->PDCTL1 &= ~(0x100);

		// Put the C64x+ Core into SwRstDisable
		PSC->MDCTL[LPSC_DSP] = (PSC->MDCTL[LPSC_DSP] & (~0x00000001F)) | 0x0;
		
		// Start power domain transition
		PSC->PTCMD |= 0x00000002;

		// Wait for external power control pending to assert
		while ( !((PSC->EPCPR) & (0x00000002)) );
        
		// Short the two power domain's voltage rails
		SYSTEM->CHP_SHRTSW = 0x1;

		// Clear the external power control bit
		PSC->EPCCR = 0x00000002;

		// Set external power good indicator
        PSC->PDCTL1 |= 0x0100;
		
		// Wait for domain transition to complete
		while ((PSC->PTSTAT) & (0x00000002));

		// Enable DSP
		PSC->MDCTL[LPSC_DSP] = (PSC->MDCTL[LPSC_DSP] & (~0x00000001F)) | 0x3;
		// Hold DSP in reset on next power up
		PSC->MDCTL[LPSC_DSP] = (PSC->MDCTL[LPSC_DSP] & (~0x100));
		// Enable IMCOP
		PSC->MDCTL[LPSC_IMCOP] = (PSC->MDCTL[LPSC_IMCOP] & (~0x00000001F)) | 0x3;

		// Start power domain transition
		PSC->PTCMD |= 0x00000002;

		// Wait for domain transition to complete
		while ((PSC->PTSTAT) & (0x00000002));
		
		// Wait until DSP local reset is asserted
		while ((PSC->MDSTAT[LPSC_DSP]) & (0x00000100));
		
		// Clear PSC force mode
		PSC->GBLCTL &= ~(0x00000001);
	}
}

void PLL2Init()
{	
        
	// Set PLL2 clock input to external osc. 
	PLL2->PLLCTL &= (~0x00000100);
	
	// Clear PLLENSRC bit and clear PLLEN bit for Bypass mode 
	PLL2->PLLCTL &= (~0x00000021);

	// Wait for PLLEN mux to switch 
	waitloop(32*(PLL1_Mult/2));
	
	PLL2->PLLCTL &= (~0x00000008);          // Put PLL into reset
	PLL2->PLLCTL |= (0x00000010);           // Disable the PLL
	PLL2->PLLCTL &= (~0x00000002);          // Power-up the PLL
	PLL2->PLLCTL &= (~0x00000010);          // Enable the PLL
	
	// Set PLL multipliers and divisors 
	PLL2->PLLM      = PLL2_Mult-1;     // 27  Mhz * (23+1) = 648 MHz 
	PLL2->PLLDIV1   = PLL2_Div1-1;     // 648 MHz / (11+1) = 54  MHz
	PLL2->PLLDIV2   = PLL2_Div2-1;     // 648 MHz / (1+1 ) = 324 MHz (the PHY DDR rate)
		
	PLL2->PLLDIV2 |= (0x00008000);          // Enable DDR divider	
	PLL2->PLLDIV1 |= (0x00008000);          // Enable VPBE divider	
	PLL2->PLLCMD |= 0x00000001;             // Tell PLL to do phase alignment
	while ((PLL2->PLLSTAT) & 0x1);          // Wait until done
	waitloop(256*(PLL1_Mult/2));

	PLL2->PLLCTL |= (0x00000008);           // Take PLL out of reset	
	waitloop(2000*(PLL1_Mult/2));                       // Wait for locking
	
	PLL2->PLLCTL |= (0x00000001);           // Switch out of bypass mode
}

void DDR2Init()
{
	Int32 tempVTP;
	
	// Set the DDR2 to enable
	LPSCTransition(LPSC_DDR2, PSC_ENABLE);
		
	// For Micron MT47H64M16BT-37E @ 162 MHz
	// Setup the read latency (CAS Latency + 3 = 6 (but write 6-1=5))
	DDR->DDRPHYCR = (0x50006400) | DDR_READ_Latency;
	// Set TIMUNLOCK bit, CAS LAtency 3, 8 banks, 1024-word page size 
	//DDR->SDBCR = 0x00138632;
	DDR->SDBCR = 0x00138000 |
	             (DDR_NM << 14)   |
	             (DDR_CL << 9)    |
	             (DDR_IBANK << 4) |
	             (DDR_PAGESIZE <<0);
	
	// Program timing registers 
	//DDR->SDTIMR = 0x28923211;
	DDR->SDTIMR = (DDR_T_RFC << 25) |              
                  (DDR_T_RP << 22)  |
                  (DDR_T_RCD << 19) |
                  (DDR_T_WR << 16)  |
                  (DDR_T_RAS << 11) |
                  (DDR_T_RC << 6)   |
                  (DDR_T_RRD << 3)  |
                  (DDR_T_WTR << 0);
                  
	//DDR->SDTIMR2 = 0x0016C722;
	DDR->SDTIMR2 = (DDR_T_XSNR << 16) |
                   (DDR_T_XSRD << 8)  |
                   (DDR_T_RTP << 5)   |
                   (DDR_T_CKE << 0);
    
    
    // Clear the TIMUNLOCK bit 
	DDR->SDBCR &= (~0x00008000);
	
	// Set the refresh rate
	DDR->SDRCR = DDR_RR;
	
	// Dummy write/read to apply timing settings
	DDRMem[0] = DDR_TEST_PATTERN;
	if (DDRMem[0] == DDR_TEST_PATTERN)
          UARTSendInt(DDRMem[0]);
	
	// Set the DDR2 to syncreset
	LPSCTransition(LPSC_DDR2, PSC_SYNCRESET);

	// Set the DDR2 to enable
	LPSCTransition(LPSC_DDR2, PSC_ENABLE);
			 
	/***************** DDR2 VTP Calibration ****************/
	DDR->VTPIOCR = 0x201F;        // Clear calibration start bit
	DDR->VTPIOCR = 0xA01F;        // Set calibration start bit 
	
	waitloop(11*33);              // Wait for calibration to complete 
		 
	SYSTEM->DDRVTPER = 0x1;       // DDRVTPR Enable register
	
	tempVTP = 0x3FF & DDRVTPR;    // Read calibration data
	
	// Write calibration data to VTP Control register 
	DDR->VTPIOCR = ((DDR->VTPIOCR) & 0xFFFFFC00) | tempVTP; 
	
	// Clear calibration enable bit
	DDR->VTPIOCR = (DDR->VTPIOCR) & (~0x00002000);
	
	// DDRVTPR Enable register - disable DDRVTPR access 
	SYSTEM->DDRVTPER = 0x0;
}

void PLL1Init()
{
    //594 version
    //Uint32 PLL1_Mult = 22;
        
    //567 version - use with 189 MHZ DDR
    //Uint32 PLL1_Mult = 21;
            
	// Set PLL2 clock input to internal osc. 
	PLL1->PLLCTL &= (~0x00000100);	
	
	// Clear PLLENSRC bit and clear PLLEN bit for Bypass mode 
	PLL1->PLLCTL &= (~0x00000021);

	// Wait for PLLEN mux to switch 
	waitloop(32);
	
	PLL1->PLLCTL &= (~0x00000008);     // Put PLL into reset
	PLL1->PLLCTL |= (0x00000010);      // Disable the PLL
	PLL1->PLLCTL &= (~0x00000002);     // Power-up the PLL
	PLL1->PLLCTL &= (~0x00000010);     // Enable the PLL
	
	// Set PLL multipliers and divisors 
	PLL1->PLLM = PLL1_Mult - 1;        // 27Mhz * (21+1) = 594 MHz 
			
	PLL1->PLLCMD |= 0x00000001;		// Tell PLL to do phase alignment
	while ((PLL1->PLLSTAT) & 0x1);	// Wait until done
	
	waitloop(256);
	PLL1->PLLCTL |= (0x00000008);		//	Take PLL out of reset	
	waitloop(2000);				// Wait for locking
	
	PLL1->PLLCTL |= (0x00000001);		// Switch out of bypass mode
}
/* 
void AEMIFInit()
{     
   AEMIF->AWCCR     = 0x00000000;
   AEMIF->AB1CR     = 0x3FFFFFFD;
   AEMIF->AB2CR     = 0x3FFFFFFD;
   AEMIF->AB3CR     = 0x3FFFFFFD;
   AEMIF->AB4CR     = 0x3FFFFFFD;
   AEMIF->NANDFCR   = 0x00000000;
}*/
 
void UARTInit()
{		
    // The DM644x pin muxing registers must be set for UART0 use. 
	SYSTEM->PINMUX[1] |=  1;
	
	// Set DLAB bit - allows setting of clock divisors 
	UART0->LCR |= 0x80;
	
	//divider = 27000000/(16*115200) = 14.64 => 15 = 0x0F (2% error is OK)
	UART0->DLL = 0x0F;
	UART0->DLH = 0x00; 

    // Enable, clear and reset FIFOs	
	UART0->FCR = 0x07;
	
	// Disable autoflow control 
	UART0->MCR = 0x00;
	
	// Enable receiver, transmitter, st to run. 
	UART0->PWREMU_MGNT |= 0x6001;
	//UART0->PWREMU_MGNT |= 0x8001;

	// Set word length to 8 bits, clear DLAB bit 
	UART0->LCR = 0x03;

	// Disable the timer 
	TIMER0->TCR = 0x00000000;
	// Set to 64-bit GP Timer mode, enable TIMER12 & TIMER34
	TIMER0->TGCR = 0x00000003;

	// Reset timers to zero 
	TIMER0->TIM34 = 0x00000000;
	TIMER0->TIM12 = 0x00000000;
	
	// Set timer period (5 second timeout = (27000000 * 5) cycles = 0x080BEFC0) 
	TIMER0->PRD34 = 0x00000000;
	TIMER0->PRD12 = 0x080BEFC0;
}

void IVTInit()
{
	VUint32 *ivect;
    extern Uint32 __IVT;
    
	if (gBootMode == NON_SECURE_NOR)
	{
		ivect = &(__IVT);
		*ivect++ = 0xEAFFFFFE;  /* Reset @ 0x00*/
	}
	else
	{
		ivect = &(__IVT) + 4;
	}
	*ivect++ = 0xEAFFFFFE;  /* Undefined Address @ 0x04 */
	*ivect++ = 0xEAFFFFFE;  /* Software Interrupt @0x08 */
	*ivect++ = 0xEAFFFFFE;  /* Pre-Fetch Abort @ 0x0C */
	*ivect++ = 0xEAFFFFFE;  /* Data Abort @ 0x10 */
	*ivect++ = 0xEAFFFFFE;  /* Reserved @ 0x14 */
	*ivect++ = 0xEAFFFFFE;  /* IRQ @ 0x18 */
	*ivect   = 0xEAFFFFFE;	/* FIQ @ 0x1C */
}

