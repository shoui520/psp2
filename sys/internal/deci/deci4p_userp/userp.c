/*
 * SceDeci4pUserp: DECI4P channels for user programs ("USERP"), plus the
 * debugger calls user programs may make (host processes, watchpoints).
 *
 * sceKernelDeci4pOpen creates a kernel object (SceUIDDeci4pUserpClass) whose
 * state lives in its own memblock: the DfMgr protocol registration, a receive
 * ring for packets from the host and one packet buffer for writes. Up to 16
 * channels are open at a time; a process's channels are closed when it exits.
 *
 * Sony's internal function names are not known (no debug records).
 */

#include <sysmem/cpu.h>
#include <sysmem/uid_guid.h>
#include <sysmem/dipsw.h>
#include <sysmem/procevent.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <deci4p_dfmgr/deci4p.h>
#include <sysmem/sysroot.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <stddef.h>

/* Sony's debug build is fortified */
#if FW_DEBUG
#include <sysmem/fortify.h>
#endif

#define USERP_CHANNELS		16
#define USERP_STATE_SIZE	0x12000
#define USERP_RING_MIN		0x10
#define USERP_RING_MAX		0x100000
#define USERP_WRITE_MAX		0x10800
#define USERP_STRBUF_SIZE	0x1000

/* the host process strings, copied from user space into the StrBuf memblock */
typedef struct UserpStrBuf {
	char pathName[0x400];
	char cmdLine[0x400];
	char workDir[0x400];
} UserpStrBuf;

/* errors (names unknown) */
#define USERP_ERROR_ABORT	0x800807fa
#define USERP_ERROR_ABORT2	0x800807fb


/* UserpState.status (also the callback argument) */
#define STATUS_PENDING_MASK	0x00ffffff	/* bytes waiting in the ring */
#define STATUS_DATA		0x10000000
#define STATUS_DOWN		0x20000000
#define STATUS_CLOSED		0x40000000
#define STATUS_KEEP		0xef000000

/* event flag bits */
#define EVF_DATA		0x01
#define EVF_SENT		0x02
#define EVF_CLOSE		0x04
#define EVF_SEND_FAIL		0x08
#define EVF_DOWN		0x10
#define EVF_CLOSED		0x20
#define EVF_ATTR_UNK8000	0x8000		/* meaning unknown */

unsigned long long __aeabi_uidivmod(unsigned int n, unsigned int d);

typedef struct UserpPacket {
	SceUInt16 size;
	SceUInt8 sizeHi;
	SceUInt8 version;
	SceUInt16 src;
	SceUInt16 dst;
	SceUInt32 protocol;
	SceUInt8 protocolHi;
	SceUInt8 reserved1;			/* SceDeci4pHeader.reserved1, sent as 0 */
	SceUInt16 reserved2;
	SceUInt8 data[USERP_WRITE_MAX];
} UserpPacket;

typedef struct UserpState {
	SceDfMgrProtocol proto;
	SceUID pid;
	SceUID puid;				/* the owner's id of the object */
	SceUID evf;
	SceUID readMutex;
	SceUID writeMutex;
	SceUID ringBlock;
	SceUID callback;
	int handle;				/* sceDfMgrOpen */
	SceUInt32 protocol;
	SceUInt32 needSpace;			/* a packet did not fit: send space when read */
	SceUInt32 status;
	SceSize ringSize;
	SceUInt8 *ring;
	SceSize writePos;
	SceSize readPos;
	SceSize used;
	SceDeci4pHeader hdr;			/* of the packet being received */
	SceSize recvCount;
	SceSize pending;			/* payload bytes in the ring */
	SceSize sent;
	const void *sendPtr;
	SceSize sendLen;
	UserpPacket pkt;
	SceUInt8 reserved[0x110a0 - 0x108ac];
	SceKernelSpinlock lock;
} UserpState;

typedef struct UserpObject {
	SceObjectBase base;
	SceUID memblock;
	UserpState *state;
} UserpObject;


_Static_assert(sizeof(UserpPacket) == 0x10810, "UserpPacket layout");
_Static_assert(offsetof(UserpState, lock) == 0x110a0, "UserpState layout");

static SceUID s_procEvent;
static SceUID s_channel[USERP_CHANNELS];
static SceUID s_mutex;
static char *s_className;
static SceClass *s_class;

/* syscalls run their kernel part with TPIDRURO shifted */
#define SYSCALL_ENTER(t)	((t) = sceSyscallEnter())
#define SYSCALL_LEAVE(t)	sceSyscallLeave(t)

/* check sceKernelDeci4pCreateHostProcess parameters; strings go to strbuf */
static int UserpCheckHostParam(UserpStrBuf *strbuf, const void *uparam, void *uout, SceKernelDeci4pCreateHostProcessParam *param,
			       SceUInt32 *out)
{
	char *p, c;
	char *workDir = NULL;

	if (uout == NULL || sceKernelIsAccessibleRange(0x20, uout, 0x20) < 0)
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
	if (sceKernelCopyFromUser(param, uparam, sizeof(*param)) < 0)
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;

	if (param->pathName != NULL) {
		if (sceKernelStrncpyFromUser(strbuf->pathName, param->pathName, sizeof(strbuf->pathName)) < 0)
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
		if (strbuf->pathName[sizeof(strbuf->pathName) - 1] != '\0')
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PARAM;
		if (strncmp(strbuf->pathName, "host0:", 6) != 0) {
			param->pathName = NULL;
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PARAM;
		}
		for (p = strbuf->pathName + 5; (c = p[1]) != '\0'; )
			*++p = c == '\\' ? '/' : c;
		param->pathName = strbuf->pathName + 6;
		if (param->pathName == NULL)
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PARAM;
	}
	if (param->cmdLine != NULL) {
		if (sceKernelStrncpyFromUser(strbuf->cmdLine, param->cmdLine, sizeof(strbuf->cmdLine)) < 0)
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
		if (strbuf->cmdLine[sizeof(strbuf->cmdLine) - 1] != '\0')
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PARAM;
		param->cmdLine = strbuf->cmdLine;
	}
	if (param->workDir != NULL) {
		workDir = strbuf->workDir;
		if (sceKernelStrncpyFromUser(workDir, param->workDir, sizeof(strbuf->workDir)) < 0)
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
		if (strbuf->workDir[sizeof(strbuf->workDir) - 1] != '\0')
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PARAM;
		param->workDir = workDir;
	}

	if (param->flags & ~7)
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PARAM;
	if (param->flags & 4) {
		if ((param->flags & 6) != 4)
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PARAM;
		if (workDir == NULL)
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
	} else if (workDir != NULL) {
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PARAM;
	}

	if (param->reserved0 != 0 || param->reserved1[0] != 0 || param->reserved1[1] != 0 ||
	    param->reserved1[2] != 0 || param->reserved1[3] != 0 || param->reserved1[4] != 0 ||
	    param->reserved1[5] != 0 || param->reserved1[6] != 0 || param->reserved1[7] != 0)
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_RESERVED;
	if (param->pathName == NULL)
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
	memset(out, 0xff, 0x20);
	memset(out + 2, 0, 0x18);
	return 0;
}

static inline __attribute__((always_inline)) void UserpResetRing(UserpState *st)
{
	st->writePos = 0;
	st->readPos = 0;
	st->status &= STATUS_KEEP;
	st->used = 0;
	memset(&st->hdr, 0, sizeof(st->hdr));
	memset(st->ring, 0, st->ringSize);
	st->recvCount = 0;
	st->pending = 0;
}

static void UserpNotify(UserpState *st)
{
	if (st->callback > 0)
		sceKernelNotifyCallback(st->callback, st->status);
}

/* name unknown: DfMgr event handler (protocol API version 2) */
static int UserpHandler(int event, void *arg, void *ctx)
{
	UserpState *st = ctx;
	const SceDfMgrStatus *status = arg;
	SceSize count, total, rem, pos, len;
	SceUInt8 *dst;
	int ret;

	switch (event) {
	case 1:
	case 2:					/* packet data; 2: the packet is complete */
		count = st->recvCount;
		if (count == 0) {
			/* the header: there must be room for the whole packet */
			ret = sceDfMgrRecv(st->handle, &st->hdr, sizeof(st->hdr));
			if (ret < 0)
				return ret;
			total = st->hdr.size[0] | st->hdr.size[1] << 8 | st->hdr.size[2] << 16;
			st->recvCount += ret;
			if (st->ringSize - st->used < total) {
				st->recvCount = count;
				st->needSpace = 1;
				return DFMGR_ERROR_QUEUE_FULL;
			}
			pos = st->writePos;
			if (st->ringSize < ret + pos) {
				memcpy(st->ring + pos, &st->hdr, st->ringSize - pos);
				memcpy(st->ring, (SceUInt8 *)&st->hdr + (st->ringSize - st->writePos),
				       st->writePos - st->ringSize + ret);
			} else {
				memcpy(st->ring + pos, &st->hdr, ret);
			}
			return 0;
		}

		total = st->hdr.size[0] | st->hdr.size[1] << 8 | st->hdr.size[2] << 16;
		if (count < total) {
			rem = total - count;
			pos = count + st->writePos;
			len = rem;
			if (rem + pos > st->ringSize) {
				if (pos < st->ringSize) {
					len = st->ringSize - pos;
					dst = st->ring + pos;
				} else {
					dst = st->ring + (pos - st->ringSize);
				}
			} else {
				dst = pos >= st->ringSize ? st->ring + (pos - st->ringSize)
							  : st->ring + pos;
			}
			ret = sceDfMgrRecv(st->handle, dst, len);
			if (ret < 0)
				return ret;
			count = st->recvCount + ret;
			st->recvCount = count;
			if (event != 2 || count == 0)
				return 0;
			total = st->hdr.size[0] | st->hdr.size[1] << 8 | st->hdr.size[2] << 16;
		} else if (event != 2) {
			return 0;
		}

		/* complete: hand the packet to the reader */
		st->writePos = (st->writePos + count) % st->ringSize;
		st->recvCount = 0;
		st->used += count;
		st->pending = total + st->pending - sizeof(SceDeci4pHeader);
		sceKernelSetEventFlag(st->evf, EVF_DATA);
		st->status = (st->pending | STATUS_DATA) | (st->status & 0xff000000);
		UserpNotify(st);
		return 0;

	case 3:					/* ready to send */
		ret = sceDfMgrSend(st->handle, (const SceUInt8 *)st->sendPtr + st->sent,
				   st->sendLen - st->sent);
		if (ret > 0)
			st->sent += ret;
		return 0;

	case 4:					/* sent */
		sceKernelSetEventFlag(st->evf, EVF_SENT);
		return 0;

	case 5:					/* status */
		UserpResetRing(st);
		if (status->kind == 0) {
			if (status->connected == 0) {
				st->status |= STATUS_DOWN | STATUS_CLOSED;
				sceKernelSetEventFlag(st->evf, EVF_DOWN | EVF_CLOSED);
			} else if (status->connected == 1) {
				st->status &= ~STATUS_DOWN;
				sceKernelClearEventFlag(st->evf, ~EVF_DOWN);
			}
			UserpNotify(st);
			return 0;
		}
		if (status->kind != 1)
			return 0;
		if (status->enabled == 0) {
			st->status |= STATUS_CLOSED;
			sceKernelSetEventFlag(st->evf, EVF_CLOSED);
		} else if (status->enabled == 1) {
			st->status &= ~STATUS_CLOSED;
			sceKernelClearEventFlag(st->evf, ~EVF_CLOSED);
		} else {
			return 0;
		}
		UserpNotify(st);
		return 0;

	case 6:					/* status (version 2) */
		UserpResetRing(st);
		if (status->kind == 1) {
			st->status |= STATUS_DOWN | STATUS_CLOSED;
			sceKernelSetEventFlag(st->evf, EVF_SEND_FAIL | EVF_DOWN | EVF_CLOSED);
		} else if (status->kind == 2) {
			st->status |= STATUS_CLOSED;
			sceKernelSetEventFlag(st->evf, EVF_SEND_FAIL | EVF_CLOSED);
		} else {
			return 0;
		}
		UserpNotify(st);
		return 0;
	}
	return 0;
}

/* free everything an object holds (fields still zero are skipped) */
static inline __attribute__((always_inline)) void UserpFreeObject(UserpObject *obj)
{
	UserpState *st = obj->state;

	if (st->handle > 0)
		sceDfMgrClose(st->handle);
	if (st->evf > 0)
		sceKernelDeleteEventFlag(st->evf);
	if (st->readMutex > 0)
		sceKernelDeleteMutex(st->readMutex);
	if (st->writeMutex > 0)
		sceKernelDeleteMutex(st->writeMutex);
	if (st->ringBlock > 0)
		SceSysmemForDriver_009E1C61(st->ringBlock);
	if (obj->memblock > 0)
		SceSysmemForDriver_009E1C61(obj->memblock);
}

/* name unknown: a process exited: close its channels */
static int UserpCloseProcess(SceUID pid)
{
	UserpObject *obj;
	UserpState *st;
	int i, ret;

	ret = sceKernelLockMutex(s_mutex, 1, NULL);
	if (ret < 0)
		return ret;
	for (i = 0; i < USERP_CHANNELS; i++) {
		if (s_channel[i] == -1)
			continue;
		if (sceGUIDReferObjectWithClass(s_channel[i], s_class, (SceObjectBase **)&obj) < 0) {
			sceKernelUnlockMutex(s_mutex, 1);
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SID;
		}
		st = obj->state;
		if (st->pid != pid) {
			sceGUIDReleaseObject(s_channel[i]);
			continue;
		}
		SceSysmemForDriver_84A4AF5E(pid, st->puid);
		ret = sceKernelLockMutex(st->writeMutex, 1, NULL);
		if (ret < 0) {
			sceGUIDReleaseObject(s_channel[i]);
			sceKernelUnlockMutex(s_mutex, 1);
			return ret;
		}
		sceKernelSetEventFlag(st->evf, EVF_CLOSE);
		UserpFreeObject(obj);
		sceGUIDReleaseObject(s_channel[i]);
		sceGUIDClose(s_channel[i]);
		s_channel[i] = -1;
	}
	sceKernelUnlockMutex(s_mutex, 1);
	return 0;
}

static int UserpProcKill(SceUID pid, ProcEventParam *param, int arg)
{
	UserpCloseProcess(pid);
	return 0;
}

static int UserpProcExit(SceUID pid, ProcEventParam *param, int arg)
{
	UserpCloseProcess(pid);
	return 0;
}

static int UserpProcStop(SceUID pid, int type, ProcEventParam *param, int arg)
{
	return 0;
}

static int UserpProcStart(SceUID pid, int type, ProcEventParam *param, int arg)
{
	return 0;
}

static const SceKernelProcEventHandler s_procHandler = {
	.size = sizeof(SceKernelProcEventHandler),
	.exit = UserpProcExit,
	.kill = UserpProcKill,
	.suspend = UserpProcStop,
	.resume = UserpProcStart,
};

int module_start(SceSize args, void *argp)
{
	SceClass *parentClass;
	char *className;
	SceUID uid;

	if (!SceDipswForDriver_A98FC2FD(0xc2))
		return 1;
	uid = sceKernelRegisterProcEventHandler("SceKernelDeci4p", &s_procHandler, 0);
	if (uid < 0)
		return 1;
	s_procEvent = uid;
	uid = sceKernelCreateMutex("SceDeci4pUserGlobal", SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
	if (uid < 0)
		goto unregister;
	s_mutex = uid;
	memset(s_channel, -1, sizeof(s_channel));
	s_class = sceKernelAllocHeapMemory(SCE_KERNEL_HEAP_ID, 0x2c);
	if (s_class == NULL)
		goto delete;
	className = sceKernelAllocHeapMemory(SCE_KERNEL_HEAP_ID, 0x20);
	s_className = className;
	if (className != NULL) {
		strncpy(className, "SceUIDDeci4pUserpClass", 0x20);
		parentClass = SceSysmemForDriver_85336A1C();
		if (sceUIDClassInitClass(s_class, className, parentClass,
					  sizeof(UserpObject), NULL, NULL) >= 0)
			return 0;
	}
	sceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, s_class);
delete:
	sceKernelDeleteMutex(s_mutex);
unregister:
	SceProcEventForDriver_3DED57CC(s_procEvent);
	return 1;
}

SceUID sceKernelDeci4pOpen(const char *name, SceUInt32 protocol, SceSize ringSize)
{
	char kname[0x20];
	UserpObject *obj;
	UserpState *st;
	SceUID mb, guid = 0, uid;
	SceUInt32 tid;
	int slot, ret;

	if ((ringSize & 3) || ringSize - USERP_RING_MIN > USERP_RING_MAX - USERP_RING_MIN)
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SIZE;
	if ((protocol & 0x7f000000) || ((protocol >> 16) & 0xff) > 0xef)
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PROTOCOL;

	SYSCALL_ENTER(tid);
	ret = sceKernelLockMutex(s_mutex, 1, NULL);
	if (ret < 0) {
		SYSCALL_LEAVE(tid);
		return ret;
	}
	for (slot = 0; slot < USERP_CHANNELS; slot++)
		if (s_channel[slot] == -1)
			break;
	if (slot == USERP_CHANNELS) {
		ret = SCE_KERNEL_ERROR_DECI4P_TOOMANY_PROTOCOL;
		goto unlock;
	}

	if (sceKernelCopyFromUser(kname, name, sizeof(kname) - 1) < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
		goto unlock;
	}
	kname[sizeof(kname) - 1] = '\0';
	mb = sceKernelAllocMemBlockForKernel(kname, SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW, USERP_STATE_SIZE, NULL);
	if (mb < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_NO_MEMORY;
		goto unlock;
	}
	if (sceKernelGetMemBlockBase(mb, (void **)&st) < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SID;
		goto unlock;
	}
	guid = sceGUIDKernelCreate(s_class, "SceKernelDeci4pUserpObject", (SceObjectBase **)&obj);
	if (guid < 0) {
		SceSysmemForDriver_009E1C61(mb);
		ret = SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SID;
		goto unlock;
	}
	obj->memblock = mb;
	obj->state = st;
	st->protocol = protocol;
	st->callback = -1;
	s_channel[slot] = guid;

	ret = uid = sceKernelCreateMutex("SceDeci4pUserRead", SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
	if (uid < 0)
		goto fail;
	st->readMutex = uid;
	ret = uid = sceKernelCreateMutex("SceDeci4pUserWrite", SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
	if (uid < 0)
		goto fail;
	st->writeMutex = uid;
	ret = uid = sceKernelCreateEventFlag("SceDeci4pUserp",
					      EVF_ATTR_UNK8000 | SCE_KERNEL_EVF_ATTR_MULTI,
					       0, NULL);
	if (uid < 0)
		goto fail;
	st->evf = uid;

	if (ringSize < 0x10810)
		ringSize += 0x10;
	uid = sceKernelAllocMemBlockForKernel("SceKernelDeci4pUserpRingBuf", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW,
				      (ringSize + 0xfff) & ~0xfff, NULL);
	if (uid < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_NO_MEMORY;
		goto fail;
	}
	st->ringBlock = uid;
	if (sceKernelGetMemBlockBase(uid, (void **)&st->ring) < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SID;
		goto fail;
	}
	st->ringSize = ringSize;
	st->proto.subVersion = 0x20001;
	st->proto.size = sizeof(st->proto);
	st->proto.version = 0x1000000;
	st->proto.flags = DFMGR_PROTOCOL_USB | DFMGR_PROTOCOL_ENABLE;
	st->proto.protocol = protocol;
	st->proto.destination = 0;
	strncpy(st->proto.name, "USERP", sizeof(st->proto.name));
	ret = sceDfMgrOpen(UserpHandler, &st->proto, st);
	if (ret < 0) {
		if (ret == (int)DFMGR_ERROR_ALREADY_OPENED)
			ret = SCE_KERNEL_ERROR_DECI4P_ALREADYUSE_PROTOCOL;
		else if (ret == (int)DFMGR_ERROR_NO_SLOT)
			ret = SCE_KERNEL_ERROR_DECI4P_TOOMANY_PROTOCOL;
		else
			ret = SCE_KERNEL_ERROR_DECI4P_UNKNOWN;
		goto fail;
	}
	st->handle = ret;
	st->pid = sceKernelGetProcessId();
	uid = SceSysmemForDriver_BF209859(0, guid);
	st->status = 0;
	st->puid = uid;
	st->writePos = 0;
	st->readPos = 0;
	st->used = 0;
	memset(&st->hdr, 0, sizeof(st->hdr));
	memset(st->ring, 0, st->ringSize);
	st->recvCount = 0;
	st->pending = 0;
	sceKernelUnlockMutex(s_mutex, 1);
	SYSCALL_LEAVE(tid);
	return st->puid;

fail:
	if (guid != 0) {
		UserpFreeObject(obj);
		sceGUIDClose(guid);
		s_channel[slot] = -1;
	}
unlock:
	sceKernelUnlockMutex(s_mutex, 1);
	SYSCALL_LEAVE(tid);
	return ret;
}

int sceKernelDeci4pClose(SceUID puid)
{
	UserpObject *obj;
	UserpState *st;
	SceUID guid;
	SceUInt32 tid;
	int i, ret;

	SYSCALL_ENTER(tid);
	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid < 0)
		goto bad;
	SceSysmemForDriver_84A4AF5E(0, puid);
	ret = sceKernelLockMutex(s_mutex, 1, NULL);
	if (ret < 0) {
		SYSCALL_LEAVE(tid);
		return ret;
	}
	for (i = 0; i < USERP_CHANNELS; i++)
		if (s_channel[i] == guid)
			s_channel[i] = -1;
	sceKernelUnlockMutex(s_mutex, 1);

	if (sceGUIDReferObjectWithClass(guid, s_class, (SceObjectBase **)&obj) < 0)
		goto bad;
	st = obj->state;
	ret = sceKernelLockMutex(st->writeMutex, 1, NULL);
	if (ret < 0) {
		sceGUIDReleaseObject(guid);
		SYSCALL_LEAVE(tid);
		return ret;
	}
	sceKernelSetEventFlag(st->evf, EVF_CLOSE);
	UserpFreeObject(obj);
	sceGUIDReleaseObject(guid);
	sceGUIDClose(guid);
	SYSCALL_LEAVE(tid);
	return 0;

bad:
	SYSCALL_LEAVE(tid);
	return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SID;
}

int sceKernelDeci4pRegisterCallback(SceUID puid, SceUID cbPuid)
{
	UserpObject *obj;
	UserpState *st;
	SceUID cb, guid;
	SceUInt32 tid;
	int intr;

	SYSCALL_ENTER(tid);
	cb = SceSysmemForDriver_45D22597(0, cbPuid);
	if (cb < 0) {
		SYSCALL_LEAVE(tid);
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_CBID;
	}
	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid < 0 || sceGUIDReferObjectWithClass(guid, s_class, (SceObjectBase **)&obj) < 0) {
		SYSCALL_LEAVE(tid);
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SID;
	}
	st = obj->state;
	intr = SceCpuForDriver_D32ACE9E(&st->lock);
	st->callback = cb;
	if (cb != 0)
		sceKernelNotifyCallback(cb, st->status);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&st->lock, intr);
	sceGUIDReleaseObject(guid);
	SYSCALL_LEAVE(tid);
	return 0;
}

/* the 16-byte header of the next packet in the ring */
static inline __attribute__((always_inline)) void UserpPeekHeader(UserpState *st, SceUInt32 *hdr)
{
	SceSize pos = st->readPos;
	const SceUInt8 *ring = st->ring;
	SceSize n;

	if (st->ringSize < pos + 0x10) {
		n = st->ringSize - pos;
		memcpy(hdr, ring + pos, n);
		memcpy((SceUInt8 *)hdr + n, ring, 0x10 - n);
	} else {
		const SceUInt32 *packet = (const SceUInt32 *)(ring + pos);
		SceUInt32 first = packet[0];
		SceUInt32 second = packet[1];
		SceUInt32 third = packet[2];
		SceUInt32 fourth = packet[3];

		hdr[0] = first;
		hdr[1] = second;
		hdr[2] = third;
		hdr[3] = fourth;
	}
}

int sceKernelDeci4pRead(SceUID puid, void *buf, SceSize size, int flags)
{
	UserpObject *obj;
	UserpState *st;
	SceUInt32 hdr[4], bits;
	SceSize skip, len, n, used;
	SceUID guid;
	SceUInt32 tid;
	int intr, ret, count;

	if (flags != 0)
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_RESERVED;
	SYSCALL_ENTER(tid);
	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid < 0 || sceGUIDReferObjectWithClass(guid, s_class, (SceObjectBase **)&obj) < 0) {
		SYSCALL_LEAVE(tid);
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SID;
	}
	st = obj->state;
	ret = sceKernelLockMutex(st->readMutex, 1, NULL);
	if (ret < 0) {
		sceGUIDReleaseObject(guid);
		SYSCALL_LEAVE(tid);
		return ret;
	}
	if (st->status & STATUS_DOWN)
		goto down;
	if (st->status & STATUS_CLOSED)
		goto closed;

	intr = SceCpuForDriver_D32ACE9E(&st->lock);
	if (st->used < 0x10) {
		/* wait for a packet */
		sceKernelClearEventFlag(st->evf, ~EVF_DATA);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&st->lock, intr);
		ret = sceKernelWaitEventFlag(st->evf, EVF_DATA | EVF_CLOSE | EVF_DOWN | EVF_CLOSED,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
		if (ret < 0) {
			sceGUIDReleaseObject(guid);
			sceKernelUnlockMutex(st->readMutex, 1);
			SYSCALL_LEAVE(tid);
			return ret;
		}
		if (bits & EVF_CLOSE) {
			/* closed while waiting (the mutex is gone) */
			sceGUIDReleaseObject(guid);
			SYSCALL_LEAVE(tid);
			return 0;
		}
		if (bits & EVF_DOWN)
			goto down;
		if (bits & EVF_CLOSED)
			goto closed;
		intr = SceCpuForDriver_D32ACE9E(&st->lock);
		skip = 0;
		if (st->used > 0xf) {
			UserpPeekHeader(st, hdr);
			skip = 0x10;
		}
	} else {
		UserpPeekHeader(st, hdr);
		skip = 0x10;
	}

	len = (hdr[0] & 0xffff) - 0x10 + ((hdr[0] >> 16) & 0xff) * 0x10000;
	if (size < len) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&st->lock, intr);
		sceGUIDReleaseObject(guid);
		sceKernelUnlockMutex(st->readMutex, 1);
		SYSCALL_LEAVE(tid);
		return SCE_KERNEL_ERROR_DECI4P_TOOSMALL_BUFFER;
	}

	st->readPos = __aeabi_uidivmod(st->readPos + skip, st->ringSize) >> 32;
	used = st->used - skip;
	st->used = used;
	if (used >= len) {
		if (st->ringSize < st->readPos + len) {
			n = st->ringSize - st->readPos;
			count = sceKernelCopyToUser(buf, st->ring + st->readPos, n);
			if (count == 0) {
				count = sceKernelCopyToUser((SceUInt8 *)buf + n, st->ring, len - n);
				if (count == 0)
					count = len;
			}
		} else {
			count = sceKernelCopyToUser(buf, st->ring + st->readPos, len);
			if (count >= 0)
				count = len;
		}
		if (count < 0) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&st->lock, intr);
			sceGUIDReleaseObject(guid);
			sceKernelUnlockMutex(st->readMutex, 1);
			SYSCALL_LEAVE(tid);
			return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
		}
		used = st->used;
	} else {
		count = 0;
	}

	st->readPos = __aeabi_uidivmod(st->readPos + count, st->ringSize) >> 32;
	st->used = used - count;
	st->pending -= count;
	if (st->used == 0)
		st->status &= STATUS_KEEP;
	else
		st->status = (st->status & 0xff000000) | STATUS_DATA | st->pending;
	UserpNotify(st);
	if (st->needSpace) {
		st->needSpace = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&st->lock, intr);
		sceDfMgrSendSpace(st->handle);
	} else {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&st->lock, intr);
	}
	sceGUIDReleaseObject(guid);
	sceKernelUnlockMutex(st->readMutex, 1);
	SYSCALL_LEAVE(tid);
	return count;

down:
	sceGUIDReleaseObject(guid);
	sceKernelUnlockMutex(st->readMutex, 1);
	SYSCALL_LEAVE(tid);
	return SCE_KERNEL_ERROR_DECI4P_NO_CONNECT;
closed:
	sceGUIDReleaseObject(guid);
	sceKernelUnlockMutex(st->readMutex, 1);
	SYSCALL_LEAVE(tid);
	return SCE_KERNEL_ERROR_DECI4P_NO_PROTO;
}

int sceKernelDeci4pWrite(SceUID puid, const void *buf, SceSize size, int flags)
{
	UserpObject *obj;
	UserpState *st;
	SceSize len = size + sizeof(SceDeci4pHeader);
	SceUInt32 bits;
	SceUID guid;
	SceUInt32 tid;
	int ret;

	if (size > USERP_WRITE_MAX)
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SIZE;
	if (flags != 0)
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_RESERVED;
	SYSCALL_ENTER(tid);
	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid < 0 || sceGUIDReferObjectWithClass(guid, s_class, (SceObjectBase **)&obj) < 0) {
		SYSCALL_LEAVE(tid);
		return SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SID;
	}
	st = obj->state;
	ret = sceKernelLockMutex(st->writeMutex, 1, NULL);
	if (ret < 0) {
		sceGUIDReleaseObject(guid);
		SYSCALL_LEAVE(tid);
		return ret;
	}
	if (st->status & STATUS_DOWN) {
		ret = SCE_KERNEL_ERROR_DECI4P_NO_CONNECT;
		goto out;
	}
	if (st->status & STATUS_CLOSED) {
		ret = SCE_KERNEL_ERROR_DECI4P_NO_PROTO;
		goto out;
	}

	st->pkt.size = len;
	st->pkt.protocol = st->protocol;
	st->pkt.dst = 0;
	st->pkt.reserved1 = 0;
	st->pkt.reserved2 = 0;
	st->pkt.protocolHi = st->protocol >> 31;
	st->pkt.sizeHi = len >> 16;
	st->pkt.version = SCE_DECI4P_VERSION;
	st->pkt.src = SCE_DECI4P_PORT_TARGET;
	if (sceKernelCopyFromUser(st->pkt.data, buf, size) < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
		goto out;
	}
	st->sent = 0;
	st->sendPtr = &st->pkt;
	st->sendLen = len;
	ret = sceDfMgrReqSend(st->handle);
	if (ret < 0) {
		sceGUIDReleaseObject(guid);
		sceKernelUnlockMutex(st->writeMutex, 1);
		SYSCALL_LEAVE(tid);
		return ret == (int)DFMGR_ERROR_NOT_CONNECTED ? SCE_KERNEL_ERROR_DECI4P_NO_CONNECT : SCE_KERNEL_ERROR_DECI4P_UNKNOWN;
	}
	ret = sceKernelWaitEventFlag(st->evf, EVF_SENT | EVF_SEND_FAIL,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (ret >= 0) {
		if (!(bits & EVF_SEND_FAIL))
			ret = len - sizeof(SceDeci4pHeader);
		else if (bits & EVF_DOWN)
			ret = USERP_ERROR_ABORT;
		else if (bits & EVF_CLOSED)
			ret = USERP_ERROR_ABORT2;
		else
			ret = ret - sizeof(SceDeci4pHeader);
	}
out:
	sceGUIDReleaseObject(guid);
	sceKernelUnlockMutex(st->writeMutex, 1);
	SYSCALL_LEAVE(tid);
	return ret;
}

int sceKernelDeci4pIsProcessAttached(void)
{
	SceUInt32 tid;
	int ret;

	SYSCALL_ENTER(tid);
	ret = SceSysrootForDriver_39F1FD07(sceKernelGetProcessId());
	SYSCALL_LEAVE(tid);
	return ret;
}

int sceKernelDeci4pEnableWatchpoint(void)
{
	SceUInt32 tid;
	int ret;

	SYSCALL_ENTER(tid);
	ret = SceSysrootForDriver_7B4A377C();
	SYSCALL_LEAVE(tid);
	return ret;
}

int sceKernelDeci4pDisableWatchpoint(void)
{
	SceUInt32 tid;
	int ret;

	SYSCALL_ENTER(tid);
	ret = SceSysrootForDriver_65F3D598();
	SYSCALL_LEAVE(tid);
	return ret;
}

int sceKernelDeci4pCreateHostProcess(const void *uparam, void *uout, SceUID cbPuid, void *ustatus)
{
	SceKernelDeci4pCreateHostProcessParam param;
	SceUInt32 out[8];
	UserpStrBuf *strbuf = NULL;
	SceUID cb, mb;
	SceUInt32 tid;
	int ret, c;

	SYSCALL_ENTER(tid);
	cb = SceSysmemForDriver_45D22597(0, cbPuid);
	if (cb < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_ILLEGAL_CBID;
	} else if (ustatus == NULL || sceKernelIsAccessibleRange(0x20, ustatus, 0x20) < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
	} else {
		mb = sceKernelAllocMemBlockForKernel("SceKernelDeci4pUserpStrBuf", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW,
					     USERP_STRBUF_SIZE, NULL);
		if (mb < 0) {
			ret = SCE_KERNEL_ERROR_DECI4P_NO_MEMORY;
		} else {
			if (sceKernelGetMemBlockBase(mb, (void **)&strbuf) < 0) {
				ret = SCE_KERNEL_ERROR_DECI4P_UNKNOWN;
			} else {
				ret = UserpCheckHostParam(strbuf, uparam, uout, &param, out);
				if (ret >= 0) {
					ret = SceSysrootForDriver_06436EF0(param.flags, param.pathName, param.cmdLine,
									   param.workDir, &out[0], &out[1],
									   cb, ustatus, 1);
					c = sceKernelCopyToUser(uout, out, sizeof(out));
					if (c < 0)
						ret = SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
				}
			}
			if (mb != 0)
				SceSysmemForDriver_009E1C61(mb);
		}
	}
	SYSCALL_LEAVE(tid);
	return ret;
}

int sceKernelDeci4pCreateHostProcessAndWait(const void *uparam, void *uout, int *ustatus)
{
	SceKernelDeci4pCreateHostProcessParam param;
	SceUInt32 out[8];
	int status = -1;
	UserpStrBuf *strbuf = NULL;
	SceUID mb;
	SceUInt32 tid;
	int ret;

	SYSCALL_ENTER(tid);
	if (ustatus == NULL || sceKernelIsAccessibleRange(0x20, ustatus, 4) < 0) {
		ret = SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
	} else {
		mb = sceKernelAllocMemBlockForKernel("SceKernelDeci4pUserpStrBuf", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW,
					     USERP_STRBUF_SIZE, NULL);
		if (mb < 0) {
			ret = SCE_KERNEL_ERROR_DECI4P_NO_MEMORY;
		} else {
			if (sceKernelGetMemBlockBase(mb, (void **)&strbuf) < 0) {
				ret = SCE_KERNEL_ERROR_DECI4P_UNKNOWN;
			} else {
				ret = UserpCheckHostParam(strbuf, uparam, uout, &param, out);
				if (ret >= 0) {
					ret = SceSysrootForDriver_06436EF0(param.flags, param.pathName, param.cmdLine,
									   param.workDir, &out[0], &out[1],
									   -1, &status, 0);
					if (sceKernelCopyToUser(uout, out, sizeof(out)) < 0 ||
					    sceKernelCopyToUser(ustatus, &status, sizeof(status)) < 0)
						ret = SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR;
				}
			}
			if (mb != 0)
				SceSysmemForDriver_009E1C61(mb);
		}
	}
	SYSCALL_LEAVE(tid);
	return ret;
}
