//
// iob_iotest.c
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

#include	"ioload.h"

STATIC void		*io_thread(void *arg);
STATIC void		do_io(struct iothread *tp, uint64_t ionum);
STATIC void		io_start(struct ioreq *rp, char *rbp, char *wbp);
STATIC void		io_finish(struct iothread *tp, struct ioreq *rp, struct iodet *idp);

STATIC uint64_t	starttime;

void
runtest(void)
{
	struct file	*fp;
	struct iothread	*itp;
	struct rusage	before;
	uint64_t	curtime, now;
	int		i;

	current->threads = 1;
	for (fp = flist; fp; fp = fp->next) {
		for (itp = fp->ithreads; itp; itp = itp->next) {
			iob_thr_create(io_thread, (void *)itp);
		}
	}
	for (fp = flist; fp; fp = fp->next) {
		for (itp = fp->ithreads; itp; itp = itp->next) {
			iob_sema_wait(&itp->stop);
		}
	}
	for (i = flist->params.initthreads; i <= flist->params.maxthreads; i += flist->params.incrthreads) {
		iob_infolog("Starting test with %d threads\n\n", i);
		(void) getrusage(RUSAGE_SELF, &before);
		curtime = iob_gettime();
		current->threads = i;
		current->endwarmup = curtime + flist->params.warmup;
		current->endinterval = current->endwarmup + flist->params.interval;
		current->finish = current->endinterval + IOB_NSEC_TO_SEC;
		starttime = curtime;
		for (fp = flist; fp; fp = fp->next) {
			for (itp = fp->ithreads; itp; itp = itp->next) {
				iob_sema_post(&itp->start);
			}
		}
		if (flist->params.status) {
			while (current->endinterval > curtime + ((uint64_t)flist->params.status + 2) * IOB_NSEC_TO_SEC) {
				sleep(flist->params.status);
				now = iob_gettime();
				output(curtime, now, &before, 1);
				curtime = now;
			}
		}
		for (fp = flist; fp; fp = fp->next) {
			for (itp = fp->ithreads; itp; itp = itp->next) {
				iob_sema_wait(&itp->stop);
			}
		}
		output(current->endwarmup, current->endinterval, &before, 0);
	}
	current->threads = 0;
	for (fp = flist; fp; fp = fp->next) {
		for (itp = fp->ithreads; itp; itp = itp->next) {
			iob_sema_post(&itp->start);
		}
	}
	exit(0);
}

STATIC void *
io_thread(void *arg)
{
	struct iothread	*itp = (struct iothread *)arg;
	uint64_t	ionum = 1ULL;

	srand48((long)iob_gettime());
	iob_sema_post(&itp->stop);
	while (current->threads) {
		iob_sema_wait(&itp->start);
		if (itp->threadid < current->threads) {
			do_io(itp, ionum);
		}
		iob_sema_post(&itp->stop);
	}
	pthread_exit(0);
#if !defined(__SUNPRO_C)
	return NULL;
#endif
}

STATIC void
do_io(struct iothread *itp, uint64_t ionum)
{
	struct ioreq	*rp = itp->iorp;
	struct itype	*ip;
	struct timespec	tm;
	double		ioselect;
	uint64_t	iotime, nextio, slptime;
	uint64_t	offset, baseoff;
	int		sz;

	if (itp->params.maxrate) {
		iotime = IOB_NSEC_TO_SEC / itp->params.maxrate;
		nextio = iob_gettime() + iotime;
	} else {
		nextio = 0ULL;
		iotime = 0ULL;
	}
	while (rp->detail.start < current->finish) {
		ioselect = drand48();
		for (ip = itp->itlist; ip->next; ip = ip->next) {
			if (ioselect < ip->pct) {
				break;
			}
			ioselect -= ip->pct;
		}
		offset = IOB_RAND64();
		offset -= offset % ip->size;
		offset += itp->params.headersz;
		offset = (offset % itp->rangeoff) * itp->params.multiplier + itp->params.minoff;
		do {
			baseoff = offset;
			sz = ip->size;
			while (sz > 0) {
				rp->detail.size = sz;
				if (rp->detail.size > itp->params.maxiosize) {
					rp->detail.size = itp->params.maxiosize;
				}
				rp->detail.ionum = ionum++;
				rp->detail.offset = baseoff;
				rp->detail.type = ip->optype;
				rp->detail.dtype = ip->dtype;
				rp->itype = ip;
				if (ip->optype == WRITE) {
					iob_iodata(rp->writebp, rp->detail.size, ip->dtype, ip->dedupblks);
				}
				io_start(rp, rp->readbp, rp->writebp);
				sz -= rp->detail.size;
				baseoff += rp->detail.size;
			}
		} while (ip->optype == WRITE && drand48() < ip->repwrite);
		if (rp->detail.end < nextio) {
			slptime = nextio - rp->detail.end;
			tm.tv_sec = slptime / 1000000000;
			tm.tv_nsec = slptime % 1000000000;
			(void) nanosleep(&tm, NULL);
		}
		nextio += (uint64_t)((unsigned)mrand48() % (iotime * 2 + 1));
	}
}

STATIC void
io_start(struct ioreq *rp, char *rbp, char *wbp)
{
	struct iothread	*tp = rp->threadp;
	struct iodet	*idp = &rp->detail;
	struct timespec	tm;
	uint32_t	slu;
	ssize_t		ret;

	rp->error = 0;
	idp->flag = 0;
	idp->start = iob_gettime();
	if (idp->type == SLEEP) {
		slu = (uint32_t)mrand48() % idp->size + 50;
		idp->size = slu - slu % 50;
		tm.tv_sec = idp->size / 1000000;
		tm.tv_nsec = (idp->size * 1000) % 1000000000;
		if ((ret = nanosleep(&tm, NULL)) == 0) {
			ret = idp->size;
		}
	} else if (idp->type == READ) {
#if defined(__CYGWIN__)
            ret = cyg_read(rp->fd, rbp, idp->size, idp->offset);
#else
		ret = pread(rp->fd, rbp, idp->size, idp->offset);
#endif
	} else {
#if defined(__CYGWIN__)
            ret = cyg_write(rp->fd, wbp, idp->size, idp->offset);
#else
		ret = pwrite(rp->fd, wbp, idp->size, idp->offset);
#endif
	}
	if (ret != idp->size) {
		rp->error = 1;
		rp->syserr = errno;
		rp->rsize = ret;
	}
	idp->end = iob_gettime();
	io_finish(tp, rp, idp);
	return;
}

STATIC void
io_finish(struct iothread *tp, struct ioreq *rp, struct iodet *idp)
{
	uint64_t	lat;
	uint32_t	bkt;

	if (rp->error) {
		if (ign_errors) {
			idp->flag |= IOD_ERROR;
		} else {
			iob_errlog(0, "I/O Error: io %lld fd %d  op %s  offset %16.16llx  size %d  errno %d  rsize %d\n",
				  idp->ionum, rp->fd,
				  (idp->type == READ) ? "read" : (idp->type == WRITE) ? "write" : "sleep",
				  idp->offset, idp->size, rp->syserr, rp->rsize);
		}
	}
	if (idp->start < current->endwarmup || idp->start >= current->endinterval) {
		return;
	}
	lat = idp->end - idp->start;
	idp->seconds = (uint32_t)((idp->end - starttime) / IOB_NSEC_TO_SEC);
	if (idp->type != SLEEP) {
		bkt = (uint32_t)(lat / LATDIV);
		if (bkt >= NLATBKTS) {
			bkt = NLATBKTS - 1;
		}
		tp->cur.latbuckets[bkt][idp->type]++;
		tp->cur.totlat[idp->type] += lat;
		tp->cur.totbytes[idp->type] += (uint64_t)idp->size;
	}
	if (idp->flag || lat >= rp->itype->longlat) {
		iob_details_add(idp);
	}
	return;
}

#if defined(__CYGWIN__)

ssize_t
cyg_write(int fd, char *buf, uint32_t count, uint64_t offset)
{
        lseek(fd, offset, SEEK_SET);
        return write(fd, buf, count);
}

ssize_t
cyg_read(int fd, char *buf, uint32_t count, uint64_t offset)
{
        lseek(fd, offset, SEEK_SET);
        return read(fd, buf, count);
}

#endif
