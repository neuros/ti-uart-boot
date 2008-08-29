ifndef PRJROOT
    $(error You must first source the BSP environment: "source neuros-env")
endif

UBL_VERSION=1.144
UBL_VERSION_FILE=ubl.version

all:
%::
		$(MAKE) -C ubl $@
		$(MAKE) -C DVFlasher $@

install: all
	@echo "$(UBL_VERSION)" > $(UBL_VERSION_FILE)
	@install exe/DVFlasher_1_14.exe $(PRJROOT)/images/ > /dev/null
	@install -p ubl/ubl_davinci_nand.bin $(PRJROOT)/images/ > /dev/null
	@install -p $(UBL_VERSION_FILE) $(PRJROOT)/images/ > /dev/null
