all .DEFAULT:
	cd kiklib/autoconf ; $(MAKE) $@
	cd mkf/autoconf ; $(MAKE) $@
	cd autoconf ; $(MAKE) $@

install:
	cd kiklib/autoconf ; $(MAKE) install-la
	cd mkf/autoconf ; $(MAKE) install-la
	cd autoconf ; $(MAKE) install

distclean:
	-cd kiklib/autoconf ; $(MAKE) $@
	-cd mkf/autoconf ; $(MAKE) $@
	-cd autoconf ; $(MAKE) $@
	rm -f config.log config.cache config.status install-sh mkf/kiklib
