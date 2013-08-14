//
// iof_main.c
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

struct fthread		*flist = NULL;
char			*outputfile = "output";
uint64_t		statusmb = (uint64_t)4096 << 20;

struct cmdparams	defparams = {
	0.0,					/* readpct */
	0,					/* randseed */
	1,					/* seqflag */
	1048576,				/* size */
	512,					/* altsize */
	(uint64_t)0,				/* altcount */
	(uint64_t)1 << 40,			/* nio */
	(uint64_t)1,				/* nrep */
	(uint64_t)0,				/* seekoff */
	(uint64_t)1024,				/* headersz */
	0,					/* skip */
	1,					/* nthreads */
	1,					/* streams */
	(uint64_t)0,				/* wroff */
	(uint64_t)0,				/* minoff */
	(uint64_t)4 << 60,			/* maxoff */
	1,					/* multiplier */
	0,					/* partition */
	O_RDONLY | IOB_DIRECT,			/* openflags */
	0,					/* writeinject */
	131072,					/* writesize */
	16 << 20,				/* segsize */
	1,					/* nsegments */
	1,					/* nsubseg */
	0,					/* randinject */
	0,					/* threadalign */
	0,					/* flush */
	1,					/* flushcount */
	SYNC_CACHE_16,				/* flushtype */
	0,					/* preflushtype */
	0,					/* postflushtype */
	4096,					/* flushalign */
	(uint64_t)1000000000,			/* flushinterval */
	(uint64_t)1000000000,			/* preflushinterval */
	(uint64_t)1000000000,			/* postflushinterval */
	(uint64_t)1000000000,			/* injectinterval */
	(uint64_t)~0,				/* injectinterval_io */
	(uint64_t)10000,			/* iodetcnt */
	1,					/* uniform */
	(uint64_t)100000000,			/* iodettime */
	(uint64_t)25000,			/* nsleeptime */
	(uint64_t)500000,			/* recoverytime */
	(uint64_t)3600000 * IOB_NSEC_TO_SEC,	/* maxtime */
	0,					/* dynamicrecovery */
	IOB_DT_VM,				/* datatype */
	(uint64_t)1 << 50,			/* dedupblks */
};

char *tstopts[] = {
#define	T_SIZE		0
	"size",
#define	T_COUNT		1
	"count",
#define	T_NREP		2
	"nrep",
#define	T_SKIP		3
	"skip",
	NULL
};

char *modopts[] = {
#define	M_THREADS	0
	"threads",
#define	M_SEEK		1
	"seek",
#define	M_SEED		2
	"seed",
#define	M_MAXRUN	3
	"maxrun",
#define	M_DTYPE		4
	"dtype",
#define	M_DEDUPBLKS	5
	"dedupblks",
#define	M_MAXOFF	6
	"maxoff",
#define	M_MINOFF	7
	"minoff",
#define	M_MULTIPLIER	8
	"multiplier",
#define	M_HEADERSZ	9
	"header",
	NULL
};

char *detopts[] = {
#define	D_COUNT		0
	"count",
#define	D_TIME		1
	"time",
	NULL
};

extern int	optind, opterr, optopt;
extern char	*optarg;

extern int	main(int argc, char **argv);
STATIC void	usage(void);
STATIC void	getdetopts(char *options, struct cmdparams *cp);
STATIC void	getmodopts(char *options, struct cmdparams *cp);
STATIC void	gettestopts(char *options, struct cmdparams *cp);

int
main(int argc, char **argv)
{
	struct cmdparams	*cp;
	char			**filenames;
	int			c, nfiles;

	iob_libinit(argv[0], usage);
	cp = &defparams;
	while ((c = getopt(argc, argv, "s:o:d:m:t:")) != -1) {
		switch (c) {

		case 's':
			statusmb = iob_strtoull(optarg, NULL, 0) << 20;
			if (statusmb == 0) {
				fprintf(stderr, "statusmb (%s) must be non-zero\n", optarg);
				usage();
			}
			break;

		case 'o':
			outputfile = strdup(optarg);
			break;

		case 'd':
			getdetopts(optarg, cp);
			break;

		case 'm':
			getmodopts(optarg, cp);
			break;

		case 't':
			gettestopts(optarg, cp);
			break;

		default:
			fprintf(stderr, "unknown option %c\n", optopt);
			usage();
			break;
		}
	}
	nfiles = argc - optind;
	filenames = argv + optind;
	if (nfiles <= 0) {
		usage();
	}
	if (cp->readpct < 1.0 || cp->writeinject) {
		cp->openflags = IOB_DIRECT | O_RDWR;
	}
	init_threads(nfiles, filenames, cp);
	runtest();
	exit(0);
}

STATIC void
usage(void)
{

	fprintf(stderr, "Usage: iofill [options] file1 [...]\n");
	fprintf(stderr, "Valid options are:\n\n");
	fprintf(stderr, "	-s num_mb		print status every num_mb megabytes (default is every 4GB)\n");
	fprintf(stderr, "	-o out_prefix		use out_prefix as prefix for output files\n");
	fprintf(stderr, "	-t test_opts\n");
	fprintf(stderr, "		size=n		size for I/Os (default is 1MB or 1048576)\n");
	fprintf(stderr, "		count=n		number of I/Os for test (default is 2^40)\n");
	fprintf(stderr, "		nrep=n		number of repetitions of fill (default is 1)\n");
	fprintf(stderr, "		skip=n		skip n I/Os between requests (default is off)\n");
	fprintf(stderr, "	-m mod_opts\n");
	fprintf(stderr, "		threads=n	use n threads (default it 1)\n");
	fprintf(stderr, "		seek=n		initial seek in KB at start (default is 0)\n");
	fprintf(stderr, "		minoff=n	min write offset in KB (default is 0)\n");
	fprintf(stderr, "		maxoff=n	max write offset in KB (default is 4EB)\n");
	fprintf(stderr, "		multipler=n	multiply offset by n to create sparse fill (default is off)\n");
	fprintf(stderr, "		seed=n		specify random number seed (default is current time)\n");
	fprintf(stderr, "		maxrun=n	maximum run time in seconds (default is 1000 hours)\n");
	fprintf(stderr, "		dedupblks=n	controls how many thousand pages in dedup set (default is 2^50)\n");
	fprintf(stderr, "		header=n	controls size of header (default is 1K)\n");
	fprintf(stderr, "		dtype=type	select the type of data to write\n");
	fprintf(stderr, "				c	write compressible data\n");
	fprintf(stderr, "				e	write email data from pst files\n");
	fprintf(stderr, "				h1 or h	write data compressible to approx 1%% of original\n");
	fprintf(stderr, "				h10	write data compressible to approx 10%% of original\n");
	fprintf(stderr, "				h20	write data compressible to approx 20%% of original\n");
	fprintf(stderr, "				h40	write data compressible to approx 40%% of original\n");
	fprintf(stderr, "				o	write a pattern of all ones\n");
	fprintf(stderr, "				p	write one byte repeating patterns\n");
	fprintf(stderr, "				u	write uncompressible data\n");
	fprintf(stderr, "				v	write virtual machine data (default)\n");
	fprintf(stderr, "				z	write a pattern of all zeros\n");
	fprintf(stderr, "	-d detail_opts\n");
	fprintf(stderr, "		count=n		maximum number of detailed I/Os to report per thread (default is 10000)\n");
	fprintf(stderr, "		time=n		report I/Os longer than n microseconds (default is 100ms)\n");
	exit(1);
}

STATIC void
getdetopts(char *options, struct cmdparams *cp)
{
	char	*value, *erropt;

	while (*options != '\0') {
		erropt = options;
		switch (getsubopt(&options, detopts, &value)) {

		case D_COUNT:
			cp->iodetcnt = iob_strtoull(value, NULL, 0);
			break;

		case D_TIME:
			cp->iodettime = iob_strtoull(value, NULL, 0) * 1000;
			break;

		default:
			fprintf(stderr, "unknown -d suboption %s\n", erropt);
			usage();
			break;
		}
	}
	return;
}

STATIC void
getmodopts(char *options, struct cmdparams *cp)
{
	char	*value, *erropt;

	while (*options != '\0') {
		erropt = options;
		switch (getsubopt(&options, modopts, &value)) {

		case M_THREADS:
			cp->nthreads = iob_strtoul(value, NULL, 0);
			break;

		case M_SEEK:
			cp->seekoff = iob_strtoull(value, NULL, 0) * 1024;
			break;

		case M_MINOFF:
			cp->minoff = iob_strtoull(value, NULL, 0) * 1024;
			break;

		case M_MAXOFF:
			cp->maxoff = iob_strtoull(value, NULL, 0) * 1024;
			break;

		case M_HEADERSZ:
			cp->headersz = iob_strtoull(value, NULL, 0) * 1024;
			break;

		case M_MULTIPLIER:
			cp->multiplier = iob_strtoul(value, NULL, 0);
			break;

		case M_SEED:
			cp->randseed = iob_strtoul(value, NULL, 0);
			break;

		case M_MAXRUN:
			cp->maxtime = (uint64_t)iob_strtoull(value, NULL, 0) * IOB_NSEC_TO_SEC;
			break;

		case M_DTYPE:
			iob_dtargparse(&cp->datatype, value);
			break;

		case M_DEDUPBLKS:
			cp->dedupblks = iob_strtoull(value, NULL, 0);
			if (cp->dedupblks == 0) {
				cp->dedupblks = 1;
			}
			if (cp->dedupblks > (uint64_t)1 << 50) {
				cp->dedupblks = (uint64_t)1 << 50;
			}
			break;

		default:
			fprintf(stderr, "unknown -m suboption %s\n", erropt);
			usage();
			break;
		}
	}
	return;
}

STATIC void
gettestopts(char *options, struct cmdparams *cp)
{
	char	*value, *erropt;

	while (*options != '\0') {
		erropt = options;
		switch (getsubopt(&options, tstopts, &value)) {

		case T_SIZE:
			cp->size = iob_strtoul(value, NULL, 0);
			if (cp->size < IOB_SECTOR || cp->size % IOB_SECTOR) {
				fprintf(stderr, "Error: I/O size (%d) must be a multiple of %d bytes\n",
					cp->size, IOB_SECTOR);
				usage();
			}
			break;

		case T_COUNT:
			cp->nio = iob_strtoull(value, NULL, 0);
			break;

		case T_NREP:
			cp->nrep = iob_strtoull(value, NULL, 0);
			break;

		case T_SKIP:
			cp->skip = iob_strtoull(value, NULL, 0);
			break;

		default:
			fprintf(stderr, "unknown -t suboption %s\n", erropt);
			usage();
			break;
		}
	}
	return;
}
