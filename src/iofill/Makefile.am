bin_PROGRAMS = iofill

iofill_SOURCES = iof_init.c \
                 iof_iotest.c \
                 iof_main.c \
                 iof_output.c

iofill_LDADD = -L../../lib -liobench
iofill_LDFLAGS = -all-static
iofill_INCLUDES = iofill.h
iofill_CPPFLAGS = -I$(top_builddir)/lib
iofill_CFLAGS = -O3
