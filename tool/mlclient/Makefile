VPATH = .
CC = gcc
CFLAGS = -O2 -Wall -g
INSTALL = /usr/bin/install -c

OBJ = main.o

PROG = mlclient

all: $(PROG) $(PROG)-con

$(PROG): $(OBJ)
	$(CC) -o $(PROG) $(OBJ) $(LIBS)
	cp $(PROG) $(PROG)x

$(PROG)-con: main-con.o
	$(CC) -o $(PROG)-con main-con.o $(LIBS)

.SUFFIXES: .c.o

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ) main-con.o $(PROG)* *core
