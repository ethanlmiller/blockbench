//
// iob_lib.c
//
//------------------------------------------------------------------------------------------
// Copyright (c) 2013, Pure Storage, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//    * Redistributions of source code must retain the above copyright notice, this list
//      of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright notice, this
//      list of conditions and the following disclaimer in the documentation and/or other
//      materials provided with the distribution.
//    * Neither the name of Pure Storage, Inc. nor the names of its contributors may be
//      used to endorse or promote products derived from this software without specific
//      prior written permission from Pure Storage, Inc.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT ARE
// DISCLAIMED EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD TO BE LEGALLY INVALID.
// IN NO EVENT SHALL PURE STORAGE, INC. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//------------------------------------------------------------------------------------------

#include	"iob_lib.h"
#include	"iobenchlibext.h"

char	*iob_cmdname = " ";

char	*uncdata = NULL;
char	*cdata = NULL;
char	*hcdata = NULL;
char	*hc40data = NULL;
char	*hc20data = NULL;
char	*hc10data = NULL;
char	*vm_data = NULL;
char	*pst_data = NULL;

void	(*usage_func)(void) = NULL;

void
iob_libinit(char *cmdname, void (*ufunc)(void))
{

	iob_cmdname = strdup(cmdname);
	usage_func = ufunc;
	return;
}

void *
iob_malloc(int nbytes)
{
	void	*addr;

	if ((addr = malloc(nbytes)) == NULL) {
		iob_errlog(0, "Error: Can't malloc %d bytes\n", nbytes);
	}
	memset(addr, 0, nbytes);
	return addr;
}

void *
iob_align_shmem(int nbytes)
{
	void	*addr;

	nbytes = (nbytes + 4095) & ~4095;
	if ((addr = mmap(0, nbytes, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0ULL)) == NULL) {
		iob_errlog(0, "Error: Can't mmap %d bytes\n", nbytes);
	}
	memset(addr, 0, nbytes);
	return addr;
}

/*VARARGS*/
void
iob_errlog(int uflag, char *fmt, ...)
{
	char	*timestr;
	time_t	seconds;
	va_list	ap;

	va_start(ap, fmt);
	seconds = time(NULL);
	timestr = ctime(&seconds);
	timestr[24] = '\0';
	fprintf(stderr, "%s: %s ", timestr, iob_cmdname);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (uflag) {
		(*usage_func)();
	}
	exit(1);
#if !defined(__SUNPRO_C)
	return;
#endif
}

/*VARARGS*/
void
iob_infolog(char *fmt, ...)
{
	char	*timestr;
	time_t	seconds;
	va_list	ap;

	va_start(ap, fmt);
	seconds = time(NULL);
	timestr = ctime(&seconds);
	timestr[24] = '\0';
	fprintf(stdout, "%s: %s ", timestr, iob_cmdname);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fflush(stdout);
	return;
}

#if defined(HAVE_GETHRTIME)

uint64_t
iob_gettime(void)
{

	return gethrtime();
}

#elif defined(HAVE_CLOCK_GETTIME)

uint64_t
iob_gettime(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * IOB_NSEC_TO_SEC + ts.tv_nsec;
}

#endif

uint32_t
iob_strtoul(char *value, char **endp, int base)
{

	if (value == NULL) {
		iob_errlog(1, "Error: Optional argument parameter not specified\n");
	}
	return (uint32_t)strtoul(value, endp, base);
}

uint64_t
iob_strtoull(char *value, char **endp, int base)
{

	if (value == NULL) {
		iob_errlog(1, "Error: Optional argument parameter not specified\n");
	}
	return (uint64_t)strtoull(value, endp, base);
}

double
iob_strtod(char *value, char **endp)
{

	if (value == NULL) {
		iob_errlog(1, "Error: Optional argument parameter not specified\n");
	}
	return strtod(value, endp);
}

void
iob_thr_create(void *(*func)(void *), void *arg)
{
	pthread_t	threadid;
	int		ret;

	if ((ret = pthread_create(&threadid, NULL, func, arg)) != 0) {
		iob_errlog(0, "Error: Can't create thread ret %d  errno %d\n", ret, errno);
	}
	return;
}

void
iob_initdata(void)
{
	unsigned short	seedval[3];
	uint8_t		pbyte, xbyte;
	int		i, j, size, count;

	size = IOB_NDATAPAGES * IOB_PAGE;
	pst_data = (char *)iob_pst_data;
	vm_data = (char *)iob_vm_data;
	uncdata = (char *)iob_malloc(size);
	cdata = (char *)iob_malloc(size);
	hcdata = (char *)iob_malloc(size);
	hc40data = (char *)iob_malloc(size);
	hc20data = (char *)iob_malloc(size);
	hc10data = (char *)iob_malloc(size);
	seedval[0] = 1234;
	seedval[1] = 35578;
	seedval[2] = 9568;
	seed48(seedval);
	for (i = 0; i < size; i += sizeof (uint32_t)) {
		*(uint32_t *)(uncdata + i) = (uint32_t)mrand48();
		*(uint32_t *)(cdata + i) = (uint32_t)mrand48() & 0x3c3c3c3c;
	}
	pbyte = 0x5E;
	for (i = 0; i < size; i += IOB_SECTOR) {
		memset(hcdata + i, pbyte, IOB_SECTOR);
		pbyte++;
		count = 24;
		for (j = 0; j < IOB_SECTOR; j += count) {
			xbyte = (uint8_t)(mrand48() & 0xff);
			if (j + count > IOB_SECTOR) {
				count = IOB_SECTOR - j;
			}
			memset(hc10data + i + j, xbyte, count);
		}
		count = 10;
		for (j = 0; j < IOB_SECTOR; j += count) {
			xbyte = (uint8_t)(mrand48() & 0xff);
			if (j + count > IOB_SECTOR) {
				count = IOB_SECTOR - j;
			}
			memset(hc20data + i + j, xbyte, count);
		}
		count = 6;
		for (j = 0; j < IOB_SECTOR; j += count) {
			xbyte = (uint8_t)(mrand48() & 0xff);
			if (j + count > IOB_SECTOR) {
				count = IOB_SECTOR - j;
			}
			memset(hc40data + i + j, xbyte, count);
		}
	}
	return;
}

void
iob_iodata(char *bp, uint32_t size, int dtype, uint64_t dedupblks)
{
	uint64_t	hdr;
	char		*dbuf;
	int		off, doff, soff, nbytes;

	if (dtype == IOB_DT_PATTERN1) {
		for (off = 0; off < size; off += IOB_SECTOR) {
			memset(bp + off, lrand48() % 256, IOB_SECTOR);
		}
	} else if (dtype == IOB_DT_ONES) {
		memset(bp, 0xff, size);
	} else if (dtype == IOB_DT_ZEROS) {
		memset(bp, 0, size);
	} else {
		if (dtype == IOB_DT_UNCOMPRESSIBLE) {
			dbuf = uncdata;
		} else if (dtype == IOB_DT_PST) {
			dbuf = pst_data;
		} else if (dtype == IOB_DT_VM) {
			dbuf = vm_data;
		} else if (dtype == IOB_DT_COMPRESSIBLE) {
			dbuf = cdata;
		} else if (dtype == IOB_DT_H40COMPRESSIBLE) {
			dbuf = hc40data;
		} else if (dtype == IOB_DT_H20COMPRESSIBLE) {
			dbuf = hc20data;
		} else if (dtype == IOB_DT_H10COMPRESSIBLE) {
			dbuf = hc10data;
		} else {
			dbuf = hcdata;
		}
		for (off = 0; off < size; off += IOB_PAGE) {
			doff = lrand48() % IOB_NDATAPAGES * IOB_PAGE;
			nbytes = IOB_PAGE;
			if (nbytes > size - off) {
				nbytes = size - off;
			}
			memcpy(bp + off, dbuf + doff, nbytes);
			hdr = ((uint64_t)dtype << 56) + IOB_RAND64() % dedupblks;
			for (soff = 0; soff < nbytes; soff += IOB_SECTOR) {
				*(uint64_t *)(bp + off + soff) = hdr;
			}
		}
	}
	return;
}

void
iob_dtargparse(int *dtype, char *value)
{
	int	dt = 0;

	if (value == NULL) {
		iob_errlog(1, "Error: Optional argument parameter not specified\n");
	} else if (strcmp(value, "c") == 0) {
		dt = IOB_DT_COMPRESSIBLE;
	} else if (strcmp(value, "u") == 0) {
		dt = IOB_DT_UNCOMPRESSIBLE;
	} else if (strcmp(value, "p") == 0) {
		dt = IOB_DT_PATTERN1;
	} else if (strcmp(value, "z") == 0) {
		dt = IOB_DT_ZEROS;
	} else if (strcmp(value, "o") == 0) {
		dt = IOB_DT_ONES;
	} else if (strcmp(value, "h40") == 0) {
		dt = IOB_DT_H40COMPRESSIBLE;
	} else if (strcmp(value, "h20") == 0) {
		dt = IOB_DT_H20COMPRESSIBLE;
	} else if (strcmp(value, "h10") == 0) {
		dt = IOB_DT_H10COMPRESSIBLE;
	} else if (strcmp(value, "h") == 0 || strcmp(value, "h01") == 0 || strcmp(value, "h1") == 0) {
		dt = IOB_DT_HCOMPRESSIBLE;
	} else if (strcmp(value, "e") == 0) {
		dt = IOB_DT_PST;
	} else if (strcmp(value, "v") == 0) {
		dt = IOB_DT_VM;
	} else {
		iob_errlog(1, "Error: Invalid data type '%s' specified\n", value);
	}
	*dtype = dt;
	return;
}

char *
iob_dttostr(int dtype)
{

	if (dtype == IOB_DT_COMPRESSIBLE) {
		return "Compressible";
	} else if (dtype == IOB_DT_UNCOMPRESSIBLE) {
		return "Uncompressible";
	} else if (dtype == IOB_DT_HCOMPRESSIBLE) {
		return "Highly Compressible to 1%";
	} else if (dtype == IOB_DT_H40COMPRESSIBLE) {
		return "Highly Compressible to 40%";
	} else if (dtype == IOB_DT_H20COMPRESSIBLE) {
		return "Highly Compressible to 20%";
	} else if (dtype == IOB_DT_H10COMPRESSIBLE) {
		return "Highly Compressible to 10%";
	} else if (dtype == IOB_DT_PST) {
		return "Email Data";
	} else if (dtype == IOB_DT_VM) {
		return "Virtual Machine Data";
	} else if (dtype == IOB_DT_ZEROS) {
		return "Zeros";
	} else if (dtype == IOB_DT_ONES) {
		return "Ones";
	} else if (dtype == IOB_DT_PATTERN1) {
		return "One Byte Patterns";
	}
	return "Unknown";
}

char *
iob_dttochr(int dtype)
{

	if (dtype == IOB_DT_COMPRESSIBLE) {
		return "C";
	} else if (dtype == IOB_DT_UNCOMPRESSIBLE) {
		return "U";
	} else if (dtype == IOB_DT_H40COMPRESSIBLE) {
		return "H40";
	} else if (dtype == IOB_DT_H20COMPRESSIBLE) {
		return "H20";
	} else if (dtype == IOB_DT_H10COMPRESSIBLE) {
		return "H10";
	} else if (dtype == IOB_DT_HCOMPRESSIBLE) {
		return "H01";
	} else if (dtype == IOB_DT_PST) {
		return "E";
	} else if (dtype == IOB_DT_VM) {
		return "VM";
	} else if (dtype == IOB_DT_ZEROS) {
		return "Z";
	} else if (dtype == IOB_DT_ONES) {
		return "O";
	} else if (dtype == IOB_DT_PATTERN1) {
		return "P";
	}
	return " ";
}

void
iob_dedupdata(char *bp, uint32_t size, int dtype, uint64_t srcbno)
{
	uint64_t	hdr;
	char		*dbuf;
	int		off, doff;

	if (dtype == IOB_DT_PATTERN1) {
		for (off = 0; off < size; off += IOB_SECTOR) {
			memset(bp + off, lrand48() % 256, IOB_SECTOR);
		}
	} else if (dtype == IOB_DT_ONES) {
		memset(bp, 0xff, size);
	} else if (dtype == IOB_DT_ZEROS) {
		memset(bp, 0, size);
	} else {
		if (dtype == IOB_DT_UNCOMPRESSIBLE) {
			dbuf = uncdata;
		} else if (dtype == IOB_DT_PST) {
			dbuf = pst_data;
		} else if (dtype == IOB_DT_VM) {
			dbuf = vm_data;
		} else if (dtype == IOB_DT_COMPRESSIBLE) {
			dbuf = cdata;
		} else if (dtype == IOB_DT_H40COMPRESSIBLE) {
			dbuf = hc40data;
		} else if (dtype == IOB_DT_H20COMPRESSIBLE) {
			dbuf = hc20data;
		} else if (dtype == IOB_DT_H10COMPRESSIBLE) {
			dbuf = hc10data;
		} else {
			dbuf = hcdata;
		}
		for (off = 0; off < size; off += IOB_SECTOR) {
			doff = srcbno % (IOB_NDATAPAGES * IOB_PAGE / IOB_SECTOR) * IOB_SECTOR;
			hdr = ((uint64_t)dtype << 56) + srcbno++;
			memcpy(bp + off, dbuf + doff, IOB_SECTOR);
			*(uint64_t *)(bp + off) = hdr;
		}
	}
	return;
}

ssize_t
iob_read (int fd, char *buf, uint32_t length, uint64_t offset)
{
#if     defined(__CYGWIN__)
        lseek(fd, offset, SEEK_SET);
        return (read(fd, buf, length) );
#else
        return (pread (fd, buf, length, offset) );
#endif
}

ssize_t
iob_write (int fd, char *buf, uint32_t length, uint64_t offset)
{
#if     defined(__CYGWIN__)
        lseek(fd, offset, SEEK_SET);
        return (write(fd, buf, length) );
#else
        return (pwrite (fd, buf, length, offset) );
#endif
}
