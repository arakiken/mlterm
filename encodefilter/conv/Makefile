CC = gcc
CFLAGS = -Wall -O3 $(CFLAGS_LOCAL)
LIBS = -lmef -lpobl $(LIBS_LOCAL)
INSTALL = install -c

PREFIX = $(HOME)
BINDIR = $(PREFIX)/bin

.SUFFIXES : .c .o

mef : mef.o
	$(CC) $(CFLAGS) -o mef mef.o $(LIBS)

install : mef
	strip mef
	$(INSTALL) -m 755 mef $(BINDIR)

.c.o :
	$(CC) $(CFLAGS) -c $<

wc :
	find . -name "*.[ch]" -a \! -name "test_*" -o -name "*.pl" -o -name "*.pm" | xargs wc -l

clean :
	rm -f mef mef.o *.core
