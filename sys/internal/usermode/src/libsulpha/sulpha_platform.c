/*
 * SceSulpha platform layer: Deci4p transport, buffered file capture,
 * logging, spinlock, timer, and clib wrappers.
 */

#include "sulpha_internal.h"

/* Imported kernel / clib / deci4p / io functions */
extern SceUID sceKernelDeci4pOpen(const char *protoname, SceUInt32 protonum, SceSize bufsize);
extern SceInt32 sceKernelDeci4pClose(SceUID socketid);
extern SceInt32 sceKernelDeci4pRegisterCallback(SceUID socketid, SceUID cbid);
extern SceInt32 sceKernelDeci4pRead(SceUID socketid, void *buffer, SceSize size, SceUInt32 reserved);
extern SceInt32 sceKernelDeci4pWrite(SceUID socketid, const void *buffer, SceSize size, SceUInt32 reserved);

extern void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len);
extern void *sceClibMemset(void *dst, int ch, SceSize len);
extern char *sceClibStrncpy(char *dst, const char *src, SceSize len);
extern SceSize sceClibStrnlen(const char *s, SceSize maxLen);
extern int sceClibPrintf(const char *fmt, ...);
extern int sceClibVsnprintf(char *dst, SceSize maxLen, const char *fmt, va_list args);

extern SceUID sceIoOpen(const char *file, int flags, int mode);
extern SceSSize sceIoWrite(SceUID fd, const void *buf, SceSize nbyte);
extern int sceIoClose(SceUID fd);

typedef SceInt32 (*SulphaThreadEntry)(SceSize args, void *argp);
typedef SceInt32 (*SulphaCallbackFunc)(SceInt32 notifyId, SceInt32 notifyCount,
				       SceInt32 notifyArg, void *userData);

extern SceUID sceKernelCreateThread(const char *name, SulphaThreadEntry entry,
				    SceInt32 initPriority, SceSize stackSize,
				    SceUInt32 attr, SceInt32 cpuAffinityMask,
				    const void *option);
extern SceInt32 sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
extern SceInt32 sceKernelWaitThreadEnd(SceUID thid, SceInt32 *stat, SceUInt32 *timeout);
extern SceInt32 sceKernelDeleteThread(SceUID thid);
extern SceInt32 SceThreadmgrCoredumpTime_sceKernelExitThread(SceInt32 status);

extern SceUID sceKernelCreateEventFlag(const char *name, SceUInt32 attr, SceUInt32 bits, const void *opt);
extern SceInt32 sceKernelDeleteEventFlag(SceUID evid);
extern SceInt32 sceKernelSetEventFlag(SceUID evid, SceUInt32 bits);
extern SceInt32 sceKernelClearEventFlag(SceUID evid, SceUInt32 bits);
extern SceInt32 sceKernelWaitEventFlag(SceUID evid, SceUInt32 bits, SceUInt32 waitMode,
				       SceUInt32 *outBits, SceUInt32 *timeout);
extern SceInt32 sceKernelWaitEventFlagCB(SceUID evid, SceUInt32 bits, SceUInt32 waitMode,
					 SceUInt32 *outBits, SceUInt32 *timeout);

extern SceUID sceKernelCreateCallback(const char *name, SceUInt32 attr,
				      SulphaCallbackFunc func, void *userData);
extern SceInt32 sceKernelDeleteCallback(SceUID cbid);
extern SceUInt64 sceKernelGetProcessTimeWide(void);

SulphaDeci4pState g_sulphaDeci4p = {
	.threadId = -1,
	.callbackId = -1,
	.socketId = -1,
	.recvEvFlag = -1,
	.doneEvFlag = -1,
};

SceInt32 g_sulphaDeci4pInitState = -1;
SceInt32 g_sulphaLogLevel = 0;

static const char *const g_sulphaLogLevelNames[] = {
	"Trace",
	"Debug",
	"Info",
	"Warn",
	"Error",
	NULL,
};

SceSize sulphaStrlen(const char *s)
{
	if (s == NULL)
		return 0;
	return sulphaClibStrnlen(s);
}

char *sulphaStrncpy(char *dst, const char *src, SceSize len)
{
	return sulphaClibStrncpy(dst, src, len);
}

void *sulphaMemset(void *dst, int ch, SceSize len)
{
	return sulphaClibMemset(dst, ch, len);
}

void *sulphaMemcpy(void *dst, const void *src, SceSize len)
{
	return sulphaClibMemcpy(dst, src, len);
}

void sulphaListenerInit(SulphaListener *listener, const SceSulphaConfig *config, SulphaServer *server)
{
	listener->server = server;
	listener->port = (SceUInt16)config->port;
	sulphaSocketInit(&listener->socket);
}

void sulphaListenerClose(SulphaListener *listener)
{
	sulphaSocketShutdown(&listener->socket);
}

SceInt32 sulphaListenerStart(SulphaListener *listener)
{
	SceInt32 res = sulphaSocketOpen(&listener->socket);
	if (res >= 0)
		return sulphaSocketBind(&listener->socket, listener->port);
	return res;
}

SceBool sulphaListenerHasPending(SulphaListener *listener)
{
	return sulphaSocketIsReadable(&listener->socket);
}

SceInt32 sulphaListenerAccept(SulphaListener *listener, SulphaSocket *outSocket)
{
	sulphaSocketAccept(&listener->socket, outSocket);
	if (sulphaSocketIsOpen(outSocket))
		return 0;
	sulphaSocketClose(outSocket);
	return SCE_SULPHA_ERROR_CONNECTION;
}

SceUInt32 sceSulphaLogSetLevel(SceUInt32 level)
{
	if (level <= 4)
		g_sulphaLogLevel = (SceInt32)level;
	return level;
}

void sceSulphaLogHandler(const char *file, SceInt32 line, SceInt32 level,
			 const char *module, const char *fmt, va_list args)
{
	char lineBuf[512];

	if (g_sulphaLogLevel > level)
		return;

	sulphaFormatString(lineBuf, sizeof(lineBuf), "%-5s: %s: %s(%d): ",
			   g_sulphaLogLevelNames[level], module, file, line);
	sceClibPrintf("%s", lineBuf);
	sulphaVformatString(lineBuf, sizeof(lineBuf), fmt, args);
	sceClibPrintf("%s", lineBuf);
	sceClibPrintf("%s", "\n");
}

static SceInt32 sulphaDeci4pCallback(SceInt32 notifyId, SceInt32 notifyCount,
				     SceInt32 notifyArg, void *userData)
{
	SulphaDeci4pState *state = (SulphaDeci4pState *)userData;
	(void)notifyId;
	(void)notifyCount;

	if ((notifyArg & 0x60000000) != 0)
		state->flags |= 4U;
	else
		state->flags &= (SceUInt8)~4U;

	if ((notifyArg & 0x10000000) != 0) {
		SceUInt32 avail = (SceUInt32)(notifyArg & 0x00FFFFFF);
		for (;;) {
			SceInt32 cur = state->availBytes;
			while ((SceInt32)sulphaAtomicLoad32(&state->availBytes) == cur) {
				if (sulphaAtomicStore32(avail, &state->availBytes) == 0)
					return 0;
			}
		}
	}
	return 0;
}

static SceInt32 sulphaDeci4pThread(SceSize argSize, void *argp)
{
	SulphaDeci4pState *state = *(SulphaDeci4pState **)argp;
	SceUInt32 outBits = 0;
	SceInt32 res;
	SceBool initialized = false;
	(void)argSize;

	res = sceKernelCreateCallback("sulpha_deci_cb", 0, sulphaDeci4pCallback, state);
	if (res >= 0) {
		state->callbackId = res;
		res = sceKernelCreateEventFlag("SceSulphaEventRecv", 0, 0, NULL);
		if (res >= 0) {
			state->recvEvFlag = res;
			res = sceKernelCreateEventFlag("SceSulphaEventDone", 0, 0, NULL);
			if (res >= 0) {
				state->doneEvFlag = res;
				state->flags |= 1U;
				initialized = true;
			}
		}
	}

	if (!initialized) {
		state->flags |= 2U;
	} else {
		for (;;) {
			res = sceKernelWaitEventFlagCB(state->recvEvFlag, 3, 1, &outBits, NULL);
			if (res < 0 || (outBits & 1U) != 0)
				break;

			SceInt32 avail = state->availBytes;
			void *buf = state->readBuf;
			SceInt32 toRead = state->readSize;
			if (avail < toRead)
				toRead = avail;

			if (buf == NULL) {
				state->readError = SCE_SULPHA_ERROR_INVALID_VALUE;
				sceKernelSetEventFlag(state->doneEvFlag, 2);
				continue;
			}

			if (toRead > 0) {
				res = sceKernelDeci4pRead(state->socketId, buf, (SceSize)toRead, outBits & 1U);
				if (res < 0) {
					state->readError = res;
					sceKernelSetEventFlag(state->doneEvFlag, 2);
					continue;
				}
				avail = state->availBytes;
			}

			state->readResult = res;
			state->readError = 0;
			SceInt32 rem = avail - res;
			state->availBytes = (rem > 0) ? rem : 0;

			res = sceKernelClearEventFlag(state->recvEvFlag, ~2U);
			if (res < 0)
				break;
			res = sceKernelSetEventFlag(state->doneEvFlag, 1);
			if (res < 0)
				break;
		}
	}

	if (state->doneEvFlag != -1) {
		if (res < 0)
			sceKernelSetEventFlag(state->doneEvFlag, 2);
		sceKernelDeleteEventFlag(state->doneEvFlag);
		state->doneEvFlag = -1;
	}
	if (state->recvEvFlag != -1) {
		sceKernelDeleteEventFlag(state->recvEvFlag);
		state->recvEvFlag = -1;
	}
	if (state->callbackId != -1) {
		sceKernelDeleteCallback(state->callbackId);
		state->callbackId = -1;
	}

	SceThreadmgrCoredumpTime_sceKernelExitThread(res);
	return 0;
}

SceInt32 sulphaDeci4pInit(void)
{
	if (g_sulphaDeci4pInitState != -1)
		return SCE_SULPHA_ERROR_ALREADY_INITIALIZED;

	SceUID thid = sceKernelCreateThread("SceSulphaDeci4p", sulphaDeci4pThread,
					    0x10000100, 0x1000, 0, 0, NULL);
	SceInt32 res = thid;
	if (thid >= 0) {
		SulphaDeci4pState *arg = &g_sulphaDeci4p;
		g_sulphaDeci4p.threadId = thid;
		g_sulphaDeci4p.flags &= (SceUInt8)~7U;

		res = sceKernelStartThread(thid, sizeof(arg), &arg);
		if (res >= 0) {
			while ((g_sulphaDeci4p.flags & 3U) == 0)
				;
			if ((g_sulphaDeci4p.flags & 2U) == 0) {
				g_sulphaDeci4pInitState = 0;
				return 0;
			}
			res = SCE_SULPHA_ERROR_CONNECTION;
		}
	}

	if (g_sulphaDeci4p.threadId != -1)
		sceKernelDeleteThread(g_sulphaDeci4p.threadId);
	g_sulphaDeci4p.threadId = -1;
	g_sulphaDeci4p.flags &= (SceUInt8)~7U;
	return res;
}

SceInt32 sulphaDeci4pShutdown(void)
{
	if (g_sulphaDeci4pInitState != 0)
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = 0;
	if (g_sulphaDeci4p.threadId != -1) {
		if (g_sulphaDeci4p.recvEvFlag != -1) {
			sceKernelSetEventFlag(g_sulphaDeci4p.recvEvFlag, 1);
			res = sceKernelWaitThreadEnd(g_sulphaDeci4p.threadId, NULL, NULL);
		}
		sceKernelDeleteThread(g_sulphaDeci4p.threadId);
		g_sulphaDeci4p.threadId = -1;
	}

	g_sulphaDeci4pInitState = -1;
	return res;
}

void sulphaSocketInit(SulphaSocket *sock)
{
	sock->fd = -1;
	sock->owned = 0;
	sock->lastError = 0;
}

void sulphaSocketShutdown(SulphaSocket *sock)
{
	if (sock->fd != -1) {
		if (sock->owned != 0)
			sceKernelDeci4pClose(sock->fd);
		sock->fd = -1;
	}
}

SceInt32 sulphaSocketOpen(SulphaSocket *sock)
{
	if (g_sulphaDeci4p.callbackId == -1)
		return SCE_SULPHA_ERROR_NETWORK_OWNERSHIP;

	SceUID fd = sceKernelDeci4pOpen("sulpha_protocol", 0x2A000U, 0x10800U);
	if (fd < 0)
		return fd;

	SceInt32 res = sceKernelDeci4pRegisterCallback(fd, g_sulphaDeci4p.callbackId);
	if (res < 0) {
		sceKernelDeci4pClose(fd);
		return res;
	}

	sock->fd = fd;
	sock->owned = 1;
	return res;
}

SceInt32 sulphaSocketClose(SulphaSocket *sock)
{
	if (sock->fd == -1)
		return 0;

	SceInt32 res = (sock->owned != 0) ? sceKernelDeci4pClose(sock->fd) : 0;
	sock->fd = -1;
	return res;
}

SceInt16 sulphaSocketAccept(const SulphaSocket *listenerSock, SulphaSocket *clientSock)
{
	if (listenerSock->fd == -1)
		return 9;
	clientSock->fd = listenerSock->fd;
	return 0;
}

SceBool sulphaSocketIsReadable(const SulphaSocket *sock)
{
	return sock->fd != -1 && g_sulphaDeci4p.callbackId != -1 && g_sulphaDeci4p.availBytes > 0;
}

SceBool sulphaSocketIsOpen(const SulphaSocket *sock)
{
	return sock->fd != -1;
}

void sulphaSocketSetNonBlocking(SulphaSocket *sock, SceInt32 nonBlock)
{
	(void)sock;
	(void)nonBlock;
}

SceInt32 sulphaSocketSend(SulphaSocket *sock, const void *buf, SceSize size)
{
	if (sock->fd == -1 || size == 0)
		return 0;

	SceInt32 res = sceKernelDeci4pWrite(sock->fd, buf, size, 0);
	return (res < 0) ? -1 : res;
}

SceInt32 sulphaSocketRecv(SulphaSocket *sock, void *buf, SceInt32 size)
{
	SceUInt32 outBits = 0;
	SceUInt32 timeout = 5000000U;

	if (sock->fd == -1 || size == 0 || buf == NULL)
		return 0;

	if (size > g_sulphaDeci4p.availBytes) {
		sock->lastError = 1;
		return -1;
	}

	sock->lastError = 0;
	g_sulphaDeci4p.socketId = sock->fd;
	g_sulphaDeci4p.readBuf = buf;
	g_sulphaDeci4p.readSize = size;

	SceInt32 res = sceKernelSetEventFlag(g_sulphaDeci4p.recvEvFlag, 2);
	if (res < 0) {
		if (sock->fd != -1) {
			if (sock->owned != 0)
				sceKernelDeci4pClose(sock->fd);
			sock->fd = -1;
		}
		return res;
	}

	res = sceKernelWaitEventFlag(g_sulphaDeci4p.doneEvFlag, 3, 1, &outBits, &timeout);
	if (res < 0) {
		if (sock->fd != -1) {
			if (sock->owned != 0)
				sceKernelDeci4pClose(sock->fd);
			sock->fd = -1;
		}
	} else if ((outBits & 2U) != 0) {
		sock->lastError = g_sulphaDeci4p.readError;
		res = 0;
	} else {
		res = g_sulphaDeci4p.readResult;
	}

	sceKernelClearEventFlag(g_sulphaDeci4p.doneEvFlag, ~3U);
	return res;
}

SceInt32 sulphaSocketGetError(const SulphaSocket *sock)
{
	if (sock->lastError == 0)
		return 0;
	return (sock->lastError == 1) ? 1 : 2;
}

SceInt32 sulphaSocketBind(SulphaSocket *sock, SceUInt16 port)
{
	(void)sock;
	(void)port;
	return 0;
}

static inline __attribute__((always_inline)) void sulphaIoWriteAll(SceUID fd, const void *buf, SceUInt32 size)
{
	const SceUInt8 *p = (const SceUInt8 *)buf;

	while (size != 0) {
		SceSize chunk = (size >= SULPHA_FILE_WRITE_CHUNK) ? SULPHA_FILE_WRITE_CHUNK : size;
		SceSSize written = sceIoWrite(fd, p, chunk);
		if (written <= 0)
			break;
		p += written;
		size -= (SceUInt32)written;
	}
}

void sulphaFileCaptureInit(SulphaFileCapture *fc, SulphaAllocator *alloc)
{
	fc->fd = -1;
	fc->enabled = 0;
	fc->buffered = 0;
	fc->capacity = SULPHA_FILE_BUF_SIZE;
	fc->filename[0] = '\0';
	fc->buffer = (SceUInt8 *)alloc->alloc(alloc, SULPHA_FILE_BUF_SIZE, 16);
}

void sulphaFileCaptureShutdown(SulphaFileCapture *fc)
{
	fc->enabled = 0;
	if (fc->fd != -1) {
		if (fc->buffered != 0) {
			sulphaIoWriteAll(fc->fd, fc->buffer, fc->buffered);
			fc->buffered = 0;
		}
		sceIoClose(fc->fd);
		fc->fd = -1;
	}
}

SceInt32 sulphaFileCaptureOpen(SulphaFileCapture *fc, const char *filename)
{
	sulphaStrncpy(fc->filename, filename, sizeof(fc->filename) - 1);
	fc->enabled = 1;
	fc->filename[sizeof(fc->filename) - 1] = '\0';
	return 0;
}

SceInt32 sulphaFileCaptureClose(SulphaFileCapture *fc)
{
	fc->enabled = 0;
	if (fc->fd != -1) {
		if (fc->buffered != 0) {
			sulphaIoWriteAll(fc->fd, fc->buffer, fc->buffered);
			fc->buffered = 0;
		}
		sceIoClose(fc->fd);
		fc->fd = -1;
	}
	return 0;
}

SceInt32 sulphaFileCaptureWrite(SulphaFileCapture *fc, const void *buf, SceUInt32 size)
{
	if (fc->enabled != 0 && fc->fd == -1) {
		SceUInt32 magic = SULPHA_FILE_MAGIC;
		SceUInt32 version = SULPHA_FILE_VERSION;
		SceUID fd = sceIoOpen(fc->filename, 0x0602, 0600);
		if (fd < 0) {
			fc->fd = -1;
			return 0;
		}
		fc->fd = fd;
		sulphaIoWriteAll(fd, &magic, sizeof(magic));
		sulphaIoWriteAll(fc->fd, &version, sizeof(version));
	}

	if (fc->fd == -1)
		return 0;

	if (size <= fc->capacity) {
		SceUInt32 space = fc->capacity - fc->buffered;
		SceUInt32 copyLen = (size < space) ? size : space;
		if (copyLen != 0) {
			sulphaMemcpy(fc->buffer + fc->buffered, buf, copyLen);
			fc->buffered += copyLen;
		}
		if (fc->buffered == fc->capacity && fc->capacity != 0) {
			sulphaIoWriteAll(fc->fd, fc->buffer, fc->capacity);
			fc->buffered = 0;
		}
		SceUInt32 rem = size - copyLen;
		if (rem != 0) {
			sulphaMemcpy(fc->buffer, (const SceUInt8 *)buf + copyLen, rem);
			fc->buffered = rem;
		}
	} else {
		if (fc->buffered != 0) {
			sulphaIoWriteAll(fc->fd, fc->buffer, fc->buffered);
			fc->buffered = 0;
		}
		sulphaIoWriteAll(fc->fd, buf, size);
	}

	return 0;
}

void *sulphaClibMemcpy(void *dst, const void *src, SceSize len)
{
	return sceClibMemcpy_safe(dst, src, len);
}

void *sulphaClibMemset(void *dst, int ch, SceSize len)
{
	return sceClibMemset(dst, ch, len);
}

char *sulphaClibStrncpy(char *dst, const char *src, SceSize len)
{
	return sceClibStrncpy(dst, src, len);
}

SceSize sulphaClibStrnlen(const char *s)
{
	return sceClibStrnlen(s, 0x1000U);
}

SceInt32 sulphaClibVsnprintf(char *dst, SceSize size, const char *fmt, va_list args)
{
	return sceClibVsnprintf(dst, size, fmt, args);
}

SceInt32 sulphaPlatformInit(void)
{
	return 0;
}

SceInt32 sulphaPlatformShutdown(void)
{
	return 0;
}

void sulphaSpinLockInit(SulphaSpinLock *lock, const char *name)
{
	(void)name;
	lock->lock = 0;
}

void sulphaSpinLockDestroy(SulphaSpinLock *lock)
{
	(void)lock;
}

void sulphaSpinLockAcquire(SulphaSpinLock *lock)
{
	do {
		while (sulphaAtomicLoad32(&lock->lock) != 0)
			;
	} while (sulphaAtomicStore32(1, &lock->lock) != 0);
}

void sulphaSpinLockRelease(SulphaSpinLock *lock)
{
	lock->lock = 0;
}

void sulphaTimerInit(SulphaTimer *timer)
{
	timer->startTime = 0;
	timer->prevTime = 0;
	timer->curTime = 0;
	timer->running = 0;
}

void sulphaTimerReset(SulphaTimer *timer)
{
	timer->running = 0;
}

SceInt32 sulphaTimerStart(SulphaTimer *timer)
{
	timer->startTime = sceKernelGetProcessTimeWide();
	timer->running = 1;
	return 0;
}

SceInt32 sulphaTimerStop(SulphaTimer *timer)
{
	timer->running = 0;
	return 0;
}

SceUInt64 sulphaTimerGetTimestamp(SulphaTimer *timer)
{
	timer->prevTime = timer->curTime;
	SceUInt64 now = sceKernelGetProcessTimeWide();
	timer->curTime = now;
	return 1000ULL * now;
}
