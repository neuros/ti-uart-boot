#############################################################
# Makefile for TI UBL project.                              #
#   Generates the binary UBL file can be used as the 		#
#	secondary bootloader for the DM644x in NOR, NAND, and	#
#	UART boot modes.										#
#############################################################
# Auth: Daniel Allred (d-allred@ti.com)
# Vers: v1.1 on 1-Feb-2007
# 
# modified for neuros
# Terry Qiu tqiu@neuros.com.cn
# v1.2 on 28-May-2008

CROSSCOMPILE=arm-linux-
CC=$(CROSSCOMPILE)gcc
OBJCOPY=$(CROSSCOMPILE)objcopy
OBJDUMP=$(CROSSCOMPILE)objdump
INCLUDEDIR=../include

CFLAGS:=-c -Os -Wall -I$(INCLUDEDIR)
ifeq ($(FLASH),nand)
	CFLAGS+= -DUBL_NAND
endif
ifeq ($(FLASH),nor)
	CFLAGS+= -DUBL_NOR
endif

ifeq ($(DEVICE),DM6441)
	CFLAGS+= -DDM6441
endif
ifeq ($(DEVICE),DM6441_LV)
	CFLAGS+= -DDM6441_LV
endif

LDFLAGS=-Wl,-T$(LINKERSCRIPT) -nostdlib 
OBJCOPYFLAGS = -R .ddrram -R .ddrram2 --gap-fill 0xFF --pad-to 0x3800 -S

SOURCES=ubl.c dm644x.c util.c uart.c uartboot.c nor.c norboot.c nand.c nandboot.c
OBJECTS:=$(patsubst %.c,%_$(FLASH).o,$(wildcard *.c))
EXECUTABLE:=ubl_davinci_$(FLASH)
BINARY:=../$(EXECUTABLE).bin
START:=../$(EXECUTABLE)_start_addr.txt
LINKERSCRIPT=ubl_davinci.lds

all: $(BINARY) $(START)

.PHONY : clean objclean
clean:
		-rm -f -v $(OBJECTS) $(BINARY) $(EXECUTABLE) $(START)
		
objclean:
		-rm -f -v $(OBJECTS)
		
$(START): $(EXECUTABLE)
		@$(OBJDUMP) -t $< | grep -E '[0-9a-f]{8}*[0-9a-f]{8} boot' | sed  's/^\([0-9a-f]\{4\}\)\([0-9a-f]\{4\}\).*/\2/' > $@

$(BINARY): $(EXECUTABLE)
		$(OBJCOPY) $(OBJCOPYFLAGS) -O binary $< $@
		@echo -n "The entry point of the binary is 0x"
		@$(OBJDUMP) -t $< | grep -E '[0-9a-f]{8}*[0-9a-f]{8} boot' | sed  's/^\([0-9a-f]\{4\}\)\([0-9a-f]\{4\}\).*/\2/'

$(EXECUTABLE): $(OBJECTS)
		$(CC) $(LDFLAGS) $(OBJECTS) -o $@
		
%_$(FLASH).o : %.c $(wildcard *.h)
		$(CC) $(CFLAGS) $< -o $@

vpath %.h $(INCLUDEDIR)
		

