/*
 * sw_wgn_dma.c — Wagner stream status/CCI logging, WSGL TX/RX ring dispatch,
 * sw dep prepare/start/stop/encparam glue, periodic software timer thread,
 * and DMAC scatter-gather ring setup/teardown.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <rtc.h>
#include "sw_wgn_internal.h"

struct SwDepTransConfig {
	uint32_t route;
	int cognizant;
	unsigned char eemi;
	unsigned char eemiAlign[3];
	int changeCci;
	int inTts;
	int outTts;
	int tstmp;
	int tsResync;
	int atsCount;
	int decEnable;
	unsigned char decKey[16];
	unsigned char decIv[16];
	uint32_t decOffset;
	int encEnable;
	unsigned char encKey[16];
	unsigned char encIv[16];
	uint32_t encOffset;
	unsigned char extraCfg[1024];
};

static int swDmaMutexCreate(SceKernelLwMutexWork *mtx)
{
	int ret = sceKernelCreateLwMutex(mtx, "sce_sws_mutex", 0x2000u, 0, NULL);

	return (ret >= 0) ? 0 : ret;
}

static void swDmaMutexDelete(SceKernelLwMutexWork *mtx)
{
	sceKernelDeleteLwMutex(mtx);
}

static void swDmaMutexLock(SceKernelLwMutexWork *mtx)
{
	sceKernelLockLwMutex(mtx, 1, NULL);
}

static void swDmaMutexUnlock(SceKernelLwMutexWork *mtx)
{
	sceKernelUnlockLwMutex(mtx, 1);
}

static int swDmaEventInit(struct SwWgnEvent *ev)
{
	ev->signaled = 0;
	if (sceKernelCreateLwMutex(&ev->mutex, "sce_sws_event_mutex", 0x2000u, 0, NULL) < 0)
		return 11;
	if (sceKernelCreateLwCond(&ev->cond, "sce_sws_event_cond", 0x2000u, &ev->mutex, NULL) >= 0)
		return 0;
	sceKernelDeleteLwMutex(&ev->mutex);
	return 11;
}

static void swDmaEventFini(struct SwWgnEvent *ev)
{
	sceKernelDeleteLwCond(&ev->cond);
	sceKernelDeleteLwMutex(&ev->mutex);
}

static int swDmaEventSignal(struct SwWgnEvent *ev)
{
	sceKernelLockLwMutex(&ev->mutex, 1, NULL);
	ev->signaled = 1;
	sceKernelSignalLwCond(&ev->cond);
	sceKernelUnlockLwMutex(&ev->mutex, 1);
	return 0;
}

static int swDmaEventWait(struct SwWgnEvent *ev)
{
	if (sceKernelLockLwMutex(&ev->mutex, 1, NULL) < 0)
		return 22;
	while (ev->signaled == 0) {
		if (sceKernelWaitLwCond(&ev->cond, NULL) < 0) {
			sceKernelUnlockLwMutex(&ev->mutex, 1);
			return 22;
		}
	}
	ev->signaled = 0;
	if (sceKernelUnlockLwMutex(&ev->mutex, 1) != 0)
		return 22;
	return 0;
}

static uint32_t swTimerGetTickMs(void)
{
	SceRtcTick tick;

	sceRtcGetCurrentTick(&tick);
	return (uint32_t)(tick.tick / 1000ULL);
}

static uint32_t swTimerElapsedMs(uint32_t now, uint32_t start)
{
	if (start > now)
		return now - start - 1u;
	return now - start;
}

static int swDmaEventWaitTimeout(struct SwWgnEvent *ev, int timeoutMs)
{
	int remain = timeoutMs;
	int status = 0;

	if (timeoutMs < 0)
		return swDmaEventWait(ev);
	if (timeoutMs == 0) {
		sceKernelLockLwMutex(&ev->mutex, 1, NULL);
		if (ev->signaled != 0) {
			ev->signaled = 0;
			sceKernelUnlockLwMutex(&ev->mutex, 1);
			return 0;
		}
		sceKernelUnlockLwMutex(&ev->mutex, 1);
		return 116;
	}

	sceKernelLockLwMutex(&ev->mutex, 1, NULL);
	if (ev->signaled != 0) {
		ev->signaled = 0;
		sceKernelUnlockLwMutex(&ev->mutex, 1);
		return 0;
	}
	for (;;) {
		uint32_t t0 = swTimerGetTickMs();
		unsigned int timeoutUs = (unsigned int)(1000 * remain);
		uint32_t t1;

		status = sceKernelWaitLwCond(&ev->cond, &timeoutUs);
		t1 = swTimerGetTickMs();
		if (status <= (int)0x80020005u) {
			if (status > (int)0x80020004u)
				status = 116;
			sceKernelUnlockLwMutex(&ev->mutex, 1);
			return status;
		}
		if (status != 0) {
			sceKernelUnlockLwMutex(&ev->mutex, 1);
			return status;
		}
		if (ev->signaled == 0) {
			remain -= (int)swTimerElapsedMs(t1, t0);
			if (remain <= 0) {
				sceKernelUnlockLwMutex(&ev->mutex, 1);
				return status;
			}
		}
		if (ev->signaled != 0) {
			ev->signaled = 0;
			sceKernelUnlockLwMutex(&ev->mutex, 1);
			return status;
		}
	}
}

static int swTimerSleep(struct SwWgnEvent *ev, int stopRequested, int periodMs)
{
	int status;

	if (stopRequested != 0)
		return periodMs;
	status = swDmaEventWaitTimeout(ev, periodMs);
	if (status == 0)
		return periodMs;
	if (status == 116)
		return 0;
	return -1;
}

static int swTimerThreadMain(void *arg)
{
	struct SwWgnTimer *timer = (struct SwWgnTimer *)arg;

	for (;;) {
		swTimerSleep(&timer->event, timer->stopRequested, timer->periodMs);
		if (timer->stopRequested != 0)
			break;
		timer->ticks++;
		timer->callback(timer->arg);
	}
	return 0;
}

static int swTimerThreadCreateRaw(struct SwWgnThread *thr, int (*entry)(void *),
				  void *arg, int threadType)
{
	SceUID thid;
	int initPriority;
	int cpuAffinityMask = 0;
	SceSize stackSize = 0x4000;
	char name[32];

	thr->entry = entry;
	thr->arg = arg;
	memset(name, 0, sizeof(name));
	memcpy(name, "SWS_THREAD_NO_NAME", 19);
	initPriority = (SwsGetToolMode() == 0) ? 268435712 : 111;

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
	thr->threadId = thid;
	return sceKernelStartThread(thid, sizeof(*thr), thr);
}

static int swTimerThreadCreate(struct SwWgnThread *thr, int (*entry)(void *),
			       void *arg, const char *label, int threadType)
{
	int ret;

	(void)label;
	ret = swTimerThreadCreateRaw(thr, entry, arg, threadType);
	if (ret == 0)
		return 0;
	swMwTrace("sw: ERROR, pthread_create(id=%d) %d\n", threadType, ret);
	return -1;
}

static int swTimerThreadJoin(struct SwWgnThread *thr)
{
	int ret = sceKernelWaitThreadEnd(thr->threadId, NULL, NULL);

	sceKernelDeleteThread(thr->threadId);
	if (ret == 0)
		return 0;
	swMwTrace("sw: ERROR, pthread_join %d\n", ret);
	return -1;
}

uint32_t swPollStreamStatus(struct SwInfo *swInfo, unsigned char streamId, int clearAll)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];
	uint32_t bits;

	if (clearAll != 0)
		swWgnCheckKxAlarm(swInfo->wgnCtx, streamId);

	swDmaMutexLock(&st->statMutex);
	if (clearAll != 0) {
		bits = st->statusBits;
		st->statusBits = 0;
	} else {
		bits = st->statusBits & 0x00081FF0u;
		st->statusBits &= 0x0FF7E000u;
	}
	swDmaMutexUnlock(&st->statMutex);

	if ((bits & 0x00000010u) != 0)
		swMwTrace("sw: stream#%d: TS in FIFO overflow\n", streamId);
	if ((bits & 0x00000020u) != 0)
		swMwTrace("sw: stream#%d: bit sync lost\n", streamId);
	if ((bits & 0x00000040u) != 0)
		swMwTrace("sw: stream#%d: sync lost\n", streamId);
	if ((bits & 0x00000080u) != 0)
		swMwTrace("sw: stream#%d: TS transport error\n", streamId);
	if ((bits & 0x00000100u) != 0)
		swMwTrace("sw: stream#%d: packet header error\n", streamId);
	if ((bits & 0x00000200u) != 0)
		swMwTrace("sw: stream#%d: ATS counter a round\n", streamId);
	if ((bits & 0x00000400u) != 0)
		swMwTrace("sw: stream#%d: transfer FIFO0 overflow\n", streamId);
	if ((bits & 0x00000800u) != 0)
		swMwTrace("sw: stream#%d: transfer FIFO1 overflow\n", streamId);
	if ((bits & 0x00001000u) != 0)
		swMwTrace("sw: stream#%d: ZERO time stamp consecutive detection\n", streamId);
	if ((bits & 0x00002000u) != 0)
		swMwTrace("sw: stream#%d: invalid E-EMI\n", streamId);
	if ((bits & 0x00004000u) != 0)
		swMwTrace("sw: stream#%d: unsupported cipher algorithm\n", streamId);
	if ((bits & 0x00008000u) != 0)
		swMwTrace("sw: stream#%d: unknown KX LABEL\n", streamId);
	if ((bits & 0x00010000u) != 0)
		swMwTrace("sw: stream#%d: invalid content length\n", streamId);
	if ((bits & 0x00040000u) != 0)
		swMwTrace("sw: stream#%d: Kx was expired\n", streamId);
	if ((bits & 0x00100000u) != 0)
		swMwTrace("sw: stream#%d: Nc not one increment\n", streamId);
	if ((bits & 0x00400000u) != 0)
		swMwTrace("sw: stream#%d: Kx will be expired soon\n", streamId);
	if ((bits & 0x00800000u) != 0)
		swMwTrace("sw: stream#%d: common key is expired\n", streamId);
	if ((bits & 0x01000000u) != 0)
		swMwTrace("sw: stream#%d: invalid TS header or PAT/PMT\n", streamId);
	if ((bits & 0x02000000u) != 0)
		swMwTrace("sw: stream#%d: PAT/PMT CRC32 error\n", streamId);
	if ((bits & 0x04000000u) != 0)
		swMwTrace("sw: stream#%d: forbided TS out by Embedded-CCI\n", streamId);
	if ((bits & 0x08000000u) != 0)
		swMwTrace("sw: stream#%d: E-EMI and DTCP descriptor are mismatched in sink\n", streamId);
	return bits;
}

int swSetStreamStatusWrap(struct SwInfo *swInfo, unsigned char streamId, uint32_t bits)
{
	swSetStreamStatus(swInfo, streamId, bits);
	return 0;
}

static void swDispatchTxWsgl(struct SwInfo *swInfo, unsigned char streamId)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];
	int isPcp = (st->route & 0x20000u) != 0;
	int pp;

	if ((g_swWgnLogMask & 0x80u) != 0)
		swMwTrace("sw: wsgl:rp=%d pp=%d wp=%d\n", st->txRp, st->txPp, st->txWp);

	pp = st->txPp;
	for (;;) {
		struct SwDmaSgl *sgl = &st->txPort->wsgl[pp];
		uint32_t len = sgl->xferLen;
		uint32_t flags = sgl->flags;

		if ((flags & 0x80000000u) == 0) {
			if ((g_swWgnLogMask & 4u) != 0)
				swMwTrace("sw: setwp: SGL_VALID not set!!!\n");
			break;
		}
		if (len != 0) {
			if (isPcp) {
				if (swPcpProcessTxSgl(swInfo, streamId, sgl, len, flags) < 0)
					break;
			} else {
				swTxQueuePush(st, (uintptr_t)sgl->vaddr, len, flags | 0x200000u);
			}
		} else {
			if (st->txR == st->txW) {
				swNotifyTxRp(swInfo, streamId, 1);
			} else {
				int prev = (pp != 0) ? (pp - 1) : (st->txPort->nWsgl - 1);

				st->txPort->wsgl[prev].flags |= 0x20000000u;
			}
			st->txLast = 1;
		}

		if (++pp == st->txPort->nWsgl)
			pp = 0;
		st->txPp = pp;
		if (pp == st->txWp) {
			st->txEmpty = 1;
			break;
		}
		if ((unsigned char)(st->txW + 1) == (unsigned char)st->txR)
			break;
	}

	if (st->txP != st->txW)
		swTxQueueFlush(swInfo, streamId);
	else if (st->txLast != 0)
		swSetLastWrap(swInfo, streamId);
}

int swSetTxWp(struct SwInfo *swInfo, unsigned char streamId, int wp)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];

	swDmaMutexLock(&st->txMutex);
	st->txWp = wp;
	st->txEmpty = 0;
	if ((unsigned char)(st->txW + 1) != (unsigned char)st->txR)
		swDispatchTxWsgl(swInfo, streamId);
	swDmaMutexUnlock(&st->txMutex);
	return 0;
}

int swSetRxRp(struct SwInfo *swInfo, unsigned char streamId, int rp)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];
	int wasFull;

	st->rxRp = rp;
	wasFull = st->rxFull;
	st->rxFull = 0;
	if ((g_swWgnLogMask & 0x80u) != 0)
		swMwTrace("sw: set_rxrp %d, rp=%d\n", streamId, rp);
	if (wasFull != 0) {
		if ((g_swWgnLogMask & 0x10u) != 0)
			swMwTrace("sw: set_rxrp: %d buffavail %d\n", streamId, rp);
		swDepCoreKickRx(swInfo->depCore, streamId);
	}
	return wasFull;
}

int swKickDmaPort(struct SwWgnInfo *info, struct SwDmaPort *port, unsigned char streamId)
{
	if (port->direction == 1) {
		int next = port->endPos + 1;

		if (next >= port->nWsgl)
			next = 0;
		return swSetTxWp(info->swInfo, streamId, next);
	}
	return swSetRxRp(info->swInfo, streamId, port->endPos);
}

void swClearDepStream(struct SwInfo *swInfo, unsigned char streamId)
{
	memset(&swInfo->stream[streamId], 0, sizeof(struct SwDepStreamCtx));
}

static int swInitDepStream(struct SwInfo *swInfo, unsigned char streamId, uint32_t rxSyncLen)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];
	struct SwWgnInfo *info = swInfo->wgnInfo;
	struct SwStreamState *strm = &info->stream[streamId];

	st->cci[0] = 0xFFu;
	st->cci[1] = 0xFFu;
	st->route = strm->route;
	swDmaMutexCreate(&st->statMutex);
	st->txEmpty = 1;

	if ((strm->txDmapNo & 0x80) != 0) {
		st->txPort = &strm->port[strm->txDmapNo & 1];
		swDmaMutexCreate(&st->txMutex);
		st->txItems = (struct SwTxItem *)swAllocMem(info, 256u * sizeof(struct SwTxItem), 0);
		if (st->txItems == NULL) {
			if ((g_swWgnLogMask & 4u) != 0)
				swMwTrace("sw: init_stream %d: malloc fail\n", streamId);
			swDmaMutexDelete(&st->txMutex);
			swDmaMutexDelete(&st->statMutex);
			st->route = 0;
			return 7;
		}
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: init_stream %d: tx nwsgl=%d\n", streamId, st->txPort->nWsgl);
	}

	if ((strm->rxDmapNo & 0x80) != 0) {
		st->rxPort = &strm->port[strm->rxDmapNo & 1];
		st->rxFull = 0;
		st->rxSyncLen = rxSyncLen;
		swSetRxRp(info->swInfo, streamId, st->rxPort->nWsgl - 1);
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: init_stream %d: rx nwsgl=%d\n", streamId, st->rxPort->nWsgl);
	}
	return 0;
}

static void swFiniDepStream(struct SwInfo *swInfo, struct SwDepStreamCtx *st)
{
	st->route = 0;
	swDmaMutexDelete(&st->statMutex);
	if (st->txItems != NULL) {
		swFreeMem(swInfo->wgnInfo, st->txItems, (int)(256u * sizeof(struct SwTxItem)));
		st->txItems = NULL;
		swDmaMutexDelete(&st->txMutex);
	}
}

int swDepPrepareTrans(struct SwInfo *swInfo, const unsigned char *param, int tsifMode,
		      int streamId)
{
	unsigned char sid = (unsigned char)streamId;
	struct SwDepTransConfig cfg;
	uint32_t route;
	uint32_t rxSyncLen;
	uint32_t atsCount;
	uint32_t offsetBlocks;
	int encEnable = 0;
	int decEnable = 0;
	int tsMode;
	int ret;

	memcpy(&rxSyncLen, param + 24, sizeof(rxSyncLen));
	ret = swInitDepStream(swInfo, sid, rxSyncLen);
	if (ret != 0)
		return ret;

	memset(&cfg, 0, sizeof(cfg));
	swInfo->stream[sid].formatMode = param[91];

	memcpy(&route, param + 64, sizeof(route));
	if ((route & 0x10001u) != 0) {
		unsigned char mode = param[93];

		if (mode != 'W') {
			switch (mode) {
			case 'L':
				decEnable = 1;
				break;
			case 'E':
				encEnable = 1;
				break;
			case 'D':
				break;
			default:
				if (mode != 0) {
					if ((route & 0x10000u) != 0)
						decEnable = 0;
					else
						encEnable = 1;
				} else if ((route & 0x10000u) != 0) {
					decEnable = 1;
				} else {
					encEnable = 1;
				}
				break;
			}
		}

		memcpy(&offsetBlocks, param + 172, sizeof(offsetBlocks));
		if (decEnable != 0) {
			cfg.decEnable = 1;
			cfg.decOffset = 16u * offsetBlocks;
			memcpy(cfg.decKey, param + 140, 16);
			memcpy(cfg.decIv, param + 156, 16);
		} else {
			cfg.decEnable = 0;
		}
		if (encEnable != 0) {
			cfg.encEnable = 1;
			cfg.encOffset = 16u * offsetBlocks;
			memcpy(cfg.encKey, param + 140, 16);
			memcpy(cfg.encIv, param + 156, 16);
		} else {
			cfg.encEnable = 0;
		}
	}

	cfg.changeCci = (param[95] != 0) ? 1 : 0;
	cfg.eemi = param[92];
	tsMode = ((route & 0x03FC03FCu) != 0) ? (unsigned char)tsifMode : 0;
	if (param[73] != 0) {
		cfg.inTts = 1;
		if (tsMode == 0) {
			cfg.outTts = 1;
			if (param[74] != 0)
				cfg.tstmp = 1;
		}
	} else if (tsMode == 0 && param[74] != 0) {
		cfg.outTts = 1;
		cfg.tstmp = 1;
	}

	cfg.cognizant = swGetCdevCognizant(swInfo->wgnCtx, param[0]);
	cfg.route = route;
	memcpy(&atsCount, param + 76, sizeof(atsCount));
	cfg.atsCount = (atsCount != 0) ? 1 : 0;
	cfg.tsResync = (param[88] != 0) ? 1 : 0;
	memcpy(cfg.extraCfg, param + 176, sizeof(cfg.extraCfg));

	if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: dep_prepare_trans: %d route=%08x cognizant=%d e_emi=%02x change_cci=%d\n",
			  sid, cfg.route, cfg.cognizant, cfg.eemi, cfg.changeCci);
		swMwTrace("sw: dep_prepare_trans: out_tts=%d in_tts=%d tstmp=%d ts_resync=%d atscount=%08x\n",
			  cfg.outTts, cfg.inTts, cfg.tstmp, cfg.tsResync, cfg.atsCount);
	}

	ret = swDepCoreConfigureStream(swInfo->depCore, sid, &cfg);
	if (ret == 0) {
		ret = swDepCoreStartStream(swInfo->depCore, sid);
		if (ret == 0)
			return 0;
	}
	swFiniDepStream(swInfo, &swInfo->stream[sid]);
	return ret;
}

int swSetLastWrap(struct SwInfo *swInfo, unsigned char streamId)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];

	if (st->setLast == 0) {
		st->setLast = 1;
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: sw_set_last: %d\n", streamId);
		if (st->stopTrans == 0)
			return swDepCoreStopStreamGraceful(swInfo->depCore, streamId);
	}
	return 0;
}

int swStopStreamWrap(struct SwInfo *swInfo, unsigned char streamId)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: sw_stop_stream: %d set_last=%d stop_trans=%d\n",
			  streamId, st->setLast, st->stopTrans);
	if (st->setLast == 0) {
		st->setLast = 1;
		if (st->stopTrans == 0) {
			st->stopTrans = 1;
			return swDepCoreStopStreamForce(swInfo->depCore, streamId);
		}
	}
	return 0;
}

int swFreeStreamWrap(struct SwInfo *swInfo, unsigned char streamId)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];

	if (st->route != 0) {
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: sw_free_stream: %d\n", streamId);
		if (st->stopTrans == 0) {
			st->stopTrans = 1;
			swDepCoreStopStreamForce(swInfo->depCore, streamId);
		}
		swFiniDepStream(swInfo, st);
	}
	return 0;
}

void swStopCompCallback(struct SwInfo *swInfo, unsigned char streamId)
{
	struct SwWgnInfo *info = swInfo->wgnInfo;
	struct SwStreamState *strm = &info->stream[streamId];
	int rxDmapNo = strm->rxDmapNo;
	int txDmapNo = strm->txDmapNo;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: stop_comp: %d rxdmapno%x txdmapno%x\n", streamId, rxDmapNo, txDmapNo);

	if ((rxDmapNo & 0x80) != 0 && (strm->port[rxDmapNo & 1].stateFlags & 0x10u) == 0) {
		if ((g_swWgnLogMask & 2u) != 0)
			swMwTrace("sw: stop_comp: rx dma still in use\n");
		swRxCompCallback(info, streamId, 0);
		swMarkDmaDone(info, streamId, rxDmapNo & 1);
	}
	if ((txDmapNo & 0x80) != 0 && (strm->port[txDmapNo & 1].stateFlags & 0x10u) == 0) {
		if ((g_swWgnLogMask & 2u) != 0)
			swMwTrace("sw: stop_comp: tx dma still in use\n");
		swTxCompCallback(info, streamId);
		swMarkDmaDone(info, streamId, txDmapNo & 1);
	}
	if ((g_swWgnLogMask & 4u) != 0) {
		swMwTrace("sw: rx.total=%lld tx.total=%lld\n",
			  (long long)info->stats.rxStat[streamId].totalBytes,
			  (long long)info->stats.txStat[streamId].totalBytes);
	}
}

void swSetCciBasic(struct SwInfo *swInfo, unsigned char streamId, unsigned char cci,
		   unsigned char eemi)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];

	if ((st->route & 0x20000u) == 0)
		return;
	swDmaMutexLock(&st->statMutex);
	st->cci[0] = cci;
	st->cci[1] = eemi;
	if (cci == 4u)
		st->cci[2] = 1u;
	swDmaMutexUnlock(&st->statMutex);
}

void swSetCciExt(struct SwInfo *swInfo, unsigned char streamId, const unsigned char *ext,
		 unsigned char cci, unsigned char eemi)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];

	if ((st->route & 0x20000u) == 0)
		return;
	swDmaMutexLock(&st->statMutex);
	st->cci[0] = cci;
	st->cci[1] = eemi;
	if (cci == 4u)
		st->cci[2] = 1u;
	st->cci[3] = ext[0];
	st->cci[4] = ext[1];
	swDmaMutexUnlock(&st->statMutex);
}

void swGetEncParamWrap(struct SwInfo *swInfo, int streamId, int eemi, int changeNc,
		       uint32_t *outEncParam, unsigned char *outPcpHdr)
{
	unsigned char sid = (unsigned char)streamId;
	unsigned char em = (unsigned char)eemi;
	uint32_t pcpCl = 0;

	swWgnGetEncParam(swInfo->wgnCtx, sid, em, changeNc,
			 outEncParam + 1, outEncParam + 5,
			 outPcpHdr + 2, outPcpHdr + 1, &pcpCl);
	if ((g_swWgnLogMask & 0x40u) != 0) {
		swMwTrace("sw: get_encparam: %d, E-EMI=0x%x, cl=%08x, chg_nc=%d(%02x%02x....%02x%02x)\n",
			  sid, em, pcpCl, changeNc,
			  outPcpHdr[2], outPcpHdr[3], outPcpHdr[8], outPcpHdr[9]);
	}
	outEncParam[0] = (em != 0) ? 1u : 0u;
	outEncParam[9] = pcpCl;
	outPcpHdr[0] = em;
	outPcpHdr[10] = (unsigned char)(pcpCl >> 24);
	outPcpHdr[11] = (unsigned char)(pcpCl >> 16);
	outPcpHdr[12] = (unsigned char)(pcpCl >> 8);
	outPcpHdr[13] = (unsigned char)pcpCl;
}

int swTimerInit(struct SwWgnTimer *timer, int timerId)
{
	timer->timerId = timerId;
	timer->callback = NULL;
	return 0;
}

int swTimerStart(struct SwWgnTimer *timer, int periodMs, void (*cb)(void *), void *arg)
{
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: timer started: %d\n", timer->timerId);
	timer->periodMs = periodMs;
	timer->callback = cb;
	timer->arg = arg;
	timer->ticks = 0;
	timer->stopRequested = 0;
	swDmaEventInit(&timer->event);
	return swTimerThreadCreate(&timer->thread, swTimerThreadMain, timer,
				   "sw_timer%d", timer->timerId);
}

void swTimerStop(struct SwWgnTimer *timer)
{
	if (timer->callback == NULL)
		return;
	timer->stopRequested = 1;
	swDmaEventSignal(&timer->event);
	swTimerThreadJoin(&timer->thread);
	swDmaEventFini(&timer->event);
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: timer stopped: %d\n", timer->timerId);
	timer->timerId = -1;
	timer->callback = NULL;
}

void swTxQueuePush(struct SwDepStreamCtx *st, uintptr_t addr, uint32_t len, uint32_t flags)
{
	int idx = st->txW;
	struct SwTxItem *item = &st->txItems[idx];

	st->txW = (unsigned char)(idx + 1);
	item->addr = addr;
	item->length = len;
	item->flags = flags;
	if ((flags & 0x20000000u) != 0 && (flags & 0x00200000u) != 0)
		st->txLast = 1;
}

void swTxQueueFlush(struct SwInfo *swInfo, unsigned char streamId)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];
	unsigned int w = (unsigned int)st->txW;
	unsigned int p = (unsigned int)st->txP;

	if (p > w) {
		unsigned int sent;

		st->txP = 0;
		sent = swDepCorePushInput(swInfo->depCore, streamId, &st->txItems[p], 256u - p);
		if (256u - p > sent)
			st->txP = (int)(p + sent);
		p = (unsigned int)st->txP;
	}
	if (p < w) {
		unsigned int sent;

		st->txP = (int)w;
		sent = swDepCorePushInput(swInfo->depCore, streamId, &st->txItems[p], w - p);
		if (w - p > sent)
			st->txP = (int)(p + sent);
	}
	if (st->txLast != 0 && st->txP == st->txW)
		swSetLastWrap(swInfo, streamId);
	if ((g_swWgnLogMask & 0x80u) != 0)
		swMwTrace("sw: flush: r=%d:p=%d:w=%d\n", st->txR, st->txP, st->txW);
}

void swNotifyTxRp(struct SwInfo *swInfo, unsigned char streamId, int count)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];
	int rp = st->txRp;
	int needNotify = 0;

	if ((g_swWgnLogMask & 0x80u) != 0) {
		swMwTrace("sw: notify_txrp: %d cnt=%d rp=%d %p\n",
			  streamId, count, st->txRp, st->txPort->wsgl);
	}
	while (count > 0) {
		struct SwDmaSgl *sgl = &st->txPort->wsgl[rp];
		uint32_t flags = sgl->flags;

		sgl->flags = flags & 0x7FFFFFFFu;
		if ((flags & 0x30000000u) != 0)
			needNotify = 1;
		if (++rp == st->txPort->nWsgl)
			rp = 0;
		count--;
	}
	st->txRp = rp;
	if ((g_swWgnLogMask & 0x80u) != 0)
		swMwTrace("sw: notify_txrp:%d\n", rp);
	if (needNotify != 0)
		swTxCompCallback(swInfo->wgnInfo, streamId);
}

void swTxCompEvent(struct SwInfo *swInfo, unsigned char streamId, int count)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];
	int r;
	int completed = 0;

	swDmaMutexLock(&st->txMutex);
	if ((g_swWgnLogMask & 0x80u) != 0)
		swMwTrace("sw: sw_tx_comp#%d txrp=%d txpp=%d\n", streamId, st->txR, st->txP);
	r = st->txR;
	while (r != st->txP && count != 0) {
		if ((st->txItems[r].flags & 0x00200000u) != 0)
			completed++;
		r = (unsigned char)(r + 1);
		count--;
	}
	st->txR = r;
	if (completed > 0)
		swNotifyTxRp(swInfo, streamId, completed);
	swDmaMutexUnlock(&st->txMutex);
}

void swTxAvailEvent(struct SwInfo *swInfo, unsigned char streamId)
{
	struct SwDepStreamCtx *st = &swInfo->stream[streamId];

	if ((g_swWgnLogMask & 0x80u) != 0)
		swMwTrace("sw: sw_tx_avail\n");
	swDmaMutexLock(&st->txMutex);
	if (st->setLast != 0) {
		if (st->txP != st->txW && (g_swWgnLogMask & 2u) != 0)
			swMwTrace("sw: sw_tx_comp: warn TXLIST not empty\n");
		if (st->txEmpty == 0 && (g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: sw_tx_comp: sglist not empty\n");
	} else {
		if (st->txP != st->txW)
			swTxQueueFlush(swInfo, streamId);
		if (st->txEmpty == 0 && (unsigned char)(st->txW + 1) != (unsigned char)st->txR)
			swDispatchTxWsgl(swInfo, streamId);
	}
	swDmaMutexUnlock(&st->txMutex);
}

int swTxErrEvent(struct SwInfo *swInfo, unsigned char streamId)
{
	return swTxErrCallback(swInfo->wgnInfo, streamId);
}

void swResetDmaPort(struct SwWgnInfo *info, struct SwDmaPort *port)
{
	while (port->doneHead != NULL) {
		struct SwDmaDoneInfo *node = port->doneHead;

		port->doneHead = node->next;
		swRecycleDoneInfo(port, node);
	}
	swFreeDonePool(info, port);
	if ((port->stateFlags & 0x200u) != 0) {
		swDmaMutexDelete(&port->dmaMutex);
		swDmaMutexDelete(&port->opMutex);
		swDmaEventFini(&port->event);
	}
	memset(port, 0, sizeof(*port));
	port->stateFlags = 1u;
}

static void swFreeSgList(struct SwWgnInfo *info, struct SwDmaMapInfo *map, int dir)
{
	swDmaUnmapSg(map->sgList, map->sgCount, dir);
	swFreeMem(info, map->sgList, (int)(sizeof(struct SwScatterEntry) * (size_t)map->pageMap.numPages));
	map->sgList = NULL;
}

static void swTeardownDmaPortBufs(struct SwWgnInfo *info, struct SwDmaPort *port)
{
	swDmaFreeZero(info, port->wsgl, (int)(sizeof(struct SwDmaSgl) * (size_t)port->nWsgl));
	port->wsgl = NULL;
	swFreeSgList(info, &port->map, port->direction);
	swDmaUnpinUserPages(info, &port->map.pageMap);
}

void swUnmapDmaPort(struct SwWgnInfo *info, struct SwDmaPort *port)
{
	port->stateFlags |= 0x200u;
	swTeardownDmaPortBufs(info, port);
}

static int swBuildSgList(struct SwWgnInfo *info, struct SwDmaMapInfo *map, int dir)
{
	struct SwDmaPageMap *pm = &map->pageMap;
	struct SwScatterEntry *sg;
	uint32_t firstAvail;
	uint32_t remain;
	int sgIdx;
	int pageIdx;

	sg = (struct SwScatterEntry *)swAllocMem(info, sizeof(struct SwScatterEntry) * (size_t)pm->numPages, 0);
	map->sgList = sg;
	if (sg == NULL) {
		swMwTrace("sw: alloc scatter/gather list fail\n");
		return 7;
	}

	firstAvail = SW_PAGE_CHUNK_SIZE - pm->firstOffset;
	remain = pm->totalLen;
	sg[0].page = pm->pages[0];
	sg[0].offset = pm->firstOffset;
	if (firstAvail >= remain) {
		sg[0].length = remain;
		sgIdx = 1;
	} else {
		sg[0].length = firstAvail;
		remain -= firstAvail;
		sgIdx = 1;
		for (pageIdx = 1; pageIdx < pm->numPages; pageIdx++) {
			uint32_t chunk;

			if (remain == 0) {
				swMwTrace("sw: wagner%d: buffer size and pages missmatch\n", info->devIndex);
				swFreeMem(info, sg, (int)(sizeof(struct SwScatterEntry) * (size_t)pm->numPages));
				map->sgList = NULL;
				return 12;
			}
			chunk = (remain <= SW_PAGE_CHUNK_SIZE) ? remain : SW_PAGE_CHUNK_SIZE;
			sg[sgIdx].page = pm->pages[pageIdx];
			sg[sgIdx].offset = 0;
			sg[sgIdx].length = chunk;
			remain -= chunk;
			sgIdx++;
		}
	}

	map->sgCount = sgIdx;
	map->mappedCount = swDmaMapSg(sg, sgIdx, dir);
	return 0;
}

static int swCountWsglPcpSmall(struct SwDmaPort *port)
{
	struct SwScatterEntry *sg = port->map.sgList;
	int count = port->map.mappedCount;
	int total = 0;

	while (count != 0 && sg->dmaLen != 0) {
		total += (int)(sg->dmaLen / port->syncLen);
		sg++;
		count--;
	}
	return total;
}

static int swCountWsglPcpLarge(struct SwDmaPort *port)
{
	uint32_t firstAvail = SW_PAGE_CHUNK_SIZE - port->map.pageMap.firstOffset;
	int numPages = port->map.pageMap.numPages;

	if (numPages == 1)
		return ((int)port->map.pageMap.totalLen < 14) ? 0 : numPages;
	if (firstAvail < 14u)
		numPages--;
	if (port->map.pageMap.totalLen - firstAvail - ((uint32_t)(port->map.pageMap.numPages - 2) << 18) < 14u)
		numPages--;
	return numPages;
}

static int swCountWsglLinear(struct SwDmaPort *port)
{
	struct SwScatterEntry *sg = port->map.sgList;
	int count = port->map.mappedCount;
	uint32_t need = port->syncLen;
	int total = 0;

	while (count != 0 && sg->dmaLen != 0) {
		uint32_t avail = sg->dmaLen;

		while (avail != 0) {
			total++;
			if (avail < need) {
				need -= avail;
				break;
			}
			avail -= need;
			need = port->syncLen;
		}
		sg++;
		count--;
	}
	return total;
}

static int swCountWsglFramed(struct SwDmaPort *port)
{
	struct SwScatterEntry *sg = port->map.sgList;
	int count = port->map.mappedCount;
	uint32_t hdrNeed = port->unitHdrLen;
	uint32_t payNeed = port->unitLen - hdrNeed;
	int inPayload = 0;
	int total = 0;

	while (count != 0 && sg->dmaLen != 0) {
		uint32_t avail = sg->dmaLen;

		while (avail != 0) {
			if (inPayload == 0) {
				if (avail < hdrNeed) {
					hdrNeed -= avail;
					break;
				}
				avail -= hdrNeed;
				hdrNeed = port->unitHdrLen;
				inPayload = 1;
			} else {
				total++;
				if (avail >= payNeed) {
					avail -= payNeed;
					payNeed = port->unitLen - port->unitHdrLen;
					inPayload = 0;
				} else {
					payNeed -= avail;
					break;
				}
			}
		}
		sg++;
		count--;
	}
	return total;
}

static int swPrepareDmaPortBufs(struct SwWgnInfo *info, struct SwFilePrivate *priv,
				struct SwDmaPort *port, const uint32_t *iov,
				int (*countFn)(struct SwDmaPort *))
{
	int ret;
	int nWsgl;
	struct SwDmaDoneInfo *done;
	struct SwDmaSgl *wsgl;

	(void)priv;
	port->map.bufSize = iov[1];
	if (iov[0] == 0)
		return 11;

	ret = swDmaPinUserPages(info, &port->map.pageMap, iov, port->direction);
	if (ret != 0)
		return ret;
	ret = swBuildSgList(info, &port->map, port->direction);
	if (ret != 0) {
		swDmaUnpinUserPages(info, &port->map.pageMap);
		return ret;
	}

	nWsgl = countFn(port);
	port->nWsgl = nWsgl;
	if (nWsgl == 0) {
		swMwTrace("sw: no buffer space\n");
		swFreeSgList(info, &port->map, port->direction);
		swDmaUnpinUserPages(info, &port->map.pageMap);
		return 7;
	}

	done = swAllocDoneInfo(info, port, 0);
	port->doneTail = done;
	port->doneHead = done;
	if (done == NULL) {
		swMwTrace("sw: alloc done info fail\n");
		swFreeSgList(info, &port->map, port->direction);
		swDmaUnpinUserPages(info, &port->map.pageMap);
		return 7;
	}

	wsgl = (struct SwDmaSgl *)swDmaAllocZero(info, sizeof(struct SwDmaSgl) * (size_t)nWsgl);
	port->wsgl = wsgl;
	if (wsgl != NULL)
		return 0;

	swMwTrace("sw: alloc position(size: %d) fail\n", (int)(sizeof(struct SwDmaSgl) * (size_t)nWsgl));
	swRecycleDoneInfo(port, port->doneHead);
	port->doneTail = NULL;
	port->doneHead = NULL;
	swFreeSgList(info, &port->map, port->direction);
	swDmaUnpinUserPages(info, &port->map.pageMap);
	return 7;
}

static void swCleanupDmaPortSetup(struct SwWgnInfo *info, struct SwDmaPort *port)
{
	swTeardownDmaPortBufs(info, port);
	while (port->doneHead != NULL) {
		struct SwDmaDoneInfo *node = port->doneHead;

		port->doneHead = node->next;
		swRecycleDoneInfo(port, node);
	}
	port->doneTail = NULL;
	port->doneHead = NULL;
}

static int swBuildFramedWsgl(struct SwWgnInfo *info, struct SwFilePrivate *priv,
			     struct SwDmaPort *port, const uint32_t *iov)
{
	int ret;
	struct SwScatterEntry *sg;
	struct SwDmaSgl *sgl;
	int sgRemain;
	int inPayload = 0;
	uint32_t hdrRemain;
	uint32_t payRemain;
	uint32_t unitAccum = 0;
	uint32_t bufOff = 0;
	int syncUnits = 0;
	uint32_t pageOff;
	int pageIdx = 0;

	ret = swPrepareDmaPortBufs(info, priv, port, iov, swCountWsglFramed);
	if (ret != 0)
		return ret;

	sg = port->map.sgList;
	if (port->direction == 1) {
		struct SwDmaDoneInfo *lastDone = swAllocDoneInfo(info, port, 0);

		port->doneTail = lastDone;
		port->doneHead->next = lastDone;
		if (lastDone == NULL) {
			swMwTrace("sw: alloc last done info fail\n");
			swCleanupDmaPortSetup(info, port);
			return 7;
		}
		if ((sg->dmaAddr & 3u) != 0) {
			swMwTrace("sw: start address(%08x) of TX?DMAC is not a word alignment\n",
				  (uint32_t)sg->dmaAddr);
			swCleanupDmaPortSetup(info, port);
			return 8;
		}
	}

	sgl = port->wsgl;
	sgRemain = port->map.mappedCount;
	hdrRemain = port->unitHdrLen;
	payRemain = port->unitLen - hdrRemain;
	pageOff = port->map.pageMap.firstOffset;

	while (sgRemain != 0 && sg->dmaLen != 0) {
		uintptr_t dmaAddr = sg->dmaAddr;
		uint32_t avail = sg->dmaLen;

		while (avail != 0) {
			if (inPayload != 0) {
				if (avail < payRemain) {
					sgl->xferLen = avail;
					sgl->flags = (port->direction == 0) ? 0x80000000u : 0x00100000u;
					sgl->offset = bufOff;
					sgl->length = avail;
					sgl->vaddr = (void *)(port->map.pageMap.pages[pageIdx] + pageOff);
					sgl->paddr = dmaAddr;
					sgl++;
					bufOff += avail;
					payRemain -= avail;
					pageOff += avail;
					while (pageOff >= SW_PAGE_CHUNK_SIZE) {
						pageOff -= SW_PAGE_CHUNK_SIZE;
						pageIdx++;
					}
					break;
				}
				sgl->xferLen = payRemain;
				unitAccum += port->unitLen;
				if (unitAccum < port->syncLen) {
					sgl->flags = (port->direction == 0) ? 0x80000000u : 0x00100000u;
				} else {
					sgl->flags = (port->direction == 0) ? 0x90000000u : 0x10100000u;
					unitAccum = 0;
				}
				sgl->offset = bufOff;
				sgl->length = payRemain;
				sgl->vaddr = (void *)(port->map.pageMap.pages[pageIdx] + pageOff);
				sgl->paddr = dmaAddr;
				sgl++;
				syncUnits++;
				bufOff += payRemain;
				dmaAddr += payRemain;
				avail -= payRemain;
				pageOff += payRemain;
				while (pageOff >= SW_PAGE_CHUNK_SIZE) {
					pageOff -= SW_PAGE_CHUNK_SIZE;
					pageIdx++;
				}
				payRemain = port->unitLen - port->unitHdrLen;
				inPayload = 0;
			} else {
				if (avail < hdrRemain) {
					bufOff += avail;
					hdrRemain -= avail;
					pageOff += avail;
					while (pageOff >= SW_PAGE_CHUNK_SIZE) {
						pageOff -= SW_PAGE_CHUNK_SIZE;
						pageIdx++;
					}
					break;
				}
				dmaAddr += hdrRemain;
				avail -= hdrRemain;
				bufOff += hdrRemain;
				hdrRemain = port->unitHdrLen;
				inPayload = 1;
				pageOff += hdrRemain;
				while (pageOff >= SW_PAGE_CHUNK_SIZE) {
					pageOff -= SW_PAGE_CHUNK_SIZE;
					pageIdx++;
				}
			}
		}
		sgRemain--;
		sg++;
	}

	sgl[-1].flags = (port->direction == 0) ? 0x90000000u : 0x10100000u;
	port->curPos = 0;
	port->endPos = port->nWsgl - 1;
	if (syncUnits >= 2)
		return 0;
	swMwTrace("sw: can't make sure of more than 2 buffers of sync len\n");
	swCleanupDmaPortSetup(info, port);
	return 11;
}

static int swBuildLinearWsgl(struct SwWgnInfo *info, struct SwFilePrivate *priv,
			     struct SwDmaPort *port, const uint32_t *iov)
{
	int ret;
	struct SwScatterEntry *sg;
	struct SwDmaSgl *sgl;
	int sgRemain;
	uintptr_t dmaAddr;
	uint32_t avail;
	uint32_t bufOff = 0;
	uint32_t pageOff;
	int pageIdx = 0;
	uint32_t need;
	int syncUnits = 0;

	ret = swPrepareDmaPortBufs(info, priv, port, iov, swCountWsglLinear);
	if (ret != 0)
		return ret;

	sg = port->map.sgList;
	sgRemain = port->map.mappedCount;
	dmaAddr = sg->dmaAddr;
	avail = sg->dmaLen;
	pageOff = port->map.pageMap.firstOffset;

	if (port->direction == 1) {
		struct SwDmaDoneInfo *lastDone = swAllocDoneInfo(info, port, 0);

		port->doneTail = lastDone;
		port->doneHead->next = lastDone;
		if (lastDone == NULL) {
			swMwTrace("sw: alloc last done info fail\n");
			swCleanupDmaPortSetup(info, port);
			return 7;
		}
		if ((dmaAddr & 3u) != 0) {
			uint32_t skip = 4u - (uint32_t)(dmaAddr & 3u);

			swMwTrace("sw: start address(%08x) of TX?DMAC is not a word alignment\n",
				  (uint32_t)dmaAddr);
			bufOff = skip;
			if (skip < avail) {
				dmaAddr += skip;
				avail -= skip;
				pageOff += skip;
			} else {
				sg++;
				sgRemain--;
				pageIdx = 1;
				pageOff = 0;
				dmaAddr = sg->dmaAddr;
				avail = sg->dmaLen;
			}
		}
	}

	sgl = port->wsgl;
	need = port->syncLen;
	while (sgRemain > 0 && sg->dmaLen != 0) {
		if (avail != 0) {
			while (avail >= need) {
				sgl->xferLen = need;
				sgl->flags = (port->direction == 0) ? 0x90000000u : 0x10100000u;
				sgl->offset = bufOff;
				sgl->length = need;
				sgl->vaddr = (void *)(port->map.pageMap.pages[pageIdx] + pageOff);
				sgl->paddr = dmaAddr;
				sgl++;
				syncUnits++;
				bufOff += need;
				dmaAddr += need;
				avail -= need;
				pageOff += need;
				while (pageOff >= SW_PAGE_CHUNK_SIZE) {
					pageOff -= SW_PAGE_CHUNK_SIZE;
					pageIdx++;
				}
				need = port->syncLen;
			}
			if (avail != 0) {
				sgl->xferLen = avail;
				sgl->flags = (port->direction == 0) ? 0x80000000u : 0x00100000u;
				sgl->offset = bufOff;
				sgl->length = avail;
				sgl->vaddr = (void *)(port->map.pageMap.pages[pageIdx] + pageOff);
				sgl->paddr = dmaAddr;
				sgl++;
				bufOff += avail;
				need -= avail;
				pageOff += avail;
				while (pageOff >= SW_PAGE_CHUNK_SIZE) {
					pageOff -= SW_PAGE_CHUNK_SIZE;
					pageIdx++;
				}
			}
		}
		if (--sgRemain > 0) {
			sg++;
			dmaAddr = sg->dmaAddr;
			avail = sg->dmaLen;
		}
	}

	sgl[-1].flags = (port->direction == 0) ? 0x90000000u : 0x10100000u;
	port->curPos = 0;
	port->endPos = port->nWsgl - 1;
	if (syncUnits >= 2)
		return 0;
	swMwTrace("sw: can't make sure of more than 2 buffers of sync len\n");
	swCleanupDmaPortSetup(info, port);
	return 11;
}

static int swSetupTxDmac(struct SwWgnInfo *info, struct SwFilePrivate *priv,
			 int streamId, int portIdx, const uint32_t *iov)
{
	struct SwStreamState *strm = &info->stream[streamId];
	struct SwDmaPort *port = &strm->port[portIdx];
	uint32_t firstOff;
	uintptr_t startAddr;
	int ret;

	if (iov[1] == 0 || port->syncLen == 0) {
		swMwTrace("sw: setup txdmac error, iov_base: %p, iov_len: %d, sync_len: %d\n",
			  (void *)(uintptr_t)iov[0], (int)iov[1], (int)port->syncLen);
		return 11;
	}
	if ((port->syncLen & 3u) != 0) {
		swMwTrace("sw: sync_len(%d) of TX?DMAC is not a multiple of word\n", (int)port->syncLen);
		return 11;
	}
	if ((port->stateFlags & 2u) != 0) {
		swMwTrace("sw: wagner%d: setup txdmac error, already use DMAC %d\n", info->devIndex, streamId);
		return 12;
	}

	if (port->unitLen != 0 && port->unitHdrLen != 0)
		ret = swBuildFramedWsgl(info, priv, port, iov);
	else
		ret = swBuildLinearWsgl(info, priv, port, iov);
	if (ret != 0)
		return ret;

	strm->txDmapNo = portIdx | 0x80;
	port->lastSyncPos = port->endPos;
	port->stateFlags = (port->stateFlags & ~1u) | 2u;
	port->userBase = (void *)(uintptr_t)iov[0];

	if (port->unitLen != 0 && port->unitHdrLen != 0)
		firstOff = port->wsgl[0].offset - port->unitHdrLen;
	else
		firstOff = port->wsgl[0].offset;
	startAddr = (uintptr_t)iov[0] + firstOff;
	port->doneTail->addr = startAddr;
	port->doneHead->addr = startAddr;
	port->doneHead->length = iov[1] - firstOff;
	port->doneHead->pos = port->endPos;
	port->doneTail->length = 0;

	swDmaEventInit(&port->event);
	swDmaMutexCreate(&port->opMutex);
	swDmaMutexCreate(&port->dmaMutex);
	return 0;
}

static int swBuildPcpLargeWsgl(struct SwWgnInfo *info, struct SwFilePrivate *priv,
			       struct SwDmaPort *port, const uint32_t *iov)
{
	int ret;
	struct SwScatterEntry *sg;
	struct SwDmaSgl *sgl;
	int sgRemain;
	uint32_t pageOff;
	uint32_t pageRem;
	int pageIdx = 0;
	uint32_t bufOff = 0;
	uint32_t accum = 0;
	int syncUnits = 0;

	ret = swPrepareDmaPortBufs(info, priv, port, iov, swCountWsglPcpLarge);
	if (ret != 0)
		return ret;
	if (port->nWsgl < 3) {
		swMwTrace("sw: alloc last done info fail\n");
		swCleanupDmaPortSetup(info, port);
		return 7;
	}

	sg = port->map.sgList;
	sgRemain = port->map.mappedCount;
	sgl = port->wsgl;
	pageOff = port->map.pageMap.firstOffset;
	pageRem = SW_PAGE_CHUNK_SIZE - pageOff;

	while (sgRemain != 0 && sg->dmaLen != 0) {
		uintptr_t dmaAddr = sg->dmaAddr;
		uint32_t avail = sg->dmaLen;

		if (pageRem < 14u) {
			avail -= pageRem;
			dmaAddr += pageRem;
			bufOff += pageRem;
			pageRem = SW_PAGE_CHUNK_SIZE;
			pageIdx++;
			pageOff = 0;
		}
		if (avail != 0) {
			while (avail >= 14u) {
				uint32_t chunk = (avail <= pageRem) ? avail : pageRem;

				accum += chunk;
				sgl->xferLen = chunk;
				sgl->flags = 0x80000000u;
				if (accum >= port->syncLen) {
					accum = 0;
					syncUnits++;
				}
				sgl->offset = bufOff;
				sgl->length = chunk;
				sgl->vaddr = (void *)(port->map.pageMap.pages[pageIdx] + pageOff);
				sgl->paddr = dmaAddr;
				dmaAddr += chunk;
				avail -= chunk;
				bufOff += chunk;
				sgl++;
				pageIdx++;
				pageOff = 0;
				pageRem = SW_PAGE_CHUNK_SIZE;
			}
			if (avail != 0) {
				bufOff += avail;
				pageRem = SW_PAGE_CHUNK_SIZE;
				pageIdx++;
				pageOff = 0;
			}
		}
		sg++;
		sgRemain--;
	}

	sgl[-1].flags = 0x80000000u;
	port->curPos = 0;
	port->endPos = port->nWsgl - 1;
	port->pcpStartPos = 0;
	port->recvLenHi = 0;
	port->recvLenLo = 0;
	port->recvLenPtr = &port->recvLenLo;
	if (syncUnits >= 2)
		return 0;
	swMwTrace("sw: can't make sure of more than 2 buffers of sync len\n");
	swCleanupDmaPortSetup(info, port);
	return 11;
}

static int swBuildPcpSmallWsgl(struct SwWgnInfo *info, struct SwFilePrivate *priv,
			       struct SwDmaPort *port, const uint32_t *iov)
{
	int ret;
	struct SwScatterEntry *sg;
	struct SwDmaSgl *sgl;
	int sgRemain;
	uint32_t pageOff;
	int pageIdx = 0;
	uint32_t bufOff = 0;
	int syncUnits = 0;

	ret = swPrepareDmaPortBufs(info, priv, port, iov, swCountWsglPcpSmall);
	if (ret != 0)
		return ret;

	sg = port->map.sgList;
	sgRemain = port->map.mappedCount;
	sgl = port->wsgl;
	pageOff = port->map.pageMap.firstOffset;

	while (sgRemain != 0 && sg->dmaLen != 0) {
		uintptr_t dmaAddr = sg->dmaAddr;
		uint32_t avail = sg->dmaLen;

		if (avail != 0) {
			while (avail >= port->syncLen) {
				if (pageOff >= SW_PAGE_CHUNK_SIZE) {
					pageIdx++;
					pageOff -= SW_PAGE_CHUNK_SIZE;
				}
				sgl->xferLen = port->syncLen;
				sgl->flags = 0x80000000u;
				sgl->offset = bufOff;
				sgl->length = port->syncLen;
				sgl->vaddr = (void *)(port->map.pageMap.pages[pageIdx] + pageOff);
				sgl->paddr = dmaAddr;
				syncUnits++;
				avail -= port->syncLen;
				sgl++;
				dmaAddr += port->syncLen;
				bufOff += port->syncLen;
				pageOff += port->syncLen;
			}
			if (avail != 0) {
				pageIdx++;
				pageOff = 0;
				bufOff += avail;
			}
		}
		sg++;
		sgRemain--;
	}

	sgl[-1].flags = 0x80000000u;
	port->curPos = 0;
	port->endPos = port->nWsgl - 1;
	if (syncUnits >= 2)
		return 0;
	swMwTrace("sw: can't make sure of more than 2 buffers of sync len\n");
	swCleanupDmaPortSetup(info, port);
	return 11;
}

static int swSetupRxDmac(struct SwWgnInfo *info, struct SwFilePrivate *priv,
			 int streamId, int portIdx, const uint32_t *iov, int isPcpOut)
{
	struct SwStreamState *strm = &info->stream[streamId];
	struct SwDmaPort *port = &strm->port[portIdx];
	uint32_t firstOff;
	int ret;

	if (iov[1] == 0 || port->syncLen == 0) {
		swMwTrace("sw: wagner%d: setup rxdmac, iov_base: %p, iov_len: %d, sync_len: %d\n",
			  info->devIndex, (void *)(uintptr_t)iov[0], (int)iov[1], (int)port->syncLen);
		return 11;
	}
	if ((port->stateFlags & 2u) != 0) {
		swMwTrace("sw: wagner%d: setup rxdmac, already use DMAC %d\n", info->devIndex, streamId);
		return 12;
	}

	if (isPcpOut != 0) {
		if ((port->syncLen & 3u) != 0) {
			swMwTrace("sw: wagner%d: setup rxdmac, content length(%d) is not word alignment\n",
				  info->devIndex, (int)port->syncLen);
			return 11;
		}
		port->syncLen = 16u * ((port->syncLen - 1u) / 16u) + 30u;
		if (port->syncLen <= SW_PAGE_CHUNK_SIZE)
			ret = swBuildPcpSmallWsgl(info, priv, port, iov);
		else
			ret = swBuildPcpLargeWsgl(info, priv, port, iov);
	} else if (port->unitLen != 0 && port->unitHdrLen != 0) {
		ret = swBuildFramedWsgl(info, priv, port, iov);
	} else {
		ret = swBuildLinearWsgl(info, priv, port, iov);
	}
	if (ret != 0)
		return ret;

	strm->rxDmapNo = portIdx | 0x80;
	port->lastSyncPos = port->endPos;
	port->stateFlags = (port->stateFlags & ~1u) | 2u;
	port->userBase = (void *)(uintptr_t)iov[0];

	if (port->unitLen != 0 && port->unitHdrLen != 0)
		firstOff = port->wsgl[0].offset - port->unitHdrLen;
	else
		firstOff = port->wsgl[0].offset;
	port->doneHead->addr = (uintptr_t)iov[0] + firstOff;

	swDmaEventInit(&port->event);
	swDmaMutexCreate(&port->opMutex);
	swDmaMutexCreate(&port->dmaMutex);
	return 0;
}

void swMarkDmaDone(struct SwWgnInfo *info, int streamId, int portIdx)
{
	struct SwDmaPort *port = &info->stream[streamId].port[portIdx];

	if ((port->stateFlags & 2u) == 0)
		return;
	if (streamId > 0) {
		swMwTrace("sw: wagner%d: Unknown DMAC\n", info->devIndex);
		port->stateFlags |= 0x10000u;
	}
	port->stateFlags = (port->stateFlags & ~2u) | 0x10u;
	if (port->direction == 1) {
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: strmno#%d: tx DMA%d Done\n", streamId, portIdx);
		info->stats.txStat[streamId].state = 16u;
	} else {
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: strmno#%d: rx DMA%d Done\n", streamId, portIdx);
		info->stats.rxStat[streamId].state = 16u;
	}
}

void swPostDoneDma(struct SwWgnInfo *info, int streamId, int portIdx, struct SwFilePrivate *priv)
{
	struct SwStreamState *strm = &info->stream[streamId];
	struct SwDmaPort *port = &strm->port[portIdx];
	struct SwDmaPort *other;

	if ((port->stateFlags & 0x10u) == 0 || (port->stateFlags & 0x200u) != 0)
		return;

	other = &strm->port[(portIdx & 1) == 0 ? 1 : 0];
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: post_done_dma: %d DMA%d\n", streamId, portIdx);
	swUnmapDmaPort(info, port);
	if (port->direction == 1)
		strm->txDmapNo = 0;
	else
		strm->rxDmapNo = 0;

	if ((other->stateFlags & 1u) != 0 || (other->stateFlags & 0x200u) != 0) {
		strm->flags |= 4u;
		if ((strm->flags & 3u) != 1u) {
			if ((g_swWgnLogMask & 4u) != 0)
				swMwTrace("sw: post_done: Free Stream #%d\n", streamId);
			strm->route = 0;
			priv->started[streamId] = 0;
			info->stats.route[streamId] = 0;
			if (swStreamDoneCheck(info, (unsigned char)streamId) == 0)
				swFreeStreamResource(info, streamId);
		}
	}
}

static int swCheckUnitLen(struct SwWgnInfo *info, uint32_t bufLen, uint32_t syncLen,
			  uint32_t unitLen, uint32_t unitHdrLen)
{
	if (unitHdrLen >= unitLen) {
		swMwTrace("sw: wagner%d: unit length(%d) <= unit header length(%d)\n",
			  info->devIndex, (int)unitLen, (int)unitHdrLen);
		return 11;
	}
	if ((bufLen % unitLen) != 0) {
		swMwTrace("sw: wagner%d: buffer length(%d) %% unit length(%d) != 0\n",
			  info->devIndex, (int)bufLen, (int)unitLen);
		return 11;
	}
	if ((syncLen % unitLen) != 0) {
		swMwTrace("sw: wagner%d: sync length(%d) %% unit length(%d) != 0\n",
			  info->devIndex, (int)syncLen, (int)unitLen);
		return 11;
	}
	return 0;
}

int swSetupAllDmacs(struct SwWgnInfo *info, struct SwFilePrivate *priv, unsigned char streamId,
		    uint32_t route, const uint32_t *param)
{
	struct SwStreamState *strm = &info->stream[streamId];
	int txStarted = 0;
	int txPortIdx = 0;
	int rxPortIdx;
	struct SwDmaPort *port;
	const uint32_t *iov;
	int ret;

	if ((route & 0x30000u) != 0) {
		if ((route & 0x20000u) != 0) {
			txPortIdx = 0;
			port = &strm->port[0];
			port->syncLen = param[6];
			iov = param + 4;
		} else {
			txPortIdx = 1;
			port = &strm->port[1];
			port->syncLen = param[12];
			iov = param + 10;
			if (param[13] != 0 && param[14] != 0) {
				ret = swCheckUnitLen(info, param[11], port->syncLen, param[13], param[14]);
				if (ret < 0)
					return ret;
				port->unitLen = param[13];
				port->unitHdrLen = param[14];
			}
		}
		if ((port->stateFlags & 2u) != 0) {
			swMwTrace("sw: wagner%d: setup all dmacs error, already use dmap#%d\n",
				  info->devIndex, txPortIdx);
			return 12;
		}
		port->direction = 1;
		ret = swSetupTxDmac(info, priv, streamId, txPortIdx, iov);
		if (ret != 0)
			return ret;
		txStarted = 1;
	}

	if ((route & 3u) == 0)
		return 0;

	if ((route & 2u) != 0 || (route & 0x10000u) != 0) {
		rxPortIdx = 0;
		port = &strm->port[0];
		port->syncLen = param[6];
		iov = param + 4;
		if ((route & 0x10000u) != 0 && (route & 1u) != 0) {
			port->stateFlags |= 8u;
			if (param[7] != 0 && param[8] != 0) {
				ret = swCheckUnitLen(info, param[5], port->syncLen, param[7], param[8]);
				if (ret < 0)
					return ret;
				port->unitLen = param[7];
				port->unitHdrLen = param[8];
			}
		}
	} else {
		rxPortIdx = 1;
		port = &strm->port[1];
		port->syncLen = param[12];
		iov = param + 10;
		if (param[13] != 0 && param[14] != 0) {
			ret = swCheckUnitLen(info, param[11], port->syncLen, param[13], param[14]);
			if (ret < 0)
				return ret;
			port->unitLen = param[13];
			port->unitHdrLen = param[14];
		}
	}

	if ((port->stateFlags & 2u) != 0) {
		swMwTrace("sw: wagner%d: setup all dmacs error, already use dmap#%d\n",
			  info->devIndex, rxPortIdx);
		ret = 12;
	} else {
		port->direction = 0;
		ret = swSetupRxDmac(info, priv, streamId, rxPortIdx, iov, (int)(route & 2u));
		if (ret == 0)
			return 0;
	}

	if (txStarted != 0) {
		swMarkDmaDone(info, streamId, txPortIdx);
		swPostDoneDma(info, streamId, txPortIdx, priv);
		swMarkPortDone(info, streamId, txPortIdx, priv);
	}
	return ret;
}
