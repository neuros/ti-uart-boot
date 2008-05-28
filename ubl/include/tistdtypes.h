/* --------------------------------------------------------------------------
    FILE        : tistdtypes.h
    PURPOSE     : TI standard types header file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Vignesh LA, Daniel Allred
    DATE	    : Jan-22-2007
 
    HISTORY
        v1.00 completion
 	        Daniel Allred - Jan-22-2007
 ----------------------------------------------------------------------------- */


#ifndef _TISTDTYPES_H_
#define _TISTDTYPES_H_

#ifndef TRUE
typedef int     Bool;
#define TRUE    ((Bool) 1)
#define FALSE   ((Bool) 0)
#endif

#ifndef NULL
#define NULL    0
#endif

/* unsigned quantities */
typedef unsigned int   				Uint32;
typedef unsigned short 				Uint16;
typedef unsigned char   			Uint8;

/* signed quantities */
typedef int             			Int32;
typedef short           			Int16;
typedef char            			Int8;

/* volatile unsigned quantities */
typedef volatile unsigned int		VUint32;
typedef volatile unsigned short 	VUint16;
typedef volatile unsigned char	 	VUint8;

/* volatile signed quantities */
typedef volatile int				VInt32;
typedef volatile short 				VInt16;
typedef volatile char	 			VInt8;


#endif /* _TISTDTYPES_H_ */

