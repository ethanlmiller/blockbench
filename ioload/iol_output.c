//
// iol_output.c
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

STATIC void	stats_diff(struct iostats *cp, struct iostats *lp, struct iostats *up);
STATIC void	output_header(void);
STATIC void	output_times(uint64_t start, uint64_t stop, struct rusage *brp, struct rusage *erp, int status);
STATIC void	printbuckets(FILE *fp, struct file *tp, char *prefix);
STATIC void	printname(FILE *fp, struct file *ftp);
STATIC void	output_details(int status);

STATIC struct file	systot;
STATIC FILE		*fpout = NULL;
STATIC int		latency_flag = 0;
STATIC struct rusage	last;

struct summary {
	int	readiops;
	int	writeiops;
	int	readlat;
	int	writelat;
} testsummary;

void
output(uint64_t start, uint64_t stop, struct rusage *brp, int status)
{
	struct file	*ftp;
	struct iothread	*itp;
	struct rusage	ru;
	int		i, j;

	if (fpout && status) {
		brp = &last;
	}
	(void) getrusage(RUSAGE_SELF, &ru);
	output_header();
	memset(&systot.cur, 0, sizeof (struct iostats));
	for (ftp = flist; ftp; ftp = ftp->next) {
		memset(&ftp->cur, 0, sizeof (struct iostats));
		for (itp = ftp->ithreads; itp; itp = itp->next) {
			itp->cur.totreads = 0ULL;
			itp->cur.totwrites = 0ULL;
			for (j = 0; j < N_OPTYPES; j++) {
				ftp->cur.totlat[j] += itp->cur.totlat[j];
				ftp->cur.totbytes[j] += itp->cur.totbytes[j];
				systot.cur.totlat[j] += itp->cur.totlat[j];
				systot.cur.totbytes[j] += itp->cur.totbytes[j];
			}
			for (i = 0; i < NLATBKTS; i++) {
				for (j = 0; j < N_OPTYPES; j++) {
					ftp->cur.latbuckets[i][j] += itp->cur.latbuckets[i][j];
					systot.cur.latbuckets[i][j] += itp->cur.latbuckets[i][j];
				}
				itp->cur.totreads += itp->cur.latbuckets[i][READ];
				itp->cur.totwrites += itp->cur.latbuckets[i][WRITE];
			}
			ftp->cur.totreads += itp->cur.totreads;
			ftp->cur.totwrites += itp->cur.totwrites;
			systot.cur.totreads += itp->cur.totreads;
			systot.cur.totwrites += itp->cur.totwrites;
		}
	}
	if (status) {
		stats_diff(&systot.cur, &systot.last, &systot.use);
		for (ftp = flist; ftp; ftp = ftp->next) {
			stats_diff(&ftp->cur, &ftp->last, &ftp->use);
		}
	} else {
		systot.use = systot.cur;
		for (ftp = flist; ftp; ftp = ftp->next) {
			ftp->use = ftp->cur;
		}
	}
	output_times(start, stop, brp, &ru, status);
	fflush(fpout);
	if (status) {
		systot.last = systot.cur;
		for (ftp = flist; ftp; ftp = ftp->next) {
			ftp->last = ftp->cur;
		}
		last = ru;
	} else {
		iob_details_reset(1);
		memset(&testsummary, 0, sizeof (struct summary));
		memset(&systot.last, 0, sizeof (struct iostats));
		for (ftp = flist; ftp; ftp = ftp->next) {
			memset(&ftp->last, 0, sizeof (struct iostats));
			for (itp = ftp->ithreads; itp; itp = itp->next) {
				memset(&itp->cur, 0, sizeof (struct iostats));
			}
		}
	}
	return;
}

STATIC void
stats_diff(struct iostats *cp, struct iostats *lp, struct iostats *up)
{
	int	i, j;

	for (j = 0; j < N_OPTYPES; j++) {
		up->totlat[j] = cp->totlat[j] - lp->totlat[j];
		up->totbytes[j] = cp->totbytes[j] - lp->totbytes[j];
	}
	up->totreads = cp->totreads - lp->totreads;
	up->totwrites = cp->totwrites - lp->totwrites;
	for (i = 0; i < NLATBKTS; i++) {
		for (j = 0; j < N_OPTYPES; j++) {
			up->latbuckets[i][j] = cp->latbuckets[i][j] - lp->latbuckets[i][j];
		}
	}
	return;
}

STATIC void
output_header(void)
{
	struct file	*ftp;
	struct itype	*ip;
	char		name[256];
	int		i;

	if (fpout == NULL) {
		memset(&testsummary, 0, sizeof (struct summary));
		memset(&systot, 0, sizeof (struct file));
		systot.filename = "Total All Files";
		snprintf(name, 256, "%s.csv", outputfile);
		if ((fpout = fopen(name, "w")) == NULL) {
			iob_errlog(0, "Can't create output file %s\n", name);
		}
		fprintf(fpout, "\n");
		fprintf(fpout, "Test Parameters,\n\n");
		ftp = flist;
		fprintf(fpout, ",\tignore errors,\t\t%s,\n", ign_errors ? "yes" : "no");
		fprintf(fpout, ",\tinit threads,\t\t%d,\n", ftp->params.initthreads);
		fprintf(fpout, ",\tmax threads,\t\t%d,\n", ftp->params.maxthreads);
		fprintf(fpout, ",\tincr threads,\t\t%d,\n", ftp->params.incrthreads);
		fprintf(fpout, ",\theader size,\t\t%llu KB,\n", (long long unsigned)ftp->params.headersz >> 10);
		fprintf(fpout, ",\tmin seek offset,\t%llu MB,\n", (long long unsigned)ftp->params.minoff >> 20);
		fprintf(fpout, ",\tmax seek offset,\t%llu MB,\n", (long long unsigned)ftp->params.maxoff >> 20);
		fprintf(fpout, ",\tmultiplier,\t\t%d,\n", ftp->params.multiplier);
		fprintf(fpout, ",\ttest interval,\t\t%lld seconds,\n",
			(long long int)ftp->params.interval / IOB_NSEC_TO_SEC);
		fprintf(fpout, ",\twarmup time,\t\t%lld seconds,\n",
			(long long int)ftp->params.warmup / IOB_NSEC_TO_SEC);
		fprintf(fpout, ",\tstatus interval,\t%u seconds,\n", ftp->params.status);
		fprintf(fpout, ",\tmaximum latency,\t%u us,\n", ftp->params.maxlatency / 1000);
		fprintf(fpout, ",\tmaximum iosize,\t\t%d,\n", ftp->params.maxiosize);
		for (i = 1, ip = flist->itlist; ip; ip = ip->next, i++) {
			fprintf(fpout, "\n\tiotype %d,\n", i);
			fprintf(fpout, ",\t\tpct of I/O,\t\t%.6f,\n", ip->pct * 100.0);
			if (ip->optype == READ) {
				fprintf(fpout, ",\t\ttype of I/O,\t\tRead,\n");
				fprintf(fpout, ",\t\tsize of I/O,\t\t%d,\n", ip->size);
			} else if (ip->optype == WRITE) {
				fprintf(fpout, ",\t\ttype of I/O,\t\tWrite,\n");
				fprintf(fpout, ",\t\tsize of I/O,\t\t%d,\n", ip->size);
			} else {
				fprintf(fpout, ",\t\ttype of I/O,\t\tSleep,\n");
				fprintf(fpout, ",\t\tmax sleep time,\t\t%d,\n", ip->size);
			}
			if (ip->optype == WRITE) {
				fprintf(fpout, ",\t\ttype of data,\t\t%s,\n", iob_dttostr(ip->dtype));
				if (IOB_IS_DEDUPABLE(ip->dtype)) {
					fprintf(fpout, ",\t\tunique pages,\t\t%llu,\n",
						((long long unsigned)ip->dedupblks * IOB_NDATAPAGES * (IOB_PAGE / IOB_SECTOR)));
				}
				fprintf(fpout, ",\t\tpct of repeats,\t%.6f,\n", ip->repwrite * 100.0);
			}
			if (ip->longlat == ~(uint64_t)0) {
				fprintf(fpout, ",\t\treport long latency,\toff,\n");
			} else {
				fprintf(fpout, ",\t\treport long latency,\t%llu,\n",
					(unsigned long long)ip->longlat / 1000);
			}
		}
	}
	return;
}

STATIC void
output_times(uint64_t start, uint64_t stop, struct rusage *brp, struct rusage *rup, int status)
{
	struct file	*ftp;
	uint64_t	us;
	time_t		seconds;
	double		totsec;
	char		*timestr, *prefix;

	seconds = time(NULL);
	timestr = ctime(&seconds);
	timestr[24] = '\0';
	if (status) {
		fprintf(fpout, "\n\nStatus,  Test with %d threads per file, status at %s\n\n", current->threads, timestr);
		prefix = "Status,  ";
	} else {
		fprintf(fpout, "\n\nSummary, Test with %d threads per file, completed at %s\n\n", current->threads, timestr);
		prefix = "Summary, ";
		latency_flag = 0;
	}
	totsec = (double)(stop - start) / (double)IOB_NSEC_TO_SEC;
	fprintf(fpout, "%sCPU Time,\n\n", prefix);
	fprintf(fpout, "%stotal time,         %9.3f, seconds\n", prefix, totsec);
	us = (rup->ru_stime.tv_sec - brp->ru_stime.tv_sec) * 1000000 + rup->ru_stime.tv_usec - brp->ru_stime.tv_usec;
	fprintf(fpout, "%stotal system time,  %9.3f, seconds\n", prefix, (double)us / 1000000.0);
	us = (rup->ru_utime.tv_sec - brp->ru_utime.tv_sec) * 1000000 + rup->ru_utime.tv_usec - brp->ru_utime.tv_usec;
	fprintf(fpout, "%stotal user time,    %9.3f, seconds\n\n", prefix, (double)us / 1000000.0);
	fprintf(fpout, "\n%sThroughput,\n\n", prefix);
	fprintf(fpout, "%s", prefix);
	printname(fpout, &systot);
	fprintf(fpout, "Reads, %9.2f, R/s,  %10.2f, KB/s,\tWrites, %9.2f, W/s,  %10.2f, KB/s\n",
		(double)systot.use.totreads / totsec, (double)systot.use.totbytes[READ] / totsec / 1024.0,
		(double)systot.use.totwrites / totsec, (double)systot.use.totbytes[WRITE] / totsec / 1024.0);
	testsummary.readiops = systot.use.totreads / totsec;
	testsummary.writeiops = systot.use.totwrites / totsec;
	if (verbose) {
		for (ftp = flist; ftp; ftp = ftp->next) {
			fprintf(fpout, "%s", prefix);
			printname(fpout, ftp);
			fprintf(fpout, "Reads, %9.2f, R/s,  %10.2f, KB/s,\tWrites, %9.2f, W/s,  %10.2f, KB/s\n",
				(double)ftp->use.totreads / totsec, (double)ftp->use.totbytes[READ] / totsec / 1024.0,
				(double)ftp->use.totwrites / totsec, (double)ftp->use.totbytes[WRITE] / totsec / 1024.0);
		}
	}
	fprintf(fpout, "\n%sLatency,\n\n", prefix);
	printbuckets(fpout, &systot, prefix);
	fprintf(fpout, "\n");
	if (verbose) {
		for (ftp = flist; ftp; ftp = ftp->next) {
			printbuckets(fpout, ftp, prefix);
			fprintf(fpout, "\n");
		}
	}
	if (status) {
		iob_infolog("Status,   %4d, threads,  %7d, R/s,  %7d, W/s,  %5d, 90%% read lat (us),  %5d, 90%% write lat (us)\n",
			   current->threads, testsummary.readiops, testsummary.writeiops,
			   testsummary.readlat, testsummary.writelat);
		fprintf(fpout,
			"\nStatus,   %4d, threads,  %7d, R/s,  %7d, W/s,  %5d, 90%% read lat (us),  %5d, 90%% write lat (us)\n",
			current->threads, testsummary.readiops, testsummary.writeiops,
			testsummary.readlat, testsummary.writelat);
	} else {
		iob_infolog("Summary,  %4d, threads,  %7d, R/s,  %7d, W/s,  %5d, 90%% read lat (us),  %5d, 90%% write lat (us)\n",
			   current->threads, testsummary.readiops, testsummary.writeiops,
			   testsummary.readlat, testsummary.writelat);
		fprintf(fpout,
			"\nSummary,  %4d, threads,  %7d, R/s,  %7d, W/s,  %5d, 90%% read lat (us),  %5d, 90%% write lat (us)\n\n",
			current->threads, testsummary.readiops, testsummary.writeiops,
			testsummary.readlat, testsummary.writelat);
		if (latency_flag == 1) {
			iob_infolog("\nNinetieth percentile read latency has exceeded test limits, HALTING\n");
			fprintf(fpout, "\nNinetieth percentile read latency has exceeded test limits, HALTING\n");
			exit(3);
		} else if (latency_flag == 2) {
			iob_infolog("\nNinetieth percentile write latency has exceeded test limits, HALTING\n");
			fprintf(fpout, "\nNinetieth percentile write latency has exceeded test limits, HALTING\n");
			exit(3);
		}
	}
	if (longlats) {
		output_details(status);
	}
	return;
}

STATIC uint32_t lgmax[] = { 10000, 0xffffffff };
STATIC uint32_t lgroup[] = { 5, 1000 };
STATIC int      lngroups = 2;

#define	NPERCENT	5
#define	NTYPE		2

double		percentile[NPERCENT] = { 0.5, 0.1, 0.01, 0.001, 0.0001 };
uint64_t	pct[NPERCENT][NTYPE];
int		pctval[NPERCENT][NTYPE];

STATIC void
printbuckets(FILE *fp, struct file *ftp, char *prefix)
{
	double		rpct, wpct;
	uint64_t	rdtot, wrtot, rc, wc;
	uint64_t	totio;
	int		i, latency, jr, jw, gn, j;

	memset(pct, 0, sizeof (pct));
	totio = ftp->use.totreads + ftp->use.totwrites;
	if (totio == 0ULL) {
		totio = 1ULL;
	}
	if (ftp->use.totreads == 0ULL) {
		ftp->use.totreads = 1ULL;
	}
	if (ftp->use.totwrites == 0ULL) {
		ftp->use.totwrites = 1ULL;
	}
	for (i = 0; i < NPERCENT; i++) {
		pct[i][0] = ftp->use.totreads - (uint64_t)((double)ftp->use.totreads * percentile[i]);
		pct[i][1] = ftp->use.totwrites - (uint64_t)((double)ftp->use.totwrites * percentile[i]);
	}
	latency = 0;
	rdtot = 0ULL;
	wrtot = 0ULL;
	jr = 0;
	jw = 0;
	for (i = 0; i < NLATBKTS; i++) {
		rdtot += ftp->use.latbuckets[i][READ];
		wrtot += ftp->use.latbuckets[i][WRITE];
		while (jr < NPERCENT && rdtot >= pct[jr][0]) {
			pctval[jr][0] = (i + 1) * LATDIV;
			jr++;
		}
		while (jw < NPERCENT && wrtot >= pct[jw][1]) {
			pctval[jw][1] = (i + 1) * LATDIV;
			jw++;
		}
	}
	fprintf(fp, "%s", prefix);
	printname(fp, ftp);
	fprintf(fp, "Average,\tReads,  %7lld, (us),\tWrites, %7lld, (us)\n",
		ftp->use.totlat[READ] / ftp->use.totreads / 1000ULL,
		ftp->use.totlat[WRITE] / ftp->use.totwrites / 1000ULL);
	for (i = 0; i < NPERCENT; i++) {
		fprintf(fp, "%s", prefix);
		printname(fp, ftp);
		fprintf(fp, "%.2f%%,\t\tReads,  %7d, (us),\tWrites, %7d, (us)\n",
			(1.0 - percentile[i]) * 100.0, pctval[i][0] / 1000, pctval[i][1] / 1000);
	}
	if (ftp == &systot) {
		testsummary.readlat = pctval[1][0] / 1000;
		testsummary.writelat = pctval[1][1] / 1000;
		if (pctval[1][0] > flist->params.maxlatency) {
			latency_flag = 1;
		} else if (pctval[1][1] > flist->params.maxlatency) {
			latency_flag = 2;
		}
	}
	if (lat_details) {
		fprintf(fp, "\n");
		fprintf(fp, "%s       ,        Read,       Read,         Write,        Write,\n", prefix);
		fprintf(fp, "%sLatency,       Count,        Pct,         Count,         Pct,\n", prefix);
		rdtot = 0ULL;
		wrtot = 0ULL;
		gn = 0;
		i = 0;
		while (i < NLATBKTS) {
			rc = 0ULL;
			wc = 0ULL;
			for (j = 0; j < lgroup[gn] && i < NLATBKTS; j++, i++) {
				rc += ftp->use.latbuckets[i][READ];
				wc += ftp->use.latbuckets[i][WRITE];
				latency += LATDIV / 1000;
			}
			rdtot += rc;
			wrtot += wc;
			rpct = (double)1.0 - ((double)rdtot / (double)ftp->use.totreads);
			wpct = (double)1.0 - ((double)wrtot / (double)ftp->use.totwrites);
			fprintf(fp, "%s%7d,    %8lld,   %11.9f,", prefix, latency, (long long unsigned)rc, rpct);
			fprintf(fp, "   %8lld,    %11.9f,\n", (long long unsigned)wc, wpct);
			if (latency >= lgmax[gn] && gn < lngroups) {
				gn++;
			}
		}
	}
	return;
}

STATIC void
printname(FILE *fp, struct file *ftp)
{
	char	string[256];
	int	len;

	snprintf(string, 256, "%s,", ftp->filename);
	len = strlen(string);
	fprintf(fp, "%s", string);
	while (len < 32) {
		fprintf(fp, "\t");
		len = (len + 8) & ~0x7;
	}
	return;
}

STATIC void
output_details(int status)
{

	if (status) {
		fprintf(fpout, "\n");
	}
	fprintf(fpout, "details for long I/Os,\n\n");
	iob_details_output(fpout, 1, "iodet, ", 0, 0, 0);
	iob_details_reset(0);
	return;
}
