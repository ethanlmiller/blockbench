//
// iof_init.c
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

#include	"iofill.h"
#include	<signal.h>

STATIC void		set_streamhead(struct iothread *itp, struct fthread *ftp, int streamid);
STATIC struct fthread	*fthreadinit(char *fname, struct cmdparams *cp);
STATIC struct iothread	*ithreadinit(struct fthread *ftp, int threadid, struct cmdparams *cp);

uint64_t		*shared = NULL;
int			sharedsize = 0;

void
init_threads(int nfiles, char *filenames[], struct cmdparams *cp)
{
	struct fthread	*ftp;
	struct iothread	*itp;
	int		i, j, threadalign;

	sighold(SIGXFSZ);
	sharedsize = IOB_CACHE_LINE * (cp->nthreads + 1) * nfiles;
	shared = (uint64_t *)iob_align_shmem(sharedsize * sizeof (uint64_t));
	iob_initdata();
	iob_details_init(cp->iodetcnt);
	if (cp->threadalign) {
		threadalign = cp->threadalign * cp->nthreads;
	} else {
		threadalign = cp->size;
	}
	for (i = nfiles - 1; i >= 0; i--) {
		ftp = fthreadinit(filenames[i], cp);
		ftp->next = flist;
		ftp->shared_block = &shared[IOB_CACHE_LINE * (cp->nthreads + 1) * i];
		ftp->fileno = i;
		flist = ftp;
	}
	for (ftp = flist, i = 1; ftp; ftp = ftp->next, i++) {
		for (j = cp->nthreads - 1; j >= 0; j--) {
			itp = ithreadinit(ftp, j, cp);
			itp->threadalign = threadalign;
			itp->shared_area = ftp->shared_block + (j + 1) * IOB_CACHE_LINE;
			itp->next = ftp->ithreads;
			ftp->ithreads = itp;
		}
		if (cp->streams > 1 && cp->streams < cp->nthreads) {
			for (itp = ftp->ithreads; itp; itp = itp->next) {
				set_streamhead(itp, ftp, itp->threadid % cp->streams);
			}
		}
	}
	return;
}

STATIC void
set_streamhead(struct iothread *itp, struct fthread *ftp, int streamid)
{
	struct iothread	*p;

	for (p = ftp->ithreads; p; p = p->next) {
		if (p->threadid == streamid) {
			itp->streamhead = p;
			break;
		}
	}
	return;
}

STATIC struct fthread *
fthreadinit(char *fname, struct cmdparams *cp)
{
	struct fthread	*tp;

	tp = (struct fthread *)iob_malloc(sizeof (struct fthread));
	iob_sema_init(&tp->stop);
	iob_sema_init(&tp->start);
	tp->params = *cp;
	tp->filename = fname;
	tp->totalio = cp->nio * cp->nrep;
	return tp;
}

STATIC struct iothread *
ithreadinit(struct fthread *ftp, int threadid, struct cmdparams *cp)
{
	struct iothread	*tp;
	int		i, j, g1, g2, gsize, nswaps = 100, tmp;

	srand48((long)iob_gettime());
	tp = (struct iothread *)iob_malloc(sizeof (struct iothread));
	iob_sema_init(&tp->stop);
	iob_sema_init(&tp->start);
	tp->threadid = threadid;
	tp->writebuf = iob_align_shmem(cp->writesize);
	tp->params = *cp;
	if (threadid != 0) {
		tp->params.writeinject = 0;
		tp->params.injectinterval_io = 0;
		tp->params.flush = 0;
	}
	tp->pthread = ftp;
	if ((tp->fd = open(ftp->filename, cp->openflags, 0666)) < 0) {
		iob_errlog(0, "iofill: can't open %s, errno %d\n", ftp->filename, errno);
	}
	tp->iorp = (struct ioreq *)iob_malloc(sizeof (struct ioreq));
	tp->iorp->threadp = tp;
	tp->iorp->threadoff = threadid * cp->threadalign;
	tp->iorp->readbp = iob_align_shmem(cp->size);
	tp->iorp->writebp = iob_align_shmem(cp->size);
	tp->iorp->fd = tp->fd;
	tp->iorp->detail.fid = ftp->fileno;
	tp->iorp->detail.tid = threadid;
	tp->streamhead = tp;
	if (cp->randseed == 0) {
		tp->params.randseed = (uint32_t)iob_gettime() + tp->threadid;
	} else {
		tp->params.randseed = cp->randseed + tp->threadid;
	}
	if (tp->params.partition) {
		tp->params.minoff += tp->params.maxoff * threadid;
		tp->params.maxoff += tp->params.maxoff * threadid;
	}
	tp->minblks = tp->params.minoff / tp->params.size;
	tp->maxblks = tp->params.maxoff / tp->params.size;
	if (tp->maxblks == 0LL) {
		tp->maxblks = 1LL;
	}
	if (tp->maxblks == tp->minblks) {
		tp->maxblks = tp->minblks + 1;
	}
	tp->rangeoff = (tp->maxblks - tp->minblks) * tp->params.size;
	tp->maxsegs = (tp->params.maxoff - tp->params.minoff) / tp->params.segsize;
	if (tp->maxsegs == 0LL) {
		tp->maxsegs = 1LL;
	}
	if (tp->params.writeinject) {
		tp->nsegoffs = tp->params.segsize / tp->params.writesize;
		for (i = 0; i < IOB_MAXSEG; i++) {
			tp->segwrite[i] = tp->nsegoffs;
		}
		tp->segofflist = iob_malloc(sizeof (int) * tp->nsegoffs);
		gsize = tp->nsegoffs / tp->params.nsubseg;
		for (i = 0; i < tp->nsegoffs; i++) {
			tp->segofflist[i] = i;
		}
		for (i = 0; i < nswaps; i++) {
			g1 = (lrand48() % tp->params.nsubseg) * gsize;
			g2 = (lrand48() % tp->params.nsubseg) * gsize;
			for (j = 0; j < gsize; j++, g1++, g2++) {
				tmp = tp->segofflist[g1];
				tp->segofflist[g1] = tp->segofflist[g2];
				tp->segofflist[g2] = tmp;
			}
		}
	}
	if (tp->params.headersz > tp->params.size) {
		tp->params.headersz = tp->params.headersz % tp->params.size;
	}
	return tp;
}
