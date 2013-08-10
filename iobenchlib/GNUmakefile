#
# Makefile for iobenchlib
#
#

SRCS = iob_lib iob_pstdata iob_scsi iob_stats iob_vmdata
INCLUDES = iob_lib.h iobenchlibext.h
OPTFLAGS = -O3
LIB = libiob.la

OBJS = $(addsuffix .o, $(SRCS))
LOBJS = $(addsuffix .lo, $(SRCS))
CPP = g++
CC = gcc
CFLAGS = $(OPTFLAGS)
CPPFLAGS = -I../sg3_utils/include
RM = /bin/rm

$(LIB): $(OBJS)
	libtool link gcc -static -o $(LIB) $(LOBJS)

clean:
	$(RM) -f $(OBJS) $(LOBJS) $(LIB)

%.o: %.c $(INCLUDES)
	libtool compile $(CC) -static $(CPPFLAGS) $(CFLAGS) -c $*.c
