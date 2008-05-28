/*
 *  Desc: Linker command file ubl application program.
 *  Auth: Daniel Allred (d-allred@ti.com)
 *  Vers: 1.0 on 08-Nov-2006
 *        1.1 on 01-Feb-2007 - Added fake entry point fixed at 0x20. 
 *							   Made the selfcopy section end at 0x100
 *                             to match what the SDI CCS tool assumes.
 *							   Also reordered sections.
 */

ENTRY(boot)
SECTIONS {
	. = 0x02000000;
	__CS2start = .;
	__NORFlash = .;
	__NANDFlash = .;
	.selfcopy : AT (0x0)
	{
		*(.selfcopy)		
		. = ALIGN(256);
	}
	
	__selfcopysrc = .;	
	__IVT = 0x00008000;
	__selfcopydest = 0x00008020;

	. = 0x20;
	.fakeentry : AT ( LOADADDR(.selfcopy) + SIZEOF(.selfcopy))
	{
		*(.fakeentry)
		. = ALIGN(4);
	}
		
	.text		: AT ( LOADADDR(.fakeentry) + SIZEOF(.fakeentry) )
	{
		*(.text)
		. = ALIGN(4);
	}
	.boot		: AT ( LOADADDR(.text) + SIZEOF(.text))
	{
		*(.boot)
		. = ALIGN(4);
	}
	
	. += 0x8000;
	
	.rodata		: AT ( LOADADDR(.boot) + SIZEOF(.boot) )
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
			
	__selfcopydestend = __selfcopydest + SIZEOF(.fakeentry) +
						SIZEOF(.text) + SIZEOF(.boot) +
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

