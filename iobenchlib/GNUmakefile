#
# Makefile for iobenchlib
#
#

SRCS = iob_lib iob_pstdata iob_scsi iob_stats iob_vmdata
INCLUDES = iobenchlib.h iobenchlib_ext.h
OPTFLAGS = -O3
LIB = libiob.a

OBJS = $(addsuffix .o, $(SRCS))
CPP = g++
CC = gcc
CFLAGS = -msse4 $(OPTFLAGS)
CPPFLAGS = -I../sg3_utils/include
LDFLAGS = -static
LIBS = -lrt -lpthread ../sg3_utils/lib/libsgutils2.la



$(LIB): $(OBJS)
	ar r $(LIB) $(OBJS)

%.o: %.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $*.c
