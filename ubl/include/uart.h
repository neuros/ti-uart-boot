/* --------------------------------------------------------------------------
    FILE        : uart.h
    PURPOSE     : UART header file
    PROJECT     : DaVinci User Boot-Loader and Flasher
    AUTHOR      : Daniel Allred
    DATE	    : Jan-22-2007  
 
    HISTORY
 	    v1.00 completion
 	        Daniel Allred - Jan-22-2007
 ----------------------------------------------------------------------------- */


#ifndef _UART_H_
#define _UART_H_

#define MAXSTRLEN 256

typedef struct _UART_ACK_HEADER{
    Uint32      magicNum;
    Uint32      appStartAddr;
    Uint32      srecByteCnt;
    Uint32      srecAddr;
    Uint32      binByteCnt;
    Uint32      binAddr;
} UART_ACK_HEADER;

// ------ Function prototypes ------ 
// Main boot function 
void UART_Boot(void);

// Simple send/recv functions
Uint32 UARTSendData(Uint8* seq, Bool includeNull);
Uint32 UARTSendInt(Uint32 value);
Int32 GetStringLen(Uint8* seq);
Uint32 UARTRecvData(Uint32 numBytes, Uint8* seq);

// Complex send/recv functions
Uint32 UARTCheckSequence(Uint8* seq, Bool includeNull);
Uint32 UARTGetHexData(Uint32 numBytes, Uint32* data);
Uint32 UARTGetCMD(Uint32* bootCmd);
Uint32 UARTGetHeaderAndData(UART_ACK_HEADER* ackHeader);

#endif // End _UART_H_
