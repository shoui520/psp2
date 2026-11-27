/*
 * sw_wgn_core.c — Wagner command processor, parameter A/B loading and saving,
 * sink/source content device and AKE state machines, PCP header detection,
 * stream preparation/teardown, periodic timer tick, and base init/exit.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "sw_wgn_internal.h"

static void swCoreLockMtx(SceKernelLwMutexWork *mtx)
{
	sceKernelLockLwMutex(mtx, 1, NULL);
}

static void swCoreUnlockMtx(SceKernelLwMutexWork *mtx)
{
	sceKernelUnlockLwMutex(mtx, 1);
}

static int swCoreCreateMtx(SceKernelLwMutexWork *mtx)
{
	int ret = sceKernelCreateLwMutex(mtx, "sce_sws_mutex", 0x2000u, 0, NULL);

	return (ret >= 0) ? 0 : ret;
}

static void swCoreDeleteMtx(SceKernelLwMutexWork *mtx)
{
	sceKernelDeleteLwMutex(mtx);
}

static void swWgnTimerTick(void *arg);
static void swSrcInitSinkList(struct SwWgnCtx *ctx);
static void swSnkInitAll(struct SwWgnCtx *ctx);
static void swSrcInitAll(struct SwWgnCtx *ctx);
static void swStrmInitAll(struct SwWgnCtx *ctx);
static void swStrmClearEntry(struct SwWgnCtx *ctx, struct SwWgnStreamTbl *entry);

static void swWgnStartTimer(struct SwWgnCtx *ctx)
{
	swSrcInitSinkList(ctx);
	swTimerStart(&ctx->timer, 500, swWgnTimerTick, ctx);
}

static void swShmInitTable(struct SwShmTable *tbl)
{
	int i;

	memset(tbl, 0, sizeof(*tbl));
	for (i = 0; i < SW_MAX_SNK_CDEVS; i++)
		tbl->cdev[SW_MAX_SRC_CDEVS + i].expired = 1;
	for (i = 0; i < SW_MAX_SRC_CDEVS; i++)
		tbl->cdev[i].expired = 1;
}

static void swNvpClear(struct SwNvpTable *nvp)
{
	memset(nvp, 0, sizeof(*nvp));
}

struct SwWgnCtx *swWgnInit(struct SwInfo *swInfo)
{
	struct SwWgnCtx *ctx;
	struct SwShmTable *shmTbl = NULL;
	struct SwNvpTable *shmNvp = NULL;

	g_swWgnLogMask = 7u;
	ctx = (struct SwWgnCtx *)swWgnAllocTbl(swInfo, 0x30303030u, (int)sizeof(*ctx));
	if (ctx == NULL)
		return NULL;

	ctx->magic = 0x30303030u;
	ctx->swInfo = swInfo;

	if (swWgnSemGet(swInfo, "swgcdev", &ctx->semCdev) < 0) {
		swWgnFreeTbl(swInfo, ctx, (int)sizeof(*ctx));
		return NULL;
	}
	swCoreLockMtx(&ctx->semCdev);

	if (swWgnSemGet(swInfo, "swgstrm", &ctx->semStrm) < 0) {
		swWgnSemFree(swInfo, &ctx->semCdev);
		swWgnFreeTbl(swInfo, ctx, (int)sizeof(*ctx));
		return NULL;
	}
	if (swWgnSemGet(swInfo, "swgnsrm", &ctx->semSrm) < 0) {
		swWgnSemFree(swInfo, &ctx->semStrm);
		swWgnSemFree(swInfo, &ctx->semCdev);
		swWgnFreeTbl(swInfo, ctx, (int)sizeof(*ctx));
		return NULL;
	}

	shmTbl = (struct SwShmTable *)swWgnShmGet(swInfo, "swgntbl", sizeof(*shmTbl), 1, &ctx->shmTblHandle);
	if (shmTbl != NULL) {
		shmNvp = (struct SwNvpTable *)swWgnShmGet(swInfo, "swgnnvp", sizeof(*shmNvp), 1, &ctx->shmNvpHandle);
		if (shmNvp != NULL) {
			swShmInitTable(shmTbl);
			swNvpClear(shmNvp);
			ctx->isPrimary = 1;
		} else {
			swWgnShmFree(swInfo, &ctx->shmTblHandle, shmTbl, (int)sizeof(*shmTbl));
			shmTbl = NULL;
		}
	} else {
		shmTbl = (struct SwShmTable *)swWgnShmGet(swInfo, "swgntbl", sizeof(*shmTbl), 0, &ctx->shmTblHandle);
		if (shmTbl != NULL) {
			if (shmTbl->paramState == 4) {
				ctx->isPrimary = 0;
				shmNvp = (struct SwNvpTable *)swWgnShmGet(swInfo, "swgnnvp", sizeof(*shmNvp), 16, &ctx->shmNvpHandle);
			}
			if (shmNvp == NULL) {
				swWgnShmFree(swInfo, &ctx->shmTblHandle, shmTbl, (int)sizeof(*shmTbl));
				shmTbl = NULL;
			}
		}
	}

	if (shmTbl == NULL) {
		swMwTrace("sw: wgn_init: fail to alloc shm\n");
		swWgnSemFree(swInfo, &ctx->semSrm);
		swWgnSemFree(swInfo, &ctx->semStrm);
		swWgnSemFree(swInfo, &ctx->semCdev);
		swWgnFreeTbl(swInfo, ctx, (int)sizeof(*ctx));
		return NULL;
	}

	shmTbl->refCount++;
	swCoreUnlockMtx(&ctx->semCdev);

	ctx->shmTbl = shmTbl;
	ctx->shmNvp = shmNvp;
	swCoreCreateMtx(&ctx->randMutex);
	swCryptoSelfTestWrap();
	swStrmInitAll(ctx);
	swSnkInitAll(ctx);
	swSrcInitAll(ctx);
	swTimerInit(&ctx->timer, 0);
	if (ctx->isPrimary == 0) {
		memcpy(ctx->prngXkey, shmTbl->prngSeed, sizeof(ctx->prngXkey));
		swWgnStartTimer(ctx);
	}
	return ctx;
}

void swWgnExit(struct SwWgnCtx *ctx)
{
	int i;

	if (ctx == NULL)
		return;
	swTimerStop(&ctx->timer);
	for (i = 0; i < SW_MAX_STREAMS; i++)
		swStrmClearEntry(ctx, &ctx->strmTbl[i]);

	if (ctx->shmTbl != NULL) {
		if (ctx->shmTbl->refCount > 0)
			ctx->shmTbl->refCount--;
		if (ctx->shmTbl->refCount == 0) {
			swWgnSemFree(ctx->swInfo, &ctx->semSrm);
			swWgnSemFree(ctx->swInfo, &ctx->semStrm);
			swWgnSemFree(ctx->swInfo, &ctx->semCdev);
			swWgnShmFree(ctx->swInfo, &ctx->shmTblHandle, ctx->shmTbl, (int)sizeof(*ctx->shmTbl));
			swWgnShmFree(ctx->swInfo, &ctx->shmNvpHandle, ctx->shmNvp, (int)sizeof(*ctx->shmNvp));
		}
	}
	ctx->shmTbl = NULL;
	ctx->shmNvp = NULL;
	swCoreDeleteMtx(&ctx->randMutex);
	swCoreDeleteMtx(&ctx->snkMutex);
	swCoreDeleteMtx(&ctx->srcMutex);
	swWgnFreeTbl(ctx->swInfo, ctx, (int)sizeof(*ctx));
}

int swWgnReset(struct SwWgnCtx *ctx)
{
	int i;

	swTimerStop(&ctx->timer);
	for (i = 0; i < SW_MAX_STREAMS; i++)
		swStrmClearEntry(ctx, &ctx->strmTbl[i]);
	swCoreDeleteMtx(&ctx->snkMutex);
	swCoreDeleteMtx(&ctx->srcMutex);

	if (ctx->isPrimary != 0 && ctx->shmTbl->refCount == 1) {
		swShmInitTable(ctx->shmTbl);
		ctx->shmTbl->refCount = 1;
		swNvpClear(ctx->shmNvp);
	}
	swCryptoSelfTestWrap();
	swStrmInitAll(ctx);
	swSnkInitAll(ctx);
	swSrcInitAll(ctx);
	swTimerInit(&ctx->timer, 0);
	ctx->isPrimary = 1;
	return 1;
}

static int64_t swGetSecureTime(int64_t *outTime)
{
	int64_t now = swDepGetSecureTimeSec();

	*outTime = now;
	return now;
}

static uint32_t swCommonKeyRemainSec(const int64_t *startTimePtr)
{
	uint64_t validPeriod;
	int64_t nowSec;
	uint64_t elapsed;

	validPeriod = SwMwKeyBinGetDeviceId();
	swGetSecureTime(&nowSec);
	nowSec += SwMwKeyBinGetSeed();
	elapsed = swDepElapsedSec(nowSec, *startTimePtr);
	if (elapsed >= validPeriod)
		return 0u;
	return (uint32_t)(validPeriod - elapsed);
}

int swWgnIsCommonKeyExpired(struct SwWgnCtx *ctx)
{
	return ctx->shmTbl->commonKeyExpired;
}

static int swWgnCheckCommonKeyExpired(struct SwWgnCtx *ctx)
{
	struct SwNvpTable *nvp = ctx->shmNvp;
	struct SwShmTable *tbl = ctx->shmTbl;

	if ((nvp->commonKeyFlags & 1u) == 0)
		return 0;
	if (ctx->isPrimary != 0 && tbl->commonKeyExpired == 0) {
		uint32_t remain = swCommonKeyRemainSec(&nvp->startTime);

		if (remain < 60u) {
			tbl->commonKeyExpired = 1;
			if ((g_swWgnLogMask & 4u) != 0)
				swMwTrace("sw: commonkey is expired: remain time=%d(s)\n", (int)remain);
		}
	}
	return tbl->commonKeyExpired;
}

void swWgnGenRandom20(struct SwWgnCtx *ctx, unsigned char *out20)
{
	swCoreLockMtx(&ctx->randMutex);
	swDepGetRandom(ctx->prngXkey, out20);
	swCoreUnlockMtx(&ctx->randMutex);
}

static int swLoadParamA(struct SwWgnCtx *ctx, unsigned char *buf, uint32_t len)
{
	struct SwNvpTable *nvp = ctx->shmNvp;
	struct SwShmTable *tbl = ctx->shmTbl;
	uint16_t blkType;
	uint16_t blkLen;
	uint32_t remain;
	unsigned char *cur;
	unsigned char aesKey[20];
	unsigned char hmacKey[20];
	unsigned char randBuf[20];

	memset(hmacKey, 0, sizeof(hmacKey));

	memcpy(&blkLen, buf + 2, sizeof(blkLen));
	if (blkLen < 0x20u) {
		swMwTrace("sw: loadParamA: ERROR, invalid block length %d\n", blkLen);
		return 2;
	}
	if (len < (uint32_t)blkLen + 4u) {
		swMwTrace("sw: loadParamA: ERROR, invalid param length %d\n", blkLen);
		return 2;
	}
	remain = len - blkLen - 4u;
	cur = buf + blkLen + 4u;

	memcpy(nvp->keyId, buf + 4, 16);
	SwMwReadKeyBinExtra(buf + 4, 16, aesKey, 16);
	memcpy(&tbl->ownerId, buf + 20, sizeof(tbl->ownerId));
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: loadParamA ownerid 0x%04x\n", tbl->ownerId);

	memcpy(&nvp->version, buf + 22, sizeof(nvp->version));
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: loadParamA: version 0x%04x\n", nvp->version);
	if (nvp->version > 0u) {
		swMwTrace("sw: loadParamA: ERROR, f/w version is old %04x\n", 0);
		return 2;
	}
	if (nvp->version != 0u) {
		uint16_t zeroVer = 0;

		swMwTrace("sw: loadParamA: new version=%04x\n", 0);
		memcpy(buf + 22, &zeroVer, sizeof(zeroVer));
		swDepHmacSha1(buf + 4, 20, hmacKey, 20, buf + 24, 12);
	}
	memcpy(nvp->meta, buf + 24, 12);

	memcpy(&blkType, cur, sizeof(blkType));
	memcpy(&blkLen, cur + 2, sizeof(blkLen));
	if (remain < (uint32_t)blkLen + 4u) {
		swMwTrace("sw: loadParamA: ERROR, invalid param length %d\n", blkLen);
		return 2;
	}
	remain -= blkLen + 4u;

	if (blkType == 0x0701u) {
		if (blkLen < 0x30u) {
			swMwTrace("sw: loadParamA: ERROR, invalid block length %d\n", blkLen);
			return 2;
		}
	} else if (blkType == 0x0702u) {
		uint32_t remainSec;

		if (blkLen < 0x38u) {
			swMwTrace("sw: loadParamA: ERROR, invalid block length %d\n", blkLen);
			return 2;
		}
		nvp->commonKeyFlags = cur[36];
		memcpy(&nvp->keyPeriod, cur + 37, sizeof(nvp->keyPeriod));
		if (cur[39] == 0) {
			int64_t nowSec;

			cur[39] = 1;
			swGetSecureTime(&nowSec);
			memcpy(cur + 40, &nowSec, sizeof(nowSec));
			swDepHmacSha1(cur + 4, 44, hmacKey, 20, cur + 48, 12);
		}
		memcpy(&nvp->startTime, cur + 40, sizeof(nvp->startTime));
		remainSec = swCommonKeyRemainSec(&nvp->startTime);
		if (remainSec < 60u) {
			swMwTrace("sw: loadParamA: ERROR, key expired. remain time=%d(s)\n", (int)remainSec);
			return 27;
		}
	} else {
		swMwTrace("sw: loadParamA: ERROR, invalid param type %04x\n", blkType);
		return 2;
	}
	memcpy(nvp->privKeyEnc, cur + 4, 32);

	cur += blkLen + 4u;
	memcpy(&blkLen, cur + 2, sizeof(blkLen));
	if (blkLen < 0x58u) {
		swMwTrace("sw: loadParamA: ERROR, invalid cert length %d\n", blkLen);
		return 2;
	}
	if (remain < (uint32_t)blkLen + 4u) {
		swMwTrace("sw: loadParamA: ERROR, invalid param length %d\n", blkLen);
		return 2;
	}
	remain -= blkLen + 4u;
	nvp->certLen = blkLen;
	memcpy(nvp->cert, cur + 4, blkLen);
	if ((g_swWgnLogMask & 4u) != 0)
		swWgnDumpHex("loadParamA: dev cert\n", nvp->cert, blkLen);

	cur += blkLen + 4u;
	memcpy(&blkType, cur, sizeof(blkType));
	memcpy(&blkLen, cur + 2, sizeof(blkLen));
	if (remain < (uint32_t)blkLen + 4u) {
		swMwTrace("sw: loadParamA: ERROR, invalid param length %d\n", blkLen);
		return 2;
	}

	if (blkType == 0x0704u) {
		if (blkLen < 0x2Cu) {
			swMwTrace("sw: loadParamA: ERROR, invalid block length %d\n", blkLen);
			return 2;
		}
		SwMwReadKeyBinExtra(nvp->keyId, 16, aesKey, 16);
		swDepAes128Ecb(cur + 4, 32, ctx->prngXkey, 32, aesKey, 1);
		memset(ctx->prngTail, 0, sizeof(ctx->prngTail));
		swWgnGenRandom20(ctx, randBuf);
		memcpy(tbl->prngSeed, ctx->prngXkey, sizeof(ctx->prngXkey));
		swDepAes128Ecb(ctx->prngXkey, 32, cur + 4, 32, aesKey, 0);
		swDepHmacSha1(cur + 4, 32, hmacKey, 20, cur + 36, 12);
		memcpy(aesKey, randBuf, sizeof(aesKey));
	} else if (blkType == 0x0705u) {
		if (blkLen < 0x34u) {
			swMwTrace("sw: loadParamA: ERROR, invalid block length %d\n", blkLen);
			return 2;
		}
		SwMwReadKeyBinExtra(nvp->keyId, 16, aesKey, 16);
		swDepAes128Ecb(cur + 12, 32, ctx->prngXkey, 32, aesKey, 1);
		memset(ctx->prngTail, 0, sizeof(ctx->prngTail));
		swWgnGenRandom20(ctx, randBuf);
		if (cur[4] == 0) {
			cur[4] = 1;
			if (swGetDeviceIduWrap(ctx, randBuf) < 0) {
				swMwTrace("sw: loadParamA: ERROR, get_idu\n");
				memcpy(aesKey, randBuf, sizeof(aesKey));
				return 2;
			}
			memcpy(cur + 7, randBuf, 5);
		}
		memcpy(nvp->idu, cur + 7, 5);
		memcpy(tbl->prngSeed, ctx->prngXkey, sizeof(ctx->prngXkey));
		swDepAes128Ecb(ctx->prngXkey, 32, cur + 12, 32, aesKey, 0);
		swDepHmacSha1(cur + 4, 40, hmacKey, 20, cur + 44, 12);
		memcpy(aesKey, randBuf, sizeof(aesKey));
	} else {
		swMwTrace("sw: loadParamA: ERROR, invalid param type %04x\n", blkType);
		return 2;
	}
	return 0;
}

static int swLoadParamB(struct SwWgnCtx *ctx, unsigned char *buf, uint32_t len)
{
	struct SwNvpTable *nvp = ctx->shmNvp;
	struct SwShmTable *tbl = ctx->shmTbl;
	uint16_t blkLen;
	uint32_t remain;
	unsigned char *cur;
	unsigned char aesKey[20];
	unsigned char randBuf[20];

	memcpy(&blkLen, buf + 2, sizeof(blkLen));
	if (blkLen < 0x40u) {
		swMwTrace("sw: loadParamB: ERROR, invalid block length %d\n", blkLen);
		return 2;
	}
	if (len < (uint32_t)blkLen + 4u) {
		swMwTrace("sw: loadParamB: ERROR, invalid param length %d\n", blkLen);
		return 2;
	}
	remain = len - blkLen - 4u;
	cur = buf + blkLen + 4u;

	SwMwReadKeyBinExtra(nvp->keyId, 16, aesKey, 16);
	if (buf[52] > 34u) {
		swMwTrace("sw: loadParamB: ERROR, sink limit=%d\n", buf[52]);
		return 2;
	}
	nvp->sinkLimit = buf[52];
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: loadParamB: sink_limit %d\n", nvp->sinkLimit);

	swWgnGenRandom20(ctx, randBuf);
	nvp->kxLabel = (buf[54] != 0) ? buf[55] : randBuf[10];
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: loadParamB: kx_label %02x\n", nvp->kxLabel);

	swDepAes128Ecb(buf + 4, 48, nvp->dtlaPubKey, 48, aesKey, 1);
	memcpy(aesKey, randBuf, sizeof(aesKey));

	if (swDepEccEcdsaVerify(nvp->cert, nvp->certLen - 40u,
				nvp->cert + nvp->certLen - 40u, nvp->dtlaPubKey) != 0) {
		swMwTrace("sw: loadParamB: ERROR, verify dev cert\n");
		return 2;
	}
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: loadParamB: verify dev cert ok\n");

	memcpy(&blkLen, cur + 2, sizeof(blkLen));
	if (remain < (uint32_t)blkLen + 4u) {
		swMwTrace("sw: loadParamB: ERROR, invalid param length %d\n", blkLen);
		return 2;
	}
	if (blkLen > SW_MAX_SRM_SIZE ||
	    (blkLen != 0 && swVerifySrmSignature(cur + 4, blkLen, nvp->dtlaPubKey) != 0)) {
		swMwTrace("sw: loadParamB: ERROR, srm (size=%d)\n", blkLen);
		return 2;
	}

	tbl->srmSize = blkLen;
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: loadParamB: srm ok (size=%d)\n", blkLen);
	if (blkLen != 0) {
		memcpy(tbl->srm, cur + 4, blkLen);
		if ((g_swWgnLogMask & 4u) != 0)
			swWgnDumpHex("", tbl->srm, blkLen);
	}
	return 0;
}

static int swCmdLoadWgnParam(struct SwWgnCtx *ctx, unsigned char *buf, uint32_t len)
{
	struct SwShmTable *tbl = ctx->shmTbl;
	uint32_t headerWord;
	uint16_t type;
	int ret;

	if (len == 0) {
		swMwTrace("sw: lod_wgn_parm: command buffer length is 0\n");
		return 2;
	}
	memcpy(&headerWord, buf, sizeof(headerWord));
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: LOD_WGN_PARAM: type=%d buff=%d\n", (int)headerWord, (int)len);

	type = (uint16_t)headerWord;
	if (type == 0x0700u) {
		if (tbl->paramState != 0) {
			swMwTrace("sw: loadParamA: ERROR, invalid status\n");
			return 1;
		}
		ret = swLoadParamA(ctx, buf, len);
		if (ret == 0)
			tbl->paramState = 2;
		return ret;
	}
	if (type == 0x0800u) {
		if (tbl->paramState != 2) {
			swMwTrace("sw: loadParamB: ERROR, invalid status\n");
			return 1;
		}
		ret = swLoadParamB(ctx, buf, len);
		if (ret == 0) {
			tbl->paramState = 4;
			swWgnStartTimer(ctx);
		}
		return ret;
	}

	swMwTrace("sw: loadParam: ERROR, unknown parameter type\n");
	return 2;
}

static int swCmdSaveWgnParam(struct SwWgnCtx *ctx, unsigned char *buf, uint32_t len)
{
	struct SwNvpTable *nvp = ctx->shmNvp;
	struct SwShmTable *tbl = ctx->shmTbl;
	uint32_t saveType;
	uint16_t tag;
	uint16_t fieldLen;
	uint32_t srmLen;
	uint32_t totalLen;
	unsigned char aesKey[20];
	unsigned char hmacKey[20];

	memset(hmacKey, 0, sizeof(hmacKey));
	memcpy(&saveType, buf, sizeof(saveType));
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: SAV_WGN_PARAM: type=%d buff=%d\n", (int)saveType, (int)len);

	if (saveType != 8u)
		return 2;
	if (len < 0x850u) {
		swMwTrace("sw: SAV_PARAMB: insufficient buffer\n");
		return 1;
	}

	SwMwReadKeyBinExtra(nvp->keyId, 16, aesKey, 16);
	tag = 0x0800u;
	fieldLen = 64u;
	memcpy(buf + 8, &tag, sizeof(tag));
	memcpy(buf + 10, &fieldLen, sizeof(fieldLen));
	swDepAes128Ecb(nvp->dtlaPubKey, 48, buf + 12, 48, aesKey, 0);
	memcpy(aesKey, ctx->prngXkey, sizeof(aesKey));
	buf[60] = nvp->sinkLimit;
	buf[61] = 0;
	buf[62] = 1;
	buf[63] = nvp->kxLabel;
	swDepHmacSha1(buf + 12, 52, hmacKey, 20, buf + 64, 12);

	tag = 0x0801u;
	memcpy(buf + 76, &tag, sizeof(tag));
	swCoreLockMtx(&ctx->semSrm);
	srmLen = tbl->srmSize;
	if (srmLen != 0) {
		if (srmLen > SW_MAX_SRM_SIZE)
			srmLen = 0;
		else
			memcpy(buf + 80, tbl->srm, srmLen);
	}
	swCoreUnlockMtx(&ctx->semSrm);
	fieldLen = (uint16_t)srmLen;
	memcpy(buf + 78, &fieldLen, sizeof(fieldLen));
	totalLen = srmLen + 72u;
	memcpy(buf + 4, &totalLen, sizeof(totalLen));
	return 0;
}

static int swCmdGetCommonKeyInfo(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwNvpTable *nvp = ctx->shmNvp;

	arg[2] = 0;
	arg[4] = nvp->commonKeyFlags;
	if ((nvp->commonKeyFlags & 1u) != 0) {
		uint32_t remain = swCommonKeyRemainSec(&nvp->startTime);

		memcpy(arg + 8, nvp->idu, 5);
		memcpy(arg + 16, &remain, sizeof(remain));
		if (remain < 60u) {
			arg[2] = 27;
			ctx->shmTbl->commonKeyExpired = 1;
		}
		if ((g_swWgnLogMask & 4u) != 0) {
			swMwTrace("sw: GET_COMMONKEY_INFO: %d flags=%02x idu=%02x%02x%02x%02x%02x remain_time=%d\n",
				  arg[2], arg[4], arg[8], arg[9], arg[10], arg[11], arg[12], (int)remain);
		}
	} else {
		memset(arg + 8, 0, 5);
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: GET_COMMONKEY_INFO: flags=%02x\n", arg[4]);
	}
	return 0;
}

/* ---- Sink content-device & AKE state machine ---- */

static void swSnkResetCdevFields(struct SwSnkCdev *cdev)
{
	cdev->shm = NULL;
	cdev->alarmTime = 0;
	cdev->akeId = 0;
	cdev->kxLabel = 0;
	memset(cdev->ksx, 0, sizeof(cdev->ksx));
	memset(cdev->kauth, 0, sizeof(cdev->kauth));
}

struct SwSnkCdev *swSnkGetCdev(struct SwWgnCtx *ctx, int cdevId)
{
	if (cdevId >= SW_MAX_SRC_CDEVS && cdevId < SW_MAX_CDEVS) {
		struct SwSnkCdev *cdev = &ctx->snkCdev[cdevId - SW_MAX_SRC_CDEVS];

		if (cdev->shm != NULL)
			return cdev;
	}
	return NULL;
}

void swSnkFreeCdev(struct SwWgnCtx *ctx, struct SwSnkCdev *cdev)
{
	if (cdev->shm == NULL) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: snk_free_cdev: not allocated\n");
		return;
	}
	swCoreLockMtx(&ctx->snkMutex);
	swListRemoveNode(&cdev->link);
	swCoreLockMtx(&ctx->semCdev);
	memset(cdev->shm, 0, sizeof(*cdev->shm));
	cdev->shm->expired = 1;
	swCoreUnlockMtx(&ctx->semCdev);
	swSnkResetCdevFields(cdev);
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: snk_free_cdev: kx discarded. cdev_id=%d\n", cdev->cdevId);
	swListAddTail(&cdev->link, &ctx->snkFreeList);
	swCoreUnlockMtx(&ctx->snkMutex);
}

static void swSnkInitAll(struct SwWgnCtx *ctx)
{
	int i;

	ctx->snkAkeSeq = 0;
	swCoreCreateMtx(&ctx->snkMutex);
	ctx->snkUsedList.next = &ctx->snkUsedList;
	ctx->snkUsedList.prev = &ctx->snkUsedList;
	ctx->snkFreeList.next = &ctx->snkFreeList;
	ctx->snkFreeList.prev = &ctx->snkFreeList;

	for (i = 0; i < SW_MAX_SNK_CDEVS; i++) {
		struct SwSnkCdev *cdev = &ctx->snkCdev[i];

		cdev->cdevId = (unsigned char)(SW_MAX_SRC_CDEVS + i);
		swSnkResetCdevFields(cdev);
		swListAddTail(&cdev->link, &ctx->snkFreeList);
	}
}

void swSnkExpireKx(struct SwSnkCdev *cdev)
{
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: snk_expire_kx: kx discarded. cdev_id=%d\n", cdev->cdevId);
	cdev->shm->expired = 1;
	cdev->shm->alarm = 0;
	cdev->shm->lifetime = 0;
	cdev->shm->kxLabel = 0;
	memset(cdev->shm->kx, 0, sizeof(cdev->shm->kx));
}

static void swSnkTimerTick(struct SwWgnCtx *ctx)
{
	struct SwListNode *node;

	swCoreLockMtx(&ctx->snkMutex);
	for (node = ctx->snkUsedList.next; node != &ctx->snkUsedList; node = node->next) {
		struct SwSnkCdev *cdev = (struct SwSnkCdev *)node;

		if (cdev->shm->streamRef != 0) {
			cdev->shm->streamRef = 0;
			swCoreLockMtx(&ctx->semCdev);
			cdev->shm->lifetime = 119;
			swCoreUnlockMtx(&ctx->semCdev);
		} else if (cdev->shm->lifetime != 0) {
			unsigned char rem;

			swCoreLockMtx(&ctx->semCdev);
			rem = (unsigned char)(cdev->shm->lifetime - 1u);
			cdev->shm->lifetime = rem;
			if (rem == 0)
				swSnkExpireKx(cdev);
			swCoreUnlockMtx(&ctx->semCdev);
			if ((int)rem == cdev->alarmTime)
				cdev->shm->alarm = 1;
		}
	}
	swCoreUnlockMtx(&ctx->snkMutex);
}

static int swCmdGenSnkCdev(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSnkCdev *cdev = NULL;

	if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: GEN_SNK_CDEV: fmt_cognizant=%d, kx_alarmtime=%d move=%d\n",
			  arg[3], arg[24], arg[25]);
	}
	swCoreLockMtx(&ctx->snkMutex);
	if (arg[25] == 0) {
		struct SwListNode *node;

		if ((g_swWgnLogMask & 4u) != 0)
			swWgnDumpHex("GEN_SNK_CDEV: auth_loc_id ", arg + 4, 20);
		for (node = ctx->snkUsedList.next; node != &ctx->snkUsedList; node = node->next) {
			struct SwSnkCdev *existing = (struct SwSnkCdev *)node;

			if (existing->isMove == 0 && memcmp(existing->authLocId, arg + 4, 20) == 0) {
				arg[0] = existing->cdevId;
				arg[2] = 0;
				swMwTrace("sw:  cdev_id=%d\n", arg[0]);
				swCoreUnlockMtx(&ctx->snkMutex);
				return 0;
			}
		}
	}

	if (ctx->snkFreeList.next != &ctx->snkFreeList) {
		struct SwListNode *node = ctx->snkFreeList.next;

		swListRemoveNode(node);
		cdev = (struct SwSnkCdev *)node;
		cdev->shm = &ctx->shmTbl->cdev[cdev->cdevId];
		swListAddTail(&cdev->link, &ctx->snkUsedList);
	}
	swCoreUnlockMtx(&ctx->snkMutex);

	if (cdev == NULL) {
		arg[2] = 3;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: GEN_SNK_CDEV: ERROR, resource full.\n");
		return 0;
	}

	swCoreLockMtx(&ctx->semCdev);
	memcpy(cdev->authLocId, arg + 4, 20);
	cdev->alarmTime = (arg[24] >= 118u) ? 118 : (int)arg[24];
	cdev->isMove = (arg[25] != 0) ? 1 : 0;
	cdev->shm->cognizant = (arg[3] != 0) ? 1u : 0u;
	swCoreUnlockMtx(&ctx->semCdev);

	arg[0] = cdev->cdevId;
	arg[2] = 0;
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GEN_SNK_CDEV: success: cdev_id=%d\n", arg[0]);
	return 0;
}

static int swCmdSttSnkAke(struct SwWgnCtx *ctx, unsigned char *arg)
{
	unsigned char cdevId = arg[0];
	struct SwSnkCdev *cdev;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: STT_SNK_AKE: cdev_id=%d\n", cdevId);
	cdev = swSnkGetCdev(ctx, cdevId);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: snk_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	if (cdev->akeState != 0) {
		arg[2] = 14;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: ake stat: ERROR, already other ake-process started.\n");
		return 0;
	}
	cdev->akeState = 1;
	if (cdev->isMove != 0)
		cdev->akeId = cdevId;
	else
		cdev->akeId = ++ctx->snkAkeSeq;
	arg[1] = cdev->akeId;
	arg[2] = 0;
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: STT_SNK_AKE: success ake_id=%d\n", arg[1]);
	return 0;
}

static int swCmdGenSnkKauth(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSnkCdev *cdev;
	unsigned char sharedX[40];

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GEN_SNK_KAUTH: cdev_id=%d ake_id=%d\n", arg[0], arg[1]);
	cdev = swSnkGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: snk_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	if (cdev->akeState == 0) {
		arg[2] = 15;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: GEN_SNK_KAUTH: ERROR, ake-stat is invalid\n");
		return 0;
	}
	if (cdev->akeId != arg[1]) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: GEN_SNK_KAUTH: ERROR, ake_id is not in agreement. cdev->ake_id=%d\n", cdev->akeId);
		return 0;
	}
	if (swDepEccEcdhPhase2(sharedX, arg + 4, arg + 24) != 0) {
		arg[2] = 1;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: gen_ecdh_parm: ERROR, creating of a ec-dh-phase2 failed.\n");
		return 0;
	}
	memcpy(cdev->kauth, sharedX + 8, 12);
	arg[2] = 0;
	return 0;
}

static int swCmdSetSnkKsx(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSnkCdev *cdev;

	if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: SET_SNK_KSX: cdev_id=%d ake_id=%d cipher_algo=0x%x kx_label=0x%x\n",
			  arg[0], arg[1], arg[17], arg[16]);
	}
	cdev = swSnkGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: snk_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	if (cdev->akeState == 0) {
		arg[2] = 15;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: SET_SNK_KSX: ERROR, ake-stat is invalid\n");
		return 0;
	}
	if (cdev->akeId != arg[1]) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: SET_SNK_KSX: ERROR, ake_id is not in agreement. cdev->ake_id=%d\n", cdev->akeId);
		return 0;
	}
	memcpy(cdev->ksx, arg + 4, 12);
	cdev->kxLabel = arg[16];
	arg[2] = 0;
	return 0;
}

static int swCmdFinSnkAke(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSnkCdev *cdev;
	unsigned char doubleKauth[24];
	unsigned char digest[20];
	const unsigned char *mask;
	int i;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: FIN_SNK_AKE: cdev_id=%d ake_id=%d\n", arg[0], arg[1]);
	cdev = swSnkGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: snk_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	if (cdev->akeState == 0) {
		arg[2] = 15;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: FIN_SNK_AKE: ERROR, ake-stat is invalid\n");
		return 0;
	}
	if (cdev->akeId != arg[1]) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: FIN_SNK_AKE: ERROR, ake_id is not in agreement. cdev->ake_id=%d\n", cdev->akeId);
		return 0;
	}
	if (cdev->isMove != 0) {
		memcpy(doubleKauth, cdev->kauth, 12);
		memcpy(doubleKauth + 12, cdev->kauth, 12);
		swDepSha1Digest(doubleKauth, 24, digest);
		memset(doubleKauth, 0, sizeof(doubleKauth));
		mask = digest + 8;
	} else {
		mask = cdev->kauth;
	}
	swCoreLockMtx(&ctx->semCdev);
	for (i = 0; i < 12; i++)
		cdev->shm->kx[i] = cdev->ksx[i] ^ mask[i];
	cdev->shm->kxLabel = cdev->kxLabel;
	cdev->shm->cipherAlgo = 1;
	cdev->shm->lifetime = 119;
	cdev->shm->expired = 0;
	cdev->shm->alarm = 0;
	cdev->akeState = 0;
	swCoreUnlockMtx(&ctx->semCdev);
	arg[2] = 0;
	return 0;
}

static int swCmdCclSnkAke(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSnkCdev *cdev;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: CCL_SNK_AKE: cdev_id=%d ake_id=%d\n", arg[0], arg[1]);
	cdev = swSnkGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: snk_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	if (cdev->akeState == 0) {
		arg[2] = 15;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: CCL_SNK_AKE: ERROR, ake-stat is invalid\n");
		return 0;
	}
	if (cdev->akeId != arg[1]) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: CCL_SNK_AKE: ERROR, ake_id is not in agreement. cdev->ake_id=%d\n", cdev->akeId);
		return 0;
	}
	cdev->akeState = 0;
	arg[2] = 0;
	return 0;
}

static int swCmdChkSnkKx(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSnkCdev *cdev;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: CHK_SNK_KX: cdev_id=%d kx_label=0x%x\n", arg[0], arg[4]);
	cdev = swSnkGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: snk_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	if (cdev->shm->kxLabel == arg[4] && cdev->shm->expired == 0) {
		arg[2] = 0;
		return 0;
	}
	arg[2] = 2;
	if ((g_swWgnLogMask & 1u) != 0)
		swMwTrace("sw: CHK_SNK_KX: ERROR, kx_label(0x%x) is different or kx expired\n", cdev->shm->kxLabel);
	return 0;
}

static int swCmdChkSnkAuth(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSnkCdev *cdev;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: CHK_SNK_AUTH: cdev_id=%d\n", arg[0]);
	cdev = swSnkGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: snk_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	if (cdev->shm->expired != 0) {
		arg[2] = 1;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: CHK_SNK_AUTH: ERROR, ake-process is not ended.\n");
	} else {
		arg[2] = 0;
	}
	return 0;
}

static int swCmdGetSnkKxLifetime(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSnkCdev *cdev = swSnkGetCdev(ctx, arg[0]);

	if (cdev != NULL) {
		arg[2] = cdev->shm->lifetime;
		arg[1] = 0;
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: GET_SNK_KXLIFETIME: cdev_id=%d, kx_lifetime=%d\n", arg[0], arg[2]);
	} else {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: GET_SNK_KXLIFETIME: ERROR, invalid cdev_id=%d\n", arg[0]);
		arg[1] = 2;
	}
	return 0;
}

void swSnkPcpDetect(struct SwWgnCtx *ctx, int streamId, const unsigned char *pcpHdr,
		    uint32_t contentLen)
{
	unsigned char sid = (unsigned char)streamId;
	struct SwWgnStreamTbl *st = &ctx->strmTbl[sid];
	struct SwShmCdev *shm = &ctx->shmTbl->cdev[st->cdevId];
	unsigned char caEemi = pcpHdr[0];
	unsigned char kxLabel = pcpHdr[1];
	const unsigned char *nc = pcpHdr + 2;
	unsigned char eemi = caEemi & 0x0Fu;
	uint32_t errBits = 0;
	int keyStatus = 0;
	void *keyPtr = NULL;
	void *ivPtr = NULL;

	if (st->expired != shm->expired) {
		if (st->expired != 0) {
			swKmResetKx(st->keyPool, shm->kx);
			st->kxLabel = shm->kxLabel;
			st->expired = 0;
		} else {
			st->expired = 1;
			st->kxLabel = 0;
			swKmResetKx(st->keyPool, NULL);
		}
	}

	switch (eemi) {
	case 0:
		if (contentLen > 0x08000000u)
			errBits = 0x10000u;
		break;
	case 2:
	case 4:
	case 6:
	case 8:
	case 10:
	case 12:
		if ((caEemi & 0x10u) != 0)
			errBits = 0x4000u;
		else if (st->expired != 0)
			errBits = 0x40000u;
		else if (kxLabel != st->kxLabel)
			errBits = 0x8000u;
		else if (contentLen > 0x08000000u)
			errBits = 0x10000u;
		break;
	default:
		errBits = 0x2000u;
		break;
	}

	if (errBits != 0) {
		eemi = 0;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: set_error: ERROR, stream_id=%d, err=%08x\n", sid, errBits);
		swSetStreamStatus(ctx->swInfo, sid, errBits);
		if ((g_swWgnLogMask & 1u) != 0) {
			swMwTrace("sw: pcp_detect: ERROR, cannot decrypto PCP. cause=0x%x ca_e_emi=0x%x kx_label=0x%x nc=%02x%02x....%02x%02x cl=0x%x\n",
				  errBits, caEemi, kxLabel, nc[0], nc[1], nc[6], nc[7], contentLen);
		}
	} else {
		if ((g_swWgnLogMask & 0x100u) != 0)
			swMwTrace("sw: pcp_detect: %d E-EMI=0x%x\n", sid, eemi);
		keyStatus = swKmGetThisKey(st->keyPool, eemi, nc, &keyPtr, &ivPtr);
		if (keyStatus == 5 &&
		    ((st->ncInitMode != 1 && eemi != 0) ||
		     (st->ncInitMode == 1 && memcmp(st->initNc, nc, 8) != 0))) {
			if ((g_swWgnLogMask & 4u) != 0)
				swMwTrace("sw: set_status: stream_id=%d, sts=%08x\n", sid, 0x200000u);
			swSetStreamStatus(ctx->swInfo, sid, 0x200000u);
		}
	}

	if (eemi != 0) {
		shm->streamRef = 1;
		if (keyStatus == 3) {
			if ((g_swWgnLogMask & 1u) != 0)
				swMwTrace("sw: set_error: ERROR, stream_id=%d, err=%08x\n", sid, 0x100000u);
			swSetStreamStatus(ctx->swInfo, sid, 0x100000u);
		} else if (keyStatus == 4 && st->ncInitMode != 1) {
			if ((g_swWgnLogMask & 4u) != 0)
				swMwTrace("sw: set_status: stream_id=%d, sts=%08x\n", sid, 0x200000u);
			swSetStreamStatus(ctx->swInfo, sid, 0x200000u);
		}
		if (contentLen != 0)
			swSendKeyChange(ctx->swInfo, sid, contentLen, keyPtr, ivPtr, pcpHdr);
	} else if (contentLen != 0) {
		swSendKeyChange(ctx->swInfo, sid, contentLen, NULL, NULL, pcpHdr);
	}

	if (st->cognizant == 0)
		swSetCciBasic(ctx->swInfo, sid, 0xFFu, eemi);
}

static int swCmdGenSnkCkc(struct SwWgnCtx *ctx, unsigned char *arg)
{
	unsigned char sid = arg[0];
	unsigned char kx[12];
	unsigned char mac[20];

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GEN_SNK_CKC: stream_id=%d\n", sid);
	if (sid != 0) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: GEN_SNK_CKC: ERROR, invalid stream_id\n");
		arg[1] = 2;
		return 0;
	}
	swKmGetKx(ctx->strmTbl[0].keyPool, kx);
	swKmGetCurrentNc(ctx->strmTbl[0].keyPool, arg + 12);
	swComputeCkcMac(kx, arg + 12, arg + 4, mac);
	memcpy(arg + 20, mac, 10);
	memcpy(kx, mac, 12);
	memset(mac, 0, sizeof(mac));
	arg[1] = 0;
	return 0;
}

static int swCmdChkSnkCkc(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSnkCdev *cdev;
	unsigned char mac[20];

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: CHK_SNK_CKC: cdev_id=%d\n", arg[0]);
	cdev = swSnkGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[1] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: CHK_SNK_CKC: ERROR, invalid cdev_id\n");
		return 0;
	}
	swComputeCkcMac(cdev->shm->kx, arg + 12, arg + 4, mac);
	arg[1] = (memcmp(mac + 10, arg + 20, 10) != 0) ? 1u : 0u;
	memset(mac, 0, sizeof(mac));
	return 0;
}

/* ---- Source content-device & AKE state machine ---- */

static void swSrcResetSink(struct SwSrcSinkEntry *sink, unsigned char akeId)
{
	memset(sink, 0, sizeof(*sink));
	sink->akeId = akeId;
}

static struct SwSrcSinkEntry *swSrcAllocSink(struct SwWgnCtx *ctx)
{
	struct SwSrcSinkEntry *sink = NULL;

	swCoreLockMtx(&ctx->srcMutex);
	if (ctx->srcSinkFreeList.next != &ctx->srcSinkFreeList) {
		struct SwListNode *node = ctx->srcSinkFreeList.next;

		swListRemoveNode(node);
		sink = (struct SwSrcSinkEntry *)node;
	}
	swCoreUnlockMtx(&ctx->srcMutex);
	return sink;
}

struct SwSrcSinkEntry *swSrcGetSink(struct SwWgnCtx *ctx, unsigned char akeId)
{
	if (akeId >= SW_MAX_SINKS)
		return NULL;
	return &ctx->srcSinks[akeId];
}

static void swSrcFreeSinkLocked(struct SwWgnCtx *ctx, struct SwSrcSinkEntry *sink)
{
	unsigned char akeId = sink->akeId;

	swListRemoveNode(&sink->link);
	swSrcResetSink(sink, akeId);
	swListAddTail(&sink->link, &ctx->srcSinkFreeList);
}

static void swSrcResetCdevFields(struct SwSrcCdev *cdev)
{
	cdev->shm = NULL;
	cdev->sinkCount = 0;
}

struct SwSrcCdev *swSrcGetCdev(struct SwWgnCtx *ctx, int cdevId)
{
	if (cdevId >= 0 && cdevId < SW_MAX_SRC_CDEVS) {
		struct SwSrcCdev *cdev = &ctx->srcCdev[cdevId];

		if (cdev->shm != NULL)
			return cdev;
	}
	return NULL;
}

static void swSrcAttachSink(struct SwWgnCtx *ctx, struct SwSrcCdev *cdev, struct SwSrcSinkEntry *sink)
{
	swCoreLockMtx(&ctx->srcMutex);
	swListAddTail(&sink->link, &ctx->srcSinkUsedList);
	swCoreUnlockMtx(&ctx->srcMutex);
	cdev->sinkCount++;
}

static struct SwSrcCdev *swSrcAllocMoveCdev(struct SwWgnCtx *ctx)
{
	struct SwSrcCdev *cdev = NULL;

	swCoreLockMtx(&ctx->srcMutex);
	if (ctx->srcFreeList.next != &ctx->srcFreeList) {
		struct SwListNode *node = ctx->srcFreeList.next;

		swListRemoveNode(node);
		swListAddTail(node, &ctx->srcUsedList);
		cdev = (struct SwSrcCdev *)node;
		cdev->shm = &ctx->shmTbl->cdev[cdev->cdevId];
		cdev->shm->lifetime = 119;
		cdev->shm->expired = 0;
		ctx->srcActiveCount++;
	}
	swCoreUnlockMtx(&ctx->srcMutex);
	return cdev;
}

void swSrcFreeCdev(struct SwWgnCtx *ctx, struct SwSrcCdev *cdev)
{
	if (cdev->shm == NULL) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: src_free_cdev: not allocated\n");
		return;
	}
	if (cdev->shm->streamRef > 0)
		swMwTrace("sw: src_free_cdev: WARN, cdev is still used by a stream.\n");

	swCoreLockMtx(&ctx->srcMutex);
	if (cdev->isMove != 0) {
		swListRemoveNode(&cdev->link);
		ctx->kxmLabels[cdev->shm->kxLabel] = 1;
		swListAddTail(&cdev->link, &ctx->srcFreeList);
	}
	if (cdev->shm->expired == 0) {
		ctx->srcActiveCount--;
		if (ctx->srcActiveCount == 0) {
			while (ctx->srcSinkUsedList.next != &ctx->srcSinkUsedList)
				swSrcFreeSinkLocked(ctx, (struct SwSrcSinkEntry *)ctx->srcSinkUsedList.next);
			if ((g_swWgnLogMask & 4u) != 0)
				swMwTrace("sw: src_free_cdev: reset sink counter\n");
		}
	}
	swCoreLockMtx(&ctx->semCdev);
	memset(cdev->shm, 0, sizeof(*cdev->shm));
	cdev->shm->expired = 1;
	swCoreUnlockMtx(&ctx->semCdev);
	swSrcResetCdevFields(cdev);
	swCoreUnlockMtx(&ctx->srcMutex);

	if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: src_free_cdev: kx/kxm discarded. cdev_id=%d num=%d\n",
			  cdev->cdevId, ctx->srcActiveCount);
	}
}

static void swSrcInitAll(struct SwWgnCtx *ctx)
{
	int i;

	for (i = 0; i < SW_MAX_SINKS; i++)
		swSrcResetSink(&ctx->srcSinks[i], (unsigned char)i);
	ctx->srcSinkFreeList.next = &ctx->srcSinkFreeList;
	ctx->srcSinkFreeList.prev = &ctx->srcSinkFreeList;
	ctx->srcSinkUsedList.next = &ctx->srcSinkUsedList;
	ctx->srcSinkUsedList.prev = &ctx->srcSinkUsedList;

	swCoreCreateMtx(&ctx->srcMutex);
	ctx->srcUsedList.next = &ctx->srcUsedList;
	ctx->srcUsedList.prev = &ctx->srcUsedList;
	ctx->srcFreeList.next = &ctx->srcFreeList;
	ctx->srcFreeList.prev = &ctx->srcFreeList;

	ctx->srcCdev[0].cdevId = 0;
	swSrcResetCdevFields(&ctx->srcCdev[0]);
	for (i = 1; i < SW_MAX_SRC_CDEVS; i++) {
		ctx->srcCdev[i].cdevId = (unsigned char)i;
		swSrcResetCdevFields(&ctx->srcCdev[i]);
		swListAddTail(&ctx->srcCdev[i].link, &ctx->srcFreeList);
	}
	memset(ctx->kxmLabels, 0, sizeof(ctx->kxmLabels));
	ctx->kxmNextLabel = 0;
	ctx->srcActiveCount = 0;
}

static void swSrcInitSinkList(struct SwWgnCtx *ctx)
{
	unsigned int i;

	ctx->srcSinkFreeList.next = &ctx->srcSinkFreeList;
	ctx->srcSinkFreeList.prev = &ctx->srcSinkFreeList;
	for (i = 0; i < ctx->shmNvp->sinkLimit; i++)
		swListAddTail(&ctx->srcSinks[i].link, &ctx->srcSinkFreeList);
	ctx->srcSinkUsedList.next = &ctx->srcSinkUsedList;
	ctx->srcSinkUsedList.prev = &ctx->srcSinkUsedList;
}

void swSrcExpireKx(struct SwWgnCtx *ctx, struct SwSrcCdev *cdev)
{
	unsigned char randBuf[20];

	if (cdev->shm->expired != 0 && (g_swWgnLogMask & 1u) != 0)
		swMwTrace("sw: src_expire_kx: already expired. cdev_id=%d\n", cdev->cdevId);
	cdev->sinkCount = 0;
	if (ctx->srcActiveCount == 1) {
		while (ctx->srcSinkUsedList.next != &ctx->srcSinkUsedList)
			swSrcFreeSinkLocked(ctx, (struct SwSrcSinkEntry *)ctx->srcSinkUsedList.next);
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: src_expire_kx: reset sink counter\n");
	}
	swWgnGenRandom20(ctx, randBuf);
	memcpy(cdev->shm->kx, randBuf + 4, 12);
	cdev->shm->kxLabel = ++ctx->shmNvp->kxLabel;
	cdev->shm->lifetime = 119;
	if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: src_expire_kx: kx regenerated. cdev_id=%d num=%d\n",
			  cdev->cdevId, ctx->srcActiveCount);
	}
}

void swSrcExpireKxm(struct SwWgnCtx *ctx, struct SwSrcCdev *cdev)
{
	if (cdev->shm->expired != 0) {
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: src_expire_kxm: already expired. cdev_id=%d\n", cdev->cdevId);
		return;
	}
	cdev->sinkCount = 0;
	ctx->srcActiveCount--;
	if (ctx->srcActiveCount == 1 && (g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: src_expire_kxm: all kxm expired\n");
	cdev->shm->expired = 1;
	cdev->shm->lifetime = 0;
	cdev->shm->kxLabel = 0;
	memset(cdev->shm->kx, 0, sizeof(cdev->shm->kx));
	if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: src_expire_kxm: kxm discarded. cdev_id=%d num=%d\n",
			  cdev->cdevId, ctx->srcActiveCount);
	}
}

static void swSrcTimerTick(struct SwWgnCtx *ctx)
{
	struct SwListNode *node;
	struct SwSrcCdev *base;

	swCoreLockMtx(&ctx->srcMutex);
	for (node = ctx->srcUsedList.next; node != &ctx->srcUsedList; node = node->next) {
		struct SwSrcCdev *cdev = (struct SwSrcCdev *)node;

		swCoreLockMtx(&ctx->semCdev);
		if (cdev->shm->streamRef == 0 && cdev->shm->lifetime != 0) {
			if (--cdev->shm->lifetime == 0)
				swSrcExpireKxm(ctx, cdev);
		}
		swCoreUnlockMtx(&ctx->semCdev);
	}
	base = swSrcGetCdev(ctx, 0);
	if (base != NULL) {
		swCoreLockMtx(&ctx->semCdev);
		if (base->shm->streamRef == 0 && base->shm->lifetime != 0) {
			if (--base->shm->lifetime == 0)
				swSrcExpireKx(ctx, base);
		}
		swCoreUnlockMtx(&ctx->semCdev);
	}
	swCoreUnlockMtx(&ctx->srcMutex);
}

static int swAllocKxmLabel(unsigned char *table, uint32_t start)
{
	uint32_t i;

	for (i = start; i < 256u; i++) {
		if (table[i] == 0) {
			table[i] = 3;
			return (int)i;
		}
	}
	for (i = 0; i < start; i++) {
		if (table[i] == 0) {
			table[i] = 3;
			return (int)i;
		}
	}
	return -1;
}

static int swFreeKxmLabel(unsigned char *table, unsigned char label)
{
	if ((table[label] & 2u) != 0)
		return -1;
	table[label] = 0;
	return 0;
}

static int swCmdGenSrcCdev(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSrcCdev *cdev;
	int kxmLabel = 0;
	unsigned char randBuf[20];

	if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: GEN_SRC_CDEV: fmt_cognizant=0x%x, CA=0x%x, move=%d\n",
			  arg[4], arg[5], arg[6]);
	}
	if (arg[5] != 1u) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: GEN_SRC_CDEV: ERROR, unsupported CA=0x%x\n", arg[5]);
		return 0;
	}

	if (arg[6] == 0) {
		cdev = &ctx->srcCdev[0];
		swCoreLockMtx(&ctx->srcMutex);
		if (cdev->shm != NULL) {
			swCoreUnlockMtx(&ctx->srcMutex);
			if ((g_swWgnLogMask & 4u) != 0) {
				swMwTrace("sw: GEN_SRC_CDEV: success: id=%d kx_label=0x%x num=%d\n",
					  cdev->cdevId, cdev->shm->kxLabel, ctx->srcActiveCount);
			}
			arg[0] = cdev->cdevId;
			arg[7] = cdev->shm->kxLabel;
			arg[2] = 0;
			return 0;
		}
		cdev->shm = &ctx->shmTbl->cdev[0];
		cdev->shm->lifetime = 119;
		cdev->shm->expired = 0;
		ctx->srcActiveCount++;
		swCoreUnlockMtx(&ctx->srcMutex);
	} else {
		uint32_t start = ctx->kxmNextLabel++;

		if (ctx->kxmNextLabel >= 0xFFu)
			ctx->kxmNextLabel = 0;
		kxmLabel = swAllocKxmLabel(ctx->kxmLabels, start);
		if (kxmLabel < 0) {
			arg[2] = 26;
			if ((g_swWgnLogMask & 1u) != 0)
				swMwTrace("sw: GEN_SRC_CDEV: ERROR, can't alloc kxm_label.\n");
			return 0;
		}
		cdev = swSrcAllocMoveCdev(ctx);
		if (cdev == NULL) {
			ctx->kxmLabels[kxmLabel] = 0;
			arg[2] = 3;
			if ((g_swWgnLogMask & 1u) != 0)
				swMwTrace("sw: GEN_SRC_CDEV: ERROR, can't creatre cdev, resource full.\n");
			return 0;
		}
	}

	swCoreLockMtx(&ctx->semCdev);
	cdev->shm->cipherAlgo = arg[5];
	if (arg[6] != 0) {
		cdev->shm->kxLabel = (unsigned char)kxmLabel;
		cdev->shm->cognizant = 0;
		cdev->isMove = 1;
	} else {
		cdev->shm->kxLabel = ++ctx->shmNvp->kxLabel;
		cdev->shm->cognizant = (arg[4] != 0) ? 1u : 0u;
		cdev->isMove = 0;
	}
	swWgnGenRandom20(ctx, randBuf);
	memcpy(cdev->shm->kx, randBuf + 4, 12);
	swCoreUnlockMtx(&ctx->semCdev);

	if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: GEN_SRC_CDEV: success: id=%d kx_label=0x%x num=%d\n",
			  cdev->cdevId, cdev->shm->kxLabel, ctx->srcActiveCount);
	}
	arg[0] = cdev->cdevId;
	arg[7] = cdev->shm->kxLabel;
	arg[2] = 0;
	return 0;
}

static int swCmdSttSrcAke(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSrcCdev *cdev;
	struct SwSrcSinkEntry *sink;

	if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: STT_SRC_AKE: cdev_id=%d ap=%d ", arg[0], arg[9]);
		swWgnDumpHex(" device_id ", arg + 4, 5);
	}
	cdev = swSrcGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: src_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	if (cdev->isMove != 0 && cdev->shm->expired != 0) {
		arg[2] = 19;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: src_get_cdev: ERROR, kxm is expired\n");
		return 0;
	}
	if (arg[9] == 0) {
		struct SwListNode *node;

		for (node = ctx->srcSinkUsedList.next; node != &ctx->srcSinkUsedList; node = node->next) {
			struct SwSrcSinkEntry *existing = (struct SwSrcSinkEntry *)node;

			if (existing->isAp == 0 && memcmp(existing->deviceId, arg + 4, 5) == 0) {
				arg[1] = existing->akeId;
				arg[2] = 0;
				return 0;
			}
		}
	}

	sink = swSrcAllocSink(ctx);
	if (sink == NULL) {
		arg[2] = 10;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: STT_SRC_AKE: ERROR, sink limitation\n");
		return 0;
	}
	if (arg[9] != 0)
		sink->isAp = 1;
	else
		memcpy(sink->deviceId, arg + 4, 5);
	sink->state = 0;
	swSrcAttachSink(ctx, cdev, sink);
	arg[1] = sink->akeId;
	arg[2] = 0;
	return 0;
}

static int swCmdGenSrcKauth(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSrcSinkEntry *sink;
	unsigned char sharedX[40];

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GEN_SRC_KAUTH: cdev_id=%d ake_id=%d\n", arg[0], arg[1]);
	sink = swSrcGetSink(ctx, arg[1]);
	if (sink == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: GEN_SRC_KAUTH: ERROR, ake_id is not in agreement\n");
		return 0;
	}
	if (swSrcGetCdev(ctx, arg[0]) == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: src_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	if (swDepEccEcdhPhase2(sharedX, arg + 4, arg + 24) != 0) {
		arg[2] = 1;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: gen_ecdh_parm: ERROR, creating of a ec-dh-phase2 failed.\n");
		return 0;
	}
	memcpy(sink->kauth, sharedX + 8, 12);
	sink->state = 1;
	arg[2] = 0;
	return 0;
}

static int swCmdFinSrcAke(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSrcCdev *cdev;
	struct SwSrcSinkEntry *sink;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: FIN_SRC_AKE: cdev_id=%d ake_id=%d\n", arg[0], arg[1]);
	cdev = swSrcGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: src_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	sink = swSrcGetSink(ctx, arg[1]);
	if (sink == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: FIN_SRC_AKE: ERROR, ake_id is not in agreement\n");
		return 0;
	}
	sink->state = 2;
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: FIN_SRC_AKE: snk_cnt=%d\n", cdev->sinkCount);
	arg[2] = 0;
	return 0;
}

static int swCmdCclSrcAke(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSrcCdev *cdev;
	struct SwSrcSinkEntry *sink;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: CCL_SRC_AKE: cdev_id=%d ake_id=%d\n", arg[0], arg[1]);
	cdev = swSrcGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: src_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	sink = swSrcGetSink(ctx, arg[1]);
	if (sink == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: CCL_SRC_AKE: ERROR, ake_id is not in agreement\n");
		return 0;
	}
	if (sink->state == 2) {
		arg[2] = 1;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: ake stat: ERROR, already ake-process finished.\n");
		return 0;
	}
	swCoreLockMtx(&ctx->srcMutex);
	swSrcFreeSinkLocked(ctx, sink);
	swCoreUnlockMtx(&ctx->srcMutex);
	cdev->sinkCount--;
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: CCL_SRC_AKE: snk_cnt=%d\n", cdev->sinkCount);
	arg[2] = 0;
	return 0;
}

static int swCmdGetSrcKsx(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSrcSinkEntry *sink;
	struct SwSrcCdev *cdev;
	unsigned char doubleKauth[24];
	unsigned char digest[20];
	const unsigned char *mask;
	int i;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GET_SRC_KSX: cdev_id=%d ake_id=%d\n", arg[0], arg[1]);
	sink = swSrcGetSink(ctx, arg[1]);
	if (sink == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: GET_SRC_KSX: ERROR, ake_id is not in agreement\n");
		return 0;
	}
	cdev = swSrcGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: src_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	if (sink->state == 0) {
		arg[2] = 15;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: src_get_cdev: ERROR, no kauth exist\n");
		return 0;
	}
	if (cdev->isMove != 0) {
		memcpy(doubleKauth, sink->kauth, 12);
		memcpy(doubleKauth + 12, sink->kauth, 12);
		swDepSha1Digest(doubleKauth, 24, digest);
		memset(doubleKauth, 0, sizeof(doubleKauth));
		mask = digest + 8;
	} else {
		mask = sink->kauth;
	}
	for (i = 0; i < 12; i++)
		arg[4 + i] = cdev->shm->kx[i] ^ mask[i];
	arg[16] = cdev->shm->kxLabel;
	arg[17] = cdev->shm->cipherAlgo;
	arg[2] = 0;
	if ((g_swWgnLogMask & 4u) != 0)
		swWgnDumpHex("GET_SRC_KSX: ksx ", arg + 4, 12);
	return 0;
}

static int swCmdGetCkrParm(struct SwWgnCtx *ctx, unsigned char *arg)
{
	struct SwSrcCdev *cdev;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GET_CKR_PARM: cdev_id=%d ake_id=%d\n", arg[0], arg[1]);
	cdev = swSrcGetCdev(ctx, arg[0]);
	if (cdev == NULL) {
		arg[2] = 2;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: src_get_cdev: ERROR, invalid cdev_id\n");
		return 0;
	}
	arg[12] = cdev->shm->kxLabel;
	arg[13] = cdev->shm->cipherAlgo;
	arg[14] = 0;
	memset(arg + 4, 0, 8);
	arg[2] = 0;
	return 0;
}

static int swCmdResKxmLabel(struct SwWgnCtx *ctx, unsigned char *arg)
{
	uint32_t count;
	uint32_t i;

	memcpy(&count, arg + 4, sizeof(count));
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: RES_KXM_LABEL: num=%d\n", (int)count);
	if (count > 256u) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: RES_KXM_LABEL: ERROR, label_num=%d\n", (int)count);
		arg[2] = 2;
		return 0;
	}
	for (i = 0; i < count; i++) {
		unsigned char label = arg[8 + i];

		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: RES_KXM_LABEL: label=%d\n", label);
		ctx->kxmLabels[label] = 1;
	}
	arg[2] = 0;
	return 0;
}

static int swCmdDelKxmLabel(struct SwWgnCtx *ctx, unsigned char *arg)
{
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: DEL_KXM_LABEL: label=%d\n", arg[4]);
	if (swFreeKxmLabel(ctx->kxmLabels, arg[4]) != 0) {
		arg[2] = 25;
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: DEL_KXM_LABEL: ERROR, kxm label is still used\n");
		return 0;
	}
	arg[2] = 0;
	return 0;
}

static int swSubBigEndian8(const unsigned char *a, const unsigned char *b)
{
	unsigned char diff[8];
	int borrow = 0;
	int i;

	for (i = 7; i >= 0; i--) {
		int v = (int)a[i] - (int)b[i] - borrow;

		diff[i] = (unsigned char)v;
		borrow = (v >> 8) & 1;
	}
	if (diff[0] != 0 || diff[1] != 0 || diff[2] != 0 || diff[3] != 0)
		return -1;
	return (int)(((uint32_t)diff[4] << 24) | ((uint32_t)diff[5] << 16) |
		     ((uint32_t)diff[6] << 8) | (uint32_t)diff[7]);
}

static int swCmdGenSrcCkc(struct SwWgnCtx *ctx, unsigned char *arg)
{
	int i;
	unsigned char curNc[8];
	unsigned char mac[20];

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: GEN_SRC_CKC: cdev_id=%d\n", arg[0]);
	if (arg[0] >= SW_MAX_SRC_CDEVS) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: GEN_SRC_CKC: ERROR, invalid cdev_id\n");
		arg[1] = 2;
		return 0;
	}

	for (i = 0; i < SW_MAX_STREAMS; i++) {
		struct SwShmStream *ss = &ctx->shmTbl->stream[i];

		swCoreLockMtx(&ctx->semStrm);
		if (ss->inUse != 0 && ss->cdevId == arg[0]) {
			memcpy(curNc, ss->nc, 8);
			swCoreUnlockMtx(&ctx->semStrm);
			if ((g_swWgnLogMask & 4u) != 0)
				swWgnDumpHex("GEN_SRC_CKC: current Nc ", curNc, 8);
			if ((uint32_t)swSubBigEndian8(curNc, arg + 12) > 5u) {
				swMwTrace("sw: GEN_SRC_CKC: (source Nc - NcT) > 5!\n");
			} else {
				swComputeCkcMac(ctx->shmTbl->cdev[arg[0]].kx, arg + 12, arg + 4, mac);
				if (memcmp(arg + 20, mac, 10) == 0) {
					memcpy(arg + 30, mac + 10, 10);
					arg[1] = 0;
					return 0;
				}
				swMwTrace("sw: GEN_SRC_CKC: WARN, mac3A!=mac3B\n");
			}
		} else {
			swCoreUnlockMtx(&ctx->semStrm);
		}
	}

	if ((g_swWgnLogMask & 4u) != 0)
		swWgnDumpHex("GEN_SRC_CKC: ERROR, valid Nc not found for NcT ", arg + 12, 8);
	arg[1] = 1;
	return 0;
}

/* ---- Stream table & transfer commands ---- */

static int swCmdSetEemi(struct SwWgnCtx *ctx, unsigned char *arg)
{
	unsigned char sid = arg[0];

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: SET_EEMI: stream_id=%d, E-EMI=0x%x\n", sid, arg[1]);
	if (sid != 0) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: SET_EEMI: ERROR, invalid stream_id\n");
		arg[2] = 2;
		return 0;
	}
	switch (arg[1]) {
	case 0:
	case 2:
	case 4:
	case 6:
	case 8:
	case 10:
	case 12:
		if (ctx->strmTbl[0].active == 0) {
			if ((g_swWgnLogMask & 1u) != 0)
				swMwTrace("sw: SET_EEMI: ERROR, stream is already stopped.\n");
			arg[2] = 1;
			return 0;
		}
		if ((ctx->strmTbl[0].route & 2u) != 0) {
			swSetStreamEemi(ctx->swInfo, 0, arg[1]);
			arg[2] = 0;
		}
		return 0;
	default:
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: SET_EEMI: ERROR, invalid eemi\n");
		arg[2] = 2;
		return 0;
	}
}

void swWgnGetEncParam(struct SwWgnCtx *ctx, unsigned char streamId, unsigned char eemi,
		      int changeNc, void *outKey, void *outIv, void *outNc,
		      unsigned char *outKxLabel, uint32_t *outPcpCl)
{
	struct SwWgnStreamTbl *st = &ctx->strmTbl[streamId];
	void *ncPtr = NULL;
	void *keyPtr = NULL;
	void *ivPtr = NULL;

	if (eemi != 0) {
		if (changeNc != 0) {
			swKmGetNextKey(st->keyPool, eemi, &ncPtr, &keyPtr, &ivPtr);
			st->ncDirty = 1;
		} else {
			swKmGetCurrentKey(st->keyPool, eemi, &ncPtr, &keyPtr, &ivPtr);
		}
		memcpy(outKey, keyPtr, 16);
		memcpy(outIv, ivPtr, 16);
		memcpy(outNc, ncPtr, 8);
	} else if (changeNc != 0) {
		swKmGetNextNc(st->keyPool, (unsigned char *)outNc);
		st->ncDirty = 1;
	} else {
		swKmGetCurrentNc(st->keyPool, (unsigned char *)outNc);
	}
	*outKxLabel = st->kxLabel;
	*outPcpCl = st->pcpCl;
}

static void swUpdateShmNc(struct SwWgnCtx *ctx)
{
	int i;
	unsigned char nc[8];

	for (i = 0; i < SW_MAX_STREAMS; i++) {
		struct SwWgnStreamTbl *st = &ctx->strmTbl[i];

		if ((st->route & 2u) != 0 && st->active != 0 && st->ncDirty != 0) {
			st->ncDirty = 0;
			swKmGetCurrentNc(st->keyPool, nc);
			memcpy(ctx->shmTbl->stream[i].nc, nc, 8);
			if ((g_swWgnLogMask & 4u) != 0) {
				swMwTrace("sw: nc_update: stream_id %d nc=%02x%02x....%02x%02x\n",
					  i, nc[0], nc[1], nc[6], nc[7]);
			}
		}
	}
}

static void swStrmInitAll(struct SwWgnCtx *ctx)
{
	memset(ctx->strmTbl, 0, sizeof(ctx->strmTbl));
}

static void swStrmClearEntry(struct SwWgnCtx *ctx, struct SwWgnStreamTbl *entry)
{
	if (entry->keyPool != NULL)
		swKmFreePool(ctx, entry->keyPool);
	memset(entry, 0, sizeof(*entry));
}

static int swInitStreamTable(struct SwWgnCtx *ctx, unsigned char *arg)
{
	uint32_t route;
	uint32_t pcpCl;
	uint16_t ownerId = ctx->shmTbl->ownerId;
	struct SwShmCdev *shmCdev = NULL;
	int isMove = 0;
	unsigned char *initNc = NULL;
	unsigned char ncBuf[20];
	struct SwKeyPool *pool = NULL;
	unsigned char lifetime = 0;
	unsigned char kxLabel = 0;
	unsigned char cognizant = 0;
	unsigned char cipherAlgo = 0;
	int expired = 0;
	int i;

	memcpy(&route, arg + 12, sizeof(route));
	memcpy(&pcpCl, arg + 16, sizeof(pcpCl));

	if ((route & 1u) != 0) {
		int ok = 0;
		unsigned char encMode = arg[20];

		switch (encMode) {
		case 0:
			if ((ownerId & 0x200u) != 0 && (route & 0x03FE0000u) != 0)
				ok = 1;
			break;
		case 'D':
			if ((ownerId & 0x400u) != 0 && (route & 0x03FC0000u) != 0)
				ok = 1;
			break;
		case 'E':
			if ((route & 0x10000u) != 0)
				ok = 1;
			break;
		case 'L':
			if ((ownerId & 0x400u) != 0 && (route & 0x10000u) != 0)
				ok = 1;
			break;
		case 'S':
			ok = 1;
			break;
		case 'W':
			if ((ownerId & 0x400u) != 0 && (route & 0x20000u) != 0)
				ok = 1;
			break;
		default:
			break;
		}
		if (ok == 0) {
			swMwTrace("sw: init_strm_tbl: ERROR, route not supported\n");
			return 2;
		}
	}

	if ((route & 2u) != 0) {
		struct SwSrcCdev *src;

		if (arg[0] >= SW_MAX_SRC_CDEVS) {
			swMwTrace("sw: init_strm_tbl: ERROR, invalid cdev_id %d\n", arg[0]);
			return 2;
		}
		if (arg[2] == 1u)
			swIncrementNc(arg + 4, ncBuf);
		else
			swWgnGenRandom20(ctx, ncBuf);
		initNc = ncBuf;
		src = swSrcGetCdev(ctx, arg[0]);
		isMove = src->isMove;
		shmCdev = src->shm;
	} else if ((route & 0x20000u) != 0) {
		struct SwSnkCdev *snk;

		if (arg[0] < SW_MAX_SRC_CDEVS || arg[0] >= SW_MAX_CDEVS) {
			swMwTrace("sw: init_strm_tbl: ERROR, invalid cdev_id %d\n", arg[0]);
			return 2;
		}
		snk = swSnkGetCdev(ctx, arg[0]);
		isMove = snk->isMove;
		shmCdev = snk->shm;
	}

	swCoreLockMtx(&ctx->semStrm);
	for (i = 0; i < SW_MAX_STREAMS; i++) {
		if (ctx->shmTbl->stream[i].inUse == 0)
			break;
	}
	if (i >= SW_MAX_STREAMS) {
		swCoreUnlockMtx(&ctx->semStrm);
		swMwTrace("sw: init_strm_tbl: ERROR, max stream exceeded\n");
		return 3;
	}
	ctx->shmTbl->stream[0].inUse = 1;
	if (shmCdev != NULL) {
		ctx->shmTbl->stream[0].cdevId = arg[0];
		if (initNc != NULL)
			memcpy(ctx->shmTbl->stream[0].nc, initNc, 8);
	}
	swCoreUnlockMtx(&ctx->semStrm);

	if (shmCdev != NULL) {
		swCoreLockMtx(&ctx->semCdev);
		if (isMove == 0 && shmCdev->cognizant == 0 && (ownerId & 0x100u) == 0) {
			swMwTrace("sw: init_strm_tbl: ERROR, route not allowed\n");
			swCoreUnlockMtx(&ctx->semCdev);
			swCoreLockMtx(&ctx->semStrm);
			memset(&ctx->shmTbl->stream[0], 0, sizeof(ctx->shmTbl->stream[0]));
			swCoreUnlockMtx(&ctx->semStrm);
			return 2;
		}
		pool = swKmAllocPool(ctx, shmCdev->kx);
		if (pool == NULL) {
			swMwTrace("sw: init_strm_tbl: ERROR, alloc_pool\n");
			swCoreUnlockMtx(&ctx->semCdev);
			swCoreLockMtx(&ctx->semStrm);
			memset(&ctx->shmTbl->stream[0], 0, sizeof(ctx->shmTbl->stream[0]));
			swCoreUnlockMtx(&ctx->semStrm);
			return 4;
		}
		if (initNc != NULL) {
			if (isMove != 0 && shmCdev->expired != 0) {
				swMwTrace("sw: init_strm_tbl: ERROR, expired\n");
				swCoreUnlockMtx(&ctx->semCdev);
				swCoreLockMtx(&ctx->semStrm);
				memset(&ctx->shmTbl->stream[0], 0, sizeof(ctx->shmTbl->stream[0]));
				swCoreUnlockMtx(&ctx->semStrm);
				return 19;
			}
			shmCdev->streamRef++;
		}
		lifetime = shmCdev->lifetime;
		kxLabel = shmCdev->kxLabel;
		cognizant = shmCdev->cognizant;
		cipherAlgo = shmCdev->cipherAlgo;
		expired = shmCdev->expired;
		swCoreUnlockMtx(&ctx->semCdev);
	}

	swStrmClearEntry(ctx, &ctx->strmTbl[0]);
	ctx->strmTbl[0].route = route;
	if (shmCdev != NULL) {
		ctx->strmTbl[0].pcpCl = pcpCl;
		ctx->strmTbl[0].cdevId = arg[0];
		ctx->strmTbl[0].kxLabel = kxLabel;
		ctx->strmTbl[0].cognizant = cognizant;
		ctx->strmTbl[0].cipherAlgo = cipherAlgo;
		ctx->strmTbl[0].keyPool = pool;
		ctx->strmTbl[0].expired = expired;
		swKmSetStreamId(pool, 0);
		if (initNc != NULL)
			swKmPrepareKeys(pool, 2, initNc);
	}
	ctx->strmTbl[0].active = 1;
	arg[1] = 0;
	arg[3] = lifetime;
	return 0;
}

static int swCmdPreTrans(struct SwWgnCtx *ctx, unsigned char *arg)
{
	uint32_t route;
	uint32_t pcpCl;

	memcpy(&route, arg + 12, sizeof(route));
	memcpy(&pcpCl, arg + 16, sizeof(pcpCl));
	if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: PRE_TRANS: cdev_id=%d init_nc_en=%d(%02x%02x%02x%02x) \n",
			  arg[0], arg[2], arg[4], arg[5], arg[10], arg[11]);
		swMwTrace("sw: PRE_TRANS: route=%08x pcp_cl=%08x local_enc_mode=%02x\n",
			  route, pcpCl, arg[20]);
	}
	return swInitStreamTable(ctx, arg);
}

static int swCmdStpTrans(struct SwWgnCtx *ctx, unsigned char *arg)
{
	unsigned char sid = arg[0];
	struct SwWgnStreamTbl *st = &ctx->strmTbl[sid];

	if (st->active == 0) {
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: STP_TRANS: %d stream is already stopped\n", sid);
		return 0;
	}

	st->active = 0;
	if ((st->route & 2u) != 0) {
		struct SwShmCdev *shm = &ctx->shmTbl->cdev[st->cdevId];

		swCoreLockMtx(&ctx->semCdev);
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: STP_TRANS: %d cdev_id=%d, referred=%d\n", sid, st->cdevId, shm->streamRef);
		if (shm->streamRef > 0)
			shm->streamRef--;
		if (shm->streamRef == 0)
			shm->lifetime = 119;
		swCoreUnlockMtx(&ctx->semCdev);
	} else if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: STP_TRANS: %d cdev_id=%d\n", sid, st->cdevId);
	}

	swCoreLockMtx(&ctx->semStrm);
	memset(&ctx->shmTbl->stream[sid], 0, sizeof(ctx->shmTbl->stream[sid]));
	swCoreUnlockMtx(&ctx->semStrm);
	return 0;
}

static int swCmdGetNc(struct SwWgnCtx *ctx, unsigned char *arg)
{
	unsigned char sid = arg[0];

	if ((g_swWgnLogMask & 0x80u) != 0)
		swMwTrace("sw: GET_NC: stream_id=%d\n", sid);
	if (sid != 0) {
		swMwTrace("sw: GET_NC: invalid stream_id\n");
		return 2;
	}
	if ((ctx->strmTbl[sid].route & 0x20002u) != 0) {
		swKmGetCurrentNc(ctx->strmTbl[sid].keyPool, arg + 4);
		return 0;
	}
	return 1;
}

static void swWgnTimerTick(void *arg)
{
	struct SwWgnCtx *ctx = (struct SwWgnCtx *)arg;

	if ((ctx->timer.ticks % 120u) == 0) {
		swSrcTimerTick(ctx);
		swSnkTimerTick(ctx);
		if (swWgnCheckCommonKeyExpired(ctx) != 0) {
			int i;

			for (i = 0; i < SW_MAX_STREAMS; i++) {
				if (ctx->strmTbl[i].active != 0) {
					if ((g_swWgnLogMask & 4u) != 0)
						swMwTrace("sw: set_status: stream_id=%d, sts=%08x\n", i, 0x800000u);
					swSetStreamStatus(ctx->swInfo, (unsigned char)i, 0x800000u);
				}
			}
		}
	}
	swUpdateShmNc(ctx);
	swKmSched(ctx);
}

void swWgnCheckKxAlarm(struct SwWgnCtx *ctx, unsigned char streamId)
{
	struct SwShmCdev *shm = &ctx->shmTbl->cdev[ctx->strmTbl[streamId].cdevId];

	if (shm->alarm != 0) {
		shm->alarm = 0;
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: set_status: stream_id=%d, sts=%08x\n", streamId, 0x400000u);
		swSetStreamStatus(ctx->swInfo, streamId, 0x400000u);
	}
}

static int swCmdGetExEvent(struct SwWgnCtx *ctx, unsigned char *arg)
{
	unsigned char sid = arg[0];
	uint32_t zero = 0;

	if ((g_swWgnLogMask & 0x80u) != 0)
		swMwTrace("sw: GET_EXEVENT: stream_id=%d\n", sid);
	memcpy(arg + 4, &zero, sizeof(zero));
	if (sid != 0) {
		swMwTrace("sw: GET_EXEVENT: invalid stream_id\n");
		return 2;
	}
	{
		struct SwShmCdev *shm = &ctx->shmTbl->cdev[ctx->strmTbl[sid].cdevId];

		if (shm->alarm != 0) {
			uint32_t ev = 0x400000u;

			shm->alarm = 0;
			memcpy(arg + 4, &ev, sizeof(ev));
		}
	}
	return 0;
}

int swWgnCommand(struct SwWgnCtx *ctx, uint32_t cmd, void *arg, uint32_t argLen)
{
	unsigned char *buf = (unsigned char *)arg;

	if (ctx->shmTbl->paramState != 4) {
		if (cmd == 0xC007F205u)
			return swCmdLoadWgnParam(ctx, buf, argLen);
		swMwTrace("sw: parameter is not loaded yet\n");
		return 22;
	}

	switch (cmd) {
	case 0x4007F229u:
		return swCmdGetCert(ctx, buf);
	case 0x4007F22Au:
		return swCmdGenEcdhParm(ctx, buf);
	case 0x4007F22Du:
		return swCmdGetSrm(ctx, buf);
	case 0x4007F22Fu:
		return swCmdGenRand(ctx, buf);
	case 0x8004F233u: {
		int cdevId = 0;

		memcpy(&cdevId, buf, sizeof(cdevId));
		return swCmdDelCdev(ctx, cdevId);
	}
	case 0xC000F216u:
		return swCmdStpTrans(ctx, buf);
	case 0xC007F206u:
		return swCmdSaveWgnParam(ctx, buf, argLen);
	case 0xC007F228u:
		return swCmdEcdsaVerify(ctx, buf);
	case 0xC007F22Bu:
		return swCmdEcdsaSign(ctx, buf);
	case 0xC007F22Cu:
		return swCmdExpKx(ctx, buf);
	case 0xC007F22Eu:
		return swCmdUpdSrm(ctx, buf);
	case 0xC007F230u:
		return swCmdChkRevoked(ctx, buf);
	case 0xC007F232u:
		return swCmdGetRttHash(ctx, buf);
	case 0xC007F234u:
		return swCmdGetMoveHash(ctx, buf);
	case 0xC007F235u:
		return swCmdGetExEvent(ctx, buf);
	case 0xC007F23Cu:
		return swCmdGenSrcCdev(ctx, buf);
	case 0xC007F23Du:
		return swCmdSttSrcAke(ctx, buf);
	case 0xC007F23Eu:
		return swCmdFinSrcAke(ctx, buf);
	case 0xC007F23Fu:
		return swCmdCclSrcAke(ctx, buf);
	case 0xC007F240u:
		return swCmdGenSrcKauth(ctx, buf);
	case 0xC007F241u:
		return swCmdGetSrcKsx(ctx, buf);
	case 0xC007F242u:
		return swCmdGetCkrParm(ctx, buf);
	case 0xC007F243u:
		return swCmdGenSrcCkc(ctx, buf);
	case 0xC007F244u:
		return swCmdSetEemi(ctx, buf);
	case 0xC007F245u:
		return swCmdResKxmLabel(ctx, buf);
	case 0xC007F246u:
		return swCmdDelKxmLabel(ctx, buf);
	case 0xC007F250u:
		return swCmdGenSnkCdev(ctx, buf);
	case 0xC007F251u:
		return swCmdSttSnkAke(ctx, buf);
	case 0xC007F252u:
		return swCmdFinSnkAke(ctx, buf);
	case 0xC007F253u:
		return swCmdCclSnkAke(ctx, buf);
	case 0xC007F254u:
		return swCmdGenSnkKauth(ctx, buf);
	case 0xC007F255u:
		return swCmdSetSnkKsx(ctx, buf);
	case 0xC007F256u:
		return swCmdChkSnkKx(ctx, buf);
	case 0xC007F257u:
		return swCmdChkSnkAuth(ctx, buf);
	case 0xC007F258u:
		return swCmdGenSnkCkc(ctx, buf);
	case 0xC007F259u:
		return swCmdChkSnkCkc(ctx, buf);
	case 0xC007F25Au:
		return swCmdGetSnkKxLifetime(ctx, buf);
	case 0xC007F25Bu:
		return swCmdGetCommonKeyInfo(ctx, buf);
	case 0xC007F264u:
		return 22;
	case 0xC04FF217u:
		return swCmdGetNc(ctx, buf);
	case 0xC4AFF214u:
		return swCmdPreTrans(ctx, buf);
	default:
		swMwTrace("sw: ERROR, unknown command 0x%x\n", cmd);
		return 22;
	}
}

void swWgnDumpHex(const char *prefix, const unsigned char *data, uint32_t len)
{
	uint32_t i;

	if (prefix != NULL)
		swMwTrace("%s", prefix);
	for (i = 0; i < len; i++) {
		swMwTrace("%02x", data[i]);
		if ((i & 3u) == 3u)
			swMwTrace(" ");
		if ((i & 15u) == 15u)
			swMwTrace("\n");
	}
	if ((len & 15u) != 0)
		swMwTrace("\n");
}

void swWgnAddBigEndian(const unsigned char *a, int aLen, const unsigned char *b,
		       int bLen, unsigned char *out)
{
	unsigned int carry = 0;
	int ai = aLen - 1;
	int bi = bLen - 1;

	while (ai >= 0) {
		unsigned int sum = carry + a[ai];

		if (bi >= 0)
			sum += b[bi--];
		out[ai] = (unsigned char)sum;
		carry = sum >> 8;
		ai--;
	}
}

static void swRecordBuildOptions(void)
{
	char optBuf[36];

	memcpy(optBuf, "options: SW_DEBUG ENABLE_DBGLOG ", 33);
	(void)optBuf;
}

void swBaseExit(struct SwInfo *swInfo)
{
	struct SwWgnInfo *info = swInfo->wgnInfo;
	int i;
	int j;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: base_exit:\n");
	swWgnExit(swInfo->wgnCtx);
	swInfo->wgnCtx = NULL;
	for (i = 0; i < SW_MAX_STREAMS; i++) {
		for (j = 0; j < 2; j++)
			swResetDmaPort(info, &info->stream[i].port[j]);
	}
	swFiniWgnInfo(info);
	swFreeMem(info, swInfo, (int)sizeof(*swInfo));
	info->swInfo = NULL;
	swFreeMem(info, info, (int)sizeof(*info));
}

int swBaseInit(void *depCore, struct SwInfo **outInfo)
{
	struct SwWgnInfo *info;
	struct SwInfo *swInfo;
	struct SwWgnCtx *ctx;

	info = (struct SwWgnInfo *)swAllocMem(NULL, sizeof(*info), 0);
	if (info == NULL) {
		swMwTrace("sw: alloc wagner_info fail\n");
		return 7;
	}
	swInfo = (struct SwInfo *)swAllocMem(info, sizeof(*swInfo), 0);
	info->swInfo = swInfo;
	if (swInfo == NULL) {
		swMwTrace("sw: alloc sw_info fail\n");
		swFreeMem(info, info, (int)sizeof(*info));
		return 7;
	}
	swInfo->depCore = depCore;
	swInfo->wgnInfo = info;
	swInitWgnInfo(info);
	ctx = swWgnInit(swInfo);
	swInfo->wgnCtx = ctx;
	info->wgnCtx = ctx;
	if (ctx == NULL) {
		swMwTrace("sw: wgn_init fail\n");
		swFreeMem(info, swInfo, (int)sizeof(*swInfo));
		swFreeMem(info, info, (int)sizeof(*info));
		return 7;
	}
	*outInfo = swInfo;
	swRecordBuildOptions();
	return 0;
}

int swDepCoreResetWrap(struct SwInfo *swInfo)
{
	return swDepCoreReset(swInfo->depCore);
}

const struct SwEccCurveTable *swGetEccCurveTable(void)
{
	return swGetEccCurveTableRaw();
}

void swGetCciInfo(struct SwInfo *swInfo, int streamId, unsigned char *outCci)
{
	struct SwDepStreamCtx *st = &swInfo->stream[(unsigned char)streamId];
	unsigned char cciCopy[8];

	swCoreLockMtx(&st->statMutex);
	memcpy(cciCopy, st->cci, sizeof(cciCopy));
	swCoreUnlockMtx(&st->statMutex);
	outCci[0] = cciCopy[0];
	outCci[1] = cciCopy[1];
	outCci[2] = cciCopy[2];
}
