//
// iol_init.c
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

STATIC struct iothread	*ithreadinit(struct file *fp, int threadid,
				     struct cmdparams *cp, struct itype *itlist, int iosize);

struct curtest		*current;
static int		maxsize;

void
init_threads(int nfiles, char *filenames[], struct cmdparams *cp, struct itype *itlist)
{
	struct file	*fp;
	struct iothread	*itp;
	struct itype	*ip;
	uint64_t	minblks, maxblks, rangeoff;
	int		i, j, iosize;

	current = (struct curtest *)iob_align_shmem(sizeof (struct curtest));
	iob_initdata();
	iob_details_init(NIOTRACE);
	maxsize = 0;
	for (ip = itlist; ip; ip = ip->next) {
		if (ip->size > maxsize) {
			maxsize = ip->size;
		}
	}
	if (cp->headersz < maxsize) {
		cp->headersz = cp->headersz % maxsize;
	}
	minblks = cp->minoff / IOB_SECTOR;
	maxblks = cp->maxoff / IOB_SECTOR;
	if (cp->headersz) {
		maxblks = (cp->maxoff - maxsize) / IOB_SECTOR;
	}
	rangeoff = (maxblks - minblks) * IOB_SECTOR;
	if (rangeoff < maxsize + cp->headersz) {
		fprintf(stderr, "Error: maximum iosize (%d) plus header (%llu) is larger than test range (%lluMB to %lluMB)\n",
			maxsize, (long long unsigned)cp->headersz,
			(long long unsigned)cp->minoff, (long long unsigned)cp->maxoff);
		exit(1);
	}
	iosize = maxsize;
	if (cp->maxiosize < maxsize) {
		iosize = cp->maxiosize;
	}
	for (i = nfiles - 1; i >= 0; i--) {
		fp = (struct file *)iob_malloc(sizeof (struct file));
		fp->filename = filenames[i];
		fp->fileno = i + 1;
		fp->params = *cp;
		fp->itlist = itlist;
		fp->next = flist;
		flist = fp;
		for (j = cp->maxthreads - 1; j >= 0; j--) {
			itp = ithreadinit(fp, j, cp, itlist, iosize);
			itp->next = fp->ithreads;
			fp->ithreads = itp;
		}
	}
	return;
}

STATIC struct iothread *
ithreadinit(struct file *fp, int threadid, struct cmdparams *cp, struct itype *itlist, int iosize)
{
	struct iothread	*tp;

	tp = (struct iothread *)iob_malloc(sizeof (struct iothread));
	iob_sema_init(&tp->stop);
	iob_sema_init(&tp->start);
	tp->threadid = threadid;
	tp->params = *cp;
	tp->itlist = itlist;
	tp->file = fp;
	if ((tp->fd = open(fp->filename, IOB_DIRECT | O_RDWR | O_CREAT, 0666)) < 0) {
		iob_errlog(0, "ioload: can't open %s, errno %d\n", fp->filename, errno);
	}
	tp->iorp = (struct ioreq *)iob_malloc(sizeof (struct ioreq));
	tp->iorp->threadp = tp;
	tp->iorp->readbp = iob_align_shmem(iosize);
	tp->iorp->writebp = iob_align_shmem(iosize);
	tp->iorp->fd = tp->fd;
	tp->iorp->detail.fid = fp->fileno;
	tp->iorp->detail.tid = threadid;
	tp->minblks = tp->params.minoff / IOB_SECTOR;
	tp->maxblks = tp->params.maxoff / IOB_SECTOR;
	if (tp->params.headersz) {
		tp->maxblks = (tp->params.maxoff - maxsize) / IOB_SECTOR;
	}
	tp->rangeoff = (tp->maxblks - tp->minblks) * IOB_SECTOR;
	return tp;
}
