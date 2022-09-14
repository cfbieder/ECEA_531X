INCLUDE_DIRS =
LIB_DIRS =
CC=gcc

CDEFS=
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS) -Wall
LIBS=

HFILES=
CFILES= seqgenex0.c seqgen.c seqgen2.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	seqgen  clock_times

clean:
	-rm -f *.o *.d
	-rm -f seqgen


seqgen: seqgen.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lrt


depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
