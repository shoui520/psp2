/*
 * pocs_hw_regs.c: ScePspemu PocketStation Pokejima (SCPS10104) adhoc bridge,
 * flash/system remap controller, interrupt controller, timers, clock divider,
 * and RTC BCD stepper (0x8102bf58..0x8102cf63).
 */
#include "pocs_internal.h"

#include <stdlib.h>
#include <string.h>

#define POCS_POKEJIMA_MAGIC_TRAP        0xE800U /* Thumb UNDEF hook */
#define POCS_POKEJIMA_WAIT_USEC         10000000U
#define POCS_FLASH_WRITE_CYCLES         79953
#define POCS_CLOCK_SETTLE_CYCLES        7808

#define POCS_ERR_NO_MEMORY              ((int)0x8001000C)
#define POCS_ERR_FATAL_ACCESS           ((int)0x80010008)

/* Pokejima RAM offsets inside 2 KiB internal SRAM (0x810680B8..0x810688B7) */
#define POCS_RAM_POKEJIMA_RESULT_A      0x54EU /* 0x81068606 */
#define POCS_RAM_POKEJIMA_STATE_A       0x550U /* 0x81068608 */
#define POCS_RAM_POKEJIMA_STATE_B       0x552U /* 0x8106860A */
#define POCS_RAM_POKEJIMA_RESULT_B      0x554U /* 0x8106860C */

typedef struct PocsPokejimaPacket {
	uint32_t chunkId;
	uint32_t payloadSize;
	uint8_t  payload[0x2000];
} PocsPokejimaPacket;

typedef struct PocsPokejimaContext {
	SceUID               eventFlag;
	PocsPokejimaPacket   txPacket;
	uint8_t              padAlign[3];
	uint8_t              rxChunkSmall[0x80];
	uint8_t              rxChunkHeader[0x680];
	uint8_t              rxChunkBodyA[0x1D80];
	uint8_t              rxChunkBodyB[0x2000];
	uint8_t              rxChunkTail[0x1000];
	uint8_t              padMutex[1];
	SceKernelLwMutexWork mutex;
} PocsPokejimaContext;

_Static_assert(offsetof(PocsPokejimaContext, txPacket) == 4U, "txPacket offset");
_Static_assert(offsetof(PocsPokejimaContext, rxChunkSmall) == 8207U, "rxChunkSmall offset");
_Static_assert(offsetof(PocsPokejimaContext, rxChunkHeader) == 8335U, "rxChunkHeader offset");
_Static_assert(offsetof(PocsPokejimaContext, rxChunkBodyA) == 9999U, "rxChunkBodyA offset");
_Static_assert(offsetof(PocsPokejimaContext, rxChunkBodyB) == 17551U, "rxChunkBodyB offset");
_Static_assert(offsetof(PocsPokejimaContext, rxChunkTail) == 25743U, "rxChunkTail offset");
_Static_assert(offsetof(PocsPokejimaContext, mutex) == 29840U, "mutex offset");
_Static_assert(sizeof(PocsPokejimaContext) == 0x74B0U, "PocsPokejimaContext size");

static int g_pocsPokejimaEnabled;
static PocsPokejimaContext *g_pocsPokejimaCtx;

static const uint8_t g_pocsDaysInMonth[13] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static inline uint32_t pocsBcdFromBin(uint32_t value)
{
	return ((value / 10U) << 4) | (value % 10U);
}

/* 0x8102bf58 */
static int pocsPokejimaSendChunk(uint32_t chunkId, size_t payloadSize, uint32_t flashOffset)
{
	PocsPokejimaContext *ctx = g_pocsPokejimaCtx;
	ctx->txPacket.chunkId = chunkId;
	ctx->txPacket.payloadSize = (uint32_t)payloadSize;
	memcpy(ctx->txPacket.payload, g_pocsState.flashDataPtr + flashOffset, payloadSize);
	return pocsAdhocSendData((int)(payloadSize + 8U), &ctx->txPacket) == 0;
}

/* 0x8102bf94 */
static int pocsPokejimaStoreChunk(void *dst, const PocsPokejimaPacket *pkt,
                                  size_t payloadSize, uint32_t eventBit)
{
	memcpy(dst, pkt->payload, payloadSize);
	return sceKernelSetEventFlag(g_pocsPokejimaCtx->eventFlag, eventBit);
}

/* 0x8102bfb4 */
static int pocsPokejimaPromptConnect(void)
{
	int alertAction = 0;
	int32_t connResult = 0;

	if (remotePocsPollPowerKeepAlive() != 0 &&
	    pspemuBgDialogShowAlert(&alertAction) >= 0 && alertAction == 3) {
		pspemuMenuSetImeDeadline(sceKernelGetProcessTimeWide());
		if (remotePocsRunConnectDialog(&connResult) == 0 && connResult == 0)
			pspemuSettingsRunPocsMenuFlow(2);
	}
	return 0;
}

/* 0x8102c00c */
int pocsPokejimaIsActive(void)
{
	if (g_pocsPokejimaEnabled != 0)
		return remotePocsGetInsertedSlot() != 0;
	return 0;
}

/* 0x8102c028 */
int pocsPokejimaReceivePacket(unsigned int packetLen, const void *packetData)
{
	const PocsPokejimaPacket *pkt = (const PocsPokejimaPacket *)packetData;

	sceKernelLockLwMutex(&g_pocsPokejimaCtx->mutex, 1, NULL);
	if (packetLen > 7U) {
		switch (pkt->chunkId) {
		case 0U:
			if (pkt->payloadSize == 0x80U && packetLen == 0x88U)
				pocsPokejimaStoreChunk(g_pocsPokejimaCtx->rxChunkSmall, pkt, 0x80U, 1U);
			break;
		case 1U:
			if (pkt->payloadSize == 0x680U && packetLen == 0x688U)
				pocsPokejimaStoreChunk(g_pocsPokejimaCtx->rxChunkHeader, pkt, 0x680U, 2U);
			break;
		case 2U:
			if (pkt->payloadSize == 0x1D80U && packetLen == 0x1D88U)
				pocsPokejimaStoreChunk(g_pocsPokejimaCtx->rxChunkBodyA, pkt, 0x1D80U, 4U);
			break;
		case 3U:
			if (pkt->payloadSize == 0x2000U && packetLen == 0x2008U)
				pocsPokejimaStoreChunk(g_pocsPokejimaCtx->rxChunkBodyB, pkt, 0x2000U, 8U);
			break;
		case 4U:
			if (pkt->payloadSize == 0x1000U && packetLen == 0x1008U)
				pocsPokejimaStoreChunk(g_pocsPokejimaCtx->rxChunkTail, pkt, 0x1000U, 0x10U);
			break;
		default:
			break;
		}
	}
	return sceKernelUnlockLwMutex(&g_pocsPokejimaCtx->mutex, 1);
}

/* 0x8102c140 */
uint32_t pocsPokejimaHandleUndefTrap(uint32_t nextPc)
{
	SceUInt timeout;

	if (g_pocsPokejimaEnabled == 0 || remotePocsGetInsertedSlot() == 0)
		return (uint32_t)pocsCpuRaiseUndef();

	switch (g_pocsState.regs[15]) {
	case 0x0200225CU: /* Send single 128-byte block */
		g_pocsState.regs[1] = g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_A];
		if (g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_A] == 1U) {
			pocsLcdNotifyDirty(2);
			g_pocsState.internalRam[POCS_RAM_POKEJIMA_RESULT_A] = 0U;
			g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_A] = 3U;
			pocsPokejimaPromptConnect();
			if (remotePocsPollPowerKeepAlive() == 0) {
				uint32_t blkOff = ((uint32_t)g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_B] + 77U) << 7;
				g_pocsState.internalRam[POCS_RAM_POKEJIMA_RESULT_A] =
					(uint8_t)pocsPokejimaSendChunk(0U, 0x80U, blkOff);
			}
		}
		return nextPc;

	case 0x02002292U:
		return 0x020022AAU;

	case 0x020023BAU: /* Receive single 128-byte block */
		g_pocsState.regs[0] = g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_A];
		if (g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_A] == 1U) {
			sceKernelClearEventFlag(g_pocsPokejimaCtx->eventFlag, ~1U);
			pocsLcdNotifyDirty(2);
			g_pocsState.internalRam[POCS_RAM_POKEJIMA_RESULT_A] = 0U;
			g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_A] = 3U;
			pocsPokejimaPromptConnect();
			if (remotePocsPollPowerKeepAlive() == 0) {
				timeout = POCS_POKEJIMA_WAIT_USEC;
				if (sceKernelWaitEventFlag(g_pocsPokejimaCtx->eventFlag, 1U,
				                           SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, &timeout) == 0) {
					uint32_t blkOff = ((uint32_t)g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_B] + 77U) << 7;
					sceKernelLockLwMutex(&g_pocsPokejimaCtx->mutex, 1, NULL);
					g_pocsState.internalRam[POCS_RAM_POKEJIMA_RESULT_A] = 1U;
					memcpy(g_pocsState.flashDataPtr + blkOff,
					       g_pocsPokejimaCtx->rxChunkSmall, 0x80U);
					remotePocsFlushVmc();
					sceKernelUnlockLwMutex(&g_pocsPokejimaCtx->mutex, 1);
				}
			}
		}
		return nextPc;

	case 0x020023EEU:
		return 0x02002406U;

	case 0x02002CDAU: /* Send full Pokejima save (4 chunks) */
		g_pocsState.regs[0] = g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_B];
		if (g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_B] == 1U) {
			pocsLcdNotifyDirty(2);
			g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_B] = 3U;
			g_pocsState.internalRam[POCS_RAM_POKEJIMA_RESULT_B] = 0U;
			pocsPokejimaPromptConnect();
			if (remotePocsPollPowerKeepAlive() == 0 &&
			    pocsPokejimaSendChunk(1U, 0x680U, 0x2000U) &&
			    pocsPokejimaSendChunk(2U, 0x1D80U, 111232U) &&
			    pocsPokejimaSendChunk(3U, 0x2000U, 118784U)) {
				g_pocsState.internalRam[POCS_RAM_POKEJIMA_RESULT_B] =
					(uint8_t)pocsPokejimaSendChunk(4U, 0x1000U, 126976U);
			}
		}
		return nextPc;

	case 0x02002DF2U: /* Receive full Pokejima save (4 chunks) */
		g_pocsState.regs[0] = g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_B];
		if (g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_B] == 1U) {
			pocsLcdNotifyDirty(2);
			sceKernelClearEventFlag(g_pocsPokejimaCtx->eventFlag, ~0x1EU);
			g_pocsState.internalRam[POCS_RAM_POKEJIMA_RESULT_B] = 0U;
			g_pocsState.internalRam[POCS_RAM_POKEJIMA_STATE_B] = 3U;
			pocsPokejimaPromptConnect();
			if (remotePocsPollPowerKeepAlive() == 0) {
				timeout = POCS_POKEJIMA_WAIT_USEC;
				if (sceKernelWaitEventFlag(g_pocsPokejimaCtx->eventFlag, 0x1EU,
				                           SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, &timeout) == 0) {
					sceKernelLockLwMutex(&g_pocsPokejimaCtx->mutex, 1, NULL);
					g_pocsState.internalRam[POCS_RAM_POKEJIMA_RESULT_B] = 1U;
					memcpy(g_pocsState.flashDataPtr + 0x2000U, g_pocsPokejimaCtx->rxChunkHeader, 0x680U);
					memcpy(g_pocsState.flashDataPtr + 111232U, g_pocsPokejimaCtx->rxChunkBodyA, 0x1D80U);
					memcpy(g_pocsState.flashDataPtr + 118784U, g_pocsPokejimaCtx->rxChunkBodyB, 0x2000U);
					memcpy(g_pocsState.flashDataPtr + 126976U, g_pocsPokejimaCtx->rxChunkTail, 0x1000U);
					remotePocsFlushVmc();
					sceKernelUnlockLwMutex(&g_pocsPokejimaCtx->mutex, 1);
				}
			}
		}
		return nextPc;

	default:
		return (uint32_t)pocsCpuRaiseUndef();
	}
}

/* 0x8102c440 */
void pocsPokejimaPatchFlashTraps(void)
{
	if (g_pocsPokejimaEnabled != 0 && remotePocsGetInsertedSlot() != 0) {
		uint16_t *flashWords = (uint16_t *)(void *)g_pocsState.flashDataPtr;
		if (flashWords[8519] == 0x2314U)
			flashWords[8519] = POCS_POKEJIMA_MAGIC_TRAP;
		if (flashWords[8693] == 0x2314U)
			flashWords[8693] = POCS_POKEJIMA_MAGIC_TRAP;
		if (flashWords[8492] == 0x5EF9U)
			flashWords[8492] = POCS_POKEJIMA_MAGIC_TRAP;
		if (flashWords[8667] == 0x5EE0U)
			flashWords[8667] = POCS_POKEJIMA_MAGIC_TRAP;
		if (flashWords[9835] == 0x5EE0U)
			flashWords[9835] = POCS_POKEJIMA_MAGIC_TRAP;
		if (flashWords[9975] == 0x5EE0U)
			flashWords[9975] = POCS_POKEJIMA_MAGIC_TRAP;
		remotePocsFlushVmc();
	}
}

/* 0x8102c4f0 */
int pocsPokejimaInit(void)
{
	if (strncmp(pspemuCompatGetTitleId(), "SCPS10104", 10U) != 0)
		return 0;

	g_pocsPokejimaCtx = (PocsPokejimaContext *)malloc(sizeof(PocsPokejimaContext));
	if (g_pocsPokejimaCtx == NULL)
		return POCS_ERR_NO_MEMORY;

	g_pocsPokejimaCtx->eventFlag = sceKernelCreateEventFlag("ScePocketStationPokejima", 0x1000U, 0U, NULL);
	if (g_pocsPokejimaCtx->eventFlag < 0)
		return g_pocsPokejimaCtx->eventFlag;

	{
		int ret = sceKernelCreateLwMutex(&g_pocsPokejimaCtx->mutex, "ScePocketStationPokejima",
		                                 0, 0, NULL);
		if (ret < 0)
			return ret;
	}

	g_pocsPokejimaEnabled = 1;
	return 0;
}

/* 0x8102c57c */
static PocsSchedEvent *pocsFlashWriteEventCallback(PocsSchedEvent *event)
{
	int16_t ctrl = (int16_t)g_pocsState.sysCtrl;

	if (ctrl >= 0 && (ctrl & 0x800) == 0) {
		if ((ctrl & 0x1000) != 0) {
			g_pocsState.sysCtrl = ((g_pocsState.lcdContrast & 0x21U) == 0x21U) ? 0xFFFCU : 0x0800U;
		} else {
			uint8_t *dst = g_pocsState.flashDataPtr + ((uint32_t)g_pocsState.sysCtrl << 7);
			memcpy(dst, g_pocsState.flashPageBuf, sizeof(g_pocsState.flashPageBuf));
			sceCompatCache(PSPEMU_COMPAT_CACHE_WRITEBACK, dst, sizeof(g_pocsState.flashPageBuf));
			pocsSchedAddEvent(event, POCS_FLASH_WRITE_CYCLES);
			sceKernelAtomicSet8(&g_pocsState.saveCountdown, 60);
			g_pocsState.sysCtrl = 0x1000U;
		}
	}
	return event;
}

/* 0x8102c5f4 */
void __attribute__((noreturn)) pocsSysFatalAccess(int isBadMap, int isWrite)
{
	if (isBadMap == -1)
		g_pocsState.flashMode |= (uint8_t)((isWrite != 0) ? 8U : 4U);
	pspemuTriggerFatalError(POCS_ERR_FATAL_ACCESS);
}

/* 0x8102c620 */
uint32_t pocsSysRegRead(uint32_t regOffset, int accessWidth)
{
	if (regOffset >= 0x100U && regOffset <= 0x13FU)
		return g_pocsState.flashBankMap[(regOffset >> 2) & 0xFU];

	switch (regOffset) {
	case 0x00U:
		return g_pocsState.clockMode;
	case 0x04U: {
		uint32_t val = g_pocsState.flashMode;
		g_pocsState.flashMode = 0U;
		return val;
	}
	case 0x08U:
		return g_pocsState.flashReg;
	case 0x0CU:
		return g_pocsState.lcdRefreshRate;
	case 0x10U: {
		uint32_t val = g_pocsState.lcdContrast;
		if ((int16_t)g_pocsState.sysCtrl < 0 || (g_pocsState.sysCtrl & 0x1000U) == 0U)
			val += 4U;
		return val;
	}
	case 0x300U:
	case 0x302U: {
		uint32_t val = g_pocsState.serialId >> (8U * (regOffset & 3U));
		if (accessWidth == -2)
			return val & 0xFFFFU;
		return val;
	}
	case 0x308U:
		return 63U;
	default:
		return 0U;
	}
}

/* 0x8102c6e8 */
void pocsSysRegWrite(uint32_t regOffset, uint16_t value)
{
	if (regOffset >= 0x100U && regOffset <= 0x13FU) {
		if ((g_pocsState.clockMode & 2U) == 0U)
			g_pocsState.flashBankMap[(regOffset >> 2) & 0xFU] = (uint8_t)(value & 0xFU);
		return;
	}

	switch (regOffset) {
	case 0x00U: {
		uint8_t mode = (uint8_t)((value | (g_pocsState.clockMode & 1U)) & 3U);
		g_pocsState.clockMode = mode;
		if ((mode & 2U) == 0U) {
			memset(g_pocsState.flashPhysMap, -2, sizeof(g_pocsState.flashPhysMap));
			return;
		}
		if (g_pocsState.flashReg == 0U) {
			g_pocsState.flashMode |= 2U;
			memset(g_pocsState.flashPhysMap, -2, sizeof(g_pocsState.flashPhysMap));
			return;
		}
		{
			uint32_t mask = g_pocsState.flashReg;
			int bank = 0;
			memset(g_pocsState.flashPhysMap, -1, sizeof(g_pocsState.flashPhysMap));
			while (mask != 0U) {
				if ((mask & 1U) != 0U) {
					uint8_t physIdx = g_pocsState.flashBankMap[bank];
					if (g_pocsState.flashPhysMap[physIdx] != -1) {
						g_pocsState.flashMode |= 1U;
						memset(g_pocsState.flashPhysMap, -2, sizeof(g_pocsState.flashPhysMap));
						return;
					}
					g_pocsState.flashPhysMap[physIdx] = (int8_t)bank;
				}
				mask >>= 1;
				bank++;
			}
		}
		break;
	}
	case 0x08U:
		if ((g_pocsState.clockMode & 2U) == 0U) {
			g_pocsState.flashReg = value;
			pocsPokejimaPatchFlashTraps();
		}
		break;
	case 0x0CU:
		g_pocsState.lcdRefreshRate = (uint8_t)((value & 0x39U) | 4U);
		break;
	case 0x10U:
		g_pocsState.lcdContrast = (uint8_t)(value & 0x7BU);
		if ((g_pocsState.sysCtrl & 0x1000U) == 0U)
			g_pocsState.sysCtrl = ((value & 0x21U) == 0x21U) ? 0xFFFCU : 0x0800U;
		break;
	default:
		break;
	}
}

/* 0x8102c844 */
int pocsSysReset(void)
{
	SceNetEtherAddr macAddr;
	int ret;

	g_pocsState.lcdRefreshRate = 20U;
	g_pocsState.lcdContrast = 16U;
	g_pocsState.sysCtrl = 2048U;
	g_pocsState.clockMode = 0U;
	g_pocsState.flashMode = 0U;
	g_pocsState.flashReg = 0U;
	memset(g_pocsState.flashBankMap, 0, sizeof(g_pocsState.flashBankMap));
	memset(g_pocsState.flashPhysMap, -2, sizeof(g_pocsState.flashPhysMap));

	ret = sceNetGetMacAddress(&macAddr, 0);
	if (ret < 0)
		pspemuTriggerFatalError(ret);

	g_pocsState.flashEvent.callback = pocsFlashWriteEventCallback;

	{
		uint16_t loA = (uint16_t)((uint32_t)macAddr.data[0] | ((uint32_t)macAddr.data[1] << 8));
		uint16_t loB = (uint16_t)((uint32_t)macAddr.data[4] | ((uint32_t)macAddr.data[5] << 8));
		uint32_t hiByte = ((uint8_t)(macAddr.data[4] ^ macAddr.data[0]) ^ 0x34U ^ macAddr.data[2]) | 0x4100U;
		g_pocsState.serialId = (uint32_t)(loB ^ loA ^ 0x1234U) + (hiByte << 16);
	}
	return 0;
}

/* 0x8102c8f0 */
int pocsIntcRead(int regOffset)
{
	switch (regOffset) {
	case 0:
		return (int)(g_pocsState.intcEnable & g_pocsState.intcLatch);
	case 4:
		return (int)g_pocsState.intcRawStatus;
	case 8:
		return (int)g_pocsState.intcEnable;
	default:
		return 0;
	}
}

/* 0x8102c930 */
int pocsIntcWrite(int regOffset, uint16_t value)
{
	switch (regOffset) {
	case 8:
		pocsSpiLock();
		g_pocsState.intcEnable |= (uint16_t)(value & 0x3FFFU);
		pocsSpiUnlock();
		if ((value & 0x1000U) != 0U)
			pocsIrdaOnIrqEnabled();
		return pocsCpuCheckPendingIrq();
	case 12:
		pocsSpiLock();
		g_pocsState.intcEnable &= (uint16_t)~value;
		return pocsSpiUnlock();
	case 16:
		pocsSpiLock();
		g_pocsState.intcLatch &= (uint16_t)~(value & 0x3FC0U);
		return pocsSpiUnlock();
	default:
		return regOffset;
	}
}

/* 0x8102c9b0 */
void pocsIntcReset(void)
{
	g_pocsState.intcEnable = 0U;
	g_pocsState.intcRawStatus = 0U;
	g_pocsState.intcLatch = 0U;
}

/* 0x8102c9c8 */
static __attribute__((noinline, noclone)) uint32_t pocsTimerGetCount(const PocsTimer *timer)
{
	uint32_t totalShift = g_pocsState.clockShift + timer->prescaleShift;
	int32_t remaining = timer->event.targetCycle - pocsGetCurrentCycle();
	return (uint32_t)((1 << totalShift) + remaining - 1) >> totalShift;
}

/* 0x8102ca08 */
static PocsSchedEvent *pocsTimerEventCallback(PocsSchedEvent *event)
{
	PocsTimer *timer = (PocsTimer *)event;
	int32_t ticks = (timer->reloadValue == 0U) ? 0x10000 : (int32_t)timer->reloadValue;
	int32_t delay = pocsScaleCpuCycles(ticks) << timer->prescaleShift;

	g_pocsState.intcLatch |= timer->irqMask;
	return pocsSchedAddEvent(event, delay);
}

/* 0x8102ca48 */
static __attribute__((noinline, noclone)) void pocsTimerRescaleAll(int oldShift, int newShift)
{
	int i;
	for (i = 0; i < 3; i++) {
		PocsTimer *timer = &g_pocsState.timers[i];
		if (timer->event.prev != NULL) {
			int32_t remaining = timer->event.targetCycle - pocsGetCurrentCycle();
			PocsSchedEvent *prevEv = timer->event.prev;
			PocsSchedEvent *nextEv = timer->event.next;

			if (newShift >= 0) {
				if (newShift <= 7)
					remaining <<= newShift;
				else if (newShift == 8)
					remaining *= 244;
			}
			if (oldShift >= 0) {
				if (oldShift <= 7)
					remaining >>= oldShift;
				else if (oldShift == 8)
					remaining /= 244;
			}

			prevEv->next = nextEv;
			nextEv->prev = prevEv;
			pocsSchedAddEvent(&timer->event, remaining);
		}
	}
}

/* 0x8102cae0 */
uint32_t pocsTimerRead(uint32_t regOffset)
{
	uint32_t timerIdx = regOffset >> 4;
	if (timerIdx > 2U)
		return 0U;

	switch (regOffset & 0xFU) {
	case 0U:
		return g_pocsState.timers[timerIdx].reloadValue;
	case 4U:
		if ((g_pocsState.timers[timerIdx].control & 4U) != 0U)
			return pocsTimerGetCount(&g_pocsState.timers[timerIdx]);
		return g_pocsState.timers[timerIdx].currentValue;
	case 8U:
		return g_pocsState.timers[timerIdx].control;
	default:
		return 0U;
	}
}

/* 0x8102cb24 */
uint32_t pocsTimerWrite(uint32_t regOffset, uint16_t value)
{
	uint32_t timerIdx = regOffset >> 4;
	uint32_t subReg = regOffset & 0xFU;
	PocsTimer *timer;

	if (timerIdx > 2U)
		return regOffset;

	timer = &g_pocsState.timers[timerIdx];
	if (subReg == 0U) {
		if ((timer->control & 4U) == 0U) {
			timer->reloadValue = value;
			timer->currentValue = value;
		}
		return subReg;
	}

	if (subReg == 8U) {
		uint8_t prevCtrl = timer->control;
		int32_t ticks;

		timer->control = (uint8_t)(value & 7U);
		if ((prevCtrl & 4U) != 0U) {
			ticks = (int32_t)pocsTimerGetCount(timer);
			timer->currentValue = (uint16_t)ticks;
			if (timer->event.prev != NULL)
				pocsSchedRemoveEvent(&timer->event);
		} else {
			ticks = (timer->currentValue == 0U) ? 0x10000 : (int32_t)timer->currentValue;
		}

		if ((value & 4U) != 0U) {
			uint32_t divSel = value & 3U;
			uint8_t shift = (divSel == 3U) ? 1U : (uint8_t)(4U * divSel + 1U);
			timer->prescaleShift = shift;
			return (uint32_t)(uintptr_t)pocsSchedAddEvent(&timer->event,
			                                              pocsScaleCpuCycles(ticks) << shift);
		}
	}
	return subReg;
}

/* 0x8102cbc4 */
int pocsTimerReset(void)
{
	int i;
	for (i = 0; i < 3; i++) {
		PocsTimer *timer = &g_pocsState.timers[i];
		timer->reloadValue = 0xFFFFU;
		timer->currentValue = 0xFFFFU;
		timer->prescaleShift = 1U;
		timer->control = 0U;
		timer->event.callback = pocsTimerEventCallback;
		if (timer->event.prev != NULL)
			pocsSchedRemoveEvent(&timer->event);
	}
	g_pocsState.timers[0].irqMask = 0x0080U;
	g_pocsState.timers[1].irqMask = 0x0100U;
	g_pocsState.timers[2].irqMask = 0x2000U;
	return 0x0080;
}

/* 0x8102cc2c */
static PocsSchedEvent *pocsClockEventCallback(PocsSchedEvent *event)
{
	int oldShift = (int)g_pocsState.clockShift;
	int clkSel = (int)(g_pocsState.clockPendingReg & 0xFU);
	int newShift = (clkSel < 8) ? (8 - clkSel) : 0;

	if (clkSel != 0)
		g_pocsState.clockPendingReg = (uint8_t)(clkSel | 0x10U);

	if (oldShift != newShift) {
		int32_t oldScaled;
		int32_t newCycles;
		int32_t newScaled;

		pocsTimerRescaleAll(oldShift, newShift);
		oldScaled = pocsScaleCpuCycles(g_pocsState.cpuCyclesLeft);

		if (newShift <= 7) {
			newCycles = oldScaled >> newShift;
			newScaled = newCycles << newShift;
		} else if (newShift == 8) {
			newCycles = oldScaled / 244;
			newScaled = newCycles * 244;
		} else {
			newCycles = oldScaled;
			newScaled = oldScaled;
		}

		g_pocsState.cpuCyclesLeft = newCycles;
		g_pocsState.clockShift = (uint8_t)newShift;
		g_pocsState.schedTargetCycle += newScaled - oldScaled;
	}
	return event;
}

/* 0x8102ccd8 */
int pocsClockRead(int regOffset)
{
	if (regOffset != 0)
		return 0;
	return (int)g_pocsState.clockPendingReg;
}

/* 0x8102ccec */
PocsSchedEvent *pocsClockWrite(int regOffset, uint8_t value)
{
	if (regOffset == 0) {
		g_pocsState.clockPendingReg = value & 0x0FU;
	} else if (regOffset == 4) {
		uint8_t haltBit = value & 1U;
		if (g_pocsState.clockStatusReg == haltBit)
			return (PocsSchedEvent *)(uintptr_t)regOffset;
		g_pocsState.clockStatusReg = haltBit;
		if (haltBit != 0U) {
			if (g_pocsState.cpuCyclesLeft > 0)
				g_pocsState.cpuCyclesLeft = 0;
		} else if ((g_pocsState.clockPendingReg & 0x0FU) == 0U) {
			return (PocsSchedEvent *)(uintptr_t)regOffset;
		}
	} else {
		return (PocsSchedEvent *)(uintptr_t)regOffset;
	}

	if (g_pocsState.clockEvent.prev != NULL)
		pocsSchedRemoveEvent(&g_pocsState.clockEvent);
	return pocsSchedAddEvent(&g_pocsState.clockEvent, POCS_CLOCK_SETTLE_CYCLES);
}

/* 0x8102cd60 */
PocsSchedEvent *pocsClockReset(void)
{
	pocsClockWrite(0, 0U);
	g_pocsState.clockEvent.callback = pocsClockEventCallback;
	if (g_pocsState.clockEvent.prev != NULL)
		return pocsSchedRemoveEvent(&g_pocsState.clockEvent);
	return &g_pocsState.clockEvent;
}

/* 0x8102cd90 */
int pocsRtcStepBcdField(int fieldIndex)
{
	int autoCarry = ((g_pocsState.rtcControl & 1U) == 0U);

	switch (fieldIndex) {
	case 0: { /* Seconds (00..59 BCD) */
		uint32_t sec = g_pocsState.rtcTimeBcd & 0x7FU;
		uint32_t next = sec + 1U;
		if ((next & 0xFU) == 0xAU)
			next = sec + 7U;
		if (next == 0x60U) {
			next = 0U;
			if (autoCarry)
				pocsRtcStepBcdField(1);
		}
		g_pocsState.rtcTimeBcd = (g_pocsState.rtcTimeBcd & ~0x7FU) | (next & 0x7FU);
		break;
	}
	case 1: { /* Minutes (00..59 BCD) */
		uint32_t min = (g_pocsState.rtcTimeBcd >> 8) & 0x7FU;
		uint32_t next = min + 1U;
		if ((next & 0xFU) == 0xAU)
			next = min + 7U;
		if (next == 0x60U) {
			next = 0U;
			if (autoCarry)
				pocsRtcStepBcdField(2);
		}
		g_pocsState.rtcTimeBcd = (g_pocsState.rtcTimeBcd & ~0x7F00U) | ((next & 0x7FU) << 8);
		break;
	}
	case 2: { /* Hours (00..23 BCD) */
		uint32_t hour = (g_pocsState.rtcTimeBcd >> 16) & 0x3FU;
		uint32_t next = hour + 1U;
		if ((next & 0xFU) == 0xAU)
			next = hour + 7U;
		if (next == 0x24U) {
			next = 0U;
			if (autoCarry) {
				pocsRtcStepBcdField(3);
				pocsRtcStepBcdField(4);
			}
		}
		g_pocsState.rtcTimeBcd = (g_pocsState.rtcTimeBcd & ~0x3F0000U) | ((next & 0x3FU) << 16);
		break;
	}
	case 3: { /* Day of week (1..7) */
		uint32_t dow = (g_pocsState.rtcTimeBcd >> 24) & 7U;
		uint32_t next = (dow == 7U) ? 1U : (dow + 1U);
		g_pocsState.rtcTimeBcd = (g_pocsState.rtcTimeBcd & ~0x07000000U) | ((next & 7U) << 24);
		break;
	}
	case 4: { /* Day of month (01..28/29/30/31 BCD) */
		uint32_t dayBcd = g_pocsState.rtcDateBcd & 0x3FU;
		uint32_t monBcd = (g_pocsState.rtcDateBcd >> 8) & 0x1FU;
		uint32_t day = dayBcd - 6U * (dayBcd >> 4);
		uint32_t mon = monBcd - 6U * (monBcd >> 4);
		uint32_t maxDay = g_pocsDaysInMonth[mon];
		uint32_t nextBcd;

		if (mon == 2U) {
			uint32_t yearBcd = (g_pocsState.rtcDateBcd >> 16) & 0xFFU;
			uint32_t year = yearBcd - 6U * (yearBcd >> 4);
			if ((year & 3U) == 0U) {
				if (year != 0U || (g_pocsState.internalRam[POCS_RAM_OFF_CENTURY_BCD] & 3U) == 0U)
					maxDay = 29U;
			}
		}

		if (day < maxDay) {
			nextBcd = pocsBcdFromBin(day + 1U);
		} else {
			nextBcd = 1U;
			if (autoCarry)
				pocsRtcStepBcdField(5);
		}
		g_pocsState.rtcDateBcd = (g_pocsState.rtcDateBcd & ~0x3FU) | (nextBcd & 0x3FU);
		break;
	}
	case 5: { /* Month (01..12 BCD) */
		uint32_t mon = (g_pocsState.rtcDateBcd >> 8) & 0x1FU;
		uint32_t next = mon + 1U;
		if ((next & 0xFU) == 0xAU)
			next = mon + 7U;
		if (next == 0x13U) {
			next = 1U;
			if (autoCarry)
				pocsRtcStepBcdField(6);
		}
		g_pocsState.rtcDateBcd = (g_pocsState.rtcDateBcd & ~0x1F00U) | ((next & 0x1FU) << 8);
		break;
	}
	case 6: { /* Year (00..99 BCD) */
		uint32_t year = (g_pocsState.rtcDateBcd >> 16) & 0xFFU;
		uint32_t next = year + 1U;
		if ((next & 0xFU) == 0xAU)
			next = year + 7U;
		if (next == 0xA0U)
			next = 0U;
		g_pocsState.rtcDateBcd = (g_pocsState.rtcDateBcd & ~0xFF0000U) | ((next & 0xFFU) << 16);
		break;
	}
	default:
		break;
	}
	return fieldIndex;
}
