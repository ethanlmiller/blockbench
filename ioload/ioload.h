//
// ioload.h
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

#define	__EXTENSIONS__		1
#define	_REENTRANT		1
#define	_FILE_OFFSET_BITS	64
#define	_LARGEFILE_SOURCE	1
#define	NLATBKTS		100000
#define	NSEC			20000
#define	LATDIV			10000
#define	STATIC			static
#define	NIOTRACE		100000

#define	_GNU_SOURCE	1

#include	<stdint.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/time.h>
#include	<sys/mman.h>
#include	<sys/resource.h>
#include	<sys/param.h>
#include	<limits.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<errno.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<string.h>
#include	<pthread.h>
#include	<iobenchlibext.h>

#if defined(sun) || defined(__sun)

#include	<synch.h>

typedef sema_t		iob_sema_t;

#define iob_sema_init(sp)	sema_init((sp), 0, USYNC_THREAD, NULL)
#define iob_sema_wait(sp)	sema_wait(sp)
#define iob_sema_post(sp)	sema_post(sp)

#define	IOB_DIRECT	0

#else	/* Linux */

#include	<semaphore.h>

typedef sem_t		iob_sema_t;

#define iob_sema_init(sp)	sem_init((sp), 0, 0)
#define iob_sema_wait(sp)	sem_wait(sp)
#define iob_sema_post(sp)	sem_post(sp)

#define	IOB_DIRECT	O_DIRECT

#endif	/* Solaris or Linux */

struct ioreq {
	struct iodet	detail;
	uint64_t	threadoff;
	int		fd;
	int		error;
	int		syserr;
	int		rsize;
	char		*readbp;
	char		*writebp;
	struct iothread	*threadp;
	struct itype	*itype;
};

struct cmdparams {
	uint64_t	minoff;
	uint64_t	maxoff;
	uint64_t	headersz;
	uint64_t	warmup;
	uint64_t	interval;
	uint32_t	maxlatency;
	uint32_t	maxrate;
	uint32_t	status;
	int		multiplier;
	int		initthreads;
	int		maxthreads;
	int		incrthreads;
	int		maxiosize;
};

struct itype {
	struct itype	*next;
	char		*buf;
	double		pct;
	uint64_t	dedupblks;
	int		optype;
	uint32_t	size;
	int		dtype;
	uint64_t	longlat;
	double		repwrite;
};

struct iostats {
	uint64_t		latbuckets[NLATBKTS][N_OPTYPES];
	uint64_t		totreads;
	uint64_t		totwrites;
	uint64_t		totlat[N_OPTYPES];
	uint64_t		totbytes[N_OPTYPES];
};

struct iothread {
	struct iothread		*next;
	struct file		*file;
	struct ioreq		*iorp;
	iob_sema_t		start;
	iob_sema_t		stop;
	struct cmdparams	params;
	struct itype		*itlist;
	int			threadid;
	int			fd;
	uint64_t		maxblks;
	uint64_t		minblks;
	uint64_t		rangeoff;
	struct iostats		cur;
};

struct file {
	struct file		*next;
	struct iothread		*ithreads;
	char			*filename;
	int			fileno;
	struct cmdparams	params;
	struct itype		*itlist;
	struct iostats		cur;
	struct iostats		use;
	struct iostats		last;
};

struct curtest {
	int		threads;
	uint64_t	endwarmup;
	uint64_t	endinterval;
	uint64_t	finish;
};

#define	IOB_CACHE_LINE		(128 / sizeof (uint64_t))

void		runtest(void);
void		output(uint64_t start, uint64_t stop, struct rusage *brp, int status);
void		init_threads(int nfiles, char *filenames[], struct cmdparams *cp, struct itype *itlist);

extern struct file	*flist;
extern struct curtest	*current;
extern char		*outputfile;
extern int		verbose;
extern int		lat_details;
extern int		longlats;
extern int		ign_errors;

#if defined(__CYGWIN__)
extern ssize_t	cyg_read(int fd, char *buf, uint32_t length, uint64_t offset);
extern ssize_t	cyg_write(int fd, char *buf, uint32_t length, uint64_t offset);
#endif
