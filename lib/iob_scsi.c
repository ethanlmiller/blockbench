//
// iob_scsi.c
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

#include        "iob_lib.h"
#include        "iobenchlibext.h"

#if defined(sun) || defined(__sun)

#include	<sys/fcntl.h>
#include	<sys/scsi/scsi.h>
#include	<netinet/in.h>
#include	<inttypes.h>

STATIC void	drive_int_flush_cache_sync_cache_16(int fd);
STATIC void	drive_int_flush_cache_sync_cache_10(int fd);
STATIC void	drive_sata_flush_cache(int fd);

void
get_scsi_info(int fd, uint64_t *serialp)
{

#if 0
	memset((char *)&uscsi, 0, sizeof (uscsi));
	memset((char *)&cdb, 0, sizeof (cdb));
	memset((char *)&sense, 0, sizeof (sense));
	memset((char *)&id_data, 0, sizeof (id_data));
	if ((fd = open(filename, O_RDONLY | O_NDELAY)) < 0) {
		return;
	}
	cdb.cdb_opaque[0] = 0x12;
	cdb.cdb_opaque[1] = 0x1;
	cdb.cdb_opaque[2] = 0x83;
	cdb.cdb_opaque[3] = 0x2;
	uscsi.uscsi_cdb = (caddr_t)&cdb;
	uscsi.uscsi_cdblen = CDB_GROUP0;
	uscsi.uscsi_timeout = 10;
	uscsi.uscsi_rqbuf = (caddr_t)sense;
	uscsi.uscsi_rqlen = sizeof (sense);
	uscsi.uscsi_bufaddr = id_data;
	uscsi.uscsi_buflen = sizeof (id_data);
	uscsi.uscsi_flags = USCSI_READ | USCSI_ISOLATE | USCSI_RQENABLE;
	if ((status = ioctl(fd, USCSICMD, &uscsi)) < 0) {
		close(fd);
		return;
	}
	close(fd);
	(void) strncpy(info, (const char *)&id_data[8], 48);
	(void) strncpy(revision, (const char *)&id_data[56], 16);

	/*
	 * strip white spaces off the end of the strings
	 */

	for (i = 47; i > 0 && info[i] == ' '; i--) {
		info[i] = '\0';
	}
	for (i = 15; i > 0 && revision[i] == ' '; i--) {
		revision[i] = '\0';
	}
#endif
	return;
}

STATIC void
drive_int_flush_cache_sync_cache_16(int fd)
{
	char			sense[255];
	int			status;
	union scsi_cdb		cdb;
	struct uscsi_cmd	uscsi;

	memset((char *)&uscsi, 0, sizeof (uscsi));
	memset((char *)&cdb, 0, sizeof (cdb));
	cdb.cdb_opaque[0] = 0x91;
	uscsi.uscsi_cdb = (caddr_t)&cdb;
	uscsi.uscsi_cdblen = CDB_GROUP4;
	uscsi.uscsi_timeout = 5;
	uscsi.uscsi_rqbuf = (caddr_t)sense;
	uscsi.uscsi_rqlen = sizeof (sense);
	uscsi.uscsi_bufaddr = NULL;
	uscsi.uscsi_buflen = 0;
	uscsi.uscsi_flags = USCSI_READ | USCSI_ISOLATE | USCSI_RQENABLE;
	status = ioctl(fd, USCSICMD, &uscsi);
	if (status < 0) {
		fprintf(stderr, "sync_cache_16 flush failed (%s)\n", strerror(errno));
	}
	return;
}

STATIC void
drive_int_flush_cache_sync_cache_10(int fd)
{
	char			sense[255];
	int			status;
	union scsi_cdb		cdb;
	struct uscsi_cmd	uscsi;

	memset((char *)&uscsi, 0, sizeof (uscsi));
	memset((char *)&cdb, 0, sizeof (cdb));
	cdb.cdb_opaque[0] = 0x35;
	uscsi.uscsi_cdb = (caddr_t)&cdb;
	uscsi.uscsi_cdblen = CDB_GROUP1;
	uscsi.uscsi_timeout = 5;
	uscsi.uscsi_rqbuf = (caddr_t)sense;
	uscsi.uscsi_rqlen = sizeof (sense);
	uscsi.uscsi_bufaddr = NULL;
	uscsi.uscsi_buflen = 0;
	uscsi.uscsi_flags = USCSI_READ | USCSI_ISOLATE | USCSI_RQENABLE;
	status = ioctl(fd, USCSICMD, &uscsi);
	if (status < 0) {
		fprintf(stderr, "sync_cache_10 flush failed (%s)\n", strerror(errno));
	}
	return;
}

STATIC void
drive_sata_flush_cache(int fd)
{
	int			status;
	union scsi_cdb		cdb;
	struct uscsi_cmd	uscsi;

	memset((char *)&uscsi, 0, sizeof (uscsi));
	memset((char *)&cdb, 0, sizeof (cdb));
	cdb.cdb_opaque[0] = 0x85;
	cdb.cdb_opaque[1] = 3 << 1;
	cdb.cdb_opaque[2] = 0x20;
	cdb.cdb_opaque[14] = 0xE7;	/* SATA FLUSH CACHE */
	uscsi.uscsi_cdb = (caddr_t)&cdb;
	uscsi.uscsi_cdblen = CDB_GROUP4;
	uscsi.uscsi_timeout = 5000;
	uscsi.uscsi_rqbuf = NULL;
	uscsi.uscsi_rqlen = 0;
	uscsi.uscsi_bufaddr = NULL;
	uscsi.uscsi_buflen = 0;
	uscsi.uscsi_flags = USCSI_READ;
	status = ioctl(fd, USCSICMD, &uscsi);
	if (status < 0) {
		fprintf(stderr, "sata flush cache failed (%s)\n", strerror(errno));
	}
	return;
}

void
drive_sata_standby_immediate(int fd)
{
	int			status;
	union scsi_cdb		cdb;
	struct uscsi_cmd	uscsi;

	memset((char *)&uscsi, 0, sizeof (uscsi));
	memset((char *)&cdb, 0, sizeof (cdb));
	cdb.cdb_opaque[0] = 0x85;
	cdb.cdb_opaque[1] = 3 << 1;
	cdb.cdb_opaque[2] = 0x20;
	cdb.cdb_opaque[14] = 0xE0;	/* SATA STANDBY IMMEDIATE */
	uscsi.uscsi_cdb = (caddr_t)&cdb;
	uscsi.uscsi_cdblen = CDB_GROUP4;
	uscsi.uscsi_timeout = 5000;
	uscsi.uscsi_rqbuf = NULL;
	uscsi.uscsi_rqlen = 0;
	uscsi.uscsi_bufaddr = NULL;
	uscsi.uscsi_buflen = 0;
	uscsi.uscsi_flags = USCSI_READ;
	status = ioctl(fd, USCSICMD, &uscsi);
	if (status < 0) {
		fprintf(stderr, "sata standby immediate failed (%s)\n", strerror(errno));
	}
	return;
}

void
get_vpd_page(int fd, int page)
{
	return;
}

void
get_mode_page(int fd, int page)
{
	return;
}

void
drive_sanitize(int fd, int stype)
{
	return;
}

void
drive_sata_identify_device(int fd)
{
	return;
}

void
drive_set_password(int fd)
{
	return;
}

void
drive_unlock(int fd)
{
	return;
}

void
drive_erase_prepare(int fd)
{
	return;
}

void
drive_secure_erase(int fd, int enhanced)
{
	return;
}

void
drive_secure_disable_password(int fd)
{
	return;
}

void
drive_read_smart_data(int fd, uint8_t *databuf)
{
	return;
}

void
drive_read_smart_log(int fd)
{
	return;
}

void
drive_write_smart_log(int fd)
{
	return;
}

void
drive_trim(int fd, char *databuf, int blen)
{
	return;
}

void
drive_write_same(int fd, uint64_t offset, int length, int unmap, int pbdata, int lbdata)
{
	return;
}

void
drive_register_ignore(int fd, uint32_t new_key)
{
	return;
}

void
drive_register(int fd, uint32_t new_key, uint32_t my_key)
{
	return;
}

void
drive_reserve(int fd, uint32_t my_key)
{
	return;
}

void
drive_preempt(int fd, uint32_t my_key)
{
	return;
}

void
drive_clear(int fd, uint32_t my_key)
{
	return;
}

void
drive_read_reserve(int fd)
{
	return;
}

void
drive_unmap(int fd, uint64_t offset, int length)
{
	return;
}

#elif defined(_AIX) || defined(__hpux)

#include        <fcntl.h>
#include        <sys/scsi.h>
#include        <netinet/in.h>
#include        <inttypes.h>

STATIC void
drive_int_flush_cache_sync_cache_16(int fd)
{
	return;
}

STATIC void
drive_int_flush_cache_sync_cache_10(int fd)
{
	return;
}	

STATIC void
drive_sata_flush_cache(int fd)
{
	return;
}	

void
drive_unmap(int fd, uint64_t offset, int length)
{
	return;
}

#elif defined(__CYGWIN__)

#include        <fcntl.h>
#include        <scsi/sg_lib.h>
#include        <scsi/sg_cmds_basic.h>
#include        <netinet/in.h>
#include        <inttypes.h>

STATIC void
drive_int_flush_cache_sync_cache_16(int fd)
{
	return;
}

STATIC void
drive_int_flush_cache_sync_cache_10(int fd)
{
	return;
}	

STATIC void
drive_sata_flush_cache(int fd)
{
	return;
}	

void
drive_unmap(int fd, uint64_t offset, int length)
{
	return;
}

#else	/* Linux */

#include	<sys/ioctl.h>
#include	<scsi/scsi_ioctl.h>
#include	<scsi/sg_lib.h>
#include	<scsi/sg_io_linux.h>

STATIC void	issue_sgio(int fd, sg_io_hdr_t *hdrp, char *str);
STATIC void	drive_int_flush_cache_sync_cache_16(int fd);
STATIC void	drive_int_flush_cache_sync_cache_10(int fd);
STATIC void	drive_sata_flush_cache(int fd);
STATIC void	load_key(unsigned char *bp, uint32_t key);

STATIC void
issue_sgio(int fd, sg_io_hdr_t *hdrp, char *str)
{
	unsigned char	sense_buffer[64];
	int		i, status;

	memset(sense_buffer, 0, sizeof (sense_buffer));
	hdrp->interface_id = 'S';
	hdrp->mx_sb_len = sizeof (sense_buffer);
	hdrp->sbp = sense_buffer;
	hdrp->timeout = 3600000;
	status = ioctl(fd, SG_IO, hdrp);
	if (status != 0 || hdrp->status || hdrp->masked_status || hdrp->driver_status | hdrp->host_status) {
		fprintf(stderr, "Error on %s status %d  masked_status %d  driver_status %d host_status %d\n",
			str, hdrp->status, hdrp->masked_status, hdrp->driver_status, hdrp->host_status);
		if (hdrp->sb_len_wr) {
			fprintf(stderr, "Sense bytes[%d]:  ", hdrp->sb_len_wr);
			for (i = 0; i < hdrp->sb_len_wr; i++) {
				fprintf(stderr, "%2.2x ", sense_buffer[i]);
			}
			fprintf(stderr, "\n");
		}
		exit(1);
	}
	return;
}

void
get_scsi_info(int fd, uint64_t *serialp)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[6] = {	0x12,		/* 0 - INQUIRY */
					0x00,		/* 1 */
					0x00,		/* 2 */
					0x02,		/* 3 - datalen 15:8 */
					0x00,		/* 4 - datalen 7:0 */
					0x00 };		/* 5 */
	unsigned char	databuf[512];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	issue_sgio(fd, &io_hdr, "inquiry");
	return;
}

void
get_vpd_page(int fd, int page)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[6] = {	0x12,		/* 0 - INQUIRY */
					0x01,		/* 1 - 0x1 is vpd */
					0x00,		/* 2 - page code */
					0x04,		/* 3 - datalen 15:8 */
					0x00,		/* 4 - datalen 7:0 */
					0x00 };		/* 5 */
	unsigned char	databuf[1024];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	cmdbuf[2] = (unsigned char)(page & 0xff);
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	issue_sgio(fd, &io_hdr, "get_vpd_page");
	return;
}

void
get_mode_page(int fd, int page)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[10] = {	0x5a,		/* 0 - MODE SENSE (10) */
					0x08,		/* 1 - 0x8 is dbd */
					0x00,		/* 2 - page code */
					0x00,		/* 3 - subpage code */
					0x00,		/* 4 */
					0x00,		/* 5 */
					0x00,		/* 6 */
					0x04,		/* 7 - datalen 15:8 */
					0x00,		/* 8 - datalen 7:0 */
					0x00 };		/* 9 */
	unsigned char	databuf[1024];

	cmdbuf[2] = (unsigned char)(page & 0xff);
	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	issue_sgio(fd, &io_hdr, "mode sense");
	return;
}

void
drive_sanitize(int fd, int stype)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[10] = {	0x21,		/* 0 - SANITIZE (vendor specific) */
					0x00,		/* 1 */
					0x00,		/* 2 - Type of erasure */
					0x00,		/* 3 */
					0x00,		/* 4 */
					0x00,		/* 5 */
					0x66,		/* 6 - per STEC */
					0x77,		/* 7 - per STEC */
					0x88,		/* 8 - per STEC */
					0x00 };		/* 9 */

	cmdbuf[2] = (unsigned char)(stype & 0x3);
	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.dxfer_len = 0;
	io_hdr.dxferp = NULL;
	issue_sgio(fd, &io_hdr, "sanitize");
	return;
}

void
drive_sata_identify_device(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[16] = {	0x85,			/* 0 - ATA PASS-THROUGH (16) */
					0x04 << 1,		/* 1 - protocol is pio_in */
					0x0e,			/* 2 - dir is in, byte_block is block, len in sc */
					0x00,			/* 3 */
					0x00,			/* 4 */
					0x00,			/* 5 - sector count 15:8 */
					0x01,			/* 6 - sector count 7:0 */
					0x00,			/* 7 */
					0x00,			/* 8 */
					0x00,			/* 9 */
					0x00,			/* A */
					0x00,			/* B */
					0x00,			/* C */
					0x00,			/* D */
					0xec,			/* E - Identify Device */
					0x00 };			/* F */
	unsigned char	databuf[512];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	issue_sgio(fd, &io_hdr, "sata_identify_device");
	return;
}

void
drive_set_password(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[12] = {	0xa1,			/* 0 - ATA PASS-THROUGH (12) */
					0x0a,			/* 1 - protocol is pio_out */
					0x06,			/* 2 - byte_block is block, len in sc */
					0x00,			/* 3 - */
					0x01,			/* 4 - sector count 7:0 */
					0x00,			/* 5 */
					0x00,			/* 6 */
					0x00,			/* 7 */
					0x00,			/* 8 */
					0xf1,			/* 9 - Security Set Password */
					0x00,			/* A */
					0x00 };			/* B */
	unsigned char	databuf[512];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	databuf[2] = 'P';
	databuf[3] = 'U';
	databuf[4] = 'R';
	databuf[5] = 'E';
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	issue_sgio(fd, &io_hdr, "drive_set_password");
	return;
}

void
drive_unlock(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[12] = {	0xa1,			/* 0 - ATA PASS-THROUGH (12) */
					0x0a,			/* 1 - protocol is pio_out */
					0x06,			/* 2 - byte_block is block, len in sc */
					0x00,			/* 3 */
					0x01,			/* 4 - sector count 7:0 */
					0x00,			/* 5 */
					0x00,			/* 6 */
					0x00,			/* 7 */
					0x00,			/* 8 */
					0xf2,			/* 9 - Security Unlock */
					0x00,			/* A */
					0x00 };			/* B */
	unsigned char	databuf[512];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	databuf[2] = 'P';
	databuf[3] = 'U';
	databuf[4] = 'R';
	databuf[5] = 'E';
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	issue_sgio(fd, &io_hdr, "drive_unlock");
	return;
}

void
drive_erase_prepare(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[12] = {	0xa1,			/* 0 - ATA PASS-THROUGH (12) */
					0x06,			/* 1 - protocol is non-data */
					0x00,			/* 2 */
					0x00,			/* 3 */
					0x00,			/* 4 */
					0x00,			/* 5 */
					0x00,			/* 6 */
					0x00,			/* 7 */
					0x00,			/* 8 */
					0xf3,			/* 9 - Security Erase Prepare */
					0x00,			/* A */
					0x00 };			/* B */

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.dxfer_len = 0;
	io_hdr.dxferp = NULL;
	issue_sgio(fd, &io_hdr, "drive_erase_prepare");
	return;
}

void
drive_secure_erase(int fd, int enhanced)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[12] = {	0xa1,			/* 0 - ATA PASS-THROUGH (12) */
					0x0a,			/* 1 - protocol is pio_out */
					0x06,			/* 2 - byte_block is block, len in sc */
					0x00,			/* 3 */
					0x01,			/* 4 - sector count 7:0 */
					0x00,			/* 5 */
					0x00,			/* 6 */
					0x00,			/* 7 */
					0x00,			/* 8 */
					0xf4,			/* 9 - Security Erase Unit */
					0x00,			/* A */
					0x00 };			/* B */
	unsigned char	databuf[512];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	if (enhanced) {
		databuf[0] = 0x2;
	}
	databuf[2] = 'P';
	databuf[3] = 'U';
	databuf[4] = 'R';
	databuf[5] = 'E';
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	issue_sgio(fd, &io_hdr, "drive_secure_erase");
	return;
}

void
drive_secure_disable_password(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[12] = {	0xa1,			/* 0 - ATA PASS-THROUGH (12) */
					0x0a,			/* 1 - protocol is pio_out */
					0x06,			/* 2 - byte_block is block, len in sc */
					0x00,			/* 3 */
					0x01,			/* 4 - sector count 7:0 */
					0x00,			/* 5 */
					0x00,			/* 6 */
					0x00,			/* 7 */
					0x00,			/* 8 */
					0xf6,			/* 9 - Security Disable Password */
					0x00,			/* A */
					0x00 };			/* B */
	unsigned char	databuf[512];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	databuf[2] = 'P';
	databuf[3] = 'U';
	databuf[4] = 'R';
	databuf[5] = 'E';
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	issue_sgio(fd, &io_hdr, "drive_security_disable_password");
	return;
}

STATIC void
drive_int_flush_cache_sync_cache_16(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[16] = {	0x91,			/* 0 - SYNCHRONIZE CACHE (16) */
					0x00,			/* 1 */
					0x00,			/* 2 */
					0x00,			/* 3 */
					0x00,			/* 4 */
					0x00,			/* 5 */
					0x00,			/* 6 */
					0x00,			/* 7 */
					0x00,			/* 8 */
					0x00,			/* 9 */
					0x00,			/* A */
					0x00,			/* B */
					0x00,			/* C */
					0x00,			/* D */
					0x00,			/* E */
					0x00 };			/* F */

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.dxfer_len = 0;
	io_hdr.dxferp = NULL;
	issue_sgio(fd, &io_hdr, "sync_cache_16");
	return;
}

STATIC void
drive_int_flush_cache_sync_cache_10(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[10] = {	0x35,			/* 0 - SYNCHRONIZE CACHE (10) */
					0x00,			/* 1 */
					0x00,			/* 2 */
					0x00,			/* 3 */
					0x00,			/* 4 */
					0x00,			/* 5 */
					0x00,			/* 6 */
					0x00,			/* 7 */
					0x00,			/* 8 */
					0x00 };			/* 9 */

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.dxfer_len = 0;
	io_hdr.dxferp = NULL;
	issue_sgio(fd, &io_hdr, "sync_cache_10");
	return;
}

STATIC void
drive_sata_flush_cache(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[16] = {	0x85,			/* 0 - ATA PASS THROUGH 16 */
					0x03 << 1,		/* 1 */
					0x00,			/* 2 */
					0x00,			/* 3 */
					0x00,			/* 4 */
					0x00,			/* 5 */
					0x00,			/* 6 */
					0x00,			/* 7 */
					0x00,			/* 8 */
					0x00,			/* 9 */
					0x00,			/* A */
					0x00,			/* B */
					0x00,			/* C */
					0x00,			/* D */
					0xEA,			/* E */
					0x00 };			/* F */

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	io_hdr.cmdp = cmdbuf;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.dxfer_len = 0;
	io_hdr.dxferp = NULL;
	issue_sgio(fd, &io_hdr, "sata_flush_cache_ext");
	return;
}

void
drive_read_smart_data(int fd, uint8_t *databuf)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[16] = {	0x85,			/* 0 - 16 byte ATA PASS-THROUGH */
					0x04 << 1,		/* 1 - protocol is PIO-in */
					0x0e,			/* 2 - T_DIR is 1, BYTE_BLOCK, T_LEN in sector count */
					0x00,			/* 3 - feature 15:8 */
					0xD0,			/* 4 - feature 7:0 */
					0x00,			/* 5 - sector count 15:8 */
					0x01,			/* 6 - sector count 7:0 */
					0x00,			/* 7 - LBA Low 15:8 */
					0x00,			/* 8 - LBA Low 7:0 */
					0x00,			/* 9 - LBA Med 15:8 */
					0x4F,			/* A - LBA Med 7:0 */
					0x00,			/* B - LBA High 15:8 */
					0xC2,			/* C - LBA High 7:0 */
					0x00,			/* D - Device - per spec these bits are obsolete */
					0xB0,			/* E - SMART */
					0x00 };			/* F - control */

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, IOB_SECTOR);
	io_hdr.cmd_len = 16;
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = IOB_SECTOR;
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	issue_sgio(fd, &io_hdr, "read_smart_data");
	return;
}

void
drive_read_smart_log(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[16] = {	0x85,			/* 0 - 16 byte ATA PASS-THROUGH */
					0x04 << 1,		/* 1 - protocol is PIO-in */
					0x0e,			/* 2 - T_DIR is 1, BYTE_BLOCK, T_LEN in sector count */
					0x00,			/* 3 - feature 15:8 */
					0xD5,			/* 4 - feature 7:0 */
					0x00,			/* 5 - sector count 15:8 */
					0x10,			/* 6 - sector count 7:0 */
					0x00,			/* 7 - LBA Low 15:8 */
					0x80,			/* 8 - LBA Low 7:0 */
					0x00,			/* 9 - LBA Med 15:8 */
					0x4F,			/* A - LBA Med 7:0 */
					0x00,			/* B - LBA High 15:8 */
					0xC2,			/* C - LBA High 7:0 */
					0x00,			/* D - Device - per spec these bits are obsolete */
					0xB0,			/* E - SMART */
					0x00 };			/* F - control */
	unsigned char	databuf[8192];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	io_hdr.cmd_len = 16;
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	issue_sgio(fd, &io_hdr, "read_smart_log");
	return;
}

void
drive_write_smart_log(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[16] = {	0x85,			/* 0 - 16 byte ATA PASS-THROUGH */
					0x05 << 1,		/* 1 - protocol is PIO-out */
					0x06,			/* 2 - T_DIR is 0, BYTE_BLOCK, T_LEN in sector count */
					0x00,			/* 3 - feature 15:8 */
					0xD6,			/* 4 - feature 7:0 */
					0x00,			/* 5 - sector count 15:8 */
					0x10,			/* 6 - sector count 7:0 */
					0x00,			/* 7 - LBA Low 15:8 */
					0x80,			/* 8 - LBA Low 7:0 */
					0x00,			/* 9 - LBA Med 15:8 */
					0x4F,			/* A - LBA Med 7:0 */
					0x00,			/* B - LBA High 15:8 */
					0xC2,			/* C - LBA High 7:0 */
					0x00,			/* D - Device - per spec these bits are obsolete */
					0xB0,			/* E - SMART */
					0x00 };			/* F - control */
	unsigned char	databuf[8192];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, (uint8_t)(getpid() & 0xff), sizeof (databuf));
	io_hdr.cmd_len = 16;
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	issue_sgio(fd, &io_hdr, "write_smart_log");
	return;
}

void
drive_sata_standby_immediate(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[16] = {	0x85,			/* 0 - 16 byte ATA PASS-THROUGH */
					0x03 << 1,		/* 1 - protocol is non-data */
					0x00,			/* 2 */
					0x00,			/* 3 - feature 15:8 */
					0x00,			/* 4 - feature 7:0 */
					0x00,			/* 5 - sector count 15:8 */
					0x00,			/* 6 - sector count 7:0 */
					0x00,			/* 7 - LBA Low 15:8 */
					0x00,			/* 8 - LBA Low 7:0 */
					0x00,			/* 9 - LBA Med 15:8 */
					0x00,			/* A - LBA Med 7:0 */
					0x00,			/* B - LBA High 15:8 */
					0x00,			/* C - LBA High 7:0 */
					0x00,			/* D - Device - per spec these bits are obsolete */
					0xE0,			/* E - STANDBY IMMEDIATE */
					0x00 };			/* F - control */

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	io_hdr.cmd_len = 16;
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.dxfer_len = 0;
	io_hdr.dxferp = NULL;
	io_hdr.cmdp = cmdbuf;
	issue_sgio(fd, &io_hdr, "sata_standby_immediate");
	return;
}

void
drive_trim(int fd, char *databuf, int blen)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[16] = {	0x85,			/* 0 - 16 byte ATA PASS-THROUGH */
					0x06 << 1,		/* 1 - protocol is DMA */
					0x01 << 2 | 0x02,	/* 2 - byte block is 1, t_length is 2 */
					0x00,			/* 3 - feature 15:8 */
					0x01,			/* 4 - feature 7:0 */
					0x00,			/* 5 - sector count 15:8 */
					0x01,			/* 6 - sector count 7:0 */
					0x00,			/* 7 - LBA Low 15:8 */
					0x00,			/* 8 - LBA Low 7:0 */
					0x00,			/* 9 - LBA Med 15:8 */
					0x00,			/* A - LBA Med 7:0 */
					0x00,			/* B - LBA High 15:8 */
					0x00,			/* C - LBA High 7:0 */
					0x40,			/* D - Device - per spec these bits are obsolete */
					0x06,			/* E - command is DATA_SET_MANAGEMENT */
					0x00 };			/* F - control */
	int		cbytes;

	cbytes = (blen + IOB_SECTOR - 1) & ~(IOB_SECTOR - 1);
	memset(databuf + blen, 0, cbytes - blen);
	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	cmdbuf[6] = cbytes / IOB_SECTOR;
	io_hdr.cmd_len = 16;
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = cbytes;
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	issue_sgio(fd, &io_hdr, "trim");
	return;
}

void
drive_write_same(int fd, uint64_t offset, int length, int unmap, int pbdata, int lbdata)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[16] = {	0x93,		/* 0 - WRITE SAME */
					0x00,		/* 1 */
					0x00,		/* 2 - offset 63:56 */
					0x00,		/* 3 - offset 55:48 */
					0x00,		/* 4 - offset 47:40 */
					0x00,		/* 5 - offset 39:32 */
					0x00,		/* 6 - offset 31:24 */
					0x00,		/* 7 - offset 23:16 */
					0x00,		/* 8 - offset 15:8 */
					0x00,		/* 9 - offset 7:0 */
					0x00,		/* A - length 31:24 */
					0x00,		/* B - length 23:16 */
					0x00,		/* C - length 15:8 */
					0x00,		/* D - length 7:0 */
					0x00,		/* E */
					0x00 };		/* F */
	unsigned char	databuf[512];

	offset /= 512;
	length /= 512;
	if (unmap) {
		cmdbuf[1] = 0x01 << 3;
	} else if (pbdata) {
		cmdbuf[1] = 0x01 << 2;
	} else if (lbdata) {
		cmdbuf[1] = 0x01 << 1;
	}
	cmdbuf[2] = (offset >> 56) & 0xff;
	cmdbuf[3] = (offset >> 48) & 0xff;
	cmdbuf[4] = (offset >> 40) & 0xff;
	cmdbuf[5] = (offset >> 32) & 0xff;
	cmdbuf[6] = (offset >> 24) & 0xff;
	cmdbuf[7] = (offset >> 16) & 0xff;
	cmdbuf[8] = (offset >> 8) & 0xff;
	cmdbuf[9] = (offset >> 0) & 0xff;
	cmdbuf[10] = (length >> 24) & 0xff;
	cmdbuf[11] = (length >> 16) & 0xff;
	cmdbuf[12] = (length >> 8) & 0xff;
	cmdbuf[13] = (length >> 0) & 0xff;
	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	io_hdr.cmd_len = 16;
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	issue_sgio(fd, &io_hdr, "write_same");
	return;
}

STATIC void
load_key(unsigned char *bp, uint32_t key)
{

	bp[0] = 'P';
	bp[1] = 'U';
	bp[2] = 'R';
	bp[3] = 'E';
	bp[4] = (key >> 24) & 0xff;
	bp[5] = (key >> 16) & 0xff;
	bp[6] = (key >>  8) & 0xff;
	bp[7] = (key >>  0) & 0xff;
	return;
}

void
drive_register_ignore(int fd, uint32_t new_key)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[10] = {	0x5f,		/* 0 - PERSISTENT RESERVE OUT */
					0x06,		/* 1 - SA is Register and Ignore */
					0x00,		/* 2 - scope/type */
					0x00,		/* 3 - reserved */
					0x00,		/* 4 - reserved */
					0x00,		/* 5 - reserved */
					0x00,		/* 6 - reserved */
					0x00,		/* 7 - Param List Len 15:8 */
					0x18,		/* 8 - Param List Len 7:0 */
					0x00 };		/* 9 - control */
	unsigned char	databuf[24];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	io_hdr.cmd_len = 10;
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = 24;
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	load_key(databuf + 8, new_key);
	issue_sgio(fd, &io_hdr, "PROUT_register_and_ignore");
	return;
}

void
drive_register(int fd, uint32_t new_key, uint32_t my_key)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[10] = {	0x5f,		/* 0 - PERSISTENT RESERVE OUT */
					0x00,		/* 1 - SA is Register */
					0x00,		/* 2 - scope/type */
					0x00,		/* 3 - reserved */
					0x00,		/* 4 - reserved */
					0x00,		/* 5 - reserved */
					0x00,		/* 6 - reserved */
					0x00,		/* 7 - Param List Len 15:8 */
					0x18,		/* 8 - Param List Len 7:0 */
					0x00 };		/* 9 - control */
	unsigned char	databuf[24];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	io_hdr.cmd_len = 10;
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = 24;
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	load_key(databuf, my_key);
	load_key(databuf + 8, new_key);
	issue_sgio(fd, &io_hdr, "PROUT_register");
	return;
}

void
drive_reserve(int fd, uint32_t my_key)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[10] = {	0x5f,		/* 0 - PERSISTENT RESERVE OUT */
					0x01,		/* 1 - SA is Reserve */
					0x07,		/* 2 - type is Write Exclusive - All Registrants */
					0x00,		/* 3 - reserved */
					0x00,		/* 4 - reserved */
					0x00,		/* 5 - reserved */
					0x00,		/* 6 - reserved */
					0x00,		/* 7 - Param List Len 15:8 */
					0x18,		/* 8 - Param List Len 7:0 */
					0x00 };		/* 9 - control */
	unsigned char	databuf[24];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	io_hdr.cmd_len = 10;
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = 24;
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	load_key(databuf, my_key);
	issue_sgio(fd, &io_hdr, "PROUT_reserve");
	return;
}

void
drive_preempt(int fd, uint32_t my_key)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[10] = {	0x5f,		/* 0 - PERSISTENT RESERVE OUT */
					0x05,		/* 1 - SA is Preempt and Abort */
					0x07,		/* 2 - type is Write Exclusive - All Registrants */
					0x00,		/* 3 - reserved */
					0x00,		/* 4 - reserved */
					0x00,		/* 5 - reserved */
					0x00,		/* 6 - reserved */
					0x00,		/* 7 - Param List Len 15:8 */
					0x18,		/* 8 - Param List Len 7:0 */
					0x00 };		/* 9 - control */
	unsigned char	databuf[24];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	io_hdr.cmd_len = 10;
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = 24;
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	load_key(databuf, my_key);
	issue_sgio(fd, &io_hdr, "PROUT_preempt_and_abort");
	return;
}

void
drive_clear(int fd, uint32_t my_key)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[10] = {	0x5f,		/* 0 - PERSISTENT RESERVE OUT */
					0x03,		/* 1 - SA is Clear */
					0x00,		/* 2 - scope/type */
					0x00,		/* 3 - reserved */
					0x00,		/* 4 - reserved */
					0x00,		/* 5 - reserved */
					0x00,		/* 6 - reserved */
					0x00,		/* 7 - Param List Len 15:8 */
					0x18,		/* 8 - Param List Len 7:0 */
					0x00 };		/* 9 - control */
	unsigned char	databuf[24];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	io_hdr.cmd_len = 10;
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = 24;
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	load_key(databuf, my_key);
	issue_sgio(fd, &io_hdr, "PROUT_clear");
	return;
}

void
drive_read_reserve(int fd)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[10] = {	0x5e,		/* 0 - PERSISTENT RESERVE IN */
					0x03,		/* 1 - SA is Read Full Status */
					0x00,		/* 2 - reserved */
					0x00,		/* 3 - reserved */
					0x00,		/* 4 - reserved */
					0x00,		/* 5 - reserved */
					0x00,		/* 6 - reserved */
					0x02,		/* 7 - Param List Len 15:8 */
					0x00,		/* 8 - Param List Len 7:0 */
					0x00 };		/* 9 - control */
	unsigned char	databuf[512];

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	io_hdr.cmd_len = 10;
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	issue_sgio(fd, &io_hdr, "PROUT_read");
	return;
}

void
drive_unmap(int fd, uint64_t offset, int length)
{
	sg_io_hdr_t	io_hdr;
	unsigned char	cmdbuf[10] = {	0x42,			/* 0 - UNMAP */
					0x00,			/* 1 */
					0x00,			/* 2 */
					0x00,			/* 3 */
					0x00,			/* 4 */
					0x00,			/* 5 */
					0x00,			/* 6 */
					0x00,			/* 7 */
					0x18,			/* 8 - length of parameter list */
					0x00 };			/* 9 - control */
	unsigned char	databuf[24];
	uint64_t	off;

	memset(&io_hdr, 0, sizeof (sg_io_hdr_t));
	memset(databuf, 0, sizeof (databuf));
	off = offset / IOB_SECTOR;
	databuf[1] = sizeof (databuf) - 2;
	databuf[3] = sizeof (databuf) - 8;
	databuf[8 + 0] = (off >> 56) & 0xff;
	databuf[8 + 1] = (off >> 48) & 0xff;
	databuf[8 + 2] = (off >> 40) & 0xff;
	databuf[8 + 3] = (off >> 32) & 0xff;
	databuf[8 + 4] = (off >> 24) & 0xff;
	databuf[8 + 5] = (off >> 16) & 0xff;
	databuf[8 + 6] = (off >>  8) & 0xff;
	databuf[8 + 7] = (off >>  0) & 0xff;
	databuf[8 + 8] = (length >>  24) & 0xff;
	databuf[8 + 9] = (length >>  16) & 0xff;
	databuf[8 + 10] = (length >>  8) & 0xff;
	databuf[8 + 11] = (length >>  0) & 0xff;
	io_hdr.cmd_len = sizeof (cmdbuf);
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	io_hdr.dxfer_len = sizeof (databuf);
	io_hdr.dxferp = databuf;
	io_hdr.cmdp = cmdbuf;
	issue_sgio(fd, &io_hdr, "unmap");
	return;
}

#endif	/* Solaris or Linux */

void
drive_flush_cache (int fd, int flushtype)
{
	switch (flushtype) {

	case IOB_SYNC_CACHE_16:
		drive_int_flush_cache_sync_cache_16(fd);
		break;

	case IOB_SYNC_CACHE_10:
		drive_int_flush_cache_sync_cache_10(fd);
		break;

	case IOB_SATA_FLUSH_CACHE:
		drive_sata_flush_cache(fd);
		break;

	default :
		fprintf(stderr, "ERROR: Unknown flush type\n");
		break;
	}
	return;
}
