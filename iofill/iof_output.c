#include	"iofill.h"

STATIC void	output_times(uint64_t start, uint64_t stop, struct rusage *brp);
STATIC void	output_seconds(void);
STATIC void	output_details(void);
STATIC void	output_latency(void);
STATIC void	printbuckets(FILE *fp, struct fthread *tp);
STATIC void	printname(FILE *fp, struct fthread *ftp);

STATIC uint32_t	lgmax[] = { 10000, 0xffffffff };
STATIC uint32_t lgroup[] = { 5, 1000 };
STATIC int	lngroups = 2;

STATIC struct fthread	systot;

void
output(uint64_t start, uint64_t stop, struct rusage *brp)
{
	struct fthread	*ftp;
	struct iothread	*itp;
	int		i, j;

	memset(&systot, 0, sizeof (struct fthread));
	systot.filename = "All Files";
	for (ftp = flist; ftp; ftp = ftp->next) {
		for (itp = ftp->ithreads; itp; itp = itp->next) {
			for (j = 0; j < N_OPTYPES; j++) {
				ftp->totlat[j] += itp->totlat[j];
				ftp->totbytes[j] += itp->totbytes[j];
				systot.totlat[j] += itp->totlat[j];
				systot.totbytes[j] += itp->totbytes[j];
			}
			for (i = 0; i < NLATBKTS; i++) {
				for (j = 0; j < N_OPTYPES; j++) {
					ftp->latbuckets[i][j] += itp->latbuckets[i][j];
					systot.latbuckets[i][j] += itp->latbuckets[i][j];
				}
				itp->totreads += itp->latbuckets[i][READ];
				itp->totwrites += itp->latbuckets[i][WRITE];
				itp->totflushes += itp->latbuckets[i][FLUSH];
				itp->totpflushes += itp->latbuckets[i][POSTFLUSH];
			}
			ftp->totreads += itp->totreads;
			ftp->totwrites += itp->totwrites;
			ftp->totflushes += itp->totflushes;
			ftp->totpflushes += itp->totpflushes;
			systot.totreads += itp->totreads;
			systot.totwrites += itp->totwrites;
			systot.totflushes += itp->totflushes;
			systot.totpflushes += itp->totpflushes;
		}
	}
	output_times(start, stop, brp);
	output_latency();
	output_details();
	output_seconds();
	return;
}

STATIC void
output_times(uint64_t start, uint64_t stop, struct rusage *brp)
{
	struct fthread	*ftp;
	struct iothread	*itp;
	struct rusage	rup;
	FILE		*fp;
	uint64_t	us;
	double		totsec;
	char		name[256];

	(void) getrusage(RUSAGE_SELF, &rup);
	snprintf(name, 256, "%s_results.csv", outputfile);
	if ((fp = fopen(name, "w")) == NULL) {
		iob_errlog(0, "Can't create output file %s\n", name);
	}
	totsec = (double)(stop - start) / (double)IOB_NSEC_TO_SEC;
	fprintf(fp, "total time,        %9.3f seconds,\n", totsec);
	us = (rup.ru_stime.tv_sec - brp->ru_stime.tv_sec) * 1000000 + rup.ru_stime.tv_usec - brp->ru_stime.tv_usec;
	fprintf(fp, "total system time, %9.3f seconds,\n", (double)us / 1000000.0);
	us = (rup.ru_utime.tv_sec - brp->ru_utime.tv_sec) * 1000000 + rup.ru_utime.tv_usec - brp->ru_utime.tv_usec;
	fprintf(fp, "total   user time, %9.3f seconds,\n\n", (double)us / 1000000.0);
	if (flist->next) {
		fprintf(fp, "            Total All Files,    ");
		fprintf(fp, "Reads, %9.2f R/s,  %10.2f KB/s,  Writes, %9.2f W/s,  %10.2f KB/s,\n",
			(double)systot.totreads / totsec, (double)systot.totbytes[READ] / totsec / 1024.0,
			(double)systot.totwrites / totsec, (double)systot.totbytes[WRITE] / totsec / 1024.0);
	}
	for (ftp = flist; ftp; ftp = ftp->next) {
		printname(fp, ftp);
		totsec = (double)(ftp->stoptime - ftp->starttime) / (double)IOB_NSEC_TO_SEC;
		fprintf(fp, "Reads, %9.2f R/s,  %10.2f KB/s,  Writes, %9.2f W/s,  %10.2f KB/s,\n",
			(double)ftp->totreads / totsec, (double)ftp->totbytes[READ] / totsec / 1024.0,
			(double)ftp->totwrites / totsec, (double)ftp->totbytes[WRITE] / totsec / 1024.0);
	}
	fprintf(fp, "\n");
	for (ftp = flist; ftp; ftp = ftp->next) {
		if (ftp->ithreads->next == NULL) {
			continue;
		}
		for (itp = ftp->ithreads; itp; itp = itp->next) {
			printname(fp, ftp);
			totsec = (double)(itp->stoptime - itp->starttime) / (double)IOB_NSEC_TO_SEC;
			fprintf(fp, "Thread %2d,  Reads, %9.2f R/s,  %10.2f KB/s,  Writes, %9.2f W/s,  %10.2f KB/s,\n",
				itp->threadid,
				(double)itp->totreads / totsec, (double)itp->totbytes[READ] / totsec / 1024.0,
				(double)itp->totwrites / totsec, (double)itp->totbytes[WRITE] / totsec / 1024.0);
		}
		fprintf(fp, "\n");
	}
	fprintf(fp, "Test Parameters,\n\n");
	for (ftp = flist; ftp; ftp = ftp->next) {
		printname(fp, ftp);
		fprintf(fp, "\n");
		fprintf(fp, ",\tnumber of repetitions,\t%llu,\n", (long long unsigned)ftp->params.nrep);
		fprintf(fp, ",\tnumber of I/Os,\t\t%llu,\n", (long long unsigned)ftp->params.nio);
		fprintf(fp, ",\tsize of I/Os,\t\t%d,\n", ftp->params.size);
		fprintf(fp, ",\tnthreads,\t\t%d,\n", ftp->params.nthreads);
		fprintf(fp, ",\theader size,\t\t%llu KB,\n", (long long unsigned)ftp->params.headersz / 1024);
		fprintf(fp, ",\tinitial offset,\t\t%llu KB,\n", (long long unsigned)ftp->params.seekoff / 1024);
		fprintf(fp, ",\tmin offset,\t\t%llu KB,\n", (long long unsigned)ftp->params.minoff / 1024);
		fprintf(fp, ",\tmax offset,\t\t%llu KB,\n", (long long unsigned)ftp->params.maxoff / 1024);
		fprintf(fp, ",\tskip,\t\t\t%d,\n", ftp->params.skip);
		fprintf(fp, ",\tmultiplier,\t\t%d,\n", ftp->params.multiplier);
		fprintf(fp, ",\tbase random seed,\t%d,\n", ftp->params.randseed);
		if (ftp->params.maxtime) {
			fprintf(fp, ",\tmax run time,\t\t%lld seconds,\n",
				(long long int)ftp->params.maxtime / IOB_NSEC_TO_SEC);
		} else {
			fprintf(fp, ",\tmax run time,\t\tno limit,\n");
		}
		fprintf(fp, ",\tdata type,\t\t%s,\n", iob_dttostr(ftp->params.datatype));
		fprintf(fp, ",\tdedup blks,\t\t%llu,\n",
			(long long unsigned)ftp->params.dedupblks * IOB_NDATAPAGES * (IOB_PAGE / IOB_SECTOR));
		fprintf(fp, "\n");
	}
	fclose(fp);
	return;
}

STATIC void
output_seconds(void)
{
	struct fthread	*ftp;
	struct iothread	*itp;
	FILE		*fp;
	char		name[256];
	int		i, maxsec, totio;

	snprintf(name, 256, "%s_seconds.csv", outputfile);
	if ((fp = fopen(name, "w")) == NULL) {
		iob_errlog(0, "Can't create seconds file %s\n", name);
	}
	for (ftp = flist; ftp; ftp = ftp->next) {
		printname(fp, ftp);
		fprintf(fp, "\n");
	}
	maxsec = 0;
	fprintf(fp, "\nI/Os per thread each second,\n");
	fprintf(fp, "Seconds,");
	for (ftp = flist; ftp; ftp = ftp->next) {
		for (itp = ftp->ithreads; itp; itp = itp->next) {
			snprintf(name, 256, "F%dT%d", ftp->fileno, itp->threadid);
			fprintf(fp, "  %6s,", name);
			if (itp->nseconds > maxsec) {
				maxsec = itp->nseconds;
			}
		}
	}
	if (flist->next) {
		fprintf(fp, "  %6s,", "ALL");
	}
	if (maxsec < NSEC) {
		maxsec++;
	}
	fprintf(fp, "\n");
	for (i = 0; i < maxsec; i++) {
		totio = 0;
		fprintf(fp, "%7d,", i);
		for (ftp = flist; ftp; ftp = ftp->next) {
			for (itp = ftp->ithreads; itp; itp = itp->next) {
				fprintf(fp, "  %6d,", itp->seconds[i]);
				totio += itp->seconds[i];
			}
		}
		if (flist->next) {
			fprintf(fp, "  %6d,", totio);
		}
		fprintf(fp, "\n");
	}
	fclose(fp);
	return;
}

STATIC void
output_details(void)
{
	struct fthread	*ftp;
	FILE		*fp;
	char		name[256];

	snprintf(name, 256, "%s_details.csv", outputfile);
	if ((fp = fopen(name, "w")) == NULL) {
		iob_errlog(0, "Can't create detailed file %s\n", name);
	}
	for (ftp = flist; ftp; ftp = ftp->next) {
		printname(fp, ftp);
		fprintf(fp, "\n");
		fprintf(fp, ",\tdetails for I/O longer than,\t\t%lld us,\n",
			(long long int)ftp->params.iodettime / 1000);
	}
	fprintf(fp, "\n");
	iob_details_output(fp, 0, "", 0, 0, 0);
	fclose(fp);
	return;
}

STATIC void
output_latency(void)
{
	struct fthread	*ftp;
	FILE		*fp;
	char		name[256];

	snprintf(name, 256, "%s_latency.csv", outputfile);
	if ((fp = fopen(name, "w")) == NULL) {
		iob_errlog(0, "Can't create latency file %s\n", name);
	}
	if (flist->next) {
		printname(fp, &systot);
		fprintf(fp, "\n\n");
		printbuckets(fp, &systot);
		fprintf(fp, "\n\n");
	}
	for (ftp = flist; ftp; ftp = ftp->next) {
		printname(fp, ftp);
		fprintf(fp, "\n\n");
		printbuckets(fp, ftp);
		fprintf(fp, "\n\n");
	}
	fclose(fp);
	return;
}

#define	NPERCENT	5
#define	NTYPE		2

double		percentile[NPERCENT] = { 0.5, 0.1, 0.01, 0.001, 0.0001 };
uint64_t	pct[NPERCENT][NTYPE];
int		pctval[NPERCENT][NTYPE];

STATIC void
printbuckets(FILE *fp, struct fthread *ftp)
{
	uint64_t	fltot, rdtot, wrtot, rc, wc, fc, pc, pfltot;
	uint64_t	avgio, totio;
	double		rpct, wpct, fpct, pfpct;
	int		i, latency, j, gn, jr, jw;

	memset(pct, 0, sizeof (pct));
	totio = ftp->totreads + ftp->totwrites;
	if (totio == 0ULL) {
		totio = 1ULL;
	}
	if (ftp->totreads == 0ULL) {
		ftp->totreads = 1ULL;
	}
	if (ftp->totwrites == 0ULL) {
		ftp->totwrites = 1ULL;
	}
	if (ftp->totflushes == 0ULL) {
		ftp->totflushes = 1ULL;
	}
	if (ftp->totpflushes == 0ULL) {
		ftp->totpflushes = 1ULL;
	}
	for (i = 0; i < NPERCENT; i++) {
		pct[i][0] = ftp->totreads - (uint64_t)((double)ftp->totreads * percentile[i]);
		pct[i][1] = ftp->totwrites - (uint64_t)((double)ftp->totwrites * percentile[i]);
	}
	avgio = (ftp->totlat[READ] + ftp->totlat[WRITE]) / totio / 1000ULL;
	latency = 0;
	printname(fp, ftp);
	fprintf(fp, "Average Read Latency,  %8lld (us),\n",
		ftp->totlat[READ] / ftp->totreads / 1000ULL);
	printname(fp, ftp);
	fprintf(fp, "Average Write Latency, %8lld (us),\n",
		ftp->totlat[WRITE] / ftp->totwrites / 1000ULL);
	printname(fp, ftp);
	fprintf(fp, "Average R+W Latency,   %8llu (us),\n\n", (long long unsigned)avgio);
	rdtot = 0ULL;
	wrtot = 0ULL;
	jr = 0;
	jw = 0;
	for (i = 0; i < NLATBKTS; i++) {
		rdtot += ftp->latbuckets[i][READ];
		wrtot += ftp->latbuckets[i][WRITE];
		while (jr < NPERCENT && rdtot >= pct[jr][0]) {
			pctval[jr][0] = (i + 1) * LATDIV;
			jr++;
		}
		while (jw < NPERCENT && wrtot >= pct[jw][1]) {
			pctval[jw][1] = (i + 1) * LATDIV;
			jw++;
		}
	}
	for (i = 0; i < NPERCENT; i++) {
		printname(fp, ftp);
		fprintf(fp, "Percentile  %.2f%%      Reads  %8d (us)      Writes %8d (us)\n",
			(1.0 - percentile[i]) * 100.0, pctval[i][0] / 1000, pctval[i][1] / 1000);
	}
	fprintf(fp, "\n");
	fprintf(fp, "       ,        Read,       Read,         Write,        Write,         Flush,        Flush,        PFlush,       PFlush,\n");
	fprintf(fp, "Latency,       Count,        Pct,         Count,         Pct,          Count,         Pct,          Count,         Pct,\n");
	rdtot = 0ULL;
	wrtot = 0ULL;
	fltot = 0ULL;
	pfltot = 0ULL;
	gn = 0;
	i = 0;
	while (i < NLATBKTS) {
		rc = 0ULL;
		wc = 0ULL;
		fc = 0ULL;
		pc = 0ULL;
		for (j = 0; j < lgroup[gn] && i < NLATBKTS; j++, i++) {
			rc += ftp->latbuckets[i][READ];
			wc += ftp->latbuckets[i][WRITE];
			fc += ftp->latbuckets[i][FLUSH];
			pc += ftp->latbuckets[i][POSTFLUSH];
			latency += LATDIV / 1000;
		}
		rdtot += rc;
		wrtot += wc;
		fltot += fc;
		pfltot += pc;
		rpct = (double)1.0 - ((double)rdtot / (double)ftp->totreads);
		wpct = (double)1.0 - ((double)wrtot / (double)ftp->totwrites);
		fpct = (double)1.0 - ((double)fltot / (double)ftp->totflushes);
		pfpct = (double)1.0 - ((double)pfltot / (double)ftp->totpflushes);
		fprintf(fp, "%7d,    %8lld,   %11.9f,", latency, (long long unsigned)rc, rpct);
		fprintf(fp, "   %8lld,    %11.9f,", (long long unsigned)wc, wpct);
		fprintf(fp, "   %8lld,    %11.9f,", (long long unsigned)fc, fpct);
		fprintf(fp, "   %8lld,    %11.9f,\n", (long long unsigned)pc, pfpct);
		if (latency >= lgmax[gn] && gn < lngroups) {
			gn++;
		}
	}
	return;
}

STATIC void
printname(FILE *fp, struct fthread *ftp)
{
	char	string[256];

	snprintf(string, 256, "File%-2d  %s,", ftp->fileno, ftp->filename);
	fprintf(fp, "%-32s", string);
	return;
}
