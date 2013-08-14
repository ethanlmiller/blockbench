//
// iob_stats.c
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
#if defined(__SUNPRO_C) || defined(__hpux)
#include        <atomic.h>
#endif

uint64_t	cur_iodet = 0;
uint64_t	niodet = 0ULL;
struct iodet	*details = NULL;

char	*typestr[MAX_OPTYPES] = {
		"RD",
		"WR",
		"FL",
		"PF",
		"TR",
		"RF",
		"FR",
		"PW",
		"RR",
		"RK",
		"RI",
		"CL",
		"RE",
		"PE",
		"SR",
		"SL",
		"UM",
		"SI",
		"WS",
		"WF",
		"SW",
		"PS",
};

void
iob_details_init(int maxiodet)
{

	if (maxiodet) {
		details = (struct iodet *)iob_malloc(maxiodet * sizeof (struct iodet));
		niodet = maxiodet;
	}
	return;
}

void
iob_details_reset(int clear)
{

	if (clear) {
		memset(details, 0, niodet * sizeof (struct iodet));
	}
#if defined(__SUNPRO_C)
        atomic_and_64(&cur_iodet, (uint64_t)0);
#elif defined(__hpux)
        atomic_inc_64(&cur_iodet);
#else
	__sync_fetch_and_and(&cur_iodet, (uint64_t)0);
#endif
	return;
}

void
iob_details_add(struct iodet *idp)
{
	uint64_t	nextdet;

#if defined(__SUNPRO_C)
        nextdet = cur_iodet;
        atomic_add_64(&cur_iodet, 1);
#elif defined(__hpux)
        nextdet = cur_iodet;
        atomic_inc_64(&cur_iodet);
#else
	nextdet = __sync_fetch_and_add(&cur_iodet, 1);
#endif
	if (nextdet < niodet) {
		idp->latency = idp->end - idp->start;
		details[nextdet] = *idp;
	}
	return;
}

#define	NBKTS	6

unsigned long long	latbkts[NBKTS] = {
	     500000ULL,	/* 500 us */
	    1000000ULL,	/* 1 ms */
	    2000000ULL,	/* 2 ms */
	    5000000ULL,	/* 5 ms */
	   10000000ULL,	/* 10 ms */
	10000000000ULL,	/* 10 sec */
};

char    *latlabels[NBKTS] = {
        "  0-500us",
	"500us-1ms",
	"  1ms-2ms",
	"  2ms-5ms",
	" 5ms-10ms",
	"over 10ms",
};

void
iob_details_output(FILE *fp, int infolog, char *lprefix, int channels, int banks, int pgsize)
{
	struct iodet		*idp;
	unsigned long long	i, lastend, longlat, maxdet, coff, lpn, pn;
	long long int		delta;
	int			latb, chan, bank, ochan;
	char			outstr[5], *type, offstr[64];

	fprintf(fp, "%sFlags, Wcycle, FID, TID,  Time,   IOnum,   Type,  Dtype,     Size,        Offset, Latency(us),  Cycle Start (ns),  Delta (ns),         End (ns),", lprefix);
	if (channels) {
		fprintf(fp, "        LPN,    LatB,  4K Chan, 4K Bank,");
		if (channels & 0xff00) {
			fprintf(fp, " 8K Chan, 8K Bank,");
		}
		if (channels & 0xff0000) {
			fprintf(fp, " 16K Chan, 16K Bank,");
		}
	}
	fprintf(fp, "\n");
	lastend = 0ULL;
	delta = 0LL;
	longlat = 0ULL;
	maxdet = cur_iodet;
	if (maxdet > niodet) {
		maxdet = niodet;
	}
	for (i = 0, idp = details; i < maxdet; i++, idp++) {
		if (idp->type == WRITE) {
			type = iob_dttochr(idp->dtype);
		} else {
			type = " ";
		}
		outstr[0] = idp->flag & IOD_RETRY ? 'R' : ' ';
		outstr[1] = idp->flag & IOD_LONG_LONG ? 'L' : ' ';
		outstr[2] = idp->flag & IOD_ERROR ? 'E' : ' ';
		outstr[3] = idp->flag & IOD_RECOVERED ? 'R' : ' ';
		outstr[4] = '\0';
		if (lastend != 0ULL) {
			delta = (long long int)idp->end - (long long int)lastend;
		}
		coff = idp->end - idp->wcstart;
		snprintf(offstr, sizeof (offstr), "0x%llx", (long long unsigned)idp->offset);
		fprintf(fp, "%s %4s,%7u, %3u, %3u, %5d, %7llu,     %2s,  %4s,  %8u, %13s,  %10llu, %5llu.%3.3llu.%3.3llu.%3.3llu,  %10lld, %16llu,",
			lprefix, outstr, idp->wcycle, idp->fid, idp->tid, idp->seconds,
			(long long unsigned)idp->ionum, typestr[idp->type], type, idp->size,
			offstr, (long long unsigned)idp->latency / 1000ULL,
			coff / IOB_NSEC_TO_SEC, coff / 1000000ULL % 1000ULL, coff / 1000ULL % 1000ULL,
			coff % 1000ULL, (long long int)delta, (long long unsigned int)idp->end);
		if (channels) {
			for (latb = 0; latb < NBKTS - 1; latb++) {
				if (idp->latency < latbkts[latb]) {
					break;
				}
			}
			lpn = idp->offset / pgsize;
			fprintf(fp, "  %9llu, %s,", (long long unsigned)lpn, latlabels[latb]);
			pn = idp->offset / 4096;
			ochan = channels & 0xff;
			chan = (int)(pn % ochan);
			bank = (int)(pn % (ochan * banks)) / ochan;
			fprintf(fp, "   CH-%d,    BK-%d,", chan, bank);
			if (channels & 0xff00) {
				pn = idp->offset / 8192;
				ochan = (channels >> 8) & 0xff;
				chan = (int)(pn % ochan);
				bank = (int)(pn % (ochan * banks)) / ochan;
				fprintf(fp, "    CH-%d,    BK-%d,", chan, bank);
			}
			if (channels & 0xff0000) {
				pn = idp->offset / 16384;
				ochan = channels >> 16;
				chan = (int)(pn % ochan);
				bank = (int)(pn % (ochan * banks)) / ochan;
				fprintf(fp, "     CH-%d,     BK-%d,", chan, bank);
			}
		}
		fprintf(fp, "\n");
		if (idp->latency > longlat) {
			if (infolog) {
				iob_infolog("longio, %4s,%7u, %3u, %3u, %5d, %7llu,     %2s,  %4s,  %8u, %12llx,     %10llu,  %9llu.%3.3llu.%3.3llu.%3.3llu,   %10lld,    %16llu,\n",
					   outstr, idp->wcycle, idp->fid, idp->tid, idp->seconds,
					   (long long unsigned)idp->ionum, typestr[idp->type], type, idp->size,
					   (long long unsigned)idp->offset, (long long unsigned)idp->latency / 1000ULL,
					   coff / IOB_NSEC_TO_SEC, coff / 1000000ULL % 1000ULL, coff / 1000ULL % 1000ULL,
					   coff % 1000ULL, delta, (long long unsigned int)idp->end);
			}
			longlat = idp->latency;
		}
		lastend = idp->end;
	}
	return;
}

char    *modestr[NMODES] = {
        " None",
        " Read",
        "Write",
        "RdLim",
        "RdRec",
        "RdBck",
};

uint64_t
iob_details_multi(FILE *fp, uint64_t start_iodet, int wcycle, int channels, int banks, int pgsize)
{
	struct iodet		*idp;
	unsigned long long	i, lastend, maxdet, coff, lpn;
	long long int		delta;
	int			latb;
	char			*outstr, *type, offstr[64], chanstr[64], bankstr[64];

	fprintf(fp, " Mode, Flags, Wcycle, TID,   IOnum,   Type,  Dtype,     Size,        Offset, Latency(us),  Cycle Start (ns),  Delta (ns),         End (ns),        LPN,    LatB,    Chan,  Bank,   IO1,  IO2,  IO3,  IO4,\n");
	lastend = 0ULL;
	delta = 0LL;
	maxdet = cur_iodet;
	if (maxdet > niodet) {
		maxdet = niodet;
	}
	for (i = start_iodet, idp = details + start_iodet; i < maxdet; i++, idp++) {
		if (idp->wcycle != wcycle) {
			return i;
		}
		if (idp->type == WRITE) {
			type = iob_dttochr(idp->dtype);
		} else {
			type = " ";
		}
		if (idp->flag & IOD_STATE) {
			if (idp->flag & IOD_LONG_LONG) {
				outstr = "   LS";
				if (idp->flag & IOD_RECOVERED) {
					outstr = "  LRS";
				}
			} else if (idp->flag & IOD_RECOVERED) {
				outstr = "   RS";
			} else {
				outstr = "    S";
			}
		} else if (idp->flag & IOD_LONG_LONG) {
			outstr = "    L";
			if (idp->flag & IOD_RECOVERED) {
				outstr = "   LR";
			}
		} else if (idp->flag & IOD_RECOVERED) {
			outstr = "    R";
		} else {
			outstr = "     ";
		}
		if (lastend != 0ULL) {
			delta = (long long int)idp->end - (long long int)lastend;
		}
		coff = idp->end - idp->wcstart;
		for (latb = 0; latb < NBKTS - 1; latb++) {
			if (idp->latency < latbkts[latb]) {
				break;
			}
		}
		lpn = idp->offset / pgsize;
		snprintf(offstr, sizeof (offstr), "0x%llx", (long long unsigned)idp->offset);
		snprintf(chanstr, sizeof (chanstr), "CH-%d", (int)(lpn % channels));
		snprintf(bankstr, sizeof (bankstr), "BK-%d", (int)(lpn % (channels * banks)) / channels);
		fprintf(fp, "%5s, %5s,%7u, %3u, %7llu,     %2s,  %4s,  %8u, %13s,  %10llu, %5llu.%3.3llu.%3.3llu.%3.3llu,  %10lld, %16llu,  %9llu, %s, %5s, %5s, %5d, %4d, %4d, %4d,\n",
			modestr[idp->fplmode], outstr, idp->wcycle, idp->tid,
			(long long unsigned)idp->ionum, typestr[idp->type], type, idp->size,
			offstr, (long long unsigned)idp->latency / 1000ULL,
			coff / IOB_NSEC_TO_SEC, coff / 1000000ULL % 1000ULL, coff / 1000ULL % 1000ULL,
			coff % 1000ULL, delta, (long long unsigned int)idp->end,
			(long long unsigned)lpn, latlabels[latb], chanstr, bankstr,
			idp->io1, idp->io2, idp->io3, idp->io4);
		lastend = idp->end;
	}
	return i;
}
