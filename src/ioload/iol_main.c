//
// iol_main.c
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

struct file		*flist = NULL;
char			*outputfile = "output";
int			verbose = 0;
int			lat_details = 0;
int			longlats = 0;
int			ign_errors = 0;

struct cmdparams	params = {
	(uint64_t)0,				/* minoff */
	(uint64_t)204800 << 20,			/* maxoff */
	(uint64_t)1024,				/* headersz */
	(uint64_t)60 * IOB_NSEC_TO_SEC,		/* warmup */
	(uint64_t)300 * IOB_NSEC_TO_SEC,	/* interval */
	2000 * 1000,				/* maxlatency */
	0,					/* maxiorate */
	0,					/* status */
	1,					/* multiplier */
	5,					/* initthreads */
	40,					/* maxthreads */
	5,					/* incrthreads */
	1048576,				/* maxiosize */
};

struct itype defitype = {
	NULL,					/* next itype */
	NULL,					/* buf */
	1.0,					/* percentage of all I/O */
	(uint64_t)1 << 50,			/* dedupblks */
	READ,					/* read or write */
	4096,					/* size */
	IOB_DT_VM,				/* data type */
	~(uint64_t)0,				/* long latency reporting threshold */
	0.0,					/* repwrite */
};

struct itype	*itlist = NULL;

char *itypeopts[] = {
#define	I_SIZE		0
	"size",
#define	I_PCT		1
	"pct",
#define	I_OP		2
	"op",
#define	I_DTYPE		3
	"dtype",
#define	I_DEDUPBLKS	4
	"dedupblks",
#define	I_LONGLAT	5
	"longlat",
#define	I_SLEEPTM	6
	"sleeptm",
#define	I_REPWRITE	7
	"repwrite",
	NULL
};

char *modopts[] = {
#define	M_MINOFF	0
	"minoff",
#define	M_MAXOFF	1
	"maxoff",
#define	M_WARMUP	2
	"warmup",
#define	M_INTERVAL	3
	"interval",
#define	M_INITTHREADS	4
	"initthreads",
#define	M_MAXTHREADS	5
	"maxthreads",
#define	M_INCRTHREADS	6
	"incrthreads",
#define	M_MAXLAT	7
	"maxlat",
#define	M_STATUS	8
	"status",
#define	M_MAXRATE	9
	"maxrate",
#define	M_MULTIPLER	10
	"multiplier",
#define	M_MAXIOSIZE	11
	"maxiosize",
#define	M_HEADERSZ	12
	"header",
	NULL
};

extern int	optind, opterr, optopt;
extern char	*optarg;

extern int	main(int argc, char **argv);
STATIC void	usage(void);
STATIC void	getmodopts(char *options);
STATIC void	getitypeopts(char *options);

int
main(int argc, char **argv)
{
	char			**filenames;
	int			c, nfiles;

	iob_libinit(argv[0], usage);
	while ((c = getopt(argc, argv, "nvlo:m:i:")) != -1) {
		switch (c) {

		case 'n':
			ign_errors = 1;
			break;

		case 'v':
			verbose = 1;
			break;

		case 'l':
			lat_details = 1;
			break;

		case 'o':
			outputfile = strdup(optarg);
			break;

		case 'm':
			getmodopts(optarg);
			break;

		case 'i':
			getitypeopts(optarg);
			break;

		default:
			fprintf(stderr, "unknown option %c\n", optopt);
			usage();
			break;
		}
	}
	if (itlist == NULL) {
		itlist = &defitype;
	}
	nfiles = argc - optind;
	filenames = argv + optind;
	if (nfiles <= 0) {
		usage();
	}
	init_threads(nfiles, filenames, &params, itlist);
	runtest();
	exit(0);
}

STATIC void
usage(void)
{

	fprintf(stderr, "Usage: ioload [options] file1 [...]\n");
	fprintf(stderr, "Valid options are:\n\n");
	fprintf(stderr, "	-n				ignore I/O errors\n");
	fprintf(stderr, "	-v				produce output details for each file\n");
	fprintf(stderr, "	-l				report latency histogram\n");
	fprintf(stderr, "	-o outfile			use outfile as basename for output file\n");
	fprintf(stderr, "	-i io_spec			each -d option adds a new I/O type\n");
	fprintf(stderr, "		pct=n			percentage of all test I/O that will be this type\n");
	fprintf(stderr, "		size=n			size of I/O (default is 4096)\n");
	fprintf(stderr, "		sleeptm=n		time to sleep in us (default is 5000)\n");
	fprintf(stderr, "		op=read|write|sleep	type of I/O operation (default is read)\n");
	fprintf(stderr, "		dtype=type		select the type of data for writes\n");
	fprintf(stderr, "					c	write compressible data\n");
	fprintf(stderr, "					e	write email data from pst files\n");
	fprintf(stderr, "					h1 or h	write data compressible to approx 1%% of original\n");
	fprintf(stderr, "					h10	write data compressible to approx 10%% of original\n");
	fprintf(stderr, "					h20	write data compressible to approx 20%% of original\n");
	fprintf(stderr, "					h40	write data compressible to approx 40%% of original\n");
	fprintf(stderr, "					o	write a pattern of all ones\n");
	fprintf(stderr, "					p	write one byte patterns in each sector\n");
	fprintf(stderr, "					u	write uncompressible data\n");
	fprintf(stderr, "					v	write virtual machine data (default)\n");
	fprintf(stderr, "					z	write a pattern of all zeros\n");
	fprintf(stderr, "		dedupblks=n		controls how many thousand pages in dedup set (default is 2^50)\n");
	fprintf(stderr, "		repwrite=pct		percentage or repeat writes (default is 0.0)\n");
	fprintf(stderr, "		longlat=n		report I/Os longer than n microseconds (default is off)\n");
	fprintf(stderr, "	-m mod_opts\n")	;
	fprintf(stderr, "		minoff=n		minimum seek offset in MB (default is 0)\n");
	fprintf(stderr, "		maxoff=n		maximum seek offset in MB (default is 200GB (204800))\n");
	fprintf(stderr, "		multipler=n		multipler for sparse offsets (default is off)\n");
	fprintf(stderr, "		initthreads=n		use n threads in initial test (default is 5)\n");
	fprintf(stderr, "		maxthreads=n		increase load to a maximum of n threads (default is 40)\n");
	fprintf(stderr, "		incrthreads=n		increase load by n threads each inteval (default is 5)\n");
	fprintf(stderr, "		warmup=n		run for n seconds before measurement (default is 60)\n");
	fprintf(stderr, "		interval=n		run for n seconds before increasing load (default is 300)\n");
	fprintf(stderr, "		status=n		during run report status every n seconds (default is 0)\n");
	fprintf(stderr, "		header=n		controls size of header (default is 1K)\n");
	fprintf(stderr, "		maxlat=n		stop test at 90th percentile latency (in us) (default is 2000)\n");
	fprintf(stderr, "		maxrate=n		maximum I/O rate per thread (default is unlimited (0))\n");
	fprintf(stderr, "		maxiosize=n		breakup I/Os larger than n bytes (default is 1048576 (1MB))\n");
	exit(1);
}

STATIC void
getmodopts(char *options)
{
	struct cmdparams	*cp = &params;
	char			*value, *erropt;

	while (*options != '\0') {
		erropt = options;
		switch (getsubopt(&options, modopts, &value)) {

		case M_MINOFF:
			cp->minoff = iob_strtoull(value, NULL, 0) << 20;
			if (cp->minoff >= cp->maxoff) {
				fprintf(stderr, "Error: minoff must be less than maxoff\n");
				usage();
			}
			break;

		case M_MAXOFF:
			cp->maxoff = iob_strtoull(value, NULL, 0) << 20;
			if (cp->minoff >= cp->maxoff) {
				fprintf(stderr, "Error: maxoff must be greater than minoff\n");
				usage();
			}
			break;

		case M_MULTIPLER:
			cp->multiplier = iob_strtoul(value, NULL, 0);
			break;

		case M_STATUS:
			cp->status = iob_strtoul(value, NULL, 0);
			break;

		case M_HEADERSZ:
			cp->headersz = (uint64_t)iob_strtoull(value, NULL, 0) * 1024;
			break;

		case M_WARMUP:
			cp->warmup = (uint64_t)iob_strtoull(value, NULL, 0) * IOB_NSEC_TO_SEC;
			break;

		case M_INTERVAL:
			cp->interval = (uint64_t)iob_strtoull(value, NULL, 0) * IOB_NSEC_TO_SEC;
			break;

		case M_INITTHREADS:
			cp->initthreads = iob_strtoul(value, NULL, 0);
			break;

		case M_MAXTHREADS:
			cp->maxthreads = iob_strtoul(value, NULL, 0);
			break;

		case M_INCRTHREADS:
			cp->incrthreads = iob_strtoul(value, NULL, 0);
			break;

		case M_MAXLAT:
			cp->maxlatency = iob_strtoul(value, NULL, 0) * 1000;
			break;

		case M_MAXRATE:
			cp->maxrate = iob_strtoul(value, NULL, 0);
			break;

		case M_MAXIOSIZE:
			cp->maxiosize = iob_strtoul(value, NULL, 0);
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
getitypeopts(char *options)
{
	struct itype	*ip, *p;
	int		sleeptm;
	char		*value, *erropt;

	ip = iob_malloc(sizeof (struct itype));
	memcpy(ip, &defitype, sizeof (struct itype));
	if (itlist == NULL) {
		itlist = ip;
	} else {
		for (p = itlist; p->next != NULL; p = p->next) {
			;
		}
		p->next = ip;
	}
	sleeptm = 0ULL;
	while (*options != '\0') {
		erropt = options;
		switch (getsubopt(&options, itypeopts, &value)) {

		case I_SIZE:
			ip->size = iob_strtoul(value, NULL, 0);
			if (ip->size < IOB_SECTOR || ip->size % IOB_SECTOR) {
				fprintf(stderr, "Error: I/O size (%d) must be a multiple of %d bytes\n",
					ip->size, IOB_SECTOR);
				usage();
			}
			break;

		case I_OP:
			if (value == NULL) {
				fprintf(stderr, "Error: Optional argument parameter not specified\n");
				usage();
			} else if (strcmp(value, "write") == 0) {
				ip->optype = WRITE;
			} else if (strcmp(value, "read") == 0) {
				ip->optype = READ;
			} else if (strcmp(value, "sleep") == 0) {
				ip->optype = SLEEP;
			} else {
				fprintf(stderr, "Error: Invalid operation specified\n");
				usage();
			}
			break;

		case I_PCT:
			ip->pct = iob_strtod(value, NULL);
			break;

		case I_REPWRITE:
			ip->repwrite = iob_strtod(value, NULL);
			break;

		case I_SLEEPTM:
			sleeptm = (int)iob_strtoul(value, NULL, 0);
			if (sleeptm < 50) {
				fprintf(stderr, "Error: Can't specify sleep shorter than 50 us\n");
				usage();
			}
			break;

		case I_LONGLAT:
			ip->longlat = iob_strtoull(value, NULL, 0) * 1000;
			longlats = 1;
			break;

		case I_DTYPE:
			iob_dtargparse(&ip->dtype, value);
			break;

		case I_DEDUPBLKS:
			ip->dedupblks = iob_strtoull(value, NULL, 0);
			if (ip->dedupblks == 0) {
				ip->dedupblks = 1;
			}
			if (ip->dedupblks > ((uint64_t)1 << 50)) {
				ip->dedupblks = (uint64_t)1 << 50;
			}
			break;

		default:
			fprintf(stderr, "unknown -i suboption %s\n", erropt);
			usage();
			break;
		}
	}
	if (ip->optype == SLEEP) {
		ip->size = (sleeptm != 0) ? sleeptm : 5000;
	}
	return;
}
