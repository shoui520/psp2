/*
 * rudp_osal.cpp: DNTP OS abstraction layer (atomics, sync, sockets, epoll, PRNG, threads).
 */
#include "rudp_internal.h"

extern "C" int fprintf(void *stream, const char *fmt, ...)
{
	(void)stream;
	(void)fmt;
	return 0;
}

/* Dummy stderr symbol referenced by libsnc/error.c */
extern "C" {
	int _Stderr = 2;
}

SceSize rudpStrlenHelper(const char *str)
{
	return sceClibStrnlen(str, 0xFFFFFFFFU);
}

void *rudpMemcpySafeHelper(void *dst, const void *src, SceSize size)
{
	return sceClibMemcpy_safe(dst, src, size);
}

static char g_componentName[32] = "DNTP";

static uint32_t g_osalSpinLock = 0;
static int g_osalRefCount = 0;
static uint64_t g_osalInitTime = 0;
static SceKernelLwMutexWork g_osalMutex;

static int g_prngIndex = 0;
static uint32_t g_prngTable[521];

uint32_t rudpAtomicCompareAndSwap(uint32_t *target, uint32_t expected, uint32_t desired)
{
	uint32_t prev;
	do {
		prev = __builtin_ldrex(target);
		if (prev != expected) {
			break;
		}
	} while (__builtin_strex(desired, target) != 0);
	return prev;
}

uint32_t rudpAtomicSwap(uint32_t *target, uint32_t desired)
{
	uint32_t cur = *target;
	uint32_t prev;
	do {
		prev = __builtin_ldrex(target);
		if (prev != cur) {
			break;
		}
	} while (__builtin_strex(desired, target) != 0);
	return prev;
}

uint32_t rudpAtomicAdd(uint32_t *target, int32_t delta)
{
	uint32_t prev;
	do {
		prev = __builtin_ldrex(target);
	} while (__builtin_strex(prev + (uint32_t)delta, target) != 0);
	return prev;
}

uint32_t rudpAtomicSub(uint32_t *target, int32_t delta)
{
	int32_t neg = -delta;
	uint32_t prev;
	do {
		prev = __builtin_ldrex(target);
	} while (__builtin_strex(prev + (uint32_t)neg, target) != 0);
	return prev;
}

uint32_t rudpAtomicAnd(uint32_t *target, uint32_t mask)
{
	uint32_t expected;
	uint32_t prev;
	do {
		expected = *target;
		uint32_t desired = expected & mask;
		do {
			prev = __builtin_ldrex(target);
			if (prev != expected) {
				break;
			}
		} while (__builtin_strex(desired, target) != 0);
	} while (prev != expected);
	return prev;
}

uint32_t rudpAtomicOr(uint32_t *target, uint32_t mask)
{
	uint32_t expected;
	uint32_t prev;
	do {
		expected = *target;
		uint32_t desired = expected | mask;
		do {
			prev = __builtin_ldrex(target);
			if (prev != expected) {
				break;
			}
		} while (__builtin_strex(desired, target) != 0);
	} while (prev != expected);
	return prev;
}

int rudpCondInit(SceKernelLwCondWork *cond, SceKernelLwMutexWork *mutex)
{
	char nameBuf[32];
	sceClibSnprintf(nameBuf, sizeof(nameBuf), "%s", g_componentName);
	if (sceKernelCreateLwCond(cond, nameBuf, 0x2000U, mutex, 0) >= 0) {
		return 0;
	}
	return -1;
}

int rudpCondTerm(SceKernelLwCondWork *cond)
{
	sceKernelDeleteLwCond(cond);
	return 0;
}

int rudpCondSignal(SceKernelLwCondWork *cond)
{
	if (sceKernelSignalLwCond(cond) >= 0) {
		return 0;
	}
	return -1;
}

int rudpCondBroadcast(SceKernelLwCondWork *cond)
{
	if (sceKernelSignalLwCondAll(cond) >= 0) {
		return 0;
	}
	return -1;
}

int rudpCondWait(SceKernelLwCondWork *cond, SceKernelLwMutexWork *mutex)
{
	(void)mutex;
	if (sceKernelWaitLwCond(cond, 0) >= 0) {
		return 0;
	}
	return -1;
}

int rudpCondTimedWait(SceKernelLwCondWork *cond, SceKernelLwMutexWork *mutex, uint64_t timeoutUsec)
{
	(void)mutex;
	SceUInt32 timeout = (SceUInt32)timeoutUsec;
	if (sceKernelWaitLwCond(cond, &timeout) >= 0) {
		return 0;
	}
	return RUDP_OSAL_ETIMEDOUT;
}

int rudpSha256Init(SceSha256Context *ctx)
{
	if (sceSha256BlockInit(ctx) >= 0) {
		return 0;
	}
	return -1;
}

int rudpSha256Update(SceSha256Context *ctx, const void *data, uint32_t len)
{
	if (sceSha256BlockUpdate(ctx, data, len) >= 0) {
		return 0;
	}
	return -1;
}

int rudpSha256Final(SceSha256Context *ctx, uint8_t *digest)
{
	if (sceSha256BlockResult(ctx, digest) >= 0) {
		return 0;
	}
	return -1;
}

void *rudpMemcpy(void *dst, const void *src, SceSize size)
{
	return sceClibMemcpy_safe(dst, src, size);
}

int rudpMemcmp(const void *lhs, const void *rhs, SceSize size)
{
	return sceClibMemcmp(lhs, rhs, size);
}

void *rudpMemset(void *dst, int value, SceSize size)
{
	return sceClibMemset(dst, value, size);
}

int rudpMutexInit(SceKernelLwMutexWork *mutex, int recursive)
{
	char nameBuf[32];
	SceUInt32 attr = 0x2000U;
	sceClibSnprintf(nameBuf, sizeof(nameBuf), "%s", g_componentName);
	if (recursive == 1) {
		attr = 0x2002U;
	}
	if (sceKernelCreateLwMutex(mutex, nameBuf, attr, 0, 0) >= 0) {
		return 0;
	}
	return -1;
}

int rudpMutexTerm(SceKernelLwMutexWork *mutex)
{
	sceKernelDeleteLwMutex(mutex);
	return 0;
}

int rudpMutexLock(SceKernelLwMutexWork *mutex)
{
	if (sceKernelLockLwMutex(mutex, 1, 0) >= 0) {
		return 0;
	}
	return -1;
}

int rudpMutexUnlock(SceKernelLwMutexWork *mutex)
{
	if (sceKernelUnlockLwMutex(mutex, 1) >= 0) {
		return 0;
	}
	return -1;
}

int rudpNetSocket(int domain, int type)
{
	char nameBuf[32];
	sceClibSnprintf(nameBuf, sizeof(nameBuf), "%s", g_componentName);
	return sceNetSocket(nameBuf, domain, type, 0);
}

int rudpNetBind(int sock, const SceNetSockaddr *addr)
{
	int ret = sceNetBind(sock, addr, sizeof(SceNetSockaddr));
	if (ret >= 0) {
		return 0;
	}
	return ret;
}

int rudpNetClose(int sock)
{
	int ret = sceNetSocketClose(sock);
	if (ret >= 0) {
		return 0;
	}
	return ret;
}

int rudpNetSendto(int sock, const void *buf, uint32_t len, int flags, const SceNetSockaddr *to)
{
	return sceNetSendto(sock, buf, len, flags, to, sizeof(SceNetSockaddr));
}

int rudpNetRecvfrom(int sock, void *buf, uint32_t len, int flags, SceNetSockaddr *from)
{
	SceNetSocklen_t fromLen = sizeof(SceNetSockaddr);
	return sceNetRecvfrom(sock, buf, len, flags, from, &fromLen);
}

int rudpNetGetsockname(int sock, SceNetSockaddr *addr)
{
	SceNetSocklen_t addrLen = sizeof(SceNetSockaddr);
	int ret = sceNetGetsockname(sock, addr, &addrLen);
	if (ret >= 0) {
		return 0;
	}
	return ret;
}

int rudpNetSetNonBlocking(int sock, uint8_t enable)
{
	int optVal = (enable != 0) ? 1 : 0;
	int ret = sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &optVal, sizeof(optVal));
	if (ret >= 0) {
		return 0;
	}
	return ret;
}

int rudpNetInetPton(int family, const char *src, RudpSockaddr *dst)
{
	int ret = sceNetInetPton(family, src, dst->sa_data + 2);
	if (ret >= 0) {
		dst->sa_family = (uint8_t)family;
		return 0;
	}
	return ret;
}

int rudpNetPollCreate(RudpOsalPoll **outPoll, int initialCapacity, const RudpAllocator *alloc)
{
	char nameBuf[32];
	sceClibSnprintf(nameBuf, sizeof(nameBuf), "%s", g_componentName);
	if (outPoll == 0 || initialCapacity < 0) {
		*sceNetErrnoLoc() = SCE_NET_ERROR_EINVAL;
		return -1;
	}
	void *(*allocFn)(SceSize) = 0;
	void (*freeFn)(void *) = 0;
	if (alloc != 0) {
		if (alloc->allocFn == 0 || alloc->freeFn == 0) {
			*sceNetErrnoLoc() = SCE_NET_ERROR_EINVAL;
			return -1;
		}
		allocFn = alloc->allocFn;
		freeFn = alloc->freeFn;
	}
	RudpOsalPoll *poll = (RudpOsalPoll *)allocFn(sizeof(RudpOsalPoll));
	*outPoll = poll;
	if (poll == 0) {
		*sceNetErrnoLoc() = SCE_NET_ERROR_ENOMEM;
		return -1;
	}
	poll->allocFn = allocFn;
	poll->freeFn = freeFn;
	poll->events = (SceNetEpollEvent *)allocFn((SceSize)initialCapacity * sizeof(SceNetEpollEvent));
	if (poll->events == 0) {
		freeFn(poll);
		*outPoll = 0;
		*sceNetErrnoLoc() = SCE_NET_ERROR_ENOMEM;
		return -1;
	}
	poll->capacity = (uint32_t)initialCapacity;
	poll->epollId = sceNetEpollCreate(nameBuf, 0);
	if (poll->epollId < 0) {
		int err = poll->epollId;
		poll->freeFn(poll->events);
		poll->events = 0;
		poll->freeFn(poll);
		*outPoll = 0;
		*sceNetErrnoLoc() = SCE_NET_ERROR_ENOMEM;
		return err;
	}
	return 0;
}

int rudpNetPollDestroy(RudpOsalPoll *poll)
{
	if (poll == 0) {
		*sceNetErrnoLoc() = SCE_NET_ERROR_EINVAL;
		return -1;
	}
	int ret = sceNetEpollDestroy(poll->epollId);
	if (ret < 0) {
		ret = -1;
	}
	poll->freeFn(poll->events);
	poll->freeFn(poll);
	return ret;
}

int rudpNetPollControl(RudpOsalPoll *poll, int op, int sock, uint16_t events)
{
	if (poll == 0) {
		*sceNetErrnoLoc() = SCE_NET_ERROR_EINVAL;
		return -1;
	}
	int epollOp = op;
	switch (op) {
	case 1:
		epollOp = SCE_NET_EPOLL_CTL_ADD;
		break;
	case 2:
		epollOp = SCE_NET_EPOLL_CTL_MOD;
		break;
	case 3:
		epollOp = SCE_NET_EPOLL_CTL_DEL;
		break;
	default:
		break;
	}

	RudpOsalPollEvent tag;
	rudpMemset(&tag, 0, sizeof(tag));
	tag.sock = sock;
	tag.events = events;

	SceNetEpollEvent ev;
	rudpMemset(&ev, 0, sizeof(ev));
	rudpMemcpy(&ev.data, &tag, sizeof(tag));
	ev.events = 0;
	if ((events & 1) != 0) {
		ev.events |= SCE_NET_EPOLLIN;
	}
	if ((events & 2) != 0) {
		ev.events |= SCE_NET_EPOLLOUT;
	}
	if ((events & 4) != 0) {
		ev.events |= SCE_NET_EPOLLERR;
	}

	int ret;
	if (epollOp == SCE_NET_EPOLL_CTL_DEL) {
		ret = sceNetEpollControl(poll->epollId, SCE_NET_EPOLL_CTL_DEL, sock, 0);
	} else {
		ret = sceNetEpollControl(poll->epollId, epollOp, sock, &ev);
	}
	if (ret >= 0) {
		return 0;
	}
	return ret;
}

int rudpNetPollWait(RudpOsalPoll *poll, RudpOsalPollEvent *events, uint32_t maxEvents, int unused, int timeoutMs)
{
	(void)unused;
	if (poll == 0) {
		*sceNetErrnoLoc() = SCE_NET_ERROR_EINVAL;
		return -1;
	}
	uint32_t limit = maxEvents;
	if (poll->capacity < maxEvents) {
		uint32_t newCap = poll->capacity * 2;
		SceNetEpollEvent *newBuf = (SceNetEpollEvent *)poll->allocFn(newCap * sizeof(SceNetEpollEvent));
		if (newBuf != 0) {
			poll->freeFn(poll->events);
			poll->events = newBuf;
			poll->capacity = newCap;
		} else {
			limit = poll->capacity;
		}
	}
	int count = sceNetEpollWait(poll->epollId, poll->events, (int)limit, timeoutMs);
	if (count > 0) {
		for (int i = 0; i < count; i++) {
			RudpOsalPollEvent tag;
			rudpMemcpy(&tag, &poll->events[i].data, sizeof(tag));
			events[i].sock = tag.sock;
			events[i].events = tag.events;
			events[i].revents = 0;
			uint32_t evBits = poll->events[i].events;
			if ((evBits & SCE_NET_EPOLLIN) != 0) {
				events[i].revents |= 1;
			}
			if ((evBits & SCE_NET_EPOLLOUT) != 0) {
				events[i].revents |= 2;
			}
			if ((evBits & SCE_NET_EPOLLERR) != 0) {
				events[i].revents |= 4;
			}
		}
	}
	return count;
}

int rudpNetGetLastError(void)
{
	int err = *sceNetErrnoLoc();
	switch (err) {
	case 0:   return 0;
	case 1:   return 1;
	case 2:   return 2;
	case 3:   return 3;
	case 4:   return 4;
	case 5:   return 5;
	case 6:   return 6;
	case 7:   return 7;
	case 8:   return 8;
	case 9:   return 9;
	case 10:  return 10;
	case 11:  return 11;
	case 12:  return 12;
	case 13:  return 13;
	case 14:  return 14;
	case 15:  return 15;
	case 16:  return 16;
	case 17:  return 17;
	case 18:  return 18;
	case 19:  return 19;
	case 20:  return 20;
	case 21:  return 21;
	case 22:  return 22;
	case 23:  return 23;
	case 24:  return 24;
	case 25:  return 25;
	case 26:  return 26;
	case 27:  return 27;
	case 28:  return 28;
	case 29:  return 29;
	case 30:  return 30;
	case 31:  return 31;
	case 32:  return 32;
	case 33:  return 33;
	case 34:  return 34;
	case 35:  return 35;
	case 36:  return 36;
	case 37:  return 37;
	case 38:  return 38;
	case 39:  return 39;
	case 40:  return 40;
	case 41:  return 41;
	case 42:  return 42;
	case 43:  return 43;
	case 44:  return 44;
	case 45:  return 45;
	case 46:  return 46;
	case 47:  return 47;
	case 48:  return 48;
	case 49:  return 49;
	case 50:  return 50;
	case 51:  return 51;
	case 52:  return 52;
	case 53:  return 53;
	case 54:  return 54;
	case 55:
	case 86:  return 86;
	case 56:  return 56;
	case 57:  return 57;
	case 58:  return 58;
	case 59:  return 59;
	case 60:  return 60;
	case 61:  return 61;
	case 62:  return 62;
	case 63:  return 63;
	case 64:  return 64;
	case 65:  return 65;
	case 66:  return 66;
	case 67:  return 67;
	case 68:  return 68;
	case 69:  return 69;
	case 70:  return 70;
	case 71:  return 71;
	case 77:  return 77;
	case 78:  return 78;
	case 79:  return 79;
	case 82:  return 82;
	case 83:  return 83;
	case 84:  return 84;
	case 85:  return 85;
	case 88:  return 88;
	case 89:  return 89;
	case 90:  return 90;
	case 91:  return 91;
	case 92:  return 92;
	case 160: return 160;
	case 161: return 161;
	case 162: return 162;
	case 163: return 163;
	case 200: return 200;
	case 201: return 201;
	case 203: return 203;
	case 204: return 204;
	case 205: return 205;
	default:
		sceClibPrintf("Unknown psp error: %d, 0x%8x\n", err, err);
		return 255;
	}
}

void rudpOsalNoop(void)
{
}

int rudpSetComponentName(const char *name)
{
	SceSize len = rudpStrlenHelper(name);
	if ((int)len >= 32) {
		return 0;
	}
	rudpMemcpySafeHelper(g_componentName, name, len);
	return 1;
}

uint64_t rudpGetTimeUsec(void)
{
	return sceKernelGetProcessTimeWide();
}

uint32_t rudpRandU32(void)
{
	int idx = ++g_prngIndex;
	if (idx >= 521) {
		for (int i = 0; i < 32; i++) {
			g_prngTable[i] ^= g_prngTable[i + 489];
		}
		for (int i = 32; i < 521; i++) {
			g_prngTable[i] ^= g_prngTable[i - 32];
		}
		idx = 0;
		g_prngIndex = 0;
	}
	return g_prngTable[idx];
}

extern "C" void abort(void)
{
	sceClibAbort();
}

struct RudpThreadStartArgs {
	int (*entry)(void *);
	void *arg;
};

static SceInt32 rudpThreadTrampoline(SceSize args, void *argp)
{
	(void)args;
	RudpThreadStartArgs *start = (RudpThreadStartArgs *)argp;
	return start->entry(start->arg);
}

int rudpThreadCreate(SceUID *outUid, int (*entry)(void *), void *arg, SceSize stackSize, int priority, int unused, uint64_t affinity)
{
	(void)unused;
	RudpThreadStartArgs startArgs;
	char nameBuf[32];
	startArgs.entry = entry;
	startArgs.arg = arg;

	sceClibSnprintf(nameBuf, sizeof(nameBuf), "%s", g_componentName);
	SceInt32 cpuMask = (affinity == 0) ? 0 : (SceInt32)affinity;
	SceUID uid = sceKernelCreateThread(nameBuf, rudpThreadTrampoline, priority, (SceInt32)stackSize, 0, cpuMask, 0);
	*outUid = uid;
	if (uid < 0) {
		return -1;
	}
	if (sceKernelStartThread(uid, sizeof(startArgs), &startArgs) != 0) {
		sceKernelDeleteThread(*outUid);
		return -1;
	}
	return 0;
}

int rudpThreadJoin(SceUID uid, int *exitStatus)
{
	*exitStatus = 0;
	int ret = sceKernelWaitThreadEnd(uid, exitStatus, 0);
	if (ret >= 0) {
		sceKernelDeleteThread(uid);
		return 0;
	}
	*exitStatus = 0;
	switch (ret) {
	case SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID:
		return 0;
	case SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID:
		return RUDP_OSAL_ESRCH;
	default:
		return RUDP_OSAL_EINVAL;
	}
}

int rudpDelay1Ms(void)
{
	return sceKernelDelayThread(1000);
}

int rudpOsalInit(int flags)
{
	(void)flags;
	int ret = 0;
	while (rudpAtomicCompareAndSwap(&g_osalSpinLock, 0, 1) != 0) {
	}
	if (g_osalRefCount == 0) {
		g_osalInitTime = rudpGetTimeUsec();
		ret = rudpMutexInit(&g_osalMutex, 0);
		if (ret >= 0) {
			g_osalRefCount++;
		}
	} else {
		g_osalRefCount++;
	}
	rudpAtomicSwap(&g_osalSpinLock, 0);
	return ret;
}

void rudpOsalTerm(void)
{
	while (rudpAtomicCompareAndSwap(&g_osalSpinLock, 0, 1) != 0) {
	}
	if (--g_osalRefCount == 0) {
		rudpMutexTerm(&g_osalMutex);
	}
	rudpAtomicSwap(&g_osalSpinLock, 0);
}
