CC = gcc
CFLAGS = -Wall -O3 $(CFLAGS_LOCAL)
LIBS = -lmkf -lkik $(LIBS_LOCAL)
INSTALL = install -c

PREFIX = $(HOME)
BINDIR = $(PREFIX)/bin

.SUFFIXES : .c .o

mkf : mkf.o
	$(CC) $(CFLAGS) -o mkf mkf.o $(LIBS)

install : mkf
	strip mkf
	$(INSTALL) -m 755 mkf $(BINDIR)

.c.o :
	$(CC) $(CFLAGS) -c $<

wc :
	find . -name "*.[ch]" -a \! -name "test_*" -o -name "*.pl" -o -name "*.pm" | xargs wc -l

clean :
	rm -f mkf mkf.o *.core
