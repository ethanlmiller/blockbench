Basic instructions / requirements for building the tools.

Currently, the tools only build on Linux; more platforms coming soon.  In particular,
we should have builds on Cygwin working shortly.

Requirements:

* C compiler (tpyically gcc)
* libtool

Instructions:

1. Obtain a copy of sg3_utils, and build it.  It should be in the same directory
   level as iobenchlib, and must be called sg3_utils (no version suffix).
2. Go into the iobenchlib directory, and run make.
3. Go into the iofill directory, and run make.
4. Go into the ioload directory, and run make.

At this point, you'll have static binaries for ioload and iofill in their respective
directories.  These binaries can be copied to most Linux versions, and should run without
concerns over the library versions on the systems: they've got all of their code
statically linked in.
