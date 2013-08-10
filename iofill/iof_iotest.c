#include	"iofill.h"
#if defined(__SUNPRO_C) || defined(__hpux)
#include        <atomic.h>
#endif

STATIC void		*file_thread(void *arg);
STATIC void		*io_thread(void *arg);
STATIC void		io_stop(struct iothread *itp);
STATIC void		io_restart(struct iothread *itp);
STATIC void		io_start(struct ioreq *rp, char *rbp, char *wbp);
STATIC void		do_flush_cache(struct iothread *itp, int prepost);
STATIC void		do_write_inject(struct iothread *itp, int flush);
STATIC void		io_finish(struct iothread *tp, struct ioreq *rp, struct iodet *idp);


void
runtest(void)
{
	struct fthread	*ftp;
	struct rusage	before;
	uint64_t	stop, start;

	for (ftp = flist; ftp; ftp = ftp->next) {
		iob_thr_create(file_thread, (void *)ftp);
	}
	for (ftp = flist; ftp; ftp = ftp->next) {
		iob_sema_wait(&ftp->stop);
	}
	(void) getrusage(RUSAGE_SELF, &before);
	start = iob_gettime();
	for (ftp = flist; ftp; ftp = ftp->next) {
		iob_sema_post(&ftp->start);
	}
	for (ftp = flist; ftp; ftp = ftp->next) {
		iob_sema_wait(&ftp->stop);
	}
	stop = iob_gettime();
	output(start, stop, &before);
	return;
}

STATIC void *
file_thread(void *arg)
{
	struct fthread	*ftp = (struct fthread *)arg;
	struct iothread	*itp;

	for (itp = ftp->ithreads; itp; itp = itp->next) {
		iob_thr_create(io_thread, (void *)itp);
	}
	for (itp = ftp->ithreads; itp; itp = itp->next) {
		iob_sema_wait(&itp->stop);
	}
	iob_sema_post(&ftp->stop);
	iob_sema_wait(&ftp->start);
	ftp->starttime = iob_gettime();
	if (ftp->params.maxtime) {
		ftp->endtime = ftp->starttime + ftp->params.maxtime;
	} else {
		ftp->endtime = ftp->starttime + IOB_NSEC_TO_SEC * (uint64_t)100000000;
	}
	for (itp = ftp->ithreads; itp; itp = itp->next) {
		iob_sema_post(&itp->start);
	}
	for (itp = ftp->ithreads; itp; itp = itp->next) {
		iob_sema_wait(&itp->stop);
	}
	ftp->stoptime = iob_gettime();
	iob_sema_post(&ftp->stop);
	pthread_exit(0);
#if !defined(__SUNPRO_C)
	return NULL;
#endif
}

STATIC void *
io_thread(void *arg)
{
	struct iothread	*itp = (struct iothread *)arg;
	struct ioreq	*rp = itp->iorp;
	int		cursize;
	uint64_t	next, nextflush, curtime;
	uint64_t	bno, offset, injbno, altcount;
	uint64_t	ionum, ftot_bno;
	int		flush;

	srand48(itp->params.randseed);
	if (itp->params.streams > 1 && itp->params.seqflag) {
		if (itp->params.partition == 0 && itp->threadid != 0) {
			itp->params.seekoff = (IOB_RAND64() % (itp->rangeoff >> 20)) << 20;
		}
	}
	if (itp->params.streams > 1 && itp->params.uniform) {
		if (drand48() < itp->params.readpct) {
			itp->params.readpct = 1.0;
		} else {
			itp->params.readpct = 0.0;
		}
	}
	iob_sema_post(&itp->stop);
	iob_sema_wait(&itp->start);
	itp->starttime = iob_gettime();
	next = itp->starttime + itp->params.injectinterval;
	nextflush = itp->starttime + itp->params.flushinterval;
	bno = 0LL;
	injbno = 0LL;
	ionum = 1LL;
	cursize = itp->params.size;
	altcount = 0LL;
	for (;;) {
		curtime = iob_gettime();
		if (curtime >= next && itp->params.writeinject) {
			next = curtime + itp->params.injectinterval;
			flush = 0;
			if (curtime > nextflush) {
				flush = 1;
				nextflush = curtime + itp->params.flushinterval;
			}
			do_write_inject(itp, flush);
		}
		if (itp->pthread->iowait) {
			iob_sema_post(&itp->iostop);
			iob_sema_wait(&itp->iostart);
		}
#if defined(__SUNPRO_C)
                bno = itp->pthread->thread_bno;
                atomic_add_64(&itp->pthread->thread_bno, 1);
		if (bno >= itp->pthread->totalio) {
#elif defined(__hpux)
                bno = itp->pthread->thread_bno;
                atomic_inc_64(&itp->pthread->thread_bno);
		if (bno >= itp->pthread->totalio) {
#else
		if ((bno = __sync_fetch_and_add(&itp->pthread->thread_bno, 1)) >= itp->pthread->totalio) {
#endif
			break;
		}
		ftot_bno = bno;
		if (itp->params.injectinterval_io && bno / itp->params.injectinterval_io > injbno) {
			do_write_inject(itp, 1);
			injbno++;
		}
		if (itp->params.seqflag == 0) {
			bno = IOB_RAND64();
		} else if (itp->params.streams > 1) {
#if defined(__SUNPRO_C)
                        bno = itp->streamhead->nextbno;
                        atomic_add_64(&itp->streamhead->nextbno, 1);
#elif defined(__hpux)
                        bno = itp->streamhead->nextbno;
                        atomic_inc_64(&itp->streamhead->nextbno);
#else
			bno = __sync_fetch_and_add(&itp->streamhead->nextbno, 1);
#endif
		} else {
			bno = bno % itp->params.nio;
		}
		if (bno == 0ULL && itp->params.headersz) {
			offset = itp->streamhead->params.seekoff;
			offset = itp->streamhead->params.minoff + (offset % itp->rangeoff) * itp->params.multiplier;
			rp->detail.ionum = ionum++;
			rp->detail.offset = offset;
			rp->detail.size = itp->params.headersz;
			if (drand48() < itp->params.readpct) {
				rp->detail.type = READ;
			} else {
				rp->detail.offset += itp->params.wroff;
				rp->detail.type = WRITE;
				rp->detail.dtype = itp->params.datatype;
				iob_iodata(rp->writebp, cursize, itp->params.datatype, itp->params.dedupblks);
			}
			io_start(rp, rp->readbp, rp->writebp);
		}
		offset = itp->streamhead->params.seekoff + itp->params.headersz +
			 (uint64_t)itp->params.size * (uint64_t)(1 + itp->params.skip) * bno;
		offset = itp->streamhead->params.minoff + (offset % itp->rangeoff) * itp->params.multiplier;
		rp->detail.ionum = ionum++;
		rp->detail.offset = offset;
		rp->detail.size = cursize;
		if (offset + cursize > itp->streamhead->params.minoff + itp->rangeoff * itp->params.multiplier) {
			rp->detail.size = itp->streamhead->params.minoff + itp->rangeoff * itp->params.multiplier -
					  offset;
		}
		if (drand48() < itp->params.readpct) {
			rp->detail.type = READ;
		} else {
			rp->detail.offset += itp->params.wroff;
			rp->detail.type = WRITE;
			rp->detail.dtype = itp->params.datatype;
			iob_iodata(rp->writebp, cursize, itp->params.datatype, itp->params.dedupblks);
		}
		io_start(rp, rp->readbp, rp->writebp);
		if (((ftot_bno - 1) * rp->detail.size) / statusmb < (ftot_bno * rp->detail.size) / statusmb) {
			iob_infolog("Completed %llu MB on %s\n",
				   (ftot_bno * rp->detail.size) >> 20, itp->pthread->filename);
		}
		if (itp->params.altcount && ++altcount % itp->params.altcount == 0) {
			if (cursize == itp->params.size) {
				cursize = itp->params.altsize;
			} else {
				cursize = itp->params.size;
			}
		}
	}
	itp->stoptime = iob_gettime();
	iob_sema_post(&itp->stop);
	pthread_exit(0);
#if !defined(__SUNPRO_C)
	return NULL;
#endif
}

STATIC void
io_stop(struct iothread *itp)
{
	struct iothread	*tp;

	itp->pthread->iowait = 1;
	for (tp = itp->pthread->ithreads; tp; tp = tp->next) {
		if (tp == itp) {
			continue;
		}
		iob_sema_wait(&tp->iostop);
	}
	return;
}

STATIC void
io_restart(struct iothread *itp)
{
	struct iothread	*tp;

	itp->pthread->iowait = 0;
	for (tp = itp->pthread->ithreads; tp; tp = tp->next) {
		if (tp == itp) {
			continue;
		}
		iob_sema_post(&tp->iostart);
	}
	return;
}

STATIC void
io_start(struct ioreq *rp, char *rbp, char *wbp)
{
	struct iothread	*tp = rp->threadp;
	struct iodet	*idp = &rp->detail;
	ssize_t		ret;

	rp->error = 0;
	idp->start = iob_gettime();
	if (idp->type == READ) {
#if defined(__CYGWIN__)
		if ((ret = cyg_read(rp->fd, rbp, idp->size, idp->offset)) != idp->size) {
#else
		if ((ret = pread(rp->fd, rbp, idp->size, idp->offset)) != idp->size) {
#endif
			rp->error = 1;
			rp->syserr = errno;
			rp->rsize = ret;
		}
	} else {
#if defined(__CYGWIN__)
		if ((ret = cyg_write(rp->fd, wbp, idp->size, idp->offset)) != idp->size) {
#else
		if ((ret = pwrite(rp->fd, wbp, idp->size, idp->offset)) != idp->size) {
#endif
			rp->error = 1;
			rp->syserr = errno;
			rp->rsize = ret;
		}
	}
	idp->end = iob_gettime();
	io_finish(tp, rp, idp);
	return;
}

STATIC void
do_flush_cache(struct iothread *itp, int prepost)
{
	struct ioreq	*rp = itp->iorp;
	uint64_t	end;
	int		i;

	if (itp->params.preflushtype == PFLUSH_SLEEP && prepost & 0x1) {
		usleep((useconds_t)(itp->params.preflushinterval / 1000));
	}
	if (prepost & 0x2) {
		for (i = 0; i < itp->params.flushcount; i++) {
			rp->detail.ionum = itp->flushnum++;
			rp->detail.type = FLUSH;
			rp->detail.size = 0;
			rp->detail.offset = itp->writenum;
			io_start(rp, NULL, NULL);
		}
	}
	if (prepost & 0x4) {
		if (itp->params.postflushtype == PFLUSH_SLEEP) {
			usleep((useconds_t)(itp->params.preflushinterval / 1000));
		} else if (itp->params.postflushtype == PFLUSH_READ || itp->params.postflushtype == PFLUSH_MIXED) {
			itp->recovered = 0;
			itp->rec_runlen = 0;
			end = iob_gettime() + itp->params.postflushinterval;
			if (itp->params.postflushtype == PFLUSH_MIXED) {
				usleep((useconds_t)itp->params.nsleeptime);
			}
			while (iob_gettime() < end && itp->recovered == 0) {
				rp->detail.ionum = itp->pflushnum++;
				rp->detail.type = POSTFLUSH;
				rp->detail.size = itp->params.size;
				rp->detail.offset = itp->params.minoff +
						    IOB_RAND64() * itp->params.flushalign % itp->rangeoff;
				io_start(rp, rp->readbp, NULL);
			}
		}
	}
	return;
}

STATIC void
do_write_inject(struct iothread *itp, int flush)
{
	struct ioreq	*rp = itp->iorp;
	int		i, j, didflush = 0, count;

	io_stop(itp);
	if (flush) {
		if (itp->params.flush == FLUSH_ONLY) {
			do_flush_cache(itp, 0x7);
			io_restart(itp);
			return;
		} else if (itp->params.flush == FLUSH_BEF) {
			do_flush_cache(itp, 0x7);
		}
	}
	if (itp->params.flush == FLUSH_SEG) {
		do_flush_cache(itp, 0x1);
	}
	for (j = 0; j < itp->params.nsegments; j++) {
		count = itp->params.writeinject;
		if (itp->params.randinject) {
			count = lrand48() % (itp->params.writeinject * 2) + 1;
		}
		for (i = 0; i < count; i++) {
			if (itp->segwrite[j] == itp->nsegoffs) {
				itp->cursegoff[j] = itp->params.minoff +
						   IOB_RAND64() % itp->maxsegs * itp->params.segsize;
				itp->segwrite[j] = 0;
				if (itp->params.flush == FLUSH_SEG) {
					do_flush_cache(itp, 0x2);
					didflush = 1;
				}
			}
			rp->detail.ionum = itp->writenum++;
			rp->detail.type = WRITE;
			rp->detail.size = itp->params.writesize;
			rp->detail.offset = itp->cursegoff[j] +
					    itp->segofflist[itp->segwrite[j]] * itp->params.writesize;
			rp->detail.dtype = IOB_DT_UNCOMPRESSIBLE;
			itp->segwrite[j]++;
			iob_iodata(itp->writebuf, itp->params.writesize, IOB_DT_UNCOMPRESSIBLE, itp->params.dedupblks);
			io_start(rp, NULL, itp->writebuf);
		}
	}
	if (didflush) {
		do_flush_cache(itp, 0x6);
	}
	if (flush && itp->params.flush == FLUSH_AFT) {
		do_flush_cache(itp, 0x7);
	}
	io_restart(itp);
	return;
}

STATIC void
io_finish(struct iothread *tp, struct ioreq *rp, struct iodet *idp)
{
	uint64_t	lat;
	uint32_t	bkt;

	if (rp->error) {
		if (rp->syserr == EFBIG || rp->syserr == ENOSPC || (rp->syserr == 0 && rp->rsize != idp->size)) {
			fprintf(stderr, "I/O Error on %s: offset %llu  size %d  errno %d  rsize %d\n",
				tp->pthread->filename, (long long unsigned int)idp->offset,
				idp->size, rp->syserr, rp->rsize);
#if defined(__SUNPRO_C)
			atomic_add_64(&tp->pthread->thread_bno, tp->pthread->totalio);
#elif defined(__hpux)
                        atomic_inc_64(&tp->pthread->thread_bno);                        
#else
			__sync_fetch_and_add(&tp->pthread->thread_bno, tp->pthread->totalio);
#endif

			return;
		}
		idp->flag = IOD_ERROR;
		iob_errlog(0, "I/O Error: io %lld fd %d  op %s  offset %16.16llx  size %d  errno %d  rsize %d\n",
			  idp->ionum, rp->fd,
			  idp->type == READ ? "read" : (idp->type == POSTFLUSH ? "pflush" : "write"),
			  idp->offset, idp->size, rp->syserr, rp->rsize);
	} else {
		idp->flag = 0;
		if (tp->nextsec == 0) {
			tp->nextsec = idp->start + IOB_NSEC_TO_SEC;
		} else if (idp->start > tp->nextsec) {
			tp->nextsec += IOB_NSEC_TO_SEC;
			if (tp->nseconds < NSEC - 1) {
				tp->nseconds++;
			}
			if (idp->start > tp->pthread->endtime) {
#if defined(__SUNPRO_C)
				atomic_add_64(&tp->pthread->thread_bno, tp->pthread->totalio);
#elif defined(__hpux)
                                atomic_inc_64(&tp->pthread->thread_bno);
#else
				__sync_fetch_and_add(&tp->pthread->thread_bno, tp->pthread->totalio);
#endif
			}
		}
		idp->seconds = tp->nseconds;
		lat = idp->end - idp->start;
		bkt = (uint32_t)(lat / LATDIV);
		if (bkt >= NLATBKTS) {
			bkt = NLATBKTS - 1;
		}
		tp->latbuckets[bkt][idp->type]++;
		tp->totlat[idp->type] += lat;
		tp->totbytes[idp->type] += (uint64_t)idp->size;
		tp->seconds[tp->nseconds]++;
		if (idp->type == POSTFLUSH) {
			idp->flag |= IOD_LONG;
			if (lat < tp->params.recoverytime) {
				tp->rec_runlen++;
				if (tp->rec_runlen == tp->params.dynamicrecovery) {
					tp->recovered = 1;
				}
			} else {
				tp->rec_runlen = 0;
			}
		} else if (lat >= tp->params.iodettime) {
			idp->flag |= IOD_LONG;
		}
	}
	if (idp->flag & (IOD_LONG | IOD_ERROR)) {
		iob_details_add(idp);
	}
	tp->iocount++;
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
