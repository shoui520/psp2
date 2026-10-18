/*
 * SceDeci4pSDrfp: DECI4P remote file protocol (DRFP, protocol 0x10011000)
 * over SceDeci4pSDfMgr. Files on the development host are mounted read-only
 * as host0: (sdrfpfs.c); every operation is one request / response exchange
 * with the host (sdrfp.c).
 */

#pragma once

#include <scetypes.h>
#include <deci4p_dfmgr/deci4p.h>
#include <deci4p_drfp/deci4p_fs.h>

#define DRFP_PROTOCOL		0x10011000

/* request codes */
#define DRFP_CMD_OPEN		4
#define DRFP_CMD_CLOSE		6
#define DRFP_CMD_READ		8
#define DRFP_CMD_GETSTAT	0xe

/* one response fragment: a packet buffer of a resource */
#define DRFP_FRAG_SIZE		0x11000
#define DRFP_FRAG_DATA_MAX	0x10000

#define DRFP_ERROR_NO_BUFFER	0x80090005
#define DRFP_ERROR_READ		0x80090001
#define DRFP_ERROR_8009000A	0x8009000a	/* ignored by close (meaning unknown) */

typedef struct DrfpHeader {
	SceUInt32 code;				/* DRFP_CMD_* */
	SceUInt16 seq;
	SceUInt16 reserved;
} DrfpHeader;

typedef struct DrfpOpenPacket {
	SceDeci4pHeader hdr;
	DrfpHeader drfp;
	union {
		struct {			/* request */
			int flags;
			int mode;
			SceSize nameLen;	/* including the terminator */
			char name[0];
		};
		struct {			/* response */
			int result;
			int fd;
			Deci4pFsStat stat;
		};
	};
} DrfpOpenPacket;

typedef struct DrfpClosePacket {
	SceDeci4pHeader hdr;
	DrfpHeader drfp;
	union {
		int fd;				/* request */
		int result;			/* response */
	};
} DrfpClosePacket;

typedef struct DrfpReadPacket {
	SceDeci4pHeader hdr;
	DrfpHeader drfp;
	union {
		struct {			/* request */
			int fd;
			SceUInt32 padding;
			SceOff offset;
			SceSize size;
		};
		struct {			/* response, one per fragment */
			int result;
			SceUInt32 reserved;
			SceUInt16 nfrag;
			SceUInt16 frag;		/* from 1 */
			SceSize nbytes;
			char data[0];
		};
	};
} DrfpReadPacket;

typedef struct DrfpGetstatPacket {
	SceDeci4pHeader hdr;
	DrfpHeader drfp;
	union {
		struct {			/* request */
			SceSize nameLen;
			char name[0];
		};
		struct {			/* response */
			int result;
			Deci4pFsStat stat;
		};
	};
} DrfpGetstatPacket;

/* sdrfp.c */
int sdrfpOpen(const char *name, int flags, int mode, Deci4pFsStat *stat);
int sdrfpRead(int fd, void *buf, SceOff offset, SceSize size);
int sdrfpClose(int fd);
int sdrfpGetstat(const char *name, Deci4pFsStat *stat);

/* sdrfpfs.c */
int sdrfpVfsMount(void);
