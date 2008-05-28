/* --------------------------------------------------------------------------
    FILE        : util.h
    PURPOSE     : Misc. utility header file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Jan-22-2007

    HISTORY
 	    v1.00 completion
 	        Daniel Allred - Jan-22-2007
 ----------------------------------------------------------------------------- */

#ifndef _UTIL_H_
#define _UTIL_H_

#define ENDIAN_SWAP(a) (((a&0xFF)<<24)|((a&0xFF0000)>>8)|((a&0xFF00)<<8)|((a&0xFF000000)>>24))

// Memory Allocation for decoing the s-record
void *ubl_alloc_mem (Uint32 size);
Uint32 get_current_mem_loc(void);
void set_current_mem_loc(Uint32 value);

// Routines to decode the S-record
Uint32 GetHexData(Uint8 *src, Uint32 numBytes, Uint8* seq);
Uint32 GetHexAddr(Uint8 *src, Uint32* addr);
Uint32 SRecDecode(Uint8 *srecAddr, Uint32 srecByteCnt, Uint32 *binAddr, Uint32 *binByteCnt);

// NOP wait loop 
void waitloop(unsigned int loopcnt);

#endif //_UTIL_H_
