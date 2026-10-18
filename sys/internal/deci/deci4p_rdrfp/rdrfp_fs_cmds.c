/*
 * SceDeci4pRDrfp file system commands (Sony's rdrfp_fs_cmds.c). Each handler
 * runs one host command on the file system thread and answers it in the
 * request's own packet; reads answer with a series of data packets.
 */
#include <psp2kern/kernel/threadmgr.h>
#include <string.h>
#include <deci4p_drfp/deci4p_fs.h>
#include <threadmgr/threadmgr.h>
#include "rdrfp.h"

#define SCE_ERROR_ERRNO_EACCES		0x8001000d
#define SCE_ERROR_ERRNO_EUNSUP		0x80010086
#define RDRFP_DFMGR_ERROR_REQUESTED	0x80080007

/* host errors: 0x12000 + the errno */
#define RDRFP_ERROR(errno)		(0x12000 + (errno))
#define RDRFP_ERROR_ENOENT		RDRFP_ERROR(2)
#define RDRFP_ERROR_EIO			RDRFP_ERROR(5)
#define RDRFP_ERROR_EBADF		RDRFP_ERROR(9)
#define RDRFP_ERROR_ENOMEM		RDRFP_ERROR(12)
#define RDRFP_ERROR_EACCES		RDRFP_ERROR(13)
#define RDRFP_ERROR_EINVAL		RDRFP_ERROR(22)
#define RDRFP_ERROR_ENAMETOOLONG	RDRFP_ERROR(36)
#define RDRFP_ERROR_UNKNOWN		0x120c0

#define RDRFP_PATH_MAX			0x3ff
#define RDRFP_READ_MAX			0x40000	/* one read buffer */
#define RDRFP_READ_PIECE		0x10000	/* one data packet */

/* the write flags of a write piece */
#define RDRFP_WRITE_MORE		2	/* more pieces follow: no reply yet */
#define RDRFP_WRITE_FIRST		4

/* the stat bits the host may change */
#define RDRFP_MODE_MASK			0xc180
#define RDRFP_CST_MASK			(SCE_CST_MODE | SCE_CST_CT | SCE_CST_AT | SCE_CST_MT)

typedef Deci4pFsStat RdrfpStat;

/* a path argument: its size, then the path, padded to 4 bytes */
typedef struct RdrfpPath {
	SceSize size;
	char path[];
} RdrfpPath;

typedef struct RdrfpReadReply {
	int result;
	SceUInt8 multi;				/* the data comes in several packets */
	SceUInt8 reserved[3];
	SceUInt16 total;			/* packets */
	SceUInt16 seq;				/* this packet, from 1 */
	SceSize size;				/* data following */
} RdrfpReadReply;

typedef struct RdrfpWriteReply {
	int result;
	SceSize written;
} RdrfpWriteReply;

typedef struct RdrfpWriteReplyPacket {
	RdrfpHeader head;
	RdrfpWriteReply reply;
} RdrfpWriteReplyPacket;

typedef struct RdrfpReaddirEntry {
	RdrfpStat stat;
	SceSize nameSize;
	char name[];				/* padded to 8 bytes */
} RdrfpReaddirEntry;

typedef int (*RdrfpCommandHandler)(RdrfpRequest *req, RdrfpHeader *packet, void *args);

typedef struct RdrfpCommand {
	SceUInt32 command;
	RdrfpCommandHandler handler;
} RdrfpCommand;

/* the replies: the result, after the packet header */
static inline int sendReply(RdrfpRequest *req, RdrfpHeader *packet, SceUInt32 type, SceSize size)
{
	rdrfpSetPacketSize(packet, size);
	rdrfpSetPacketType(packet, type);
	return rdrfpSendPacket(req);
}

/* a path argument holds a terminated path that fills it */
static inline int checkPath(RdrfpPath *arg, SceSize *size)
{
	SceSize len = strnlen(arg->path, RDRFP_PATH_MAX);

	*size = (arg->size + 3) & ~3;
	if (*size != ((len + 4) & ~3))
		return RDRFP_ERROR_ENAMETOOLONG;
	arg->path[*size - 1] = '\0';
	if (rdrfpCheckDeviceName(arg->path, *size) != 0)
		return RDRFP_ERROR_EACCES;
	return 0;
}

/* Sony's build keeps these out of line */
static __attribute__((noinline)) int rdrfpConvertErrorCode(int error)
{
	switch (error) {
	case 0:
		return 0;
	case 0x80010001:			/* EPERM */
	case 0x8001000d:			/* EACCES */
	case 0x800f090d:
		return RDRFP_ERROR(13);
	case 0x80010002:			/* ENOENT */
	case 0x80010013:			/* ENODEV */
		return RDRFP_ERROR(2);
	case 0x80010005:
		return RDRFP_ERROR(5);
	case 0x80010009:
		return RDRFP_ERROR(9);
	case 0x8001000c:
		return RDRFP_ERROR(12);
	case 0x80010010:
	case 0x80800003:
		return RDRFP_ERROR(16);
	case 0x80010011:
	case 0x80010078:
		return RDRFP_ERROR(17);
	case 0x80010014:
		return RDRFP_ERROR(20);
	case 0x80010015:
		return RDRFP_ERROR(21);
	case 0x80010016:
		return RDRFP_ERROR(22);
	case 0x80010018:
		return RDRFP_ERROR(24);
	case 0x8001001c:
		return RDRFP_ERROR(28);
	case 0x8001001e:
		return RDRFP_ERROR(30);
	case 0x8001005a:
		return RDRFP_ERROR(39);
	case 0x8001005b:
		return RDRFP_ERROR(36);
	case 0x80800006:
		return RDRFP_ERROR(0x80);
	case 0x80010086:
		return RDRFP_ERROR_UNKNOWN;
	default:
		SCE_DBG_PRINTF_WITH_INFO(0xf, 293, "RDRFP: unknown error code 0x%x\n", error);
		return RDRFP_ERROR_UNKNOWN;
	}
}

static __attribute__((noinline)) void tickToDateTime(SceUInt64 tick, SceDateTime *dt)
{
	deci4pFsUsecToDateTime(tick, dt);
}

/* the host's stat of a file */
static inline __attribute__((always_inline)) void setStat(RdrfpStat *out, const SceIoStat *stat)
{
	SceUInt32 mode = 0;

	if (stat->st_mode & SCE_STM_FDIR)
		mode |= DECI4P_FS_MODE_IFDIR;
	if (stat->st_mode & SCE_STM_FREG)
		mode |= DECI4P_FS_MODE_IFREG;
	if (stat->st_mode & (SCE_STM_RU | IO_STM_RS))
		mode |= DECI4P_FS_MODE_IRUSR;
	if (stat->st_mode & IO_STM_WU)
		mode |= DECI4P_FS_MODE_IWUSR;
	out->mode = mode;
	out->dev = 0;
	out->fid = 0;
	out->size = stat->st_size;
	out->atime = deci4pFsDateTimeToUsec(&stat->st_atime);
	out->mtime = deci4pFsDateTimeToUsec(&stat->st_mtime);
	out->ctime = deci4pFsDateTimeToUsec(&stat->st_ctime);
}

static __attribute__((noinline)) void convertStat(RdrfpStat *out, const SceIoStat *stat)
{
	setStat(out, stat);
}


/* ===== commands ===== */

static int rdrfpFSCommandHandlerGetOptions(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	SceUInt32 *options = args;
	int ret;

	rdrfpSetPacketSize(packet, sizeof(*packet) + 5 * 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_GET_OPTIONS + 1);
	options[0] = 0;
	options[1] = 1;
	options[2] = 0x10000000;
	options[3] = 4;
	options[4] = 8;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 586, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

typedef struct RdrfpAppDataArgs {
	SceUInt8 type;
	SceUInt8 reserved[7];
	char titleId[16];
	SceUInt32 arg[];
} RdrfpAppDataArgs;

static int rdrfpFSCommandHandlerUmountAppData(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpAppDataArgs *arg = args;
	int result, ret;

	if ((arg->type & 0x7f) == 0)
		result = rdrfpConvertErrorCode(rdrfpUmountAppData(arg->type, arg->titleId));
	else
		result = RDRFP_ERROR_UNKNOWN;
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_UMOUNT_APP_DATA + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1950, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

static int rdrfpFSCommandHandlerMountAppData(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpAppDataArgs *arg = args;
	int result, ret;

	if ((arg->type & 0x7f) == 0)
		result = rdrfpConvertErrorCode(rdrfpMountAppData(arg->type, arg->titleId, arg->arg));
	else
		result = RDRFP_ERROR_UNKNOWN;
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_MOUNT_APP_DATA + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1909, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

static int rdrfpFSCommandHandlerRename(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpPath *from = (RdrfpPath *)(packet + 1), *to;
	SceSize fromSize, toSize, len;
	int result, ret;

	len = strnlen(from->path, RDRFP_PATH_MAX);
	fromSize = (from->size + 3) & ~3;
	if (fromSize != ((len + 4) & ~3))
		goto too_long;
	to = (RdrfpPath *)(from->path + fromSize);
	from->path[fromSize - 1] = '\0';
	len = strnlen(to->path, RDRFP_PATH_MAX);
	toSize = (to->size + 3) & ~3;
	if (toSize != ((len + 4) & ~3))
		goto too_long;
	to->path[toSize - 1] = '\0';
	if (rdrfpCheckDeviceName(from->path, fromSize) != 0 ||
	    rdrfpCheckDeviceName(to->path, toSize) != 0) {
		result = RDRFP_ERROR_EACCES;
	} else {
		ret = rdrfpRename(from->path, fromSize, to->path, toSize);
		result = ret < 0 ? rdrfpConvertErrorCode(ret) : 0;
	}
	goto reply;
too_long:
	result = RDRFP_ERROR_ENAMETOOLONG;
reply:
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_RENAME + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1599, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

static int rdrfpFSCommandHandlerUnlink(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpPath *arg = args;
	SceSize size;
	int result, ret;

	result = checkPath(arg, &size);
	if (result == 0) {
		ret = rdrfpRemove(arg->path, size);
		result = ret < 0 ? rdrfpConvertErrorCode(ret) : 0;
	}
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_UNLINK + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1518, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

static int rdrfpFSCommandHandlerRmdir(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpPath *arg = args;
	SceSize size;
	int result, ret;

	result = checkPath(arg, &size);
	if (result == 0) {
		ret = rdrfpRmdir(arg->path, size);
		result = ret < 0 ? rdrfpConvertErrorCode(ret) : 0;
	}
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_RMDIR + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1464, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

typedef struct RdrfpMkdirArgs {
	SceIoMode mode;
	RdrfpPath path;
} RdrfpMkdirArgs;

static int rdrfpFSCommandHandlerMkdir(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpMkdirArgs *arg = args;
	SceSize size;
	int result, ret;

	result = checkPath(&arg->path, &size);
	if (result == 0) {
		ret = rdrfpMkdir(arg->path.path, size, arg->mode & RDRFP_MODE_MASK);
		result = ret < 0 ? rdrfpConvertErrorCode(ret) : 0;
	}
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_MKDIR + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1410, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

/*
 * A write, or the write buffer collecting several: each piece the host sent
 * gets its reply. A failed write is remembered in the file cache, and the
 * pieces still to come for it are answered with the same error.
 */
static int rdrfpFSCommandHandlerWrite(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpWriteHeader *arg = args;
	RdrfpDesc *desc = req->desc;
	RdrfpWritebuf *wb = desc->extra;
	RdrfpFileCacheEntry *cache;
	RdrfpWriteReplyPacket *reply;
	RdrfpWriteReply *response = args;
	RdrfpRequest *pieceReq;
	SceUInt32 fd = arg->fd, id;
	SceOff offset = arg->offset;
	SceSize size = arg->size;
	int result, written, ret, error, i, count;

	cache = rdrfpFindFileCache(fd, offset);
	if (cache) {
		if (arg->kind & RDRFP_WRITE_FIRST) {
			rdrfpDeleteFileCache(cache);
			goto write;
		}
		result = cache->result;
		if (arg->kind & RDRFP_WRITE_MORE)
			rdrfpFileCacheAddOffset(cache, size);
		else
			rdrfpDeleteFileCache(cache);
		goto reply;
	}

write:
	if (size > RDRFP_WRITE_MAX) {
		result = RDRFP_ERROR_EINVAL;
	} else {
		written = rdrfpWrite(arg->fd, (char *)packet + sizeof(*packet) + sizeof(*arg), size, offset);
		if (written >= 0) {
			result = arg->kind & RDRFP_WRITE_MORE;
			if (result) {
				rdrfpTerminateRequest(req);
				return 0;
			}
			rdrfpSetPacketSize(packet, sizeof(RdrfpWriteReplyPacket));
			rdrfpSetPacketType(packet, RDRFP_CMD_WRITE + 1);
			response->result = result;
			response->written = written;
			ret = rdrfpSendPacket(req);
			if (ret < 0)
				SCE_DBG_PRINTF_WITH_INFO(0xf, 1301, "rdrfpSendPacket failed? (0x%08x)\n", ret);
			return ret;
		}
		if (written == (int)SCE_ERROR_ERRNO_EACCES)
			result = RDRFP_ERROR_EBADF;
		else
			result = rdrfpConvertErrorCode(written);
	}
	if (arg->kind & RDRFP_WRITE_MORE) {
		if (cache == NULL)
			rdrfpAddFileCache(fd, offset + size, result);
		else
			rdrfpFileCacheAddOffset(cache, size);
	} else if (cache) {
		rdrfpDeleteFileCache(cache);
	}

reply:
	error = 0;
	if (wb) {
		/* the collected pieces but the last are answered from the buffer */
		count = wb->count;
		reply = (RdrfpWriteReplyPacket *)((char *)packet + sizeof(RdrfpWriteReplyPacket));
		for (i = 0; i < count - 1; i++, reply++) {
			id = wb->ids[i];
			pieceReq = rdrfpPrepareRequest(id, desc->bufType, desc->bufIndex, reply);
			rdrfpSetDesc(pieceReq->desc, desc->bufType, desc->bufIndex, reply, NULL, 0, 0);
			rdrfpSetPacketSize(reply, sizeof(*reply));
			rdrfpSetPacketType(&reply->head, RDRFP_CMD_WRITE + 1);
			rdrfpSetPacketId(&reply->head, id);
			reply->reply.result = result;
			reply->reply.written = 0;
			ret = rdrfpSendPacket(pieceReq);
			if (ret < 0) {
				SCE_DBG_PRINTF_WITH_INFO(0xf, 1337, "rdrfpSendPacket failed? (0x%08x)\n", ret);
				error = ret;
			}
		}
		/* Sony sets the last ID one slot past the replies, not in the packet */
		rdrfpSetPacketId(&reply->head, wb->ids[i]);
	}
	rdrfpSetPacketSize(packet, sizeof(RdrfpWriteReplyPacket));
	rdrfpSetPacketType(packet, RDRFP_CMD_WRITE + 1);
	response->result = result;
	response->written = 0;
	ret = rdrfpSendPacket(req);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1353, "rdrfpSendPacket failed? (0x%08x)\n", ret);
		return ret;
	}
	return error;
}

static int rdrfpFSCommandHandlerClose(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	SceUInt32 *fd = args;
	int result, ret;

	ret = rdrfpClose(*fd);
	result = ret < 0 ? rdrfpConvertErrorCode(ret) : 0;
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_CLOSE + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1090, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

static int rdrfpFSCommandHandlerClosedir(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	SceUInt32 *fd = args;
	int result, ret;

	ret = rdrfpDclose(*fd);
	result = ret < 0 ? rdrfpConvertErrorCode(ret) : 0;
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_CLOSEDIR + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 737, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

static int rdrfpFSCommandHandlerOpendir(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpPath *arg = args;
	RdrfpFd *dir;
	SceSize size;
	int result, fd, ret;

	result = checkPath(arg, &size);
	if (result != 0) {
		fd = -1;
	} else {
		ret = rdrfpDopen(arg->path, size, &dir);
		if (ret < 0) {
			result = rdrfpConvertErrorCode(ret);
			fd = -1;
		} else {
			fd = dir->fd;
			result = 0;
		}
	}
	rdrfpSetPacketSize(packet, sizeof(*packet) + 8);
	rdrfpSetPacketType(packet, RDRFP_CMD_OPENDIR + 1);
	((int *)args)[0] = result;
	((int *)args)[1] = fd;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 698, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

static int rdrfpFSCommandHandlerGetDeviceList(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	SceSize size = sizeof(*packet) + 8;
	SceOff total, free;
	int dev, count = 0, ret;

	rdrfpSetPacketType(packet, RDRFP_CMD_GET_DEVICE_LIST + 1);
	for (dev = 0; dev < RDRFP_DEVICES; dev++) {
		if (getDeviceInfo(dev, &total, &free) < 0)
			continue;
		count++;
		size += rdrfpMakeDeviceInfo(dev, total, free, (RdrfpDeviceInfo *)((char *)packet + size));
	}
	((int *)args)[0] = 0;
	((int *)args)[1] = count;
	rdrfpSetPacketSize(packet, size);
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 637, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

typedef struct RdrfpFtruncateArgs {
	SceUInt32 fd;
	SceUInt32 reserved;
	SceOff size __attribute__((packed, aligned(4)));
} RdrfpFtruncateArgs;

static int rdrfpFSCommandHandlerFtruncate(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpFtruncateArgs *arg = args;
	SceIoStat stat;
	int result, ret;

	memset(&stat, 0, sizeof(stat));
	stat.st_size = arg->size;
	ret = rdrfpChstatByFd(arg->fd, &stat, SCE_CST_SIZE);
	result = ret < 0 ? rdrfpConvertErrorCode(ret) : 0;
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_FTRUNCATE + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1702, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

typedef struct RdrfpTruncateArgs {
	SceOff size __attribute__((packed, aligned(4)));
	RdrfpPath path;
} RdrfpTruncateArgs;

static int rdrfpFSCommandHandlerTruncate(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpTruncateArgs *arg = args;
	SceIoStat stat;
	SceSize size;
	int result, ret;

	result = checkPath(&arg->path, &size);
	if (result == 0) {
		memset(&stat, 0, sizeof(stat));
		stat.st_size = arg->size;
		ret = rdrfpChstat(arg->path.path, size, &stat, SCE_CST_SIZE);
		result = ret < 0 ? rdrfpConvertErrorCode(ret) : 0;
	}
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_TRUNCATE + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1658, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

/* the stat fields the host changes, and which */
static inline int makeChstat(const SceUInt32 *bits, const RdrfpStat *in, SceIoStat *stat)
{
	SceUInt32 current = *bits;
	int cbit = 0;

	memset(stat, 0, sizeof(*stat));
	if (current & SCE_CST_MODE) {
		stat->st_mode = in->mode & RDRFP_MODE_MASK;
		cbit |= SCE_CST_MODE;
	}
	if (current & SCE_CST_CT) {
		tickToDateTime(in->ctime, &stat->st_ctime);
		cbit |= SCE_CST_CT;
		current = *bits;
	}
	if (current & SCE_CST_AT) {
		tickToDateTime(in->atime, &stat->st_atime);
		cbit |= SCE_CST_AT;
		current = *bits;
	}
	if (current & SCE_CST_MT) {
		tickToDateTime(in->mtime, &stat->st_mtime);
		cbit |= SCE_CST_MT;
	}
	return cbit;
}

typedef struct RdrfpFchstatArgs {
	SceUInt32 fd;
	SceUInt32 bits;
	RdrfpStat stat;
} RdrfpFchstatArgs;

static int rdrfpFSCommandHandlerFchstat(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpFchstatArgs *arg = args;
	SceIoStat stat;
	int result, ret;

	if (arg->bits & ~RDRFP_CST_MASK) {
		result = RDRFP_ERROR_EINVAL;
	} else {
		int cbit = makeChstat(&arg->bits, &arg->stat, &stat);

		ret = rdrfpChstatByFd(arg->fd, &stat, cbit);
		result = ret < 0 ? rdrfpConvertErrorCode(ret) : 0;
	}
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_FCHSTAT + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1867, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

typedef struct RdrfpChstatArgs {
	SceUInt32 bits;
	SceUInt32 reserved;
	RdrfpStat stat;
	RdrfpPath path;
} RdrfpChstatArgs;

static int rdrfpFSCommandHandlerChstat(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpChstatArgs *arg = args;
	SceIoStat stat;
	SceSize size;
	int result, ret;

	result = checkPath(&arg->path, &size);
	if (result == 0) {
		if (arg->bits & ~RDRFP_CST_MASK) {
			result = RDRFP_ERROR_EINVAL;
		} else {
			int cbit = makeChstat(&arg->bits, &arg->stat, &stat);

			ret = rdrfpChstat(arg->path.path, size, &stat, cbit);
			result = ret < 0 ? rdrfpConvertErrorCode(ret) : 0;
		}
	}
	rdrfpSetPacketSize(packet, sizeof(*packet) + 4);
	rdrfpSetPacketType(packet, RDRFP_CMD_CHSTAT + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1792, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

typedef struct RdrfpOpenArgs {
	SceUInt32 flags;
	SceIoMode mode;
	RdrfpPath path;
} RdrfpOpenArgs;

typedef struct RdrfpOpenReply {
	int result;
	int fd;
	RdrfpStat stat;
} RdrfpOpenReply;

static int rdrfpFSCommandHandlerOpen(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpOpenArgs *arg = args;
	RdrfpOpenReply *reply = args;
	SceUInt32 flags = arg->flags;
	SceIoMode mode;
	SceIoStat stat;
	RdrfpFd *file;
	SceSize size, replySize, len;
	int result, fd, ret;

	len = strnlen(arg->path.path, RDRFP_PATH_MAX);
	mode = arg->mode;
	size = (arg->path.size + 3) & ~3;
	if (size != ((len + 4) & ~3)) {
		result = RDRFP_ERROR_ENAMETOOLONG;
	} else {
		arg->path.path[size - 1] = '\0';
		result = rdrfpCheckDeviceName(arg->path.path, size);
		if (result != 0)
			result = RDRFP_ERROR_EACCES;
	}
	if (result != 0) {
		fd = -1;
		replySize = sizeof(*packet) + 8;
	} else {
		ret = rdrfpOpen(arg->path.path, size, flags & 0xf03, mode & RDRFP_MODE_MASK,
				&file);
		if (ret < 0) {
			result = rdrfpConvertErrorCode(ret);
			fd = -1;
			replySize = sizeof(*packet) + 8;
		} else {
			ret = rdrfpGetstat(arg->path.path, size, &stat);
			if (ret < 0) {
				fd = -1;
				rdrfpClose(file->fd);
				result = rdrfpConvertErrorCode(ret);
				replySize = sizeof(*packet) + 8;
			} else {
				fd = file->fd;
				convertStat(&reply->stat, &stat);
				replySize = sizeof(*packet) + sizeof(*reply);
				result = 0;
			}
		}
	}
	rdrfpSetPacketSize(packet, replySize);
	rdrfpSetPacketType(packet, RDRFP_CMD_OPEN + 1);
	reply->result = result;
	reply->fd = fd;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1051, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

typedef struct RdrfpStatReply {
	int result;
	RdrfpStat stat;
} RdrfpStatReply;

static int rdrfpFSCommandHandlerStat(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpPath *arg = args;
	SceIoStat stat;
	SceSize size, replySize;
	int result, ret;

	result = checkPath(arg, &size);
	if (result != 0) {
		replySize = sizeof(*packet) + 4;
	} else {
		ret = rdrfpGetstat(arg->path, size, &stat);
		if (ret < 0) {
			result = rdrfpConvertErrorCode(ret);
			replySize = sizeof(*packet) + 4;
		} else {
			convertStat(&((RdrfpStatReply *)args)->stat, &stat);
			replySize = sizeof(*packet) + sizeof(RdrfpStatReply);
			result = 0;
		}
	}
	rdrfpSetPacketType(packet, RDRFP_CMD_STAT + 1);
	rdrfpSetPacketSize(packet, replySize);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 967, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

typedef struct RdrfpReaddirArgs {
	SceUInt32 fd;
	SceUInt32 count;			/* entries at most, 0: no limit */
	SceSize size;				/* bytes at most */
} RdrfpReaddirArgs;

typedef struct RdrfpReaddirReply {
	int result;
	SceUInt32 reserved;
	RdrfpStat stat;				/* of the directory */
	SceUInt32 count;
	SceSize size;
} RdrfpReaddirReply;

#define RDRFP_READDIR_ENTRY_MAX		(sizeof(RdrfpReaddirEntry) + 0x100)

static int rdrfpFSCommandHandlerReaddir(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpReaddirArgs *arg = args;
	RdrfpReaddirReply *reply = args;
	RdrfpReaddirEntry *entry;
	SceIoStat dirStat;
	SceIoDirent dirent;
	RdrfpStat stat;
	SceSize maxSize, size, len, entrySize, pad;
	SceUInt32 fd, maxCount, count;
	int ret;

	maxSize = arg->size;
	if (maxSize > RDRFP_PACKET_MAX)
		maxSize = RDRFP_PACKET_MAX;
	fd = arg->fd;
	maxCount = arg->count;
	memset(&dirStat, 0, sizeof(dirStat));
	ret = rdrfpGetstatByFd(fd, &dirStat);
	if (ret < 0)
		goto failed;

	size = 0;
	count = 0;
	entry = (RdrfpReaddirEntry *)((char *)packet + sizeof(*packet) + sizeof(*reply));
	do {
		if (maxCount != 0 && count >= maxCount)
			break;
		memset(&dirent, 0, sizeof(dirent));
		ret = rdrfpDread(fd, &dirent);
		if (ret < 0) {
			rdrfpSetPacketSize(packet, sizeof(*packet) + sizeof(*reply));
			rdrfpSetPacketType(packet, RDRFP_CMD_READDIR + 1);
			reply->result = rdrfpConvertErrorCode(ret);
			ret = rdrfpSendPacket(req);
			if (ret < 0)
				SCE_DBG_PRINTF_WITH_INFO(0xf, 817, "rdrfpSendPacket failed? (0x%08x)\n", ret);
			return ret;
		}
		if (ret == 0)
			break;
		setStat(&entry->stat, &dirent.d_stat);
		len = strnlen(dirent.d_name, sizeof(dirent.d_name));
		entry->nameSize = len + 1;
		memcpy(entry->name, dirent.d_name, len + 1);
		entrySize = (len + offsetof(RdrfpReaddirEntry, name) + 1 + 7) & ~7;
		pad = entrySize - (offsetof(RdrfpReaddirEntry, name) + 1) - len;
		if (pad + len + 1 > 0x100)
			pad = 0xff - len;
		memset(entry->name + len + 1, 0, pad);
		if (count == 0 && maxSize < entrySize) {
			rdrfpSetPacketSize(packet, sizeof(*packet) + sizeof(*reply));
			rdrfpSetPacketType(packet, RDRFP_CMD_READDIR + 1);
			reply->result = RDRFP_ERROR_ENOMEM;
			ret = rdrfpSendPacket(req);
			if (ret < 0)
				SCE_DBG_PRINTF_WITH_INFO(0xf, 838, "rdrfpSendPacket failed? (0x%08x)\n", ret);
			return ret;
		}
		entry = (RdrfpReaddirEntry *)((char *)entry + entrySize);
		size += entrySize;
		count++;
	} while (maxSize - size >= RDRFP_READDIR_ENTRY_MAX);

	__builtin_memset(&stat, 0, sizeof(stat));
	reply->result = 0;
	convertStat(&stat, &dirStat);
	reply->stat = stat;
	reply->count = count;
	reply->size = size;
	rdrfpSetPacketSize(packet, size + sizeof(*packet) + sizeof(*reply));
	rdrfpSetPacketType(packet, RDRFP_CMD_READDIR + 1);
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 867, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;

failed:
	rdrfpSetPacketSize(packet, sizeof(*packet) + sizeof(*reply));
	rdrfpSetPacketType(packet, RDRFP_CMD_READDIR + 1);
	reply->result = rdrfpConvertErrorCode(ret);
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 791, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

typedef struct RdrfpReadArgs {
	SceUInt32 fd;
	SceUInt32 reserved;
	SceOff offset __attribute__((packed, aligned(4)));
	SceSize size;
} RdrfpReadArgs;

typedef struct RdrfpReadReplyPacket {
	RdrfpHeader head;
	RdrfpReadReply reply;
} RdrfpReadReplyPacket;

static inline void setReadReply(RdrfpReadReplyPacket *packet, SceSize dataSize, SceUInt32 id,
				int result, int multi, SceUInt16 total, SceUInt16 seq)
{
	rdrfpSetPacketSize(packet, sizeof(*packet) + dataSize);
	rdrfpSetPacketType(&packet->head, RDRFP_CMD_READ + 1);
	rdrfpSetPacketId(&packet->head, id);
	packet->reply.size = dataSize;
	packet->reply.result = result;
	packet->reply.multi = multi;
	packet->reply.seq = seq;
	packet->reply.total = total;
}

/*
 * A read, a read buffer at a time, each sent in data packets of 64 KiB. When
 * no read buffer is free and other requests wait, the read is queued again
 * and continues later from where it stopped.
 */
static int sendReadRes(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	RdrfpReadArgs *arg = args;
	RdrfpDesc *desc = req->desc;
	RdrfpReadState *state = desc->extra;
	RdrfpReadReplyPacket *reply;
	RdrfpRequest *pieceReq;
	SceUInt32 fd = arg->fd, id = desc->id;
	SceSize remaining = arg->size, chunk, left, piece;
	SceOff offset = arg->offset;
	SceUInt16 total = (remaining + 0xffff) >> 16, seq = 1;
	int multi = total > 1;
	int index, n, last, result, ret, error;
	char *buf, *data;

	if (state->flags) {
		offset = state->offset;
		remaining = state->remaining;
		seq = state->seq;
	}
	do {
		while ((index = rdrfpReserveBuffer(RDRFP_BUF_READ, 0)) < 0) {
			if (!rdrfpIsRequestQueueEmpty())
				goto requeue;
			sceKernelDelayThread(1);
		}
		buf = rdrfpBufferAddr(RDRFP_BUF_READ, index);
		reply = (RdrfpReadReplyPacket *)(buf + RDRFP_READ_MAX);
		chunk = remaining < RDRFP_READ_MAX ? remaining : RDRFP_READ_MAX;
		n = rdrfpRead(fd, buf, chunk, offset);
		if (n < 0) {
			if (n == (int)SCE_ERROR_ERRNO_EACCES)
				result = RDRFP_ERROR_EBADF;
			else
				result = rdrfpConvertErrorCode(n);
			rdrfpReleaseBuffer(RDRFP_BUF_READ, index);
			error = 0;
			goto reply_error;
		}
		remaining -= n;
		last = (SceSize)n != chunk || remaining == 0;

		data = buf;
		left = n;
		do {
			piece = left < RDRFP_READ_PIECE ? left : RDRFP_READ_PIECE;
			pieceReq = rdrfpPrepareRequest(id, RDRFP_BUF_READ, index, reply);
			rdrfpSetDesc(pieceReq->desc, RDRFP_BUF_READ, index, reply, data, piece, left <= piece);
			setReadReply(reply, piece, id & 0xffff, 0, multi, total, seq);
			ret = rdrfpSendPacket(pieceReq);
			if (ret < 0 && ret != (int)RDRFP_DFMGR_ERROR_REQUESTED) {
				SCE_DBG_PRINTF_WITH_INFO(0xf, 516, "rdrfpSendPacket failed? (0x%08x)\n", ret);
				if (left > piece)
					rdrfpReleaseBuffer(RDRFP_BUF_READ, index);
				result = RDRFP_ERROR_EIO;
				error = ret;
				goto reply_error;
			}
			seq++;
			left -= piece;
			reply++;
			data += piece;
		} while (left != 0);

		if (last) {
			if (piece != RDRFP_READ_PIECE || remaining == 0) {
				rdrfpTerminateRequest(req);
				return 0;
			}
			/* the data ended on a whole packet: an empty one ends it */
			setReadReply((RdrfpReadReplyPacket *)packet, 0, id & 0xffff, 0, multi, total, seq);
			ret = rdrfpSendPacket(req);
			if (ret >= 0 || ret == (int)RDRFP_DFMGR_ERROR_REQUESTED)
				return 0;
			result = RDRFP_ERROR_EIO;
			SCE_DBG_PRINTF_WITH_INFO(0xf, 516, "rdrfpSendPacket failed? (0x%08x)\n", ret);
			error = ret;
			goto reply_error;
		}
		offset += n;
	} while (rdrfpIsRequestQueueEmpty());

requeue:
	state = desc->extra;
	state->offset = offset;
	state->seq = seq;
	state->continued = 1;
	state->reserved[0] = 0;
	state->reserved[1] = 0;
	state->remaining = remaining;
	state->reserved[2] = 0;
	rdrfpQueueRequest(req);
	return 0;

reply_error:
	setReadReply((RdrfpReadReplyPacket *)packet, 0, id & 0xffff, result, multi, total, seq);
	ret = rdrfpSendPacket(req);
	if (ret < 0 && ret != (int)RDRFP_DFMGR_ERROR_REQUESTED)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 516, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	else
		ret = 0;
	return error ? error : ret;
}

static int rdrfpFSCommandHandlerFstat(RdrfpRequest *req, RdrfpHeader *packet, void *args)
{
	SceUInt32 *fd = args;
	SceIoStat stat;
	int result, ret;

	ret = rdrfpGetstatByFd(*fd, &stat);
	if (ret < 0) {
		result = rdrfpConvertErrorCode(ret);
	} else {
		setStat(&((RdrfpStatReply *)args)->stat, &stat);
		result = 0;
	}
	rdrfpSetPacketSize(packet, sizeof(*packet) + sizeof(RdrfpStatReply));
	rdrfpSetPacketType(packet, RDRFP_CMD_FSTAT + 1);
	*(int *)args = result;
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 908, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return ret;
}

static const RdrfpCommand rdrfpCommands[] = {
	{ RDRFP_CMD_GET_OPTIONS, rdrfpFSCommandHandlerGetOptions },
	{ RDRFP_CMD_GET_DEVICE_LIST, rdrfpFSCommandHandlerGetDeviceList },
	{ RDRFP_CMD_OPENDIR, rdrfpFSCommandHandlerOpendir },
	{ RDRFP_CMD_CLOSEDIR, rdrfpFSCommandHandlerClosedir },
	{ RDRFP_CMD_READDIR, rdrfpFSCommandHandlerReaddir },
	{ RDRFP_CMD_STAT, rdrfpFSCommandHandlerStat },
	{ RDRFP_CMD_FSTAT, rdrfpFSCommandHandlerFstat },
	{ RDRFP_CMD_CHSTAT, rdrfpFSCommandHandlerChstat },
	{ RDRFP_CMD_FCHSTAT, rdrfpFSCommandHandlerFchstat },
	{ RDRFP_CMD_OPEN, rdrfpFSCommandHandlerOpen },
	{ RDRFP_CMD_CLOSE, rdrfpFSCommandHandlerClose },
	{ RDRFP_CMD_READ, sendReadRes },
	{ RDRFP_CMD_WRITE, rdrfpFSCommandHandlerWrite },
	{ RDRFP_CMD_MKDIR, rdrfpFSCommandHandlerMkdir },
	{ RDRFP_CMD_RMDIR, rdrfpFSCommandHandlerRmdir },
	{ RDRFP_CMD_UNLINK, rdrfpFSCommandHandlerUnlink },
	{ RDRFP_CMD_RENAME, rdrfpFSCommandHandlerRename },
	{ RDRFP_CMD_TRUNCATE, rdrfpFSCommandHandlerTruncate },
	{ RDRFP_CMD_FTRUNCATE, rdrfpFSCommandHandlerFtruncate },
	{ RDRFP_CMD_MOUNT_APP_DATA, rdrfpFSCommandHandlerMountAppData },
	{ RDRFP_CMD_UMOUNT_APP_DATA, rdrfpFSCommandHandlerUmountAppData },
	{ (SceUInt32)-1, NULL },
};

int rdrfpFSCallCommandHandler(RdrfpRequest *req, SceUInt32 command, RdrfpHeader *packet, void *args)
{
	int i, ret;

	for (i = 0; rdrfpCommands[i].command != command;) {
		i++;
		if (rdrfpCommands[i].command == (SceUInt32)-1)
			goto unknown;
	}
	if (rdrfpCommands[i].handler)
		return rdrfpCommands[i].handler(req, packet, args);
unknown:
	SCE_DBG_PRINTF_WITH_INFO(0xf, 179, "unknown command 0x%08x\n", command);
	rdrfpSetPacketSize(packet, sizeof(*packet));
	rdrfpSetPacketType(packet, (SceUInt32)-1);
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 189, "rdrfpSendPacket failed? (0x%08x)\n", ret);
	return SCE_ERROR_ERRNO_EUNSUP;
}
