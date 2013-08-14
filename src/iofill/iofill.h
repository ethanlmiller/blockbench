//
// iofill.h
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
#define	IOB_MAXSEG		4
#define	STATIC			static

/* Methods of flushing a device cache*/

#define	SYNC_CACHE_16		0
#define	SYNC_CACHE_10		1
#define	SATA_STANDBY_IMM	2
#define	SATA_FLUSH_CACHE	3

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
};

struct cmdparams {
	double		readpct;
	uint32_t	randseed;
	int		seqflag;
	int		size;
	int		altsize;
	uint64_t	altcount;
	uint64_t	nio;
	uint64_t	nrep;
	uint64_t	seekoff;
	uint64_t	headersz;
	int		skip;
	int		nthreads;
	int		streams;
	uint64_t	wroff;
	uint64_t	minoff;
	uint64_t	maxoff;
	int		multiplier;
	int		partition;
	int		openflags;
	int		writeinject;
	int		writesize;
	int		segsize;
	int		nsegments;
	int		nsubseg;
	int		randinject;
	int		threadalign;
	int		flush;
	int		flushcount;
	int		flushtype;
	int		preflushtype;
	int		postflushtype;
	int		flushalign;
	uint64_t	flushinterval;
	uint64_t	preflushinterval;
	uint64_t	postflushinterval;
	uint64_t	injectinterval;
	uint64_t	injectinterval_io;
	uint64_t	iodetcnt;
	int		uniform;
	uint64_t	iodettime;
	uint64_t	nsleeptime;
	uint64_t	recoverytime;
	uint64_t	maxtime;
	int		dynamicrecovery;
	int		datatype;
	uint64_t	dedupblks;
};

struct iothread {
	struct iothread		*next;
	struct fthread		*pthread;
	struct ioreq		*iorp;
	iob_sema_t		start;
	iob_sema_t		stop;
	iob_sema_t		iostart;
	iob_sema_t		iostop;
	struct cmdparams	params;
	int			threadid;
	int			threadalign;
	int			fd;
	uint64_t		*shared_area;
	uint64_t		iocount;
	uint64_t		nextbno;
	struct iothread		*streamhead;
	char			*buf;
	char			*writebuf;
	uint64_t		cursegoff[IOB_MAXSEG];
	int			segwrite[IOB_MAXSEG];
	int			nsegoffs;
	int			*segofflist;
	int			writenum;
	int			flushnum;
	int			pflushnum;
	int			recovered;
	int			rec_runlen;
	uint64_t		latbuckets[NLATBKTS][N_STATTYPES];
	uint64_t		totreads;
	uint64_t		totwrites;
	uint64_t		totflushes;
	uint64_t		totpflushes;
	uint64_t		totlat[N_STATTYPES];
	uint64_t		totbytes[N_STATTYPES];
	int			seconds[NSEC];
	int			nseconds;
	uint64_t		nextsec;
	uint64_t		starttime;
	uint64_t		stoptime;
	uint64_t		maxblks;
	uint64_t		minblks;
	uint64_t		rangeoff;
	uint64_t		maxsegs;
};

struct fthread {
	struct fthread		*next;
	struct iothread		*ithreads;
	iob_sema_t		start;
	iob_sema_t		stop;
	struct cmdparams	params;
	char			*filename;
	int			fileno;
	int			iowait;
	uint64_t		*shared_block;
	uint64_t		thread_bno;
	uint64_t		totalio;
	uint64_t		latbuckets[NLATBKTS][N_STATTYPES];
	uint64_t		totreads;
	uint64_t		totwrites;
	uint64_t		totflushes;
	uint64_t		totpflushes;
	uint64_t		starttime;
	uint64_t		stoptime;
	uint64_t		endtime;
	uint64_t		totlat[N_STATTYPES];
	uint64_t		totbytes[N_STATTYPES];
};

#define	IOB_CACHE_LINE		(128 / sizeof (uint64_t))

#define	FLUSH_OFF		0
#define	FLUSH_AFT		1
#define	FLUSH_BEF		2
#define	FLUSH_SEG		3
#define	FLUSH_ONLY		4

#define	PFLUSH_SLEEP		1
#define	PFLUSH_READ		2
#define	PFLUSH_MIXED		3

void		runtest(void);
void		output(uint64_t start, uint64_t stop, struct rusage *brp);
void		init_threads(int nfiles, char *filenames[], struct cmdparams *cp);

extern struct fthread	*flist;
extern char		*outputfile;
extern uint64_t		statusmb;
