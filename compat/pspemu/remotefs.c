/*
 * remotefs.c: guest-to-host address translation, Kermit IPC request/reply
 * primitives, RTC local/UTC helpers, and the generic remote filesystem loop.
 */
#include "host_common.h"
#include <string.h>

static uint8_t *g_fsUncachedCdram;
static uint8_t *g_fsCachedCdram;
static uint8_t *g_fsActiveCdram;

#define PSPEMU_FS_CMD_CHSTAT  3165
#define PSPEMU_FS_CMD_READ    3166
#define PSPEMU_FS_CMD_DOPEN   3167
#define PSPEMU_FS_CMD_DEVCTL  3168
#define PSPEMU_FS_CMD_RMDIR   3169
#define PSPEMU_FS_CMD_LSEEK   3170
#define PSPEMU_FS_CMD_INIT    3171
#define PSPEMU_FS_CMD_DCLOSE  3172
#define PSPEMU_FS_CMD_DREAD   3173
#define PSPEMU_FS_CMD_MKDIR   3174
#define PSPEMU_FS_CMD_OPEN    3175
#define PSPEMU_FS_CMD_RENAME  3176
#define PSPEMU_FS_CMD_GETSTAT 3177
#define PSPEMU_FS_CMD_WRITE   3178
#define PSPEMU_FS_CMD_CLOSE   3179
#define PSPEMU_FS_CMD_REMOVE  3180
#define PSPEMU_FS_CMD_CHDIR   3181
#define PSPEMU_FS_CMD_EXIT    3182
#define PSPEMU_FS_CMD_IOCTL   3183

/* 0x81006324 */
int pspemuIsAlternateDisplayHead(void)
{
	return sceCompatGetPrimaryHead() != 0;
}

/* 0x81006334 */
void *remotefsSetCdramBase(void *activeCdram, void *cachedCdram, void *uncachedCdram)
{
	g_fsActiveCdram = (uint8_t *)activeCdram;
	g_fsCachedCdram = (uint8_t *)cachedCdram;
	g_fsUncachedCdram = (uint8_t *)uncachedCdram;
	return activeCdram;
}

/* 0x81006358 */
void *remotefsGetCdramBase(void)
{
	return g_fsActiveCdram;
}

/* 0x81006364 */
void *remotefsTranslateGuestAddr(uint32_t guestAddr, int cacheMode, SceSize size)
{
	uint8_t *base;
	uint8_t *hostPtr = NULL;
	uint32_t physAddr;
	uint32_t ramOffset;
	int useCached = 0;

	if (size != 0 && ((size | guestAddr) & 0x3FU) == 0 && g_fsCachedCdram != NULL) {
		base = g_fsCachedCdram;
		useCached = 1;
	} else {
		base = g_fsUncachedCdram;
	}

	physAddr = guestAddr & 0x1FFFFFFFU;
	ramOffset = physAddr - 0x08000000U;
	if (ramOffset < 0x04000000U) {
		if ((int32_t)guestAddr > 0 && (physAddr - 0x08400000U) >= 0x01C00000U)
			return NULL;
		hostPtr = base + ramOffset;
	} else if ((physAddr - 0x04000000U) < 0x00800000U) {
		hostPtr = base + (physAddr - 0x00200000U);
	} else if ((physAddr - 0x00010000U) < 0x00004000U) {
		hostPtr = base + (physAddr + 0x03CF0000U);
	} else {
		return NULL;
	}

	if (hostPtr != NULL && useCached != 0 && (cacheMode & 1) != 0 &&
	    pspemuIsCachedCdramAddress((uintptr_t)hostPtr) != 0) {
		sceCompatCache(1, hostPtr, size);
	}
	return hostPtr;
}

/* 0x81006404 */
void *remotefsTranslateUserGuestAddr(uint32_t guestAddr, int cacheMode, SceSize size)
{
	uint8_t *base;
	uint8_t *hostPtr;
	uint32_t physAddr;
	uint32_t ramOffset;
	int useCached = 0;

	if (size != 0 && ((size | guestAddr) & 0x3FU) == 0 && g_fsCachedCdram != NULL) {
		base = g_fsCachedCdram;
		useCached = 1;
	} else {
		base = g_fsUncachedCdram;
	}

	physAddr = guestAddr & 0x1FFFFFFFU;
	ramOffset = physAddr - 0x08000000U;
	if (ramOffset >= 0x04000000U)
		return NULL;
	if (((int32_t)guestAddr > 0 && (physAddr - 0x08400000U) >= 0x01C00000U) ||
	    ramOffset < 0x00300000U) {
		return NULL;
	}

	hostPtr = base + ramOffset;
	if (hostPtr != NULL && useCached != 0 && (cacheMode & 1) != 0 &&
	    pspemuIsCachedCdramAddress((uintptr_t)hostPtr) != 0) {
		sceCompatCache(1, hostPtr, size);
	}
	return hostPtr;
}

/* 0x81006490 */
int remotefsFlushGuestAddr(void *hostPtr, SceSize size)
{
	uintptr_t addr = (uintptr_t)hostPtr;
	uintptr_t base = (uintptr_t)g_fsActiveCdram;

	if (addr >= base &&
	    addr <= base + PSPEMU_RAM_CDRAM_SIZE &&
	    size <= PSPEMU_RAM_CDRAM_SIZE &&
	    base <= addr + size) {
		if (pspemuIsCachedCdramAddress(addr) != 0)
			return sceCompatCache(PSPEMU_COMPAT_CACHE_WRITEBACK, hostPtr, size);
	}
	return 0;
}

/* 0x810064d0 */
int remotefsWaitAndGetRequest(int mode, PspemuKermitRequest **outReq)
{
	int channel;
	int rawReq;
	uint32_t guestAddr;
	uint32_t physAddr;
	PspemuKermitRequest *req = NULL;

	if (mode == 5)
		channel = 1;
	else if (mode == 6)
		channel = 2;
	else
		channel = 0;

	rawReq = sceCompatWaitAndGetRequest(mode, channel);
	if (rawReq < 0)
		pspemuTriggerFatalError(rawReq);

	guestAddr = (uint32_t)(rawReq << 1);
	physAddr = guestAddr & 0x1FFFFFFFU;
	if ((physAddr - 0x08000000U) < 0x04000000U) {
		if ((int32_t)guestAddr <= 0 || (physAddr - 0x08400000U) < 0x01C00000U)
			req = (PspemuKermitRequest *)(void *)(g_fsActiveCdram + (physAddr - 0x08000000U));
	} else if ((physAddr - 0x04000000U) < 0x00800000U) {
		req = (PspemuKermitRequest *)(void *)(g_fsActiveCdram + (physAddr - 0x00200000U));
	} else if ((physAddr - 0x00010000U) < 0x00004000U) {
		req = (PspemuKermitRequest *)(void *)(g_fsActiveCdram + (physAddr + 0x03CF0000U));
	}

	*outReq = req;
	return req->cmd;
}

/* 0x81006560 */
int remotefsReturnValue(int mode, const PspemuKermitRequest *req, int64_t retVal)
{
	uint32_t channel;
	SceUInt32 ids[2];
	int res;

	if (mode == 5)
		channel = 1;
	else if (mode == 6)
		channel = 2;
	else
		channel = 0;

	res = sceCompatWaitIntr(channel);
	if (res < 0)
		pspemuTriggerFatalError(res);

	ids[0] = req->semaId;
	ids[1] = req->retAddr;
	res = sceCompatReturnValueEx(channel, ids, (SceUInt64)retVal);
	if (res < 0)
		pspemuTriggerFatalError(res);

	return res;
}

/* 0x810065bc */
int remotefsConvertLocalToUtc(SceDateTime *dateTime)
{
	SceRtcTick localTick;
	SceRtcTick utcTick;

	sceRtcGetTick(dateTime, &localTick);
	sceRtcConvertLocalTimeToUtc(&localTick, &utcTick);
	return sceRtcSetTick(dateTime, &utcTick);
}

/* 0x810065f4 */
int remotefsConvertUtcToLocal(SceDateTime *dateTime)
{
	SceRtcTick utcTick;
	SceRtcTick localTick;

	sceRtcGetTick(dateTime, &utcTick);
	sceRtcConvertUtcToLocalTime(&utcTick, &localTick);
	return sceRtcSetTick(dateTime, &localTick);
}

/* 0x810081a0: generic Kermit filesystem request dispatch loop */
void compatFsLoop(int channel, const PspemuFsOps *ops)
{
	int accessPolicy;
	PspemuKermitRequest *req;

	accessPolicy = (pspemuIsPs1Mode() == 0 && pspemuCompatHasFlag(0x800000U) != 0) ? 1 : 0;
	if (pspemuIsPs1Mode() == 1)
		accessPolicy = 2;

	for (;;) {
		int64_t retVal;
		int cmd = remotefsWaitAndGetRequest(channel, &req);

		switch (cmd) {
		case PSPEMU_FS_CMD_CHSTAT: {
			const char *path = (const char *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0x4000U);
			const SceIoStat *stat = (const SceIoStat *)remotefsTranslateGuestAddr((uint32_t)req->args[1], 1, 0x58U);
			retVal = ops->chstat(path, stat, (int)req->args[2]);
			break;
		}
		case PSPEMU_FS_CMD_READ: {
			SceUID fd = (SceUID)req->args[0];
			SceSize size = (SceSize)req->args[2];
			void *buf = remotefsTranslateGuestAddr((uint32_t)req->args[1], 2, size);
			SceSSize readRes = ops->read(fd, buf, size);
			retVal = readRes;
			if (readRes >= 0)
				remotefsFlushGuestAddr(buf, size);
			break;
		}
		case PSPEMU_FS_CMD_DOPEN: {
			const char *path = (const char *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0x4000U);
			retVal = ops->dopen(path);
			break;
		}
		case PSPEMU_FS_CMD_DEVCTL: {
			const char *dev = (const char *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0x4000U);
			unsigned int devCmd = (unsigned int)req->args[1];
			SceSize inLen = (SceSize)req->args[3];
			void *inData = remotefsTranslateGuestAddr((uint32_t)req->args[2], 1, inLen);
			SceSize outLen = (SceSize)req->args[5];
			void *outData = remotefsTranslateGuestAddr((uint32_t)req->args[4], 2, outLen);
			int devRes = ops->devctl(dev, devCmd, inData, inLen, outData, outLen);
			retVal = devRes;
			if (devRes >= 0)
				remotefsFlushGuestAddr(outData, outLen);
			break;
		}
		case PSPEMU_FS_CMD_RMDIR: {
			const char *path = (const char *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0x4000U);
			retVal = ops->rmdir(path);
			break;
		}
		case PSPEMU_FS_CMD_LSEEK:
			retVal = ops->lseek((SceUID)req->args[0], (SceOff)req->args[1], (int)req->args[2]);
			break;
		case PSPEMU_FS_CMD_INIT: {
			void *arg = remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0);
			retVal = ops->init(arg);
			break;
		}
		case PSPEMU_FS_CMD_DCLOSE:
			retVal = ops->dclose((SceUID)req->args[0]);
			break;
		case PSPEMU_FS_CMD_DREAD: {
			SceUID dfd = (SceUID)req->args[0];
			SceIoDirent *dir = (SceIoDirent *)remotefsTranslateGuestAddr((uint32_t)req->args[1], 3, 0x160U);
			int dreadRes = ops->dread(dfd, dir);
			retVal = dreadRes;
			if (dreadRes >= 0)
				remotefsFlushGuestAddr(dir, 0x160U);
			break;
		}
		case PSPEMU_FS_CMD_MKDIR: {
			const char *path = (const char *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0x4000U);
			retVal = ops->mkdir(path, (SceMode)req->args[1]);
			break;
		}
		case PSPEMU_FS_CMD_OPEN: {
			const char *path = (const char *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0x4000U);
			int flags = (int)req->args[1];
			SceMode mode = (SceMode)req->args[2];
			int allowed = 1;

			if (accessPolicy != 0 && channel == 3) {
				uint32_t callerAddr = (uint32_t)req->args[3];

				if (accessPolicy == 2) {
					if (strncmp(path, g_originalPath + 4, 30) != 0) {
						if (memcmp(path, "/PSP/SAVEDATA/", 14) != 0 &&
						    memcmp(path, "/PSP/LICENSE/", 13) != 0) {
							allowed = 0;
						}
					} else {
						callerAddr = 0;
					}
				}
				if (allowed && callerAddr != 0 &&
				    (flags & 0x40000000) == 0 &&
				    !(remotePeripheralIsKernelOpenAllowed() != 0 && (callerAddr >> 22) == 33U) &&
				    remoteMsfsCheckMusicPath(path, flags) != 0) {
					allowed = 0;
				}
			}

			if (allowed)
				retVal = ops->open(path, flags, mode);
			else
				retVal = (int64_t)(int32_t)0x80010002U;
			break;
		}
		case PSPEMU_FS_CMD_RENAME: {
			const char *oldPath = (const char *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0x4000U);
			const char *newPath = (const char *)remotefsTranslateGuestAddr((uint32_t)req->args[1], 1, 0x4000U);
			retVal = ops->rename(oldPath, newPath);
			break;
		}
		case PSPEMU_FS_CMD_GETSTAT: {
			const char *path = (const char *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0x4000U);
			SceIoStat *stat = (SceIoStat *)remotefsTranslateGuestAddr((uint32_t)req->args[1], 2, 0x58U);
			int statRes = ops->getstat(path, stat);
			retVal = statRes;
			if (statRes >= 0)
				remotefsFlushGuestAddr(stat, 0x58U);
			break;
		}
		case PSPEMU_FS_CMD_WRITE: {
			SceUID fd = (SceUID)req->args[0];
			SceSize size = (SceSize)req->args[2];
			const void *buf = remotefsTranslateGuestAddr((uint32_t)req->args[1], 1, size);
			retVal = ops->write(fd, buf, size);
			break;
		}
		case PSPEMU_FS_CMD_CLOSE:
			retVal = ops->close((SceUID)req->args[0]);
			break;
		case PSPEMU_FS_CMD_REMOVE: {
			const char *path = (const char *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0x4000U);
			retVal = ops->remove(path);
			break;
		}
		case PSPEMU_FS_CMD_CHDIR: {
			const char *path = (const char *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0x4000U);
			retVal = ops->chdir(path);
			break;
		}
		case PSPEMU_FS_CMD_EXIT: {
			void *arg = remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 0);
			retVal = ops->exit(arg);
			break;
		}
		case PSPEMU_FS_CMD_IOCTL: {
			SceUID fd = (SceUID)req->args[0];
			unsigned int ioctlCmd = (unsigned int)req->args[1];
			SceSize inLen = (SceSize)req->args[3];
			void *inData = remotefsTranslateGuestAddr((uint32_t)req->args[2], 1, inLen);
			SceSize outLen = (SceSize)req->args[5];
			void *outData = remotefsTranslateGuestAddr((uint32_t)req->args[4], 2, outLen);
			int ioctlRes = ops->ioctl(fd, ioctlCmd, inData, inLen, outData, outLen);
			retVal = ioctlRes;
			if (ioctlRes >= 0)
				remotefsFlushGuestAddr(outData, outLen);
			break;
		}
		default:
			retVal = -1;
			pspemuRecordTitleListFlags(16);
			break;
		}

		remotefsReturnValue(channel, req, retVal);
		if (ops->postOp != NULL)
			ops->postOp();
	}
}
