prefix=/usr/local
bindir=$(prefix)/bin

DESTDIR=
BINDIR=$(DESTDIR)$(bindir)

TARGETS=xconsole.sh google.sh mlsearch.sh mlscp.sh

INSTALL=install -c

install:
	$(INSTALL) -m 755 $(TARGETS) $(BINDIR)/
