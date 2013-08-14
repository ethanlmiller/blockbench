//
// iobenchlibext.h
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

#ifdef	__cplusplus
extern "C" {
#endif

#define	IOB_NSEC_TO_SEC		1000000000ULL
#define	IOB_SECTOR		512
#define	IOB_PAGE			4096
#define	IOB_NDATAPAGES		1000
#define	IOB_DT_ZEROS		0x01
#define	IOB_DT_ONES		0x02
#define	IOB_DT_PATTERN1		0x03
#define	IOB_DT_UNCOMPRESSIBLE	0x10
#define	IOB_DT_COMPRESSIBLE	0x11
#define	IOB_DT_HCOMPRESSIBLE	0x12
#define	IOB_DT_PST		0x13
#define	IOB_DT_VM		0x14
#define	IOB_DT_H20COMPRESSIBLE	0x15
#define	IOB_DT_H10COMPRESSIBLE	0x16
#define	IOB_DT_H40COMPRESSIBLE	0x17

#define	IOB_IS_DEDUPABLE(dtype)	(dtype & 0x10)

#define	IOB_RAND64()		(((uint64_t)mrand48() << 32) + (uint64_t)mrand48())

void		iob_libinit(char *cmdname, void (*ufunc)(void));
void		*iob_malloc(int nbytes);
void		*iob_align_shmem(int nbytes);
void		iob_errlog(int uflag, char *fmt, ...);
void		iob_infolog(char *fmt, ...);
uint64_t	iob_gettime(void);
uint32_t	iob_strtoul(char *value, char **endp, int base);
uint64_t	iob_strtoull(char *value, char **endp, int base);
double		iob_strtod(char *value, char **endp);
void		iob_thr_create(void *(*func)(void *), void *arg);
void		iob_initdata(void);
void		iob_iodata(char *bp, uint32_t size, int dtype, uint64_t dedupblks);
void		iob_dtargparse(int *dtype, char *value);
char		*iob_dttostr(int dtype);
char		*iob_dttochr(int dtype);
void		iob_dedupdata(char *bp, uint32_t size, int dtype, uint64_t srcbno);
void		iob_verifydata(char *bp, char *bp2, uint32_t size, int dtype, uint64_t dedupblks,
			      uint64_t seqno, uint16_t runid, uint32_t basetime, uint32_t *cksump, uint16_t *srcidp);

#define	IOB_SYNC_CACHE_16		0
#define	IOB_SYNC_CACHE_10		1
#define	IOB_SATA_FLUSH_CACHE		2
#define	IOB_SATA_STANDBY_IMM		3
#define	IOB_REGISTER_IGN			4
#define	IOB_READ_RESERVE			5
#define	IOB_REGISTER			6

void		get_scsi_info(int fd, uint64_t *serialp);
void		get_vpd_page(int fd, int page);
void		get_mode_page(int fd, int page);
void		drive_sanitize(int fd, int stype);
void		drive_sata_identify_device(int fd);
void		drive_set_password(int fd);
void		drive_unlock(int fd);
void		drive_erase_prepare(int fd);
void		drive_secure_erase(int fd, int enhanced);
void		drive_secure_disable_password(int fd);
void		drive_read_smart_data(int fd, uint8_t *databuf);
void		drive_read_smart_log(int fd);
void		drive_write_smart_log(int fd);
void		drive_sata_standby_immediate(int fd);
void		drive_trim(int fd, char *databuf, int blen);
void		drive_write_same(int fd, uint64_t offset, int length, int unmap, int pbdata, int lbdata);
void		drive_register_ignore(int fd, uint32_t new_key);
void		drive_register(int fd, uint32_t new_key, uint32_t my_key);
void		drive_reserve(int fd, uint32_t my_key);
void		drive_preempt(int fd, uint32_t my_key);
void		drive_clear(int fd, uint32_t my_key);
void		drive_read_reserve(int fd);
void		drive_flush_cache (int fd, int flushtype);
void		drive_unmap(int fd, uint64_t offset, int length);

#define	IOD_LONG		0x01
#define	IOD_LONG_LONG		0x02
#define	IOD_RECOVERED		0x04
#define	IOD_ERROR		0x08
#define	IOD_RETRY		0x10
#define	IOD_STATE		0x20

#define	MAX_OPTYPES		22
#define	N_OPTYPES		2
#define	N_STATTYPES		8
#define	READ			0
#define	WRITE			1
#define	FLUSH			2
#define	POSTFLUSH		3
#define	TRIM			4
#define	READ_FUA		5
#define	PREFLUSH		6
#define	PHYSWRITE		7
#define	READ_RESERVE		8
#define	REGISTER		9
#define	REGISTER_IGNORE		10
#define	REGISTER_CLEAR		11
#define	RESERVE			12
#define	PREEMPT_ABORT		13
#define	READ_SMART_DATA		14
#define	SLEEP			15
#define	UNMAP			16
#define	STANDBY_IMMED		17
#define	WRITE_SAME		18
#define	WRITE_FUA		19
#define	WRITE_SMART_DATA	20
#define	SET_PASSWORD		21

extern char	*typestr[MAX_OPTYPES];

struct iodet {
	uint64_t	start;
	uint64_t	end;
	uint64_t	wcstart;
	uint64_t	offset;
	uint64_t	latency;
	uint32_t	ionum;
	uint32_t	size;
	uint32_t	seconds;
	uint32_t	fplmode;
	uint16_t	wcycle;
	uint16_t	flag;
	uint16_t	type;
	uint16_t	dtype;
	uint16_t	fid;
	uint16_t	tid;
	uint16_t	io1;
	uint16_t	io2;
	uint16_t	io3;
	uint16_t	io4;
	uint64_t	wmask;
};

#define	NMODES		6
#define	MD_NONE		0
#define	MD_READ		1
#define	MD_WRITEONLY	2
#define	MD_READLIMIT	3
#define	MD_READRECOVER	4
#define	MD_READBACKOFF	5

void		iob_details_init(int maxiodet);
void		iob_details_reset(int clear);
void		iob_details_add(struct iodet *idp);
void		iob_details_output(FILE *fp, int infolog, char *lprefix, int channels, int banks, int pgsize);
uint64_t	iob_details_multi(FILE *fp, uint64_t start_iodet, int wcycle, int channels, int banks, int pgsize);

#ifdef	__cplusplus
}
#endif
