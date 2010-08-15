CFLAGS = -Wall $(CFLAGS_LOCAL) `pkg-config gtk+-2.0 --cflags`

LIBS = $(LIBS_LOCAL) `pkg-config gtk+-2.0 --libs`

LIBTOOL_LINK = $(LIBTOOL) --mode=link $(CC)
LIBTOOL_INSTALL = $(LIBTOOL) --mode=install $(INSTALL)

OBJ = main.o mlvte.o

gmlterm: $(OBJ)
	gcc $(OBJ) $(CFLAGS) -o gmlterm $(LIBS)

.c.o:
	gcc $(CFLAGS) -g -c $<

clean:
	rm -rf $(OBJ) gmlterm *.core .libs

distclean: clean
	rm -f Makefile

install:
	install -c gmlterm /usr/local/bin
