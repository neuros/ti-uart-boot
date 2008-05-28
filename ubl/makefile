all:
		$(MAKE) -C src FLASH=nand
		$(MAKE) -C src FLASH=nor
DM6441:
		$(MAKE) -C src FLASH=nand DEVICE=DM6441
		$(MAKE) -C src FLASH=nor DEVICE=DM6441
		
DM6441_LV:
		$(MAKE) -C src FLASH=nand DEVICE=DM6441_LV
		$(MAKE) -C src FLASH=nor DEVICE=DM6441_LV
		
clean:
		$(MAKE) -C src FLASH=nand clean
		$(MAKE) -C src FLASH=nor clean
%::
		$(MAKE) -C src FLASH=nand $@
		$(MAKE) -C src FLASH=nor $@
		
