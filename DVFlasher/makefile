#############################################################
# Makefile for TI UBL project.                              #
#   Generates the Host executable file that is used in      #
#   in conjunction with the UBL to send and boot over UART. #
#   This executable is dependent on the UBL binary having   #
#   been generated first.                                   #
#############################################################
# Auth: Daniel Allred
# Vers: v1.00 on 22-Jan-2006
#       v1.10 on 01-Feb-2006
VER=1_10

MONOCOMPILE=gmcs
DOTNETCOMPILE=csc

SOURCES=DVFlasher.cs CRC32.cs 
EXECUTABLE=DVFlasher_$(VER).exe 
NORUBLIMAGE=../ubl/ubl_davinci_nor.bin
NORUBLSTARTADDR=$(shell cat ../ubl/ubl_davinci_nor_start_addr.txt)
NANDUBLIMAGE=../ubl/ubl_davinci_nand.bin
NANDUBLSTARTADDR=$(shell cat ../ubl/ubl_davinci_nand_start_addr.txt)

.PHONY : all clean mono dotnet

all: $(EXECUTABLE)

DM6441: $(EXECUTABLE)

DM6441_LV: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES) $(NORUBLIMAGE) $(NANDUBLIMAGE)
		@echo "Adjusting binary entry point addresses in the host source code."
		sed -i 's/\([:space:]*myCmdParams\.NORUBLExecAddr = 0x\)\([0-9a-fA-F]\{4\}\)/\1$(NORUBLSTARTADDR)/' ./DVFlasher.cs
		sed -i 's/\([:space:]*myCmdParams\.NANDUBLExecAddr = 0x\)\([0-9a-fA-F]\{4\}\)/\1$(NANDUBLSTARTADDR)/' ./DVFlasher.cs
ifeq ($(shell uname),Linux)
		@$(MONOCOMPILE) -t:exe -resource:$(NORUBLIMAGE) -resource:$(NANDUBLIMAGE) -out:$(EXECUTABLE) $(SOURCES)
else
		@$(DOTNETCOMPILE) /t:exe /resource:$(NORUBLIMAGE) /resource:$(NANDUBLIMAGE) /out:$(EXECUTABLE) $(SOURCES)
endif

clean:
		-rm -f -v $(EXECUTABLE)



