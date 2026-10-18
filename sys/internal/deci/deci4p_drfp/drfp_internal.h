/*
 * SceDeci4pDrfp internals: the DECI4P remote file protocol (DRFP) of the
 * development kit. Files on the development host are mounted as host0:
 * (drfpfs.c); every file operation is one request / response exchange with
 * the host's file server (drfpif.c) through the DECI4P link manager
 * SceDeci4pDfMgr (drfp.c). Sony's files: drfp.c, drfpif.c and drfpfs.c.
 */

#pragma once

#include <fw.h>
#include <scetypes.h>
#include <sysmem/cpu.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/debug.h>
#include <sysmem/list.h>
#include <kernel/error.h>
/* deci4p_dfmgr/deci4p.h declares sceDfMgrClose void; it returns an error code */
#define sceDfMgrClose sceDfMgrClose_deci4p_h
#include <deci4p_dfmgr/deci4p.h>
#include <deci4p_drfp/deci4p_fs.h>
#include <threadmgr/threadmgr.h>
#undef sceDfMgrClose
int sceDfMgrClose(SceUID handle);

/*
 * Level 0 messages are kept in every firmware; the others exist only in the
 * debug build (4.00).
 */
#if FW_DEBUG
#define DRFP_PRINTF(_level, _line, _fmt, ...)				\
	SCE_PRINTF_WITH_INFO(_level, _line, _fmt, ##__VA_ARGS__)
#else
#define DRFP_PRINTF(_level, _line, _fmt, ...)	do { } while (0)
#endif

/* sceDfMgr* results this module tells apart */

/* errno 19 / 48 of the SCE errno space (no shared names yet) */

/* the protocol registration */
#define DRFP_PROTOCOL			0x11100
#define DRFP_PROTOCOL_ALIAS		0x11000
#define DRFP_PROTOCOL_VERSION		0x1000000
#define DRFP_PROTOCOL_SUBVERSION		0x20001

/* request codes; a response carries the same code (-1: not supported) */
#define DRFP_CODE_OPEN			4
#define DRFP_CODE_CLOSE			6
#define DRFP_CODE_READ			8
#define DRFP_CODE_WRITE			10
#define DRFP_CODE_FSTAT			0xc
#define DRFP_CODE_STAT			0xe
#define DRFP_CODE_MKDIR			0x10
#define DRFP_CODE_RMDIR			0x12
#define DRFP_CODE_DOPEN			0x14
#define DRFP_CODE_DCLOSE		0x16
#define DRFP_CODE_DREAD			0x18
#define DRFP_CODE_FTRUNCATE		0x1a
#define DRFP_CODE_TRUNCATE		0x1c
#define DRFP_CODE_RENAME		0x1e
#define DRFP_CODE_REMOVE		0x20
#define DRFP_CODE_CREATE		0x26
#define DRFP_CODE_FCHSTAT		0x28
#define DRFP_CODE_CHSTAT		0x2a
#define DRFP_CODE_TRANSLATE_PATH	0x2c
#define DRFP_CODE_UNSUPPORTED		0xffffffff

/* the host's results: 0x11000 + the host's (Linux) errno */
#define DRFP_HOST_ENOENT		0x11002
#define DRFP_HOST_EIO			0x11005
#define DRFP_HOST_EBADF			0x11009
#define DRFP_HOST_ENOMEM		0x1100c
#define DRFP_HOST_EACCES		0x1100d
#define DRFP_HOST_EBUSY			0x11010
#define DRFP_HOST_EEXIST		0x11011
#define DRFP_HOST_ENOTDIR		0x11014
#define DRFP_HOST_EISDIR		0x11015
#define DRFP_HOST_EINVAL		0x11016
#define DRFP_HOST_EMFILE		0x11018
#define DRFP_HOST_ENOSPC		0x1101c
#define DRFP_HOST_EROFS			0x1101e
#define DRFP_HOST_ENAMETOOLONG		0x11024
#define DRFP_HOST_ENOTEMPTY		0x11027
#define DRFP_HOST_ENOTSUP		0x110c0

/* host file modes (POSIX; the bits in deci4p_drfp/deci4p_fs.h) */
#define DRFP_MODE_DIR_DEFAULT		0x1c0
#define DRFP_MODE_FILE_DEFAULT		0x180

#define DRFP_PATH_MAX			0x400
#define DRFP_FRAG_DATA_MAX		0x10000		/* one response fragment */
#define DRFP_READ_CHUNK_MAX		0x400000	/* one read request */
#define DRFP_WRITE_CHUNK_MAX		0x10000		/* one write request */
#define DRFP_DREAD_SIZE			0x435		/* one entry's room */

/* write flags (DrfpWritePacket.flags) */
#define DRFP_WRITE_MORE			2	/* no response: more chunks follow */
#define DRFP_WRITE_SYNC			4	/* on 4 MiB boundaries */

/* a string in a packet: length with the NUL, then the characters padded to words */
typedef struct DrfpString {
	SceSize len;
	char str[0];
} DrfpString;

#define DRFP_STRING_PAD(len)		(((len) + 4) & ~3)

/* every DRFP packet */
typedef struct DrfpHeader {
	SceDeci4pHeader hdr;
	SceUInt32 code;				/* DRFP_CODE_* */
	SceUInt16 seq;
	SceUInt16 reserved;
} DrfpHeader;

/* one Dread entry */
typedef struct DrfpDirent {
	Deci4pFsStat stat;
	SceSize nameLen;
	char name[0];
} DrfpDirent;

typedef struct DrfpOpenPacket {			/* also create */
	DrfpHeader h;
	union {
		struct {
			int flags;
			int mode;
			DrfpString name;
		} req;
		struct {
			int result;
			int fd;
			Deci4pFsStat stat;
		} res;
	};
} DrfpOpenPacket;

typedef struct DrfpFdPacket {			/* close, dclose, fstat */
	DrfpHeader h;
	union {
		int fd;
		struct {
			int result;
			Deci4pFsStat stat;
		} res;
	};
} DrfpFdPacket;

typedef struct DrfpPathPacket {			/* stat, rmdir, remove, dopen */
	DrfpHeader h;
	union {
		DrfpString name;
		struct {
			int result;
			union {
				int fd;
				Deci4pFsStat stat;
			};
		} res;
	};
} DrfpPathPacket;

typedef struct DrfpMkdirPacket {
	DrfpHeader h;
	union {
		struct {
			int mode;
			DrfpString name;
		} req;
		struct {
			int result;
			SceUInt32 padding;
			Deci4pFsStat stat;		/* missing from old hosts */
		} res;
	};
} DrfpMkdirPacket;

typedef struct DrfpTwoPathPacket {		/* rename, translate path */
	DrfpHeader h;
	union {
		DrfpString name;		/* the second string follows the first */
		struct {
			int result;
			union {
				DrfpString path;	/* translate path */
				struct {
					SceUInt32 padding;
					Deci4pFsStat stat;	/* rename; missing from old hosts */
				};
			};
		} res;
	};
} DrfpTwoPathPacket;

typedef struct __attribute__((packed, aligned(4))) DrfpReadPacket {
	DrfpHeader h;
	int fd;
	SceUInt32 padding;
	SceOff offset;
	SceSize size;
} DrfpReadPacket;

/* follows the header of every read response fragment */
typedef struct DrfpReadResult {
	int result;
	SceUInt32 reserved;
	SceUInt16 nfrag;
	SceUInt16 frag;				/* from 1 */
	SceSize nbytes;
} DrfpReadResult;

typedef struct __attribute__((packed, aligned(4))) DrfpWritePacket {
	DrfpHeader h;
	union {
		struct __attribute__((packed, aligned(4))) {
			int fd;
			SceUInt8 flags;		/* DRFP_WRITE_* */
			SceUInt8 padding0;
			SceUInt16 padding1;
			SceOff offset;
			SceSize size;
		};
		int result;
	};
} DrfpWritePacket;

typedef struct DrfpFchstatPacket {
	DrfpHeader h;
	union {
		struct {
			int fd;
			SceUInt32 bits;		/* SCE_CST_* */
			Deci4pFsStat stat;
		};
		int result;
	};
} DrfpFchstatPacket;

typedef struct DrfpChstatPacket {
	DrfpHeader h;
	union {
		struct {
			SceUInt32 bits;		/* SCE_CST_* */
			SceUInt32 padding;
			Deci4pFsStat stat;
			DrfpString name;
		};
		int result;
	};
} DrfpChstatPacket;

typedef struct DrfpDreadPacket {
	DrfpHeader h;
	union {
		struct {
			int fd;
			SceUInt32 count;	/* entries wanted (1) */
			SceSize size;		/* DRFP_DREAD_SIZE */
		} req;
		struct {
			int result;
			SceUInt32 reserved[13];
			int count;
			SceSize size;
			DrfpDirent ent[0];
		} res;
	};
} DrfpDreadPacket;

typedef struct __attribute__((packed, aligned(4))) DrfpFtruncatePacket {
	DrfpHeader h;
	union {
		struct __attribute__((packed, aligned(4))) {
			int fd;
			SceUInt32 padding;
			SceOff length;
		};
		int result;
	};
} DrfpFtruncatePacket;

typedef struct __attribute__((packed, aligned(4))) DrfpTruncatePacket {
	DrfpHeader h;
	union {
		struct __attribute__((packed, aligned(4))) {
			SceOff length;
			DrfpString name;
		};
		int result;
	};
} DrfpTruncatePacket;

/* what a read request collects; copied in and out by sceDeci4pDrfpSend */
typedef struct DrfpReadInfo {
	void *buf;
	SceSize size;
	SceUInt32 nfrag;
	int hostResult;
	SceSize received;
} DrfpReadInfo;

/* the data of a write request */
typedef struct DrfpWriteInfo {
	const void *buf;
	SceSize size;
	int state;				/* 0: header next, 1: data next */
} DrfpWriteInfo;

/* request flags */
#define DRFP_REQ_USED			1
#define DRFP_REQ_DONE			2
#define DRFP_REQ_ERROR			4

/* request event flag bits */
#define DRFP_EVF_RESPONSE		1
#define DRFP_EVF_SENT			2
#define DRFP_EVF_CANCEL			4

typedef struct DrfpRequest {
	ListNode link;
	SceUInt32 flags;			/* DRFP_REQ_* */
	SceUInt32 code;
	SceUInt32 seq;
	void *packet;
	SceSize size;
	SceUID evf;
	union {
		DrfpReadInfo read;
		DrfpWriteInfo write;
	};
	int result;
} DrfpRequest;

/* the queue head and every request contain the same list link */
typedef ListNode DrfpQueue;

/* the protocol handler's context */
typedef struct DrfpProtocol {
	SceUID handle;
	int connected;
	int registered;
	int retry;				/* wait for the link to come back */
	DrfpRequest *readRequest;		/* receiving */
	DrfpRequest *writeRequest;		/* sent without a response */
} DrfpProtocol;

#define DRFP_REQUEST_COUNT		10
#define DRFP_BUFFER_COUNT		4

/* the packet buffers: 0 general, 1 dread, 2 read, 3 write */
typedef struct DrfpBufferPool {
	SceSize size;
	SceUID memBlock;
	void *base;
	SceUID sema;
	int count;
} DrfpBufferPool;

#define DRFP_BUFFER_GENERAL		0
#define DRFP_BUFFER_DREAD		1
#define DRFP_BUFFER_READ		2
#define DRFP_BUFFER_WRITE		3

_Static_assert(sizeof(DrfpRequest) == 0x38, "DrfpRequest");
_Static_assert(sizeof(DrfpBufferPool) == 0x14, "DrfpBufferPool");
_Static_assert(sizeof(Deci4pFsStat) == 0x30, "Deci4pFsStat");
_Static_assert(sizeof(DrfpReadPacket) == 0x2c, "DrfpReadPacket");
_Static_assert(sizeof(DrfpWritePacket) == 0x2c, "DrfpWritePacket");
_Static_assert(sizeof(DrfpFchstatPacket) == 0x50, "DrfpFchstatPacket");
_Static_assert(sizeof(DrfpFtruncatePacket) == 0x28, "DrfpFtruncatePacket");
_Static_assert(sizeof(DrfpReadResult) == 0x10, "DrfpReadResult");

/* drfp.c */
int sceDeci4pDrfpInit(void);
int sceDeci4pDrfpSend(void *packet, SceSize size, void *info);
SceUID drfpGetHeap(void);
DrfpBufferPool *drfpGetBuffer(int index);
void drfpReleaseBuffer(int index);

/* drfpif.c */
int sceDeci4pDrfpCreate(const char *path, int mode, Deci4pFsStat *stat);
int sceDeci4pDrfpOpen(const char *path, int flags, int mode, Deci4pFsStat *stat);
int sceDeci4pDrfpRead(int fd, void *buf, SceOff offset, SceSize size);
int sceDeci4pDrfpWrite(int fd, const void *buf, SceOff offset, SceSize size);
int sceDeci4pDrfpFstat(int fd, Deci4pFsStat *stat);
int sceDeci4pDrfpStat(const char *path, Deci4pFsStat *stat);
int sceDeci4pDrfpFchstat(int fd, SceUInt32 bits, const Deci4pFsStat *stat);
int sceDeci4pDrfpChstat(const char *path, SceUInt32 bits, const Deci4pFsStat *stat);
int sceDeci4pDrfpTranslatePath(const char *base, const char *path, SceSize size, char *out,
			       SceSize *outLen);
int sceDeci4pDrfpClose(int fd);
int sceDeci4pDrfpMkdir(const char *path, int mode, Deci4pFsStat *stat);
int sceDeci4pDrfpRmdir(const char *path);
int sceDeci4pDrfpRemove(const char *path);
int sceDeci4pDrfpRename(const char *from, const char *to, Deci4pFsStat *stat);
int sceDeci4pDrfpDopen(const char *path);
int sceDeci4pDrfpDclose(int fd);
int sceDeci4pDrfpDread(int fd, DrfpDirent **ent);
int sceDeci4pDrfpFtruncate(int fd, SceOff length);
int sceDeci4pDrfpTruncate(const char *path, SceOff length);

/* drfpfs.c */
int sceDeci4pDrfpFsInit(void);
int sceDeci4pDrfpFsStart(void);

/* imports without a declaration elsewhere */
