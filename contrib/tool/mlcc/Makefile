prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin

CC = gcc
PROG = mlcc
CFLAGS = -g -O2
INSTALL = /usr/bin/install -c


OBJECTS = data.o main.o comm.o \
 data_entry_string.o data_entry_bool.o data_entry_numeric.o \
 data_entry_color.o data_entry_radio.o

all: $(PROG)

$(PROG): $(OBJECTS)
	$(CC) -g -o $(PROG) $(OBJECTS)

.SUFFIXES: .c.o

comm.o: comm.h
data.o: comm.h data.h
main.o: comm.h data.h

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJECTS) $(PROG) core core.* $(PROG).core 

install:
	$(INSTALL) -m 755 $(PROG) $(bindir)

uninstall:
	rm -f $(bindir)/$(PROG)

mostlyclean:
	rm -f $(OBJECTS) core core.* $(PROG).core 

distclean: clean
	rm -f Makefile config.h config.log config.status
