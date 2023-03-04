VPATH = .

CC = gcc
PROG = mlcc
CFLAGS = -O2 -Wall -g

OBJECTS = data.o main.o comm.o \
 data_entry_string.o data_entry_bool.o data_entry_numeric.o \
 data_entry_color.o data_entry_radio.o

all: $(PROG)

$(PROG): $(OBJECTS)
	$(CC) -o $(PROG) $(OBJECTS)

.SUFFIXES: .c.o

comm.o: comm.h
data.o: comm.h data.h
main.o: comm.h data.h

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJECTS) $(PROG) core core.* $(PROG).core

mostlyclean:
	rm -f $(OBJECTS) core core.* $(PROG).core
