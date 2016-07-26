VPATH = .
CC = gcc
CFLAGS = -O2 -Wall -g
INSTALL = /usr/bin/install -c

OBJ = main.o

PROG = mlclient

all: $(PROG)

$(PROG): $(OBJ)
	$(CC) -o $(PROG) $(OBJ) $(LIBS)
	cp $(PROG) $(PROG)x

.SUFFIXES: .c.o

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ) $(PROG)* *core
