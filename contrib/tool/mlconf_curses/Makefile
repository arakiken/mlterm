PERL = /usr/bin/perl

TARGETS = mlconf_curses
SRC = mlconf_curses.pl

DESTDIR = /
BINDIR = /usr/bin


MKDIR           = mkdir -p
INSTALL         = install -c
INSTALL_SCRIPT  = $(INSTALL) -m 755

all: $(TARGETS)

$(TARGETS): $(SRC)
	$(PERL) -pe 's#/usr/bin/perl#$(PERL)#' $< > $@

install: all
	-$(MKDIR) $(DESTDIR)$(BINDIR)
	for file in $(TARGETS);     \
	do      \
		$(INSTALL_SCRIPT) $$file $(DESTDIR)$(BINDIR);  \
        done

