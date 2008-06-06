/*
 *  Desc: Linker command file ubl application program.
 *  Auth: Daniel Allred
 *  Vers: 1.0 on 08-Nov-2006
 *        1.1 on 01-Feb-2007 - Added fake entry point fixed at 0x20. 
 *							   Made the selfcopy section end at 0x100
 *                             to match what the SDI CCS tool assumes.
 *							   Also reordered sections.
 *		  1.13 on 04-Jun-2007 - Revised entry point to correspond with new 
 *                             CCS tools, DV_NANDWriter and DV_NORWriter.
 *        1.14 on 13-Sep-2007
 */

ENTRY(boot)
SECTIONS {
	. = 0x02000000;
	__CS2start = .;
	__NORFlash = .;
	__NANDFlash = .;
	
	__selfcopysrc = .;
	__IVT = 0x00008000;
	__selfcopydest = 0x00008020;
	
	/* Self copy code is in first 256-32=224 bytes of image */
	.selfcopy : AT (0x0)
	{
		*(.selfcopy)		
		. = 0xE0;
	}

	/* The entry point address is fixed at 0x100 */
	. = 0x100;
	.boot		: AT ( LOADADDR(.selfcopy) + SIZEOF(.selfcopy))
	{
		*(.boot)
		. = ALIGN(4);
	}
	.text		: AT ( LOADADDR(.boot) + SIZEOF(.boot) )
	{
		*(.text)
		. = ALIGN(4);
	}
	
	. += 0x8000;
	
	.rodata		: AT ( LOADADDR(.text) + SIZEOF(.text) )
	{
		*(.rodata*)
		*(.rodata)
		. = ALIGN(4);
	}	

	.data		: AT ( LOADADDR(.rodata) + SIZEOF(.rodata) )
	{
		*(.data)
		. = ALIGN(4);
	}
			
	__selfcopydestend = __selfcopydest + SIZEOF(.selfcopy) +
						SIZEOF(.boot) + SIZEOF(.text) + 
						SIZEOF(.data) + SIZEOF(.rodata);
	
	.bss		:
	{
		*(.bss) *(COMMON)
		. = ALIGN(4);
	}
	
	__topstack = 0xC000 - 0x4;
	
	/* 128 MB of DDR2 */
   	. = 0x80000000;
	.ddrram	:
	{
   		*(.ddrram)
	}

	/* 128 MB of DDR2 */
   	. = 0x88000000;
	.ddrram2	 :
	{
		*(.ddrram2)
    }
    
}

