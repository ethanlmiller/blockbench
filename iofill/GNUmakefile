#
# Makefile for iofill
#
#

SRCS = iof_init iof_iotest iof_main iof_output
INCLUDES = iofill.h
OPTFLAGS = -O3
TARGET = iofill

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
	$(RM) -f $(LOBJS) $(OBJS) $(TARGET)

%.o: %.c $(INCLUDES)
	libtool compile $(CC) $(CPPFLAGS) $(CFLAGS) -c $*.c
