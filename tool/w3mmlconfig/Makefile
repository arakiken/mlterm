
prefix		= /usr/local
BIN_DIR		= $(prefix)/bin
LIB_DIR		= $(prefix)/libexec/w3mmlconfig

distdir		= ./distfiles
DISTNAME	= w3mmlconfig

TARGETS		= w3mmlconfig
LIB_TARGETS	= mlconfig.cgi
DATA_TARGETS	= mlconfig-data.pl
DATA		= mlconfig-data section/* key/*
DISTFILES	= Makefile w3mmlconfig.in mlconfig.cgi.in	\
		mk_mlconfig-data.pl mlconfig-data section key

PERL		= /usr/local/bin/perl

MKDIR		= mkdir -p
INSTALL		= install -c
INSTALL_SCRIPT	= $(INSTALL) -m 755
INSTALL_DATA	= $(INSTALL) -m 644

.SUFFIXES: .in

all: $(TARGETS) $(LIB_TARGETS) $(DATA_TARGETS)

.in:
	sed -e 's;@CGI_BIN@;$(LIB_DIR);'	\
	    -e 's;@PERL@;$(PERL);'	\
	    $< > $@

mlconfig-data.pl: $(DATA)
	$(PERL) mk_mlconfig-data.pl mlconfig-data > $@

dist:
	@rm -rf $(distdir)/$(DISTNAME)
	$(MKDIR) $(distdir)/$(DISTNAME)
	cp -rp $(DISTFILES) $(distdir)/$(DISTNAME)
	(cd $(distdir); tar -cf - $(DISTNAME) | GZIP='' gzip)	\
		> $(distdir)/$(DISTNAME).tar.gz
	@rm -rf $(distdir)/$(DISTNAME)

install: all
	-$(MKDIR) $(DESTDIR)$(BIN_DIR)
	-$(MKDIR) $(DESTDIR)$(LIB_DIR)
	for file in $(TARGETS);     \
	do      \
		$(INSTALL_SCRIPT) $$file $(DESTDIR)$(BIN_DIR);  \
	done
	for file in $(LIB_TARGETS);     \
	do      \
		$(INSTALL_SCRIPT) $$file $(DESTDIR)$(LIB_DIR);  \
	done
	for file in $(DATA_TARGETS);     \
	do      \
		$(INSTALL_DATA) $$file $(DESTDIR)$(LIB_DIR);  \
	done
