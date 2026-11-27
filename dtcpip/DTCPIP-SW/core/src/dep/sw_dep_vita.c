/*
 * Vita OSAL and crypto bridge for DTCPIP-SW: secure clock delta
 * tracking, lightweight event/mutex/thread wrappers (SWS_THREAD_TYPE_*), OpenPSID-derived
 * device IDu, hardware DMAC5 / software AES-128-ECB/CBC, SHA-1, HMAC-SHA1, and ECDSA/ECDH
 * curve-parameter adapters.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <scetypes.h>
#include <kernel/openpsid.h>
#include <kernel/threadmgr.h>
#include <rtc.h>
#include <libaes.h>
#include <libhmac.h>
#include <libsha1.h>

struct SwDepEvent {
	SceKernelLwCondWork cond;
	SceKernelLwMutexWork mutex;
	int signaled;
};

struct SwDepThread {
	int (*entry)(void *user);
	SceUID threadId;
	void *user;
};

struct SwDepWorker {
	struct SwDepThread thread;
	unsigned int pad;
	struct SwDepEvent event;
	unsigned char reservedGap[32];
	int stopRequested;
	int running;
	int stopped;
	int (*hasWork)(void *ctx);
	int (*isDone)(void *ctx);
	void (*process)(void *ctx);
	void (*onExit)(void *ctx);
	void *ctx;
};

struct SwDepAesCbcContext {
	SceAesContext aes;
	unsigned char iv[32];
};

struct SwDepDmac5Param {
	const void *src;
	void *dst;
	SceSize length;
	const void *key;
	SceSize keySize;
	void *iv;
};

struct SwDepEccPoint {
	void *x;
	void *y;
};

struct SwDepEccCurve {
	void *prime;
	void *coeffA;
	void *coeffB;
	void *order;
	void *genX;
	void *genY;
};

struct SwDepCurveTable {
	unsigned int header;
	unsigned char prime[24];
	unsigned char coeffA[24];
	unsigned char coeffB[24];
	unsigned char order[24];
	unsigned char genX[24];
	unsigned char genY[24];
};

/* external SWS core, VshBridge, DMAC5 and ECC bignum hooks */
int vshRtcGetCurrentSecureTick(SceRtcTick *tick);
int sceSblDmac5EncDec(struct SwDepDmac5Param *param, SceUInt32 command);
int swMwTrace(const char *format, ...);
int SwsGetToolMode(void);
int SwsGetAffinity(void);
int SwMwThreadTrampoline(SceSize args, void *argp);
struct SwDepCurveTable *swGetEccCurveTable(void);
int swDepInitCore(void *dep);
int swDepFiniCore(void *dep);
int swEccScalarMulBase(struct SwDepEccPoint *outPoint, const unsigned char *privKey,
		       const struct SwDepEccCurve *curve);
int swEccScalarMulPoint(struct SwDepEccPoint *outPoint, const unsigned char *privKey,
			const struct SwDepEccPoint *peerPoint, const struct SwDepEccCurve *curve);
int swEccEcdsaSign(struct SwDepEccPoint *outSig, const unsigned char *hash,
		   const unsigned char *kNonce, const unsigned char *privKey,
		   const struct SwDepEccCurve *curve);
int swEccEcdsaVerify(const struct SwDepEccPoint *sig, const unsigned char *hash,
		     const struct SwDepEccPoint *pubKey, const struct SwDepEccCurve *curve);

/* cached secure RTC anchor and tick baseline */
static SceUInt64 g_swDepSecureBaseSec;
static SceUInt64 g_swDepLocalBaseSec;

/* OpenPSID HMAC input template and 16-byte HMAC key */
static unsigned char g_swDepPsidInput[64] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xae, 0xd9, 0xbb, 0x67, 0x10, 0x0d, 0x54, 0xe0, 0x7b, 0x33, 0x32, 0x63, 0x20, 0x9b, 0xd1, 0xb6,
	0x9b, 0x1c, 0x44, 0xaf, 0x29, 0x6b, 0x0d, 0xb2, 0x50, 0xa7, 0xf4, 0x3a, 0x6c, 0xf9, 0x17, 0x84,
	0x89, 0x91, 0xab, 0xad, 0x4c, 0x87, 0xfb, 0x7a, 0xf4, 0xff, 0x01, 0xd3, 0xc4, 0xca, 0xfb, 0x2e,
};

static unsigned char g_swDepPsidKey[16] = {
	0x7f, 0x79, 0x72, 0x08, 0x94, 0x74, 0x9c, 0xe4, 0x3c, 0x68, 0xcf, 0x4c, 0x96, 0x61, 0xff, 0xb9,
};

/* aligned scratch buffers for DMAC5 / unaligned AES-CBC */
static unsigned char g_swDepDmac5Bounce[0x8000];
static unsigned char g_swDepAesBlockBounce[16];

/* forward declarations */
SceInt64 swDepElapsedSec(SceInt64 nowSec, SceInt64 baseSec);
SceInt64 swDepAddSec(SceInt64 baseSec, int deltaSec);
void *swDepMalloc(size_t size);
void swDepFree(void *mem);
int swDepWorkerLoop(void *arg);
int swDepSha1Digest(const void *input, unsigned int size, unsigned char *digest);
int swDepHmacSha1(const void *input, unsigned int inputLen, const void *key,
		  unsigned int keyLen, unsigned char *outMac, size_t outLen);

/* read current local RTC time as UNIX seconds */
SceUInt64 swDepGetLocalTimeSec(void)
{
	SceRtcTick tick;
	SceUInt64 seconds;
	SceDateTime dateTime;

	if (sceRtcGetCurrentTick(&tick) < 0)
		return 0;
	if (sceRtcSetTick(&dateTime, &tick) < 0)
		return 0;
	if (sceRtcGetTime64_t(&dateTime, &seconds) < 0)
		return 0;
	return seconds;
}

/* read secure RTC time anchored on first call and advanced by local tick delta */
SceUInt64 swDepGetSecureTimeSec(void)
{
	SceUInt64 localNow;
	SceInt64 elapsed;
	SceUInt64 result = 0;
	SceRtcTick tick;
	SceDateTime dateTime;

	if (g_swDepSecureBaseSec != 0) {
		localNow = swDepGetLocalTimeSec();
		elapsed = swDepElapsedSec((SceInt64)localNow, (SceInt64)g_swDepLocalBaseSec);
		result = (SceUInt64)swDepAddSec((SceInt64)g_swDepSecureBaseSec, (int)elapsed);
		return result;
	}
	if (vshRtcGetCurrentSecureTick(&tick) < 0)
		return 0;
	g_swDepLocalBaseSec = swDepGetLocalTimeSec();
	if (sceRtcSetTick(&dateTime, &tick) < 0)
		return 0;
	if (sceRtcGetTime64_t(&dateTime, &result) < 0)
		return 0;
	g_swDepSecureBaseSec = result;
	return result;
}

/* compute positive elapsed seconds with 31-bit wrap adjustment */
SceInt64 swDepElapsedSec(SceInt64 nowSec, SceInt64 baseSec)
{
	if (baseSec > nowSec)
		return nowSec + 0x7FFFFFFFLL - baseSec;
	return nowSec - baseSec;
}

/* add a signed delta in seconds to a 64-bit timestamp */
SceInt64 swDepAddSec(SceInt64 baseSec, int deltaSec)
{
	return baseSec + deltaSec;
}

/* allocate and initialize the 345584-byte sw dep library instance */
int swDepInitLib(void **outDep, void **outCore)
{
	void **dep;
	int status;

	dep = (void **)swDepMalloc(345584);
	if (dep == NULL)
		return 7;
	memset(dep, 0, 345584);
	status = swDepInitCore(dep);
	if (status != 0) {
		swDepFree(dep);
	} else {
		*outDep = dep;
		*outCore = dep[0];
	}
	return status;
}

/* tear down and free the sw dep library instance */
void swDepFiniLib(void *dep)
{
	swDepFiniCore(dep);
	swDepFree(dep);
}

/* initialize a lightweight condition-variable event */
int swDepEventInit(struct SwDepEvent *event)
{
	event->signaled = 0;
	if (sceKernelCreateLwMutex(&event->mutex, "sce_sws_event_mutex", 0x2000u, 0, NULL) < 0)
		return 11;
	if (sceKernelCreateLwCond(&event->cond, "sce_sws_event_cond", 0x2000u,
				  &event->mutex, NULL) >= 0)
		return 0;
	sceKernelDeleteLwMutex(&event->mutex);
	return 11;
}

/* initialize the worker's embedded event */
int swDepWorkerEventInit(struct SwDepWorker *worker)
{
	swDepEventInit(&worker->event);
	return 0;
}

/* destroy a lightweight condition-variable event */
int swDepEventFini(struct SwDepEvent *event)
{
	sceKernelDeleteLwCond(&event->cond);
	sceKernelDeleteLwMutex(&event->mutex);
	return 0;
}

/* destroy the worker's embedded event */
int swDepWorkerEventFini(struct SwDepWorker *worker)
{
	swDepEventFini(&worker->event);
	return 0;
}

/* create and start a Vita thread for an SWS thread type */
int swDepThreadCreateRaw(struct SwDepThread *thread, int (*entry)(void *), void *user,
			 int threadType)
{
	SceUID thid;
	int initPriority;
	int cpuAffinityMask;
	SceSize stackSize;
	char name[32];

	thread->entry = entry;
	thread->user = user;
	stackSize = 0x4000;
	memset(name, 0, sizeof(name));
	memcpy(name, "SWS_THREAD_NO_NAME", 19);
	initPriority = (SwsGetToolMode() == 0) ? 268435712 : 111;
	cpuAffinityMask = 0;

	switch (threadType) {
	case 0:
		strncpy(name, "SWS_THREAD_TYPE_ANONYMOUS", 31);
		break;
	case 1:
		strncpy(name, "SWS_THREAD_TYPE_AKE_CMD_TIMER", 31);
		break;
	case 2:
		strncpy(name, "SWS_THREAD_TYPE_KX_EXP", 31);
		break;
	case 3:
		strncpy(name, "SWS_THREAD_TYPE_SINK_AUTH", 31);
		stackSize = 0x8000;
		break;
	case 4:
		strncpy(name, "SWS_THREAD_TYPE_SINK_AKE_TIMER", 31);
		break;
	case 5:
		strncpy(name, "SWS_THREAD_TYPE_SINK_CKC", 31);
		break;
	case 6:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AUTH", 31);
		stackSize = 0x8000;
		break;
	case 7:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AUTH_SERVER", 31);
		stackSize = 0x8000;
		break;
	case 8:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AKE_TIMER", 31);
		stackSize = 0x8000;
		break;
	case 9:
		strncpy(name, "SWS_THREAD_TYPE_RTT_REG", 31);
		stackSize = 0x8000;
		break;
	case 10:
		strncpy(name, "SWS_THREAD_TYPE_TTL_CHECK", 31);
		break;
	case 11:
		strncpy(name, "SWS_THREAD_TYPE_MEDIA_DISCONNECT", 31);
		break;
	case 16:
		strncpy(name, "SWS_THREAD_TYPE_SW_TIMER", 31);
		break;
	case 17:
		strncpy(name, "SWS_THREAD_TYPE_SW_DEP", 31);
		cpuAffinityMask = SwsGetAffinity();
		if (cpuAffinityMask != 0) {
			cpuAffinityMask |= 0x80000;
			if (cpuAffinityMask != 0x80000)
				initPriority = 192;
		}
		break;
	case 18:
		stackSize = 0x20000;
		strncpy(name, "SWS_THREAD_TYPE_TOOL", 31);
		break;
	case 19:
		strncpy(name, "SWS_THREAD_TYPE_ANONYMOUS", 31);
		cpuAffinityMask = SwsGetAffinity();
		if (cpuAffinityMask != 0) {
			cpuAffinityMask |= 0x80000;
			if (cpuAffinityMask != 0x80000)
				initPriority = 192;
		}
		break;
	default:
		break;
	}
	name[31] = '\0';

	thid = sceKernelCreateThread(name, (SceKernelThreadEntry)SwMwThreadTrampoline,
				     initPriority, stackSize, 0, cpuAffinityMask, NULL);
	if (thid < 0)
		return thid;
	thread->threadId = thid;
	return sceKernelStartThread(thid, sizeof(*thread), thread);
}

/* create a thread with logging on failure */
int swDepThreadCreate(struct SwDepThread *thread, int (*entry)(void *), void *user,
		      const char *label, int threadType)
{
	int status;

	(void)label;
	status = swDepThreadCreateRaw(thread, entry, user, threadType);
	if (status == 0)
		return 0;
	swMwTrace("sw: ERROR, pthread_create(id=%d) %d\n", threadType, status);
	return -1;
}

/* configure and launch a sw dep worker thread */
int swDepWorkerStart(struct SwDepWorker *worker, const char *unusedName,
		     int (*hasWork)(void *), int (*isDone)(void *),
		     void (*process)(void *), void (*onExit)(void *), void *ctx)
{
	(void)unusedName;
	worker->hasWork = hasWork;
	worker->isDone = isDone;
	worker->process = process;
	worker->onExit = onExit;
	worker->ctx = ctx;
	worker->stopRequested = 0;
	worker->running = 0;
	worker->stopped = 0;
	if (swDepThreadCreate(&worker->thread, swDepWorkerLoop, worker, "dep", 17) == 0)
		return 0;
	swMwTrace("failed to start thread\n");
	return 6;
}

/* signal a lightweight event */
int swDepEventSignal(struct SwDepEvent *event)
{
	sceKernelLockLwMutex(&event->mutex, 1, NULL);
	event->signaled = 1;
	sceKernelSignalLwCond(&event->cond);
	sceKernelUnlockLwMutex(&event->mutex, 1);
	return 0;
}

/* wait for a thread to end and delete its handle */
int swDepThreadJoinRaw(struct SwDepThread *thread)
{
	int status;

	status = sceKernelWaitThreadEnd(thread->threadId, NULL, NULL);
	sceKernelDeleteThread(thread->threadId);
	return status;
}

/* join a thread with logging on failure */
int swDepThreadJoin(struct SwDepThread *thread)
{
	int status;

	status = swDepThreadJoinRaw(thread);
	if (status == 0)
		return 0;
	swMwTrace("sw: ERROR, pthread_join %d\n", status);
	return -1;
}

/* request stop and join a sw dep worker thread */
int swDepWorkerStop(struct SwDepWorker *worker)
{
	if (worker->stopped == 0) {
		worker->stopRequested = 1;
		swDepEventSignal(&worker->event);
		swDepThreadJoin(&worker->thread);
		worker->stopped = 1;
	}
	return 0;
}

/* wake a running worker thread */
int swDepWorkerWake(struct SwDepWorker *worker)
{
	if (worker->running != 0)
		return swDepEventSignal(&worker->event);
	return 0;
}

/* create/delete/lock/unlock lwmutex helpers */
int swDepMutexCreateRaw(SceKernelLwMutexWork *mutex)
{
	int status;

	status = sceKernelCreateLwMutex(mutex, "sce_sws_mutex", 0x2000u, 0, NULL);
	if (status >= 0)
		return 0;
	return status;
}

int swDepMutexCreate(SceKernelLwMutexWork *mutex)
{
	return swDepMutexCreateRaw(mutex);
}

int swDepMutexDeleteRaw(SceKernelLwMutexWork *mutex)
{
	return sceKernelDeleteLwMutex(mutex);
}

int swDepMutexDelete(SceKernelLwMutexWork *mutex)
{
	return swDepMutexDeleteRaw(mutex);
}

int swDepMutexLockRaw(SceKernelLwMutexWork *mutex)
{
	return sceKernelLockLwMutex(mutex, 1, NULL);
}

int swDepMutexLock(SceKernelLwMutexWork *mutex)
{
	return swDepMutexLockRaw(mutex);
}

int swDepMutexUnlockRaw(SceKernelLwMutexWork *mutex)
{
	return sceKernelUnlockLwMutex(mutex, 1);
}

int swDepMutexUnlock(SceKernelLwMutexWork *mutex)
{
	return swDepMutexUnlockRaw(mutex);
}

/* heap wrappers */
void *swDepMalloc(size_t size)
{
	return malloc(size);
}

void swDepFree(void *mem)
{
	free(mem);
}

/* wait until a lightweight event is signaled */
int swDepEventWait(struct SwDepEvent *event)
{
	if (sceKernelLockLwMutex(&event->mutex, 1, NULL) < 0)
		return 22;
	while (event->signaled == 0) {
		if (sceKernelWaitLwCond(&event->cond, NULL) < 0) {
			sceKernelUnlockLwMutex(&event->mutex, 1);
			return 22;
		}
	}
	event->signaled = 0;
	if (sceKernelUnlockLwMutex(&event->mutex, 1) != 0)
		return 22;
	return 0;
}

/* wait on event unless work is already pending */
int swDepEventWaitIfIdle(struct SwDepEvent *event, int hasPending)
{
	if (hasPending != 0)
		return 0;
	if (swDepEventWait(event) != 0)
		return -1;
	return 0;
}

/* main loop of a sw dep worker thread */
int swDepWorkerLoop(void *user)
{
	struct SwDepWorker *worker = (struct SwDepWorker *)user;
	int finished = 0;

	worker->running = 1;
	for (;;) {
		int ready = 0;

		if (worker->stopRequested != 0 ||
		    (finished == 0 && worker->hasWork != NULL &&
		     worker->hasWork(worker->ctx) != 0))
			ready = 1;
		if (swDepEventWaitIfIdle(&worker->event, ready) != 0 ||
		    worker->stopRequested != 0)
			break;
		if (finished == 0) {
			if (worker->process != NULL)
				worker->process(worker->ctx);
			if (worker->isDone != NULL && worker->isDone(worker->ctx) != 0)
				finished = 1;
		}
	}
	worker->running = 0;
	if (worker->onExit != NULL)
		worker->onExit(worker->ctx);
	return 0;
}

/* derive the 5-byte DTCP IDu from the console OpenPSID via HMAC-SHA1 */
int swDepGetDeviceIdu(void *unusedCtx, unsigned char *outIdu)
{
	int status;
	unsigned char digest[5];
	SceKernelOpenPsId openPsId;

	(void)unusedCtx;
	status = sceKernelGetOpenPsId(&openPsId);
	memcpy(g_swDepPsidInput, &openPsId, sizeof(openPsId));
	swDepHmacSha1(g_swDepPsidInput, sizeof(g_swDepPsidInput),
		      g_swDepPsidKey, sizeof(g_swDepPsidKey), digest, sizeof(digest));
	if (status != 0)
		return -1;
	outIdu[0] = digest[0];
	outIdu[1] = digest[1];
	outIdu[2] = digest[2];
	outIdu[3] = digest[3];
	outIdu[4] = digest[4];
	return 0;
}

/* AES-128-ECB encrypt (decrypt==0) or decrypt (decrypt!=0) */
int swDepAes128Ecb(const unsigned char *src, unsigned int srcLen, unsigned char *dst,
		   unsigned int dstLen, const void *key, int decrypt)
{
	unsigned int i;
	SceAesContext aes;

	if (src == NULL || dst == NULL || srcLen > dstLen)
		return -1;
	sceAesScheduleKey(&aes, 128, 128, key);
	if (decrypt != 0) {
		for (i = 0; i < (srcLen >> 4); ++i) {
			sceAesDecrypt(&aes, src, dst);
			src += 16;
			dst += 16;
		}
	} else {
		for (i = 0; i < (srcLen >> 4); ++i) {
			sceAesEncrypt(&aes, src, dst);
			src += 16;
			dst += 16;
		}
	}
	return 0;
}

/* schedule AES-CBC key and copy IV */
int swDepAesCbcInit(struct SwDepAesCbcContext *ctx, SceUInt32 blockBits, SceUInt32 keyBits,
		    const void *key, const unsigned char *iv)
{
	unsigned int i;
	int status;

	status = sceAesScheduleKey(&ctx->aes, blockBits, keyBits, key);
	if (status < 0)
		return status;
	for (i = 0; i < 4u * ctx->aes.blockWords; ++i)
		ctx->iv[i] = iv[i];
	return 0;
}

/* encrypt one AES-CBC block in software */
int swDepAesCbcEncryptBlock(struct SwDepAesCbcContext *ctx, const unsigned char *inBlock,
			    unsigned char *outBlock)
{
	unsigned int i;
	unsigned char xored[32];

	for (i = 0; i < 4u * ctx->aes.blockWords; ++i)
		xored[i] = inBlock[i] ^ ctx->iv[i];
	sceAesEncrypt(&ctx->aes, xored, outBlock);
	for (i = 0; i < 4u * ctx->aes.blockWords; ++i)
		ctx->iv[i] = outBlock[i];
	return 0;
}

/* AES-128-CBC encrypt buffer in software with unaligned bounce support */
int swDepAes128CbcEncrypt(const unsigned char *src, unsigned char *dst, unsigned int size,
			  const void *key, const unsigned char *iv)
{
	unsigned int i;
	int status;
	struct SwDepAesCbcContext ctx;

	if (src == NULL || dst == NULL || key == NULL || iv == NULL || (size & 0xFu) != 0)
		return -1;
	status = swDepAesCbcInit(&ctx, 128, 128, key, iv);
	if (status != 0)
		return status;

	if (((uintptr_t)dst & 0xFu) == 0) {
		for (i = 0; i < (size >> 4); ++i) {
			status = swDepAesCbcEncryptBlock(&ctx, src, dst);
			if (status != 0)
				return status;
			src += 16;
			dst += 16;
		}
		return 0;
	}
	for (i = 0; i < (size >> 4); ++i) {
		memcpy(g_swDepAesBlockBounce, dst, 16);
		status = swDepAesCbcEncryptBlock(&ctx, src, g_swDepAesBlockBounce);
		if (status != 0)
			return status;
		memcpy(dst, g_swDepAesBlockBounce, 16);
		src += 16;
		dst += 16;
	}
	return 0;
}

/* hardware AES-128-CBC decrypt via SceSblDmac5Mgr command 0x0A */
int swDepDmac5AesCbcDecrypt(const void *src, void *dst, SceSize size, const void *key,
			    SceSize keyBits, void *iv)
{
	struct SwDepDmac5Param desc;

	desc.src = src;
	desc.dst = dst;
	desc.length = size;
	desc.key = key;
	desc.keySize = keyBits;
	desc.iv = iv;
	return sceSblDmac5EncDec(&desc, 0xAu);
}

/* decrypt one 16-byte AES-128-CBC block in software */
int swDepAesCbcDecrypt128(struct SwDepAesCbcContext *ctx, const unsigned char *inBlock,
			  unsigned char *outBlock)
{
	unsigned char decrypted[16];
	unsigned char nextIv[16];
	int i;

	sceAesDecrypt(&ctx->aes, inBlock, decrypted);
	memcpy(nextIv, inBlock, 16);
	for (i = 0; i < 16; ++i)
		outBlock[i] = decrypted[i] ^ ctx->iv[i];
	memcpy(ctx->iv, nextIv, 16);
	return 0;
}

/* AES-128-CBC decrypt via DMAC5 hardware or software fallback */
int swDepAes128CbcDecrypt(const unsigned char *src, unsigned char *dst, unsigned int size,
			  const void *key, unsigned char *iv)
{
	unsigned int i;
	int status;
	struct SwDepAesCbcContext ctx;

	if (src == NULL || dst == NULL || key == NULL || iv == NULL || (size & 0xFu) != 0)
		return -1;

	if (SwsGetToolMode() == 0) {
		unsigned int remain = size;

		while (remain != 0) {
			size_t chunk = (remain <= 0x8000u) ? remain : 0x8000u;

			if (((uintptr_t)src & 0xFu) != 0) {
				memcpy(g_swDepDmac5Bounce, src, chunk);
				status = swDepDmac5AesCbcDecrypt(g_swDepDmac5Bounce, dst,
								 chunk, key, 128, iv);
			} else {
				status = swDepDmac5AesCbcDecrypt(src, dst, chunk, key, 128, iv);
			}
			if (status != 0)
				return status;
			remain -= (unsigned int)chunk;
			src += chunk;
			dst += chunk;
		}
		return 0;
	}

	status = swDepAesCbcInit(&ctx, 128, 128, key, iv);
	if (status != 0)
		return status;
	for (i = 0; i < (size >> 4); ++i) {
		status = swDepAesCbcDecrypt128(&ctx, src, dst);
		if (status != 0)
			return status;
		src += 16;
		dst += 16;
	}
	return 0;
}

/* compute SHA-1 and right-shift to 154-bit DTCP ECDSA hash */
int swDepEccHash154(const void *message, unsigned int messageLen, unsigned char *outHash)
{
	unsigned char shiftRight = 6;
	unsigned char shiftLeft = 2;
	unsigned int i;
	unsigned char sha1[20];

	if (message == NULL || outHash == NULL)
		return -1;

	swDepSha1Digest(message, messageLen, sha1);
	memset(outHash, 0, 20);
	for (i = 0; i < 20u; ++i)
		outHash[i] = sha1[i];
	if (shiftRight != 0) {
		for (i = 20u; i != 0; --i) {
			if (i == 1u)
				outHash[0] = (unsigned char)(outHash[0] >> shiftRight);
			else
				outHash[i - 1u] = (unsigned char)((outHash[i - 2u] << shiftLeft) |
								  (outHash[i - 1u] >> shiftRight));
		}
	}
	return 0;
}

/* verify a 40-byte DTCP ECDSA signature over message using 40-byte pubkey */
int swDepEccEcdsaVerify(const void *message, unsigned int messageLen,
			const unsigned char *sigBytes, const unsigned char *pubKeyBytes)
{
	struct SwDepCurveTable *table;
	struct SwDepEccPoint sig;
	struct SwDepEccPoint pubKey;
	struct SwDepEccCurve curve;
	unsigned char hash[20];
	unsigned char sigR[20];
	unsigned char sigS[20];
	unsigned char pubX[20];
	unsigned char pubY[20];

	sig.x = sigR;
	sig.y = sigS;
	pubKey.x = pubX;
	pubKey.y = pubY;
	if (message == NULL || sigBytes == NULL || pubKeyBytes == NULL)
		return -1;

	swDepEccHash154(message, messageLen, hash);
	memcpy(sig.x, sigBytes, 20);
	memcpy(sig.y, sigBytes + 20, 20);
	memcpy(pubKey.x, pubKeyBytes, 20);
	memcpy(pubKey.y, pubKeyBytes + 20, 20);
	memset(&curve, 0, sizeof(curve));
	table = swGetEccCurveTable();
	curve.prime = table->prime;
	curve.coeffA = table->coeffA;
	curve.coeffB = table->coeffB;
	curve.order = table->order;
	curve.genX = table->genX;
	curve.genY = table->genY;
	return swEccEcdsaVerify(&sig, hash, &pubKey, &curve);
}

/* mask a 20-byte random scalar to 153 bits (top byte masked to bit 0) */
unsigned char swDepEccMaskScalar(unsigned char *dst, const void *src)
{
	memcpy(dst, src, 20);
	dst[0] &= 1u;
	return dst[0];
}

/* sign message with DTCP ECDSA private key and random nonce */
int swDepEccEcdsaSign(const void *message, unsigned int messageLen,
		      const void *privKeyBytes, const void *nonceBytes,
		      unsigned char *outSig)
{
	struct SwDepCurveTable *table;
	int status;
	struct SwDepEccPoint sig;
	struct SwDepEccCurve curve;
	unsigned char hash[20];
	unsigned char nonce[20];
	unsigned char privKey[20];
	unsigned char sigR[20];
	unsigned char sigS[20];

	sig.x = sigR;
	sig.y = sigS;
	if (message == NULL || privKeyBytes == NULL || nonceBytes == NULL || outSig == NULL)
		return -1;

	swDepEccHash154(message, messageLen, hash);
	swDepEccMaskScalar(nonce, nonceBytes);
	memcpy(privKey, privKeyBytes, 20);
	memset(&curve, 0, sizeof(curve));
	table = swGetEccCurveTable();
	curve.prime = table->prime;
	curve.coeffA = table->coeffA;
	curve.coeffB = table->coeffB;
	curve.order = table->order;
	curve.genX = table->genX;
	curve.genY = table->genY;
	status = swEccEcdsaSign(&sig, hash, nonce, privKey, &curve);
	if (status == 0) {
		memcpy(outSig, sig.x, 20);
		memcpy(outSig + 20, sig.y, 20);
	}
	return status;
}

/* ECDH phase 1: mask private scalar and compute public point k*G */
int swDepEccEcdhPhase1(void *outPrivKey, unsigned char *outPubPoint, const void *randomBytes)
{
	struct SwDepCurveTable *table;
	int status;
	struct SwDepEccPoint pubPoint;
	struct SwDepEccCurve curve;
	unsigned char privKey[20];
	unsigned char pubX[20];
	unsigned char pubY[20];

	pubPoint.x = pubX;
	pubPoint.y = pubY;
	swDepEccMaskScalar(privKey, randomBytes);
	memcpy(outPrivKey, privKey, 20);
	memset(&curve, 0, sizeof(curve));
	table = swGetEccCurveTable();
	curve.prime = table->prime;
	curve.coeffA = table->coeffA;
	curve.coeffB = table->coeffB;
	curve.order = table->order;
	curve.genX = table->genX;
	curve.genY = table->genY;
	status = swEccScalarMulBase(&pubPoint, privKey, &curve);
	if (status == 0) {
		memcpy(outPubPoint, pubPoint.x, 20);
		memcpy(outPubPoint + 20, pubPoint.y, 20);
	}
	return status;
}

/* ECDH phase 2: compute shared secret X-coordinate from private scalar and peer point */
int swDepEccEcdhPhase2(void *outSharedX, const void *privKeyBytes,
		       const unsigned char *peerPubBytes)
{
	struct SwDepCurveTable *table;
	int status;
	struct SwDepEccPoint peerPoint;
	struct SwDepEccPoint sharedPoint;
	struct SwDepEccCurve curve;
	unsigned char peerX[20];
	unsigned char peerY[20];
	unsigned char sharedX[20];
	unsigned char sharedY[20];
	unsigned char privKey[24];

	peerPoint.x = peerX;
	peerPoint.y = peerY;
	sharedPoint.x = sharedX;
	sharedPoint.y = sharedY;
	memcpy(privKey, privKeyBytes, 20);
	memcpy(peerX, peerPubBytes, 20);
	memcpy(peerY, peerPubBytes + 20, 20);
	memset(&curve, 0, sizeof(curve));
	table = swGetEccCurveTable();
	curve.prime = table->prime;
	curve.coeffA = table->coeffA;
	curve.coeffB = table->coeffB;
	curve.order = table->order;
	curve.genX = table->genX;
	curve.genY = table->genY;
	status = swEccScalarMulPoint(&sharedPoint, privKey, &peerPoint, &curve);
	if (status == 0)
		memcpy(outSharedX, sharedPoint.x, 20);
	return status;
}

/* crypto self-test stub returning 0 */
int swDepCryptoNoop(void *unused)
{
	(void)unused;
	return 0;
}

static int g_swDepPrngSeedSlot;
static uint64_t g_swDepPrngSeed[3];

extern int swPrngFips186(unsigned char *outRandom, unsigned char *xkey,
			 const unsigned char *xseed);

/* generate 20 random bytes via FIPS 186-2 SHA-1 PRNG seeded from RTC tick */
void swDepGetRandom(unsigned char *xkey, unsigned char *outRandom)
{
	SceRtcTick tick;

	if (xkey == NULL || outRandom == NULL)
		return;
	sceRtcGetCurrentTick(&tick);
	g_swDepPrngSeed[g_swDepPrngSeedSlot] = tick.tick % 1000000000ULL;
	g_swDepPrngSeedSlot = (g_swDepPrngSeedSlot + 1) % 3;
	swPrngFips186(outRandom, xkey, (const unsigned char *)g_swDepPrngSeed);
}

/* compute SHA-1 digest via SDK libsha1 */
int swDepSha1Digest(const void *input, unsigned int size, unsigned char *digest)
{
	if (input != NULL && digest != NULL)
		return sceSha1Digest(input, size, digest);
	return -1;
}

/* compute HMAC-SHA1 via SDK libhmac, truncating if outLen < 20 */
int swDepHmacSha1(const void *input, unsigned int inputLen, const void *key,
		  unsigned int keyLen, unsigned char *outMac, size_t outLen)
{
	unsigned char *target = outMac;
	unsigned char fullMac[20];

	if (input == NULL || key == NULL || outMac == NULL)
		return -1;
	if (inputLen == 0 || keyLen == 0)
		return -1;
	if (outLen < sizeof(fullMac))
		target = fullMac;
	if (sceHmacSha1Digest(key, keyLen, input, inputLen, target) < 0)
		return -1;
	if (outLen < sizeof(fullMac))
		memcpy(outMac, fullMac, outLen);
	return 0;
}
