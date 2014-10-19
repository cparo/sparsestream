OBJS = deltacheck.o sparsecheck.o sparseencode.o sparsedecode.o sparsefilter.o
CC = gcc
CFLAGS = -Wall -c -std=c99 -O3 -funroll-loops
LFLAGS = -Wall

all: deltacheck sparsecheck sparsedecode sparseencode sparsefilter

deltacheck: deltacheck.o
	$(CC) $(LFLAGS) deltacheck.o -o deltacheck

sparsecheck: sparsecheck.o
	$(CC) $(LFLAGS) sparsecheck.o -o sparsecheck

sparsedecode: sparsedecode.o
	$(CC) $(LFLAGS) sparsedecode.o -o sparsedecode

sparseencode: sparseencode.o
	$(CC) $(LFLAGS) sparseencode.o -o sparseencode

sparsefilter: sparsefilter.o
	$(CC) $(LFLAGS) sparsefilter.o -o sparsefilter

deltacheck.o : deltacheck.c sparsestream.h
	$(CC) $(CFLAGS) deltacheck.c

sparsecheck.o : sparsecheck.c sparseencode.h
	$(CC) $(CFLAGS) sparsecheck.c

sparsedecode.o : sparsedecode.c sparsestream.h
	$(CC) $(CFLAGS) sparsedecode.c

sparseencode.o : sparseencode.c sparseencode.h
	$(CC) $(CFLAGS) sparseencode.c

sparsefilter.o : sparsefilter.c sparseencode.h
	$(CC) $(CFLAGS) sparsefilter.c

clean:
	rm -f *.o *~ deltacheck sparsecheck sparseencode sparsedecode sparsefilter
