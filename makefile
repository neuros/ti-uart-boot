all:
%::
		$(MAKE) -C ubl $@
		$(MAKE) -C DVFlasher $@

		
		