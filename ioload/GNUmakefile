#
# Makefile for ioload
#
#

SRCS = iol_init iol_iotest iol_main iol_output
INCLUDES = ioload.h
OPTFLAGS = -O3
TARGET = ioload

OBJS = $(addsuffix .o, $(SRCS))
LOBJS = $(addsuffix .lo, $(SRCS))
CPP = g++
CC = gcc
CFLAGS = $(OPTFLAGS) -static
CPPFLAGS = -I../sg3_utils/include -I../iobenchlib
LDFLAGS = -all-static
RM = /bin/rm
LIBS = ../sg3_utils/lib/libsgutils2.la ../iobenchlib/libiob.la -lrt -lpthread

$(TARGET): $(OBJS)
	libtool link gcc $(LDFLAGS) $(LOBJS) $(LIBS) -o $(TARGET)

clean:
	$(RM) -f $(OBJS) $(TARGET)

%.o: %.c $(INCLUDES)
	libtool compile $(CC) $(CPPFLAGS) $(CFLAGS) -c $*.c
