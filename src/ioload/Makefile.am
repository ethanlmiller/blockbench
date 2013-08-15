bin_PROGRAMS = ioload

ioload_SOURCES = iol_init.c \
                 iol_iotest.c \
                 iol_main.c \
                 iol_output.c

ioload_LDADD = -L../../lib -liobench
ioload_LDFLAGS = -all-static
ioload_INCLUDES = ioload.h
ioload_CPPFLAGS = -I$(top_builddir)/lib
ioload_CFLAGS = -O3
