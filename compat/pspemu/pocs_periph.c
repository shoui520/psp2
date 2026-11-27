/*
 * pocs_periph.c: ScePspemu PocketStation Kermit thread launcher, IRDA adhoc
 * pulse codec, LCD/input controller, SPI bridge, and ARM7TDMI ALU/shifter/
 * memory/mode-switch helpers (0x810227d0..0x81025e27).
 */
#include "pocs_bus.h"

#include <ctrl.h>
#include <string.h>

#define POCS_IRDA_EVF_RX_READY          0x01U
#define POCS_IRDA_EVF_SEND_READY        0x02U
#define POCS_IRDA_EVF_CONNECTED         0x04U
#define POCS_IRDA_EVF_RX_DONE           0x08U
#define POCS_IRDA_MAX_PULSES            2048U
#define POCS_IRDA_MAX_PACKET_BYTES      0x2004U
#define POCS_IRDA_TIMEOUT_US_DEFAULT    2000000U
#define POCS_IRDA_SEND_TIMEOUT_DEFAULT  1000000U
#define POCS_IRDA_PROMPT_COOLDOWN_US    60000000U
#define POCS_IRDA_IDLE_FLUSH_CYCLES     10000000U
#define POCS_IRDA_RX_STALE_CYCLES       20000000U
#define POCS_IRDA_STREAM_START_DELAY    3000

#define POCS_IRQ_SPI                    0x0040U
#define POCS_IRQ_IRDA                   0x1000U
#define POCS_IRQ_FIQ_MASK               0x2040U
#define POCS_IRQ_NORMAL_MASK            0xDFBFU

#define POCS_LCD_DUTY_MASK_PACKED       ((int32_t)0xF8421C63U)
#define POCS_ARM_HALFWORD_MODE_LUT      0x76504240U

typedef struct PocsIrdaLegacySlot {
	uint32_t packetCount;
	uint32_t pulseCount;
	uint32_t cursor;
	uint32_t txActive;
	uint32_t pulses[POCS_IRDA_MAX_PULSES];
} PocsIrdaLegacySlot;

_Static_assert(sizeof(PocsIrdaLegacySlot) == 0x2010U, "PocsIrdaLegacySlot size");

typedef struct PocsIrdaPacketBuf {
	uint32_t header;
	uint32_t pulses[POCS_IRDA_MAX_PULSES];
} PocsIrdaPacketBuf;

_Static_assert(sizeof(PocsIrdaPacketBuf) == 0x2004U, "PocsIrdaPacketBuf size");

typedef struct PocsLcdFrameOutput {
	uint32_t rows[32];
	uint32_t ledStatus;
} PocsLcdFrameOutput;

/* Game-specific IRDA quirk states (initialized to -1 in .data) */
static int g_irdaConnectSkipPc = -1;
static int g_irdaScps10075State = -1;
static int g_irdaScps10097State = -1;
static int g_irdaSlps03121State = -1;

/* Shared CPU break flag (0x821C6258) */
int g_pocsCpuBreakRequested;

/* IRDA synchronization and legacy/stream state */
static SceUID g_irdaEventFlag;
static int g_irdaStreamMode;
static int g_irdaLegacySlotIdx;
static PocsIrdaLegacySlot g_irdaLegacySlots[3];
static PocsIrdaPacketBuf g_irdaStreamRxBuf;
static uint32_t g_irdaBreakPc;
static PocsIrdaPacketBuf g_irdaLegacyTxBuf;
static SceKernelLwMutexWork g_irdaMutex;
static SceUInt g_irdaWaitTimeoutUs;
static uint32_t g_irdaPromptCooldownTime;
static SceUInt g_irdaSendTimeoutUs;
static int g_irdaTxIdleState;
static int32_t g_irdaLegacyLastCycle;
static uint32_t g_irdaLegacyTxCount;
static int32_t g_irdaStreamLastTxCycle;
static int32_t g_irdaStreamPulseBias;
static int32_t g_irdaStreamMinGapCycles;
static uint32_t g_irdaStreamRxCursor;
static int g_irdaStreamCompressed;
static int32_t g_irdaStreamRxStartCycle;
static uint32_t g_irdaStreamRxCount;
static int32_t g_irdaStreamLastRxCycle;
static int32_t g_irdaStreamModeSwitchCycle;
static uint32_t g_irdaStreamTxCount;
static int32_t g_irdaStreamRxDelay;
static int g_irdaStreamDropFirst;
static PocsIrdaPacketBuf g_irdaStreamTxBuf;

/* LCD flicker-reduction state */
static uint32_t g_lcdFlickerOverlay[32];
static uint32_t g_lcdFlickerRowMax;
static uint32_t g_lcdFlickerPrev[32];
static uint32_t g_lcdFlickerPrevPrev[32];
static uint32_t g_lcdFlickerRowMin;

/* SPI synchronization */
static SceKernelLwMutexWork g_spiMutex;
static SceUID g_spiEventFlag;

static int pocsIrdaEventCallbackStream(PocsSchedEvent *event);
static int pocsIrdaReceivePacketStream(unsigned int packetLen, const uint8_t *packetData);
static int pocsIrdaWriteStream(int regOffset, int value);
static int pocsIrdaFlushIdleTxStream(void);
static int pocsIrdaResetStream(void);

static inline uint32_t pocsReverseBits32(uint32_t v)
{
	v = ((v >> 1) & 0x55555555U) | ((v & 0x55555555U) << 1);
	v = ((v >> 2) & 0x33333333U) | ((v & 0x33333333U) << 2);
	v = ((v >> 4) & 0x0F0F0F0FU) | ((v & 0x0F0F0F0FU) << 4);
	v = ((v >> 8) & 0x00FF00FFU) | ((v & 0x00FF00FFU) << 8);
	return (v >> 16) | (v << 16);
}

static inline uint32_t pocsCountLeadingZeros32(uint32_t value)
{
	return (value == 0U) ? 32U : (uint32_t)__builtin_clz(value);
}

/* 0x810227d0 */
int remotePocsInit(void)
{
	SceUID thid = sceKernelCreateThread("ScePspemuRemotePocs",
	                                    (SceKernelThreadEntry)remotePocsKermitThread,
	                                    96, 0x1000U, 0U, 0x10000, NULL);
	if (thid >= 0)
		return sceKernelStartThread(thid, 0U, NULL);
	return thid;
}

/* 0x81022810 */
static __attribute__((noinline, noclone)) int pocsIrdaRecordPulseLegacy(uint8_t level)
{
	uint32_t idx = g_irdaLegacyTxCount;
	int32_t now = pocsGetCurrentCycle();
	int32_t delta = now - g_irdaLegacyLastCycle;

	g_irdaLegacyTxBuf.pulses[idx] = (uint32_t)level | ((uint32_t)delta << 8);
	g_irdaLegacyLastCycle = now;
	if (idx < POCS_IRDA_MAX_PULSES)
		g_irdaLegacyTxCount = idx + 1U;
	return delta;
}

/* 0x810228a8 */
static __attribute__((noinline, noclone)) int pocsIrdaWaitFlagUnlocked(uint32_t pattern)
{
	SceUInt timeout = g_irdaWaitTimeoutUs;
	int ret;

	sceKernelUnlockLwMutex(&g_irdaMutex, 1);
	ret = sceKernelWaitEventFlag(g_irdaEventFlag, pattern, 0U, NULL, &timeout);
	sceKernelLockLwMutex(&g_irdaMutex, 1, NULL);
	return ret;
}

/* 0x81022914 */
static PocsSchedEvent *pocsIrdaEventCallback(PocsSchedEvent *event)
{
	if (g_irdaStreamMode != 0) {
		sceKernelLockLwMutex(&g_irdaMutex, 1, NULL);
		pocsIrdaEventCallbackStream(event);
		sceKernelUnlockLwMutex(&g_irdaMutex, 1);
		return event;
	}

	{
		PocsIrdaLegacySlot *slot = &g_irdaLegacySlots[g_irdaLegacySlotIdx];
		uint32_t cursor = slot->cursor;
		int pulseLevel = ((slot->pulses[cursor] & 0xFFU) == 0U);
		int irqLevel = ((g_pocsState.intcRawStatus & POCS_IRQ_IRDA) != 0U);

		if (pulseLevel != irqLevel) {
			pocsSpiLock();
			g_pocsState.intcLatch |= POCS_IRQ_IRDA;
			g_pocsState.intcRawStatus ^= POCS_IRQ_IRDA;
			pocsSpiUnlock();
			cursor = slot->cursor;
		}

		slot->cursor = cursor + 1U;
		if (cursor + 1U < slot->pulseCount) {
			int32_t delay = (int32_t)(slot->pulses[cursor + 1U] >> 8);
			return pocsSchedAddEvent(event, delay);
		}

		slot->packetCount = 0U;
		slot->pulseCount = 0U;
		slot->cursor = 0U;
	}
	return event;
}

/* 0x810229d4 */
int pocsIrdaOnIrqEnabled(void)
{
	int hasKeepStream = pspemuCompatHasFlag(0x20000000U);
	if (hasKeepStream == 0)
		g_irdaStreamMode = 0;
	return hasKeepStream;
}

/* 0x810229ec */
static __attribute__((noinline, noclone)) int pocsIrdaSignalRxDone(void)
{
	int ret = sceKernelClearEventFlag(g_irdaEventFlag, ~POCS_IRDA_EVF_RX_READY);
	if (ret >= 0)
		return sceKernelSetEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_RX_DONE);
	return ret;
}

/* 0x81022a10 */
static __attribute__((noinline, noclone)) int pocsIrdaSignalRxReady(void)
{
	int ret = sceKernelClearEventFlag(g_irdaEventFlag, ~POCS_IRDA_EVF_RX_DONE);
	if (ret >= 0)
		return sceKernelSetEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_RX_READY);
	return ret;
}

/* 0x81022a34 */
static __attribute__((noinline, noclone)) int pocsIrdaWaitRxReady(void)
{
	return pocsIrdaWaitFlagUnlocked(POCS_IRDA_EVF_RX_READY);
}

/* 0x81022a40 */
static __attribute__((noinline, noclone)) int pocsIrdaWaitRxDone(void)
{
	return pocsIrdaWaitFlagUnlocked(POCS_IRDA_EVF_RX_DONE);
}

/* 0x81022a4c */
int pocsIrdaIsConnected(void)
{
	return sceKernelPollEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_CONNECTED, 0U, NULL);
}

/* 0x81022a64 */
int pocsIrdaBeginFrame(int32_t currentCycle)
{
	if (g_irdaScps10075State == 1) {
		g_irdaScps10075State = 0;
		pocsIrdaWaitFlagUnlocked(POCS_IRDA_EVF_RX_READY);
	}

	if (g_irdaStreamMode != 0) {
		if (pspemuCompatHasFlag(0x00800000U) != 0 &&
		    sceKernelPollEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_RX_READY, 0U, NULL) == 0 &&
		    g_pocsState.irdaEvent.prev == NULL) {
			pocsSchedAddEvent(&g_pocsState.irdaEvent, POCS_IRDA_STREAM_START_DELAY);
		}
		return 0;
	}

	g_irdaLegacyLastCycle = currentCycle;
	{
		PocsIrdaLegacySlot *slot = &g_irdaLegacySlots[g_irdaLegacySlotIdx];
		if (slot->packetCount <= 1U) {
			if (slot->txActive != 0U) {
				if (sceKernelPollEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_CONNECTED, 0U, NULL) == 0) {
					SceUInt timeout = g_irdaSendTimeoutUs;
					sceKernelWaitEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_RX_READY,
					                       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, &timeout);
				}
			} else {
				sceKernelClearEventFlag(g_irdaEventFlag, ~POCS_IRDA_EVF_RX_READY);
			}
			slot->txActive = 0U;

			sceKernelLockLwMutex(&g_irdaMutex, 1, NULL);
			{
				int nextIdx = (g_irdaLegacySlotIdx + 1) % 3;
				PocsIrdaLegacySlot *nextSlot = &g_irdaLegacySlots[nextIdx];
				if (nextSlot->packetCount != 0U) {
					if (g_pocsState.irdaEvent.prev != NULL)
						pocsSchedRemoveEvent(&g_pocsState.irdaEvent);
					pocsSchedAddEvent(&g_pocsState.irdaEvent, (int32_t)(nextSlot->pulses[0] >> 8));
					nextIdx = (g_irdaLegacySlotIdx + 1) % 3;
				}
				g_irdaLegacySlotIdx = nextIdx;
			}
			sceKernelUnlockLwMutex(&g_irdaMutex, 1);
		} else {
			slot->packetCount--;
		}
	}
	return 0;
}

/* 0x81022c08 */
int pocsIrdaSignalConnected(void)
{
	return sceKernelSetEventFlag(g_irdaEventFlag,
	                             POCS_IRDA_EVF_SEND_READY | POCS_IRDA_EVF_CONNECTED | POCS_IRDA_EVF_RX_DONE);
}

/* 0x81022c1c */
int pocsIrdaWaitSendReady(void)
{
	SceUInt timeout = g_irdaSendTimeoutUs;
	return sceKernelWaitEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_SEND_READY,
	                              SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, &timeout);
}

/* 0x81022c64 */
int pocsIrdaCancelSession(void)
{
	SceUInt32 numWaitThreads = 0U;
	sceKernelCancelEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_RX_READY | POCS_IRDA_EVF_SEND_READY,
	                         &numWaitThreads);
	return sceKernelClearEventFlag(g_irdaEventFlag, ~POCS_IRDA_EVF_CONNECTED);
}

/* 0x81022ca0 */
int pocsIrdaEndFrame(int32_t currentCycle)
{
	if (g_irdaStreamMode != 0) {
		pocsIrdaFlushIdleTxStream();
		return 0;
	}

	if (g_irdaLegacyTxCount == 0U)
		return 0;

	{
		uint8_t txFlag = g_pocsState.irdaControl & 1U;
		int ret;

		g_irdaLegacyTxBuf.header = (g_irdaLegacyTxBuf.header & 0xFFFFFF00U) | txFlag;
		ret = sceKernelPollEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_CONNECTED, 0U, NULL);
		if (ret == 0)
			ret = pocsAdhocSendData((int)(4U * (g_irdaLegacyTxCount + 1U)), &g_irdaLegacyTxBuf);
		g_irdaLegacyTxCount = 0U;
		g_irdaLegacyTxBuf.header = (uint32_t)txFlag | ((uint32_t)(currentCycle - g_irdaLegacyLastCycle) << 8);
		return ret;
	}
}

/* 0x81022d20 */
int pocsIrdaReceivePacket(unsigned int packetLen, const uint8_t *packetData)
{
	if (g_irdaStreamMode != 0) {
		int ret;
		sceKernelLockLwMutex(&g_irdaMutex, 1, NULL);
		ret = pocsIrdaReceivePacketStream(packetLen, packetData);
		sceKernelUnlockLwMutex(&g_irdaMutex, 1);
		return ret;
	}

	sceKernelLockLwMutex(&g_irdaMutex, 1, NULL);
	{
		int nextIdx = (g_irdaLegacySlotIdx + 1) % 3;
		PocsIrdaLegacySlot *slot = &g_irdaLegacySlots[nextIdx];

		if (packetLen > POCS_IRDA_MAX_PACKET_BYTES &&
		    packetLen + 4U * slot->pulseCount > POCS_IRDA_MAX_PACKET_BYTES) {
			slot->pulseCount = 0U;
			sceKernelUnlockLwMutex(&g_irdaMutex, 1);
			return -1;
		}

		if (pspemuCompatHasFlag(0x04000000U) != 0 && g_irdaTxIdleState == 2) {
			if (packetData[0] == 1U) {
				sceKernelUnlockLwMutex(&g_irdaMutex, 1);
				return 0;
			}
			g_irdaTxIdleState = 0;
		}

		slot->packetCount++;
		memcpy(&slot->pulses[slot->pulseCount], &packetData[4], packetLen - 4U);
		if (slot->pulseCount != 0U) {
			uint32_t hdrWord;
			uint32_t firstPulse = slot->pulses[slot->pulseCount];
			memcpy(&hdrWord, packetData, sizeof(hdrWord));
			slot->pulses[slot->pulseCount] =
				(firstPulse & 0xFFU) | (((hdrWord >> 8) + (firstPulse >> 8)) << 8);
		}
		slot->pulseCount += (packetLen - 4U) >> 2;
		slot->txActive = packetData[0];
	}
	sceKernelUnlockLwMutex(&g_irdaMutex, 1);
	sceKernelSetEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_RX_READY);
	return 0;
}

/* 0x81022e78 */
int pocsIrdaRead(int regOffset)
{
	if (regOffset == 0)
		return g_pocsState.irdaControl;
	if (regOffset == 4)
		return g_pocsState.irdaRxLevel;
	return 0;
}

/* 0x81022ea0 */
static __attribute__((noinline, noclone)) int pocsIrdaMaybePromptConnect(uint8_t ctrlValue)
{
	if (g_irdaPromptCooldownTime != 0U &&
	    sceKernelGetProcessTimeLow() - g_irdaPromptCooldownTime > POCS_IRDA_PROMPT_COOLDOWN_US) {
		g_irdaPromptCooldownTime = 0U;
	}

	if (sceKernelPollEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_CONNECTED, 0U, NULL) != 0 &&
	    g_irdaPromptCooldownTime == 0U &&
	    (ctrlValue & 2U) == 0U) {
		if (pspemuCompatHasFlag(0x08000000U) != 0) {
			if (g_irdaConnectSkipPc == -1) {
				g_irdaConnectSkipPc = (int)g_pocsState.regs[15];
				return 0;
			}
			if ((uint32_t)g_irdaConnectSkipPc == g_pocsState.regs[15])
				return 0;
		}

		g_irdaPromptCooldownTime = sceKernelGetProcessTimeLow();
		{
			int alertAction = 0;
			if (pspemuBgDialogShowAlert(&alertAction) >= 0 && alertAction == 3) {
				int32_t connectResult = 0;
				pspemuMenuSetImeDeadline(sceKernelGetProcessTimeWide());
				if (remotePocsRunConnectDialog(&connectResult) == 0 && connectResult == 0) {
					pspemuSettingsRunPocsMenuFlow(2);
					if (sceKernelPollEventFlag(g_irdaEventFlag, POCS_IRDA_EVF_CONNECTED, 0U, NULL) == 0)
						g_irdaPromptCooldownTime = 0U;
				}
			}
		}
	}
	return 0;
}

/* 0x81022f8c */
int pocsIrdaWrite(int regOffset, int value)
{
	if (g_irdaStreamMode != 0)
		return pocsIrdaWriteStream(regOffset, value);

	if (regOffset != 0) {
		if (regOffset != 4)
			return regOffset;

		if (g_irdaScps10075State == 0 &&
		    (g_pocsState.regs[13] & 0x0E000000U) == 0U &&
		    (g_pocsState.regs[13] & 3U) == 0U &&
		    g_pocsState.regs[14] == 0x0200ADEDU) {
			uint32_t spOff = g_pocsState.regs[13] & 0x7FCU;
			uint32_t retA = pocsLoadLe32(g_pocsState.internalRam, (spOff + 8U) & 0x7FCU);
			uint32_t retB = pocsLoadLe32(g_pocsState.internalRam, (spOff + 48U) & 0x7FCU);
			uint32_t retC = pocsLoadLe32(g_pocsState.internalRam, (spOff + 56U) & 0x7FCU);

			if (retA == 0x0200B063U &&
			    ((retB == 0x0200B1C7U && retC == 0x0200B34DU) ||
			     (retB == 0x0200B207U && retC == 0x0200B2F5U))) {
				g_irdaScps10075State = 1;
				g_pocsCpuBreakRequested = 1;
			}
		}

		{
			uint8_t bit = (uint8_t)(value & 1);
			int forcePulse = pspemuCompatHasFlag(0x02000000U);
			if (forcePulse != 0 || g_pocsState.irdaRxLevel != bit) {
				g_pocsState.irdaRxLevel = bit;
				if ((g_pocsState.irdaControl & 2U) == 0U) {
					if ((g_pocsState.irdaControl & 1U) != 0U) {
						uint8_t level = bit;
						if (bit != 0U)
							level = ((g_pocsState.irdaControl & 4U) != 0U) ? 1U : 2U;
						pocsIrdaRecordPulseLegacy(level);
					}
					if (g_pocsState.irdaRxLevel != 0U)
						return pocsLcdNotifyDirty(2);
				}
			}
			return (int)g_pocsState.irdaRxLevel;
		}
	}

	{
		uint8_t ctrlVal;

		g_pocsCpuBreakRequested = (g_pocsState.regs[15] == g_irdaBreakPc) ? 1 : 0;
		if (g_irdaScps10075State != -1 &&
		    g_pocsState.regs[15] == 0x0200ACB4U &&
		    (g_pocsState.regs[13] & 0x0E000000U) == 0U &&
		    (g_pocsState.regs[13] & 3U) == 0U &&
		    pocsLoadLe32(g_pocsState.internalRam, ((g_pocsState.regs[13] & 0x7FCU) + 4U) & 0x7FCU) == 0x0200A259U) {
			value = 2;
		}

		ctrlVal = (uint8_t)value;
		pocsIrdaMaybePromptConnect(ctrlVal);

		if ((g_pocsState.irdaControl & 3U) == 1U) {
			if ((ctrlVal & 3U) != 1U) {
				pocsIrdaRecordPulseLegacy(0U);
				if ((g_pocsState.irdaControl & 3U) != 0U && (ctrlVal & 2U) == 0U) {
					if (pspemuCompatHasFlag(0x04000000U) != 0)
						g_irdaTxIdleState = 1;
					pocsSpiLock();
					g_pocsState.intcRawStatus |= POCS_IRQ_IRDA;
					pocsSpiUnlock();
					pocsCpuCheckPendingIrq();
				}
			}
		} else if ((g_pocsState.irdaControl & 3U) != 0U) {
			if ((ctrlVal & 2U) == 0U) {
				if (pspemuCompatHasFlag(0x04000000U) != 0)
					g_irdaTxIdleState = 1;
				pocsSpiLock();
				g_pocsState.intcRawStatus |= POCS_IRQ_IRDA;
				pocsSpiUnlock();
				pocsCpuCheckPendingIrq();
			}
		}

		if (pspemuCompatHasFlag(0x04000000U) != 0 &&
		    (g_pocsState.irdaControl & 2U) == 0U &&
		    (ctrlVal & 2U) != 0U) {
			g_irdaTxIdleState = 2;
		}

		if ((ctrlVal & 2U) != 0U) {
			pocsSpiLock();
			g_pocsState.intcRawStatus |= POCS_IRQ_IRDA;
			pocsSpiUnlock();
		}

		g_pocsState.irdaControl = ctrlVal & 0xFU;
	}
	return 0;
}

/* 0x810231bc */
int pocsIrdaInit(void)
{
	int ret = sceKernelCreateLwMutex(&g_irdaMutex, "ScePocketStationInfr", 0U, 0, NULL);
	if (ret >= 0) {
		SceUID evf = sceKernelCreateEventFlag("ScePocketStationInfr", 0x1000U, 0U, NULL);
		g_irdaEventFlag = evf;
		return (evf < 0) ? evf : 0;
	}
	return ret;
}

/* 0x81023208 */
int pocsIrdaReset(void)
{
	uint32_t irdaConfigParam;
	uint32_t timeoutSec;

	g_irdaLegacyTxCount = 0U;
	g_pocsState.irdaControl = 2U;
	g_pocsState.irdaRxLevel = 0U;
	memset(g_irdaLegacySlots, 0, sizeof(g_irdaLegacySlots));
	g_irdaLegacySlotIdx = 0;
	memset(&g_irdaLegacyTxBuf, 0, sizeof(g_irdaLegacyTxBuf));
	g_pocsState.irdaEvent.callback = pocsIrdaEventCallback;
	if (g_pocsState.irdaEvent.prev != NULL)
		pocsSchedRemoveEvent(&g_pocsState.irdaEvent);

	g_irdaStreamMode = (pspemuCompatHasFlag(0x02000000U) == 0) ? 1 : 0;
	g_irdaPromptCooldownTime = 0U;

	irdaConfigParam = (uint32_t)pspemuCompatGetParam(25);
	timeoutSec = (irdaConfigParam >> 24) & 0x7FU;
	g_irdaSendTimeoutUs = 1000000U * timeoutSec;
	if (irdaConfigParam != 0xFFFFFFFFU && g_irdaSendTimeoutUs != 0U) {
		g_irdaWaitTimeoutUs = g_irdaSendTimeoutUs;
	} else {
		g_irdaSendTimeoutUs = POCS_IRDA_SEND_TIMEOUT_DEFAULT;
		g_irdaWaitTimeoutUs = POCS_IRDA_TIMEOUT_US_DEFAULT;
	}

	g_irdaBreakPc = (uint32_t)pspemuCompatGetParam(27);
	pocsIrdaResetStream();
	if (memcmp(pspemuCompatGetTitleId(), "SCPS10075", 9U) == 0)
		g_irdaScps10075State = 0;
	return 0;
}

/* 0x81023310 */
static __attribute__((noinline, noclone)) int pocsIrdaFlushTxStream(uint32_t count)
{
	if (g_irdaStreamCompressed == 0)
		return pocsAdhocSendData((int)(4U * (count + 1U)), &g_irdaStreamTxBuf);

	{
		uint16_t *packed = (uint16_t *)(void *)g_irdaStreamTxBuf.pulses;
		uint32_t outWords = 0U;
		uint32_t i;

		for (i = 0U; i < count; i++) {
			uint32_t entry = g_irdaStreamTxBuf.pulses[i];
			uint16_t levelBit = ((entry & 0xFFU) != 0U) ? 2U : 0U;
			uint32_t delay = entry >> 8;

			if (delay >= 0x4000U) {
				packed[outWords++] = (uint16_t)(levelBit | 1U | ((delay & 0x3FFFU) << 2));
				packed[outWords++] = (uint16_t)(levelBit | 1U | (((delay >> 14) & 0x3FFFU) << 2));
			} else {
				packed[outWords++] = (uint16_t)(levelBit | ((delay & 0x3FFFU) << 2));
			}
		}
		return pocsAdhocSendData((int)(2U * (count + 2U)), &g_irdaStreamTxBuf);
	}
}

/* 0x810233c8 */
static __attribute__((noinline, noclone)) int pocsIrdaRecordPulseStream(uint8_t level)
{
	int32_t now = pocsGetCurrentCycle();
	uint32_t idx = g_irdaStreamTxCount;
	int32_t bias = (g_irdaStreamPulseBias + (int32_t)(g_irdaStreamTxBuf.pulses[idx] >> 8)) & 0xFFFFFF;
	int32_t delta = now - g_irdaStreamLastTxCycle;
	int32_t sum = bias + delta;
	uint32_t encodedDelta = (uint32_t)((sum <= 0) ? delta : sum) & 0xFFFFFFU;

	g_irdaStreamTxBuf.pulses[idx] = (uint32_t)level | (encodedDelta << 8);
	g_irdaStreamLastTxCycle = now;

	if (idx < POCS_IRDA_MAX_PULSES) {
		uint32_t cur = idx;
		if (idx == 0U) {
			if ((g_irdaStreamTxBuf.header & 0xFFU) == 1U) {
				pocsIrdaFlushTxStream(POCS_IRDA_MAX_PULSES);
				cur = g_irdaStreamTxCount;
			}
			g_irdaStreamTxBuf.pulses[0] &= 0xFFU;
			g_irdaStreamTxBuf.header &= 0xFFFFFF00U;
		}
		if (cur == POCS_IRDA_MAX_PULSES - 1U) {
			g_irdaStreamTxCount = 0U;
			g_irdaStreamTxBuf.header = (g_irdaStreamTxBuf.header & 0xFFFFFF00U) | 1U;
		} else {
			g_irdaStreamTxCount = cur + 1U;
		}
		g_irdaStreamTxBuf.pulses[g_irdaStreamTxCount] &= 0xFFU;
	}
	return 0;
}

/* 0x810234bc */
static __attribute__((noinline, noclone)) int pocsIrdaEventCallbackStream(PocsSchedEvent *event)
{
	uint32_t cursor = g_irdaStreamRxCursor;

	if (cursor == 0U) {
		if (g_irdaStreamRxStartCycle != 0) {
			int32_t now = pocsGetCurrentCycle();
			if ((uint32_t)(now - g_irdaStreamRxStartCycle) > POCS_IRDA_RX_STALE_CYCLES) {
				g_irdaStreamRxCount = 0U;
				pocsIrdaSignalRxDone();
			}
		}
		{
			int ret = pocsIrdaWaitRxReady();
			if (ret < 0)
				return ret;
		}
		cursor = g_irdaStreamRxCursor;
	}

	{
		int pulseLevel = ((g_irdaStreamRxBuf.pulses[cursor] & 0xFFU) == 0U);
		int irqLevel = ((g_pocsState.intcRawStatus & POCS_IRQ_IRDA) != 0U);

		if (pulseLevel != irqLevel) {
			pocsSpiLock();
			g_pocsState.intcLatch |= POCS_IRQ_IRDA;
			g_pocsState.intcRawStatus ^= POCS_IRQ_IRDA;
			pocsSpiUnlock();
			cursor = g_irdaStreamRxCursor;
		}

		g_irdaStreamRxCursor = cursor + 1U;
		if (cursor + 1U < g_irdaStreamRxCount) {
			pocsSchedAddEvent(event, (int32_t)(g_irdaStreamRxBuf.pulses[cursor + 1U] >> 8));
			return 0;
		}

		{
			uint8_t contFlag = (uint8_t)(g_irdaStreamRxBuf.header & 0xFFU);
			uint32_t nextDelay = g_irdaStreamRxBuf.pulses[0] >> 8;

			g_irdaStreamRxCursor = 0U;
			g_irdaStreamRxCount = 0U;
			pocsIrdaSignalRxDone();
			if (contFlag == 1U)
				pocsSchedAddEvent(event, (int32_t)nextDelay);
		}
	}
	return 0;
}

/* 0x810235c0 */
static __attribute__((noinline, noclone)) int pocsIrdaReceivePacketStream(unsigned int packetLen, const uint8_t *packetData)
{
	int32_t prevRxCycle = g_irdaStreamLastRxCycle;
	int32_t now;

	if (packetLen > POCS_IRDA_MAX_PACKET_BYTES)
		return -1;

	pocsIrdaWaitRxDone();
	now = pocsGetCurrentCycle();
	g_irdaStreamRxCursor = 0U;
	g_irdaStreamLastRxCycle = now;

	if (g_irdaStreamDropFirst != 0 &&
	    (prevRxCycle == 0 || (uint32_t)(now - prevRxCycle) > POCS_IRDA_RX_STALE_CYCLES)) {
		g_irdaStreamRxCount = 0U;
		return 0;
	}

	if (g_irdaStreamCompressed != 0) {
		uint32_t hdrWord;
		uint32_t count = (packetLen - 4U) >> 1;
		uint32_t srcIdx = 0U;
		uint32_t i;

		memcpy(&hdrWord, packetData, sizeof(hdrWord));
		g_irdaStreamRxBuf.header = hdrWord;
		g_irdaStreamRxCount = count;

		for (i = 0U; i < count; i++) {
			uint16_t loWord = pocsLoadLe16(packetData, 4U + srcIdx * 2U);
			uint8_t level = (uint8_t)((loWord & 2U) != 0U);
			uint32_t delay = (uint32_t)loWord >> 2;
			uint16_t nextWord = pocsLoadLe16(packetData, 4U + (srcIdx + 1U) * 2U);

			if ((nextWord & 1U) != 0U) {
				delay |= ((uint32_t)nextWord >> 2) << 14;
				srcIdx += 2U;
			} else {
				srcIdx += 1U;
			}
			g_irdaStreamRxBuf.pulses[i] = (uint32_t)level | (delay << 8);
		}
	} else {
		memcpy(&g_irdaStreamRxBuf, packetData, packetLen);
		g_irdaStreamRxCount = (packetLen - 4U) >> 2;
	}

	g_irdaStreamRxStartCycle = pocsGetCurrentCycle();
	pocsIrdaSignalRxReady();
	return 0;
}

/* 0x81023744 */
static __attribute__((noinline, noclone)) int pocsIrdaWriteStream(int regOffset, int value)
{
	if (regOffset == 0) {
		uint8_t ctrlVal = (uint8_t)value;
		uint8_t oldMode;

		if (g_irdaScps10097State >= 0) {
			if (g_pocsState.regs[15] == 0x02004858U) {
				g_irdaScps10097State = 0;
				return 0;
			}
			if (g_pocsState.regs[15] == 0x02004390U && g_irdaScps10097State == 0) {
				g_irdaScps10097State = 1;
				return 0;
			}
			g_irdaScps10097State = 0;
		} else if (g_irdaSlps03121State >= 0) {
			uint32_t lr = g_pocsState.regs[14];
			uint32_t sp = g_pocsState.regs[13];

			if (lr == 0x02001850U) {
				if ((sp & 0x0E000000U) == 0U && (sp & 3U) == 0U &&
				    pocsLoadLe32(g_pocsState.internalRam, ((sp & 0x7FCU) + 44U) & 0x7FCU) == 0x02001B78U) {
					g_irdaSlps03121State = 1;
				}
			} else if (g_pocsState.regs[15] == 0x0200191CU && lr != 0x02001A98U) {
				if (g_irdaSlps03121State == 1) {
					g_irdaSlps03121State = 0;
					g_irdaStreamTxCount = 0U;
				}
			}
		}

		if (pocsIrdaIsConnected() != 0) {
			g_irdaStreamRxStartCycle = 0;
			g_irdaStreamLastRxCycle = 0;
		}
		pocsIrdaMaybePromptConnect(ctrlVal);

		oldMode = g_pocsState.irdaControl & 3U;
		if (oldMode == 1U) {
			if ((ctrlVal & 3U) != 1U) {
				if (g_irdaStreamTxCount != 0U) {
					pocsIrdaRecordPulseStream(0U);
					pocsIrdaFlushTxStream(g_irdaStreamTxCount);
					g_irdaStreamTxCount = 0U;
					oldMode = g_pocsState.irdaControl & 3U;
				}
			}
		}

		{
			int enterRx = 0;
			if ((oldMode == 1U && (ctrlVal & 3U) == 0U) ||
			    (oldMode != 1U && oldMode != 0U && (ctrlVal & 3U) == 0U)) {
				enterRx = 1;
			} else if (oldMode == 0U && g_irdaStreamMinGapCycles != -1 && (ctrlVal & 3U) == 0U) {
				int32_t now = pocsGetCurrentCycle();
				if ((uint32_t)(now - g_irdaStreamModeSwitchCycle) > (uint32_t)g_irdaStreamMinGapCycles)
					enterRx = 1;
			}

			if (enterRx) {
				g_irdaStreamModeSwitchCycle = pocsGetCurrentCycle();
				if (g_pocsState.irdaEvent.prev != NULL) {
					pocsSchedRemoveEvent(&g_pocsState.irdaEvent);
					g_irdaStreamRxCursor = 0U;
					pocsIrdaSignalRxDone();
				}
				pocsSpiLock();
				g_pocsState.intcRawStatus |= POCS_IRQ_IRDA;
				pocsSpiUnlock();
				if (pocsIrdaIsConnected() != 0)
					pocsCpuCheckPendingIrq();
				else
					pocsSchedAddEvent(&g_pocsState.irdaEvent, g_irdaStreamRxDelay);
			}
		}

		if ((ctrlVal & 2U) != 0U) {
			pocsSpiLock();
			g_pocsState.intcRawStatus |= POCS_IRQ_IRDA;
			pocsSpiUnlock();
		}
		g_pocsState.irdaControl = ctrlVal & 0xFU;
		return 0;
	}

	if (regOffset == 4) {
		uint8_t newLevel = (uint8_t)(value & 1);
		uint8_t oldLevel = g_pocsState.irdaRxLevel;

		g_pocsState.irdaRxLevel = newLevel;
		if ((g_pocsState.irdaControl & 2U) == 0U) {
			if ((g_pocsState.irdaControl & 1U) != 0U) {
				if (newLevel == oldLevel) {
					uint32_t cur = g_irdaStreamTxBuf.pulses[g_irdaStreamTxCount];
					uint32_t nextDelay = (uint32_t)((int32_t)(cur >> 8) + g_irdaStreamPulseBias);
					g_irdaStreamTxBuf.pulses[g_irdaStreamTxCount] = (cur & 0xFFU) | (nextDelay << 8);
					return g_irdaStreamPulseBias;
				}
				{
					uint8_t pulseLevel = newLevel;
					if (newLevel != 0U)
						pulseLevel = ((g_pocsState.irdaControl & 4U) != 0U) ? 1U : 2U;
					pocsIrdaRecordPulseStream(pulseLevel);
				}
			}
			if (g_pocsState.irdaRxLevel != 0U)
				return pocsLcdNotifyDirty(2);
		}
		return (int)g_pocsState.irdaRxLevel;
	}

	return regOffset;
}

/* 0x810239e8 */
static __attribute__((noinline, noclone)) int pocsIrdaFlushIdleTxStream(void)
{
	if (g_irdaStreamTxCount != 0U) {
		int32_t now = pocsGetCurrentCycle();
		if ((uint32_t)(now - g_irdaStreamLastTxCycle) > POCS_IRDA_IDLE_FLUSH_CYCLES) {
			int ret = pocsIrdaFlushTxStream(g_irdaStreamTxCount);
			g_irdaStreamTxCount = 0U;
			return ret;
		}
	}
	return (int)g_irdaStreamTxCount;
}

/* 0x81023a3c */
int pocsIrdaSetRole(int isAcceptor)
{
	int hasFlag = pspemuCompatHasFlag(0x10000000U);
	if (hasFlag != 0)
		g_irdaStreamDropFirst = isAcceptor;
	return hasFlag;
}

/* 0x81023a58 */
static __attribute__((noinline, noclone)) int pocsIrdaResetStream(void)
{
	int32_t irdaConfigParam;

	g_irdaStreamLastTxCycle = 0;
	g_irdaStreamDropFirst = 0;
	g_irdaStreamTxCount = 0U;
	g_irdaStreamRxCursor = 0U;
	g_irdaStreamRxCount = 0U;
	g_irdaStreamCompressed = 0;

	irdaConfigParam = pspemuCompatGetParam(25);
	g_irdaStreamPulseBias = (irdaConfigParam << 18) >> 18;
	if (irdaConfigParam == -1) {
		g_irdaStreamPulseBias = 0;
		g_irdaStreamRxDelay = POCS_IRDA_STREAM_START_DELAY;
	} else {
		uint32_t delay = ((uint32_t)irdaConfigParam >> 16) & 0xFFU;
		g_irdaStreamRxDelay = (delay == 0U) ? POCS_IRDA_STREAM_START_DELAY : (int32_t)delay;
		g_irdaStreamCompressed = ((irdaConfigParam & 0x4000) != 0) ? 1 : 0;
	}

	if (memcmp(pspemuCompatGetTitleId(), "SCPS10097", 9U) == 0)
		g_irdaScps10097State = 0;
	else if (memcmp(pspemuCompatGetTitleId(), "SLPS03121", 9U) == 0)
		g_irdaSlps03121State = 0;

	g_irdaStreamMinGapCycles = pspemuCompatGetParam(26);
	return g_irdaStreamMinGapCycles;
}

/* 0x81023b30 */
static PocsSchedEvent *pocsLcdEventCallback(PocsSchedEvent *event)
{
	int32_t refreshCycles;

	switch (g_pocsState.lcdControl & 0x38U) {
	case 0x18U:
		refreshCycles = 122880;
		break;
	case 0x28U:
		refreshCycles = 245760;
		break;
	case 0x38U:
		refreshCycles = 491520;
		break;
	default:
		memset(g_pocsState.lcdVramBack, 0, sizeof(g_pocsState.lcdVramBack));
		g_pocsState.lcdDirtyFlags |= 2U;
		return event;
	}

	if (g_lcdFlickerRowMin > 31U || g_lcdFlickerRowMax > 31U) {
		if (pspemuCompatHasFlag(0x01000000U) != 0) {
			memcpy(g_pocsState.lcdVramBack, g_pocsState.lcdVramFront,
			       sizeof(g_pocsState.lcdVramBack));
			pocsSchedAddEvent(event, refreshCycles);
		} else {
			uint8_t phase = g_pocsState.ledState;
			uint32_t *histBuf = (phase == 0U) ? g_pocsState.lcdLineHistory : g_pocsState.lcdShadowBuf;

			memcpy(histBuf, g_pocsState.lcdVramFront, sizeof(g_pocsState.lcdVramFront));
			phase ^= 1U;
			g_pocsState.ledState = phase;
			if (phase == 0U) {
				int row;
				for (row = 0; row < 32; row++)
					g_pocsState.lcdVramBack[row] =
						g_pocsState.lcdLineHistory[row] | g_pocsState.lcdShadowBuf[row];
			}
			pocsSchedAddEvent(event, refreshCycles >> 1);
		}
	} else {
		uint32_t row;

		memcpy(g_pocsState.lcdVramBack, g_pocsState.lcdVramFront,
		       sizeof(g_pocsState.lcdVramBack));
		for (row = g_lcdFlickerRowMin; row <= g_lcdFlickerRowMax; row++) {
			uint32_t cur = g_pocsState.lcdVramFront[row];
			uint32_t prev = g_lcdFlickerPrev[row];
			uint32_t prevPrev = g_lcdFlickerPrevPrev[row];
			uint32_t flicker = (prevPrev ^ cur) | (cur ^ prev);

			g_lcdFlickerPrevPrev[row] = prev;
			g_lcdFlickerPrev[row] = cur;
			g_lcdFlickerOverlay[row] = flicker;
			g_pocsState.lcdVramBack[row] = cur & ~flicker;
		}
		pocsSchedAddEvent(event, refreshCycles);
	}

	g_pocsState.lcdDirtyFlags |= 2U;
	return event;
}

/* 0x81023c9c */
int pocsLcdNotifyDirty(int mode)
{
	uint8_t flags = g_pocsState.lcdDirtyFlags;
	uint8_t addBits;

	if (mode == 0) {
		if ((flags & 4U) != 0U) {
			g_pocsState.lcdDirtyFlags = 10U;
			return 0;
		}
		addBits = 2U;
	} else {
		addBits = ((mode & 3) != 0) ? 10U : 2U;
	}

	flags |= addBits;
	g_pocsState.lcdDirtyFlags = flags;
	if ((flags & 8U) != 0U)
		g_pocsState.ledNotifyFlag = 1U;
	return mode;
}

/* 0x81023ce8 */
int pocsInputPollAndUpdate(void *gxmFramebufferState)
{
	PocsLcdFrameOutput *outFrame = (PocsLcdFrameOutput *)gxmFramebufferState;
	uint8_t keyBits = 0U;

	if (g_pocsState.buttonDebounce != 0U)
		g_pocsState.buttonDebounce--;

	if (g_pocsState.inputSuppress == 0U &&
	    (g_pocsState.standaloneActive != 0U ||
	     (g_pocsState.vmcInserted == 0U && popsHostGetPocsAutoBoot() == 1))) {
		SceCtrlData padData;
		uint32_t buttons;
		uint32_t btnAction = 0U;
		uint32_t btnRight = 0U;
		uint32_t btnLeft = 0U;
		uint32_t btnDown = 0U;
		uint32_t btnUp = 0U;
		int wakePress = 0;

		sceCtrlPeekBufferPositive(0, &padData, 1);
		buttons = padData.buttons;
		if (padData.lx <= 0x3FU)
			buttons |= 0x80U;
		else if (padData.lx > 0xC0U)
			buttons |= 0x20U;
		if (padData.ly <= 0x3FU)
			buttons |= 0x10U;
		else if (padData.ly > 0xC0U)
			buttons |= 0x40U;

		if ((g_pocsState.gpioData & 0x10U) == 0U && g_pocsState.standaloneActive != 0U) {
			btnAction = buttons & 0x2000U;
			btnRight = buttons & 0x0020U;
			btnLeft = buttons & 0x8080U;
			btnDown = buttons & 0x4040U;
			btnUp = buttons & 0x1010U;
		}

		if (g_pocsState.clockStatusReg != 0U) {
			if (pspemuCompatHasFlag(0x00400000U) == 0 ||
			    g_pocsState.internalRam[POCS_RAM_OFF_KERNEL_RUNNING] == 0U) {
				wakePress = 1;
			} else if ((g_pocsState.lcdControl & 8U) == 0U) {
				if (pspemuCompatHasFlag(0x1000U) == 0) {
					wakePress = 1;
				} else if (g_pocsState.buttonDebounce == 0U &&
				           pspemuCompatHasFlag(0x40000000U) != 0) {
					wakePress = 1;
				}
			}
		} else if ((g_pocsState.lcdControl & 8U) == 0U &&
		           g_pocsState.buttonDebounce == 0U &&
		           pspemuCompatHasFlag(0x40000000U) != 0) {
			wakePress = 1;
		}

		if (wakePress) {
			keyBits = 1U;
			g_pocsState.buttonDebounce = 2U;
		}
		if (btnAction != 0U)
			keyBits = 1U;
		if (btnRight != 0U)
			keyBits |= 2U;
		if (btnLeft != 0U)
			keyBits |= 4U;
		if (btnDown != 0U)
			keyBits |= 8U;
		if (btnUp != 0U)
			keyBits |= 0x10U;
	}

	pocsSpiLock();
	g_pocsState.intcRawStatus = (uint16_t)((g_pocsState.intcRawStatus & 0xFFE0U) | (keyBits & 0x1FU));
	g_pocsState.intcLatch = (uint16_t)((g_pocsState.intcLatch & 0xFFE0U) | (keyBits & 0x1FU));
	pocsSpiUnlock();

	if (g_pocsState.ledNotifyFlag != 0U) {
		if ((uint8_t)(g_pocsState.ledNotifyFlag + 1U) == 10U) {
			g_pocsState.ledNotifyFlag = 0U;
			g_pocsState.lcdDirtyFlags |= 2U;
		} else {
			g_pocsState.ledNotifyFlag++;
		}
	}

	if ((g_pocsState.lcdDirtyFlags & 2U) == 0U)
		return 0;

	{
		uint8_t lcdCtrl = g_pocsState.lcdControl;
		int rotated;
		int dutyShift;
		int row;

		g_pocsState.lcdDirtyFlags = (uint8_t)(g_pocsState.lcdDirtyFlags - 2U);
		if ((g_pocsState.gpioData & 0x10U) != 0U)
			lcdCtrl ^= 0x80U;
		rotated = (lcdCtrl & 0x80U) != 0U;
		dutyShift = 4 * (int)(lcdCtrl & 7U);

		for (row = 0; row < 32; row++) {
			int srcRow = rotated ? (31 - row) : row;
			uint32_t dutyMask = (uint32_t)((POCS_LCD_DUTY_MASK_PACKED << (dutyShift + (srcRow >> 3))) >> 31);
			uint32_t pixels = g_pocsState.lcdVramBack[srcRow] & dutyMask;

			outFrame->rows[row] = rotated ? pocsReverseBits32(pixels) : pixels;
		}

		outFrame->ledStatus = (uint32_t)(g_pocsState.lcdDirtyFlags & 0x0CU) | g_pocsState.ledNotifyFlag;
		g_pocsState.lcdDirtyFlags &= (uint8_t)~8U;
	}
	return 1;
}

/* 0x81023f74 */
void *pocsLcdGetFlickerOverlay(void)
{
	if (g_lcdFlickerRowMin > 31U || g_lcdFlickerRowMax > 31U)
		return NULL;
	return g_lcdFlickerOverlay;
}

/* 0x81023fa4 */
int pocsLcdRead(int regOffset)
{
	if (regOffset == 0)
		return g_pocsState.lcdControl;
	if (regOffset == 4)
		return g_pocsState.lcdLine;
	return 0;
}

/* 0x81023fcc */
void *pocsLcdWrite(int regOffset, int value)
{
	if (regOffset == 0) {
		uint8_t prev = g_pocsState.lcdControl;
		g_pocsState.lcdControl = (uint8_t)value;
		if ((((uint8_t)value & (uint8_t)~prev) & 8U) != 0U) {
			if (g_pocsState.lcdEvent.prev != NULL)
				pocsSchedRemoveEvent(&g_pocsState.lcdEvent);
			return pocsLcdEventCallback(&g_pocsState.lcdEvent);
		}
	} else if (regOffset == 4) {
		g_pocsState.lcdLine = (uint8_t)(value & 0x3F);
	}
	return (void *)(uintptr_t)regOffset;
}

/* 0x8102401c */
void *pocsLcdReset(void)
{
	g_pocsState.lcdControl = 0U;
	g_pocsState.lcdLine = 0U;
	g_pocsState.lcdDirtyFlags = 0U;
	memset(g_pocsState.lcdVramBack, 0, sizeof(g_pocsState.lcdVramBack));
	memset(g_pocsState.lcdVramFront, 0, sizeof(g_pocsState.lcdVramFront));
	g_pocsState.lcdEvent.callback = pocsLcdEventCallback;

	{
		int32_t flickerRange = pspemuCompatGetParam(28);
		g_lcdFlickerRowMin = (uint32_t)(flickerRange >> 16);
		g_lcdFlickerRowMax = (uint32_t)(flickerRange & 0xFFFF);
	}
	memset(g_lcdFlickerOverlay, 0, sizeof(g_lcdFlickerOverlay));
	memset(g_lcdFlickerPrev, 0, sizeof(g_lcdFlickerPrev));
	return memset(g_lcdFlickerPrevPrev, 0, sizeof(g_lcdFlickerPrevPrev));
}

/* 0x810240a4 */
static __attribute__((noinline, noclone)) void pocsSpiRaiseIrq(void)
{
	g_pocsState.intcRawStatus |= POCS_IRQ_SPI;
	g_pocsState.intcLatch |= POCS_IRQ_SPI;
}

/* 0x810240c8 */
static __attribute__((noinline, noclone)) void pocsSpiSetIdle(void)
{
	if ((g_pocsState.spiStatus & 2U) == 0U) {
		g_pocsState.spiStatus |= 2U;
		g_pocsState.spiPhase |= 2U;
		if ((g_pocsState.spiPhase & g_pocsState.spiShiftReg) != 0U)
			pocsSpiRaiseIrq();
	}
}

/* 0x81024100 */
int pocsSpiLock(void)
{
	return sceKernelLockLwMutex(&g_spiMutex, 1, NULL);
}

/* 0x81024114 */
int pocsSpiUnlock(void)
{
	return sceKernelUnlockLwMutex(&g_spiMutex, 1);
}

/* 0x81024128 */
int pocsSpiExchange(int portMode, uint8_t txByte)
{
	int result = 511;

	sceKernelAtomicSet8(&g_pocsState.spiActive, 1);
	sceKernelLockLwMutex(&g_spiMutex, 1, NULL);

	if (portMode != 0) {
		if ((g_pocsState.spiStatus & 2U) == 0U) {
			sceKernelUnlockLwMutex(&g_spiMutex, 1);
			sceKernelWaitEventFlag(g_spiEventFlag, 3U,
			                       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
			                       NULL, NULL);
			sceKernelLockLwMutex(&g_spiMutex, 1, NULL);
			if ((g_pocsState.spiStatus & 2U) == 0U) {
				g_pocsState.spiRxByte = txByte;
				g_pocsState.spiCmdFlag = (uint8_t)((uint32_t)portMode >> 8);
				result = g_pocsState.spiTxByte;
				sceKernelSetEventFlag(g_spiEventFlag, 4U);
			}
		}
	} else {
		sceKernelSetEventFlag(g_spiEventFlag, 4U);
		if (g_pocsState.spiCmdFlag == 0U && (g_pocsState.spiStatus & 2U) == 0U) {
			sceKernelUnlockLwMutex(&g_spiMutex, 1);
			sceKernelWaitEventFlag(g_spiEventFlag, 3U, SCE_KERNEL_EVF_WAITMODE_OR, NULL, NULL);
			sceKernelLockLwMutex(&g_spiMutex, 1, NULL);
			pocsSpiSetIdle();
			if ((g_pocsState.spiCtrl & 4U) == 0U)
				sceKernelClearEventFlag(g_spiEventFlag, ~2U);
		}
		sceKernelUnlockLwMutex(&g_spiMutex, 1);
		sceKernelWaitEventFlag(g_spiEventFlag, 3U, SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL, NULL, NULL);
		sceKernelLockLwMutex(&g_spiMutex, 1, NULL);

		if ((g_pocsState.spiCtrl & 4U) != 0U) {
			if ((g_pocsState.spiStatus & 2U) != 0U) {
				g_pocsState.spiStatus = (uint8_t)(g_pocsState.spiStatus - 2U);
				g_pocsState.spiPhase |= 2U;
				if ((g_pocsState.spiPhase & g_pocsState.spiShiftReg) != 0U)
					pocsSpiRaiseIrq();
				if ((g_pocsState.spiCtrl & 4U) != 0U && (g_pocsState.spiStatus & 2U) == 0U)
					g_pocsState.spiCtrl |= 1U;
			}
			g_pocsState.spiRxByte = txByte;
			g_pocsState.spiCmdFlag = 0U;
			result = g_pocsState.spiTxByte;
			sceKernelSetEventFlag(g_spiEventFlag, 4U);
		} else {
			pocsSpiSetIdle();
			sceKernelSetEventFlag(g_spiEventFlag, 2U);
		}
	}

	sceKernelUnlockLwMutex(&g_spiMutex, 1);
	sceKernelAtomicSet8(&g_pocsState.spiActive, 0);
	return result;
}

/* 0x810242fc */
int pocsSpiRead(int regOffset)
{
	switch (regOffset) {
	case 0:
		return g_pocsState.spiCtrl;
	case 4: {
		int status;
		sceKernelLockLwMutex(&g_spiMutex, 1, NULL);
		if (g_pocsState.regs[15] == 0x040007BCU)
			pocsSpiSetIdle();
		status = g_pocsState.spiStatus;
		g_pocsState.spiStatus &= 0xFAU;
		sceKernelUnlockLwMutex(&g_spiMutex, 1);
		return status;
	}
	case 8:
		return g_pocsState.spiRxByte;
	case 16:
		return g_pocsState.spiShiftReg;
	case 20:
		return g_pocsState.spiPhase;
	default:
		return 0;
	}
}

/* 0x810243a4 */
int pocsSpiWrite(int regOffset, int value)
{
	uint32_t pc = g_pocsState.regs[15];
	int waitHost = 0;

	sceKernelLockLwMutex(&g_spiMutex, 1, NULL);

	switch (regOffset) {
	case 0:
		if (value == 6) {
			sceKernelSetEventFlag(g_spiEventFlag, 1U);
			g_pocsState.spiCtrl = 6U;
			if ((g_pocsState.spiStatus & 2U) == 0U)
				g_pocsState.spiCtrl = 7U;
		} else {
			g_pocsState.spiCtrl = (uint8_t)(value & 7);
			if ((value & 4) != 0 && (g_pocsState.spiStatus & 2U) == 0U)
				g_pocsState.spiCtrl |= 1U;
		}
		break;
	case 8:
		g_pocsState.spiTxByte = (uint8_t)value;
		sceKernelSetEventFlag(g_spiEventFlag, 2U);
		if (g_pocsState.spiCmdFlag != 0U)
			pocsSpiSetIdle();
		else if (pc == 0x0400157AU)
			waitHost = 1;
		break;
	case 16:
		g_pocsState.spiShiftReg = (uint8_t)(value & 3);
		if ((g_pocsState.spiShiftReg & g_pocsState.spiPhase) != 0U)
			pocsSpiRaiseIrq();
		break;
	case 24:
		g_pocsState.spiPhase &= (uint8_t)~value;
		if (pc == 0x04001076U || pc == 0x040007E8U) {
			uint32_t biosStatus = pocsLoadLe32(g_pocsState.internalRam, POCS_RAM_OFF_BIOS_STATUS);
			if (pc == 0x04001076U && (biosStatus & 0x200U) == 0U) {
				sceKernelSetEventFlag(g_spiEventFlag, 3U);
				pocsSpiSetIdle();
			}
			if ((g_pocsState.spiStatus & 2U) == 0U)
				waitHost = 1;
		}
		break;
	default:
		break;
	}

	if (waitHost) {
		g_pocsState.spiStatus |= 1U;
		g_pocsState.spiPhase |= 1U;
		if ((g_pocsState.spiPhase & g_pocsState.spiShiftReg) != 0U)
			pocsSpiRaiseIrq();
		sceKernelUnlockLwMutex(&g_spiMutex, 1);
		g_pocsState.spiBusyFlag = 1U;
		pocsUnlockMainMutex();
		sceKernelWaitEventFlag(g_spiEventFlag, 4U, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
		pocsLockMainMutex();
		g_pocsState.spiBusyFlag = 0U;
		return 0;
	}

	return sceKernelUnlockLwMutex(&g_spiMutex, 1);
}

/* 0x8102454c */
int pocsSpiInit(void)
{
	int ret = sceKernelCreateLwMutex(&g_spiMutex, "ScePocketStationSpiLock", 0U, 0, NULL);
	if (ret >= 0) {
		SceUID evf = sceKernelCreateEventFlag("ScePocketStationSpiEvent", 0x1000U, 0U, NULL);
		g_spiEventFlag = evf;
		return (evf < 0) ? evf : 0;
	}
	return ret;
}

/* 0x81024598 */
int pocsSpiReset(void)
{
	sceKernelLockLwMutex(&g_spiMutex, 1, NULL);
	g_pocsState.spiTxByte = 0U;
	g_pocsState.spiCtrl = 2U;
	g_pocsState.spiStatus = 2U;
	g_pocsState.spiShiftReg = 0U;
	g_pocsState.spiPhase = 0U;
	g_pocsState.flashDataPtr = (uint8_t *)pocsGetActiveFlashData();
	return sceKernelUnlockLwMutex(&g_spiMutex, 1);
}

/* 0x810245e0 */
int pocsSpiAbortPending(void)
{
	if (g_pocsState.spiBusyFlag != 0U) {
		sceKernelLockLwMutex(&g_spiMutex, 1, NULL);
		pocsSpiSetIdle();
		sceKernelSetEventFlag(g_spiEventFlag, 4U);
		return sceKernelUnlockLwMutex(&g_spiMutex, 1);
	}
	return 0;
}

/* 0x81024624 */
int pocsSpiSyncHost(void)
{
	pocsLockMainMutex();
	pocsSpiAbortPending();
	return pocsUnlockMainMutex();
}

/* 0x81024634 */
uint32_t pocsCpuAluOp(int op, int setFlags, uint32_t lhs, uint32_t rhs, int carryIn)
{
	uint32_t result;
	uint32_t opLhs = lhs;
	uint32_t opRhs = rhs;

	switch (op) {
	case 0:
		result = lhs & rhs;
		opRhs = 0U;
		break;
	case 1:
		result = lhs ^ rhs;
		opRhs = 0U;
		break;
	case 2:
		result = lhs - rhs;
		break;
	case 3:
		opLhs = rhs;
		opRhs = lhs;
		result = rhs - lhs;
		break;
	case 4:
		result = lhs + rhs;
		break;
	case 5:
		result = lhs + rhs + (uint32_t)carryIn;
		break;
	case 6:
		result = lhs - rhs - (uint32_t)(carryIn == 0);
		break;
	default:
		opLhs = rhs;
		opRhs = lhs;
		result = rhs - lhs - (uint32_t)(carryIn == 0);
		break;
	}

	if (setFlags != 0) {
		uint32_t cpsr = (result & 0x80000000U) |
		                (((pocsCountLeadingZeros32(result) & 0x3FU) << 25) & 0x40000000U) |
		                (g_pocsState.cpsr & 0x3FFFFFFFU);

		if (op > 1) {
			int isSub = ((op & 2) != 0);
			uint32_t lhsSign = opLhs >> 31;
			uint32_t rhsSign = opRhs >> 31;
			uint32_t resSign = result >> 31;
			uint32_t aBit = (uint32_t)isSub ^ lhsSign;
			uint32_t rBit = (uint32_t)(!isSub) ^ resSign;
			uint32_t carry = ((uint32_t)isSub ^ (((rBit | rhsSign) & aBit) | (rBit & rhsSign))) & 1U;
			uint32_t overflow = ((rhsSign & rBit & aBit) | (((rBit | rhsSign | aBit) == 0U) ? 1U : 0U)) & 1U;

			cpsr = (cpsr & 0xCFFFFFFFU) | (carry << 29) | (overflow << 28);
		}
		g_pocsState.cpsr = cpsr;
	}
	return result;
}

/* 0x810246f8 */
uint32_t pocsCpuLogicOp(int op, int setFlags, uint32_t lhs, uint32_t rhs)
{
	uint32_t result = rhs;

	if (op == 0)
		result = lhs | rhs;
	else if (op == 2)
		result = lhs & ~rhs;
	else if (op != 1)
		result = ~rhs;

	if (setFlags != 0) {
		g_pocsState.cpsr = (result & 0x80000000U) |
		                   (((pocsCountLeadingZeros32(result) & 0x3FU) << 25) & 0x40000000U) |
		                   (g_pocsState.cpsr & 0x3FFFFFFFU);
	}
	return result;
}

/* 0x81024734 */
uint32_t pocsCpuMemAccess(uint32_t instr, int accessMode, int baseReg, int dstReg, int offset)
{
	uint32_t addr = g_pocsState.regs[baseReg];
	int preIndex = (instr & 0x01000000U) != 0U;
	int signedOffset = ((instr & 0x00800000U) != 0U) ? offset : -offset;
	uint32_t result = 0U;

	if (preIndex)
		addr += (uint32_t)signedOffset;

	g_pocsState.cpuCyclesLeft += ~(accessMode & 1);

	switch (accessMode) {
	case 0:
		pocsBusWrite32(addr, g_pocsState.regs[dstReg]);
		break;
	case 1:
		result = pocsBusRead32(addr);
		g_pocsState.regs[dstReg] = pocsRotateRight32(result, 8U * (addr & 3U));
		break;
	case 2:
		pocsBusWrite8(addr, g_pocsState.regs[dstReg]);
		break;
	case 3:
		result = pocsBusRead8(addr);
		g_pocsState.regs[dstReg] = result;
		break;
	case 4:
		pocsBusWrite16(addr, g_pocsState.regs[dstReg]);
		break;
	case 5:
		result = pocsBusRead16(addr);
		g_pocsState.regs[dstReg] = result;
		break;
	case 6:
		result = pocsBusReadSigned8(addr);
		g_pocsState.regs[dstReg] = result;
		break;
	default:
		result = pocsBusReadSigned16(addr);
		g_pocsState.regs[dstReg] = result;
		break;
	}

	if (!preIndex) {
		g_pocsState.regs[baseReg] = addr + (uint32_t)signedOffset;
	} else if ((instr & 0x00200000U) != 0U) {
		g_pocsState.regs[baseReg] = addr;
	}
	return result;
}

/* 0x810254cc */
uint32_t pocsCpuHalfwordAccess(uint32_t instr, int shOp, int baseReg, int dstReg)
{
	int rmReg = (int)(instr & 0xFU);
	int accessMode = (int)((POCS_ARM_HALFWORD_MODE_LUT >> (4 * shOp + 16 * ((instr & 0x00100000U) != 0U))) & 7U);
	int offset;

	if ((instr & 0x00400000U) != 0U)
		offset = rmReg | (int)(((instr >> 8) & 0xFU) << 4);
	else
		offset = (int)g_pocsState.regs[rmReg];

	return pocsCpuMemAccess(instr, accessMode, baseReg, dstReg, offset);
}

/* 0x81025510 */
uint32_t pocsCpuBarrelShift(int rmReg, int shiftType, int shiftAmountOrReg, int isRegShift, int updateCarry)
{
	uint32_t value = g_pocsState.regs[rmReg];
	int shiftAmt = shiftAmountOrReg;
	int over32 = 0;

	if (isRegShift != 0) {
		uint32_t rsVal = g_pocsState.regs[shiftAmountOrReg];
		g_pocsState.cpuCyclesLeft--;
		if (shiftAmountOrReg == 15) {
			uint32_t pcDelta = ((g_pocsState.cpsr & 0x20U) != 0U) ? 2U : 4U;
			shiftAmt = (int)((rsVal + pcDelta) & 0xFFU);
		} else {
			shiftAmt = (int)(rsVal & 0xFFU);
		}
		if (shiftAmt == 0)
			return value;
		over32 = (shiftAmt > 32);
	}

	switch (shiftType) {
	case 0: /* LSL */
		if (shiftAmt != 0) {
			uint32_t carryBit;
			if (over32) {
				value = 0U;
				carryBit = 0U;
			} else {
				uint32_t pre = value << (shiftAmt - 1);
				carryBit = pre >> 31;
				value = pre << 1;
			}
			if (updateCarry != 0)
				g_pocsState.cpsr = (g_pocsState.cpsr & 0xDFFFFFFFU) | (carryBit << 29);
		}
		return value;

	case 1: /* LSR */
		if (shiftAmt == 0) {
			if (updateCarry != 0)
				g_pocsState.cpsr = (g_pocsState.cpsr & 0xDFFFFFFFU) | ((value >> 31) << 29);
			return 0U;
		}
		{
			uint32_t carryBit;
			if (over32) {
				value = 0U;
				carryBit = 0U;
			} else {
				uint32_t pre = value >> (shiftAmt - 1);
				carryBit = pre & 1U;
				value = pre >> 1;
			}
			if (updateCarry != 0)
				g_pocsState.cpsr = (g_pocsState.cpsr & 0xDFFFFFFFU) | (carryBit << 29);
		}
		return value;

	case 2: { /* ASR */
		int32_t signedVal = (int32_t)value;
		int32_t pre;
		if (shiftAmt == 0 || over32)
			pre = signedVal >> 31;
		else
			pre = signedVal >> (shiftAmt - 1);
		if (updateCarry != 0)
			g_pocsState.cpsr = (g_pocsState.cpsr & 0xDFFFFFFFU) | (((uint32_t)pre & 1U) << 29);
		return (uint32_t)(pre >> 1);
	}

	default: /* ROR / RRX */
		if (shiftAmt != 0) {
			value = pocsRotateRight32(value, (unsigned int)shiftAmt);
			if (updateCarry != 0)
				g_pocsState.cpsr = (g_pocsState.cpsr & 0xDFFFFFFFU) | ((value >> 31) << 29);
			return value;
		}
		{
			uint32_t carryIn = (g_pocsState.cpsr >> 29) & 1U;
			uint32_t pre = (value & 0xFFFFFFFEU) | carryIn;
			if (updateCarry != 0)
				g_pocsState.cpsr = (g_pocsState.cpsr & 0xDFFFFFFFU) | ((value & 1U) << 29);
			return pocsRotateRight32(pre, 1U);
		}
	}
}

/* 0x81025638 */
int pocsCpuCheckPendingIrq(void)
{
	uint16_t pending = g_pocsState.intcLatch & g_pocsState.intcEnable;
	uint32_t cpsr = g_pocsState.cpsr;

	if (((cpsr & 0x40U) == 0U && (pending & POCS_IRQ_FIQ_MASK) != 0U) ||
	    ((cpsr & 0x80U) == 0U && (pending & POCS_IRQ_NORMAL_MASK) != 0U)) {
		g_pocsState.schedTargetCycle -= pocsScaleCpuCycles(g_pocsState.cpuCyclesLeft);
		g_pocsState.cpuCyclesLeft = 0;
	}
	return 0;
}

static inline uint32_t *pocsGetModeSpLr(int mode)
{
	if ((mode & 3) == 2)
		return g_pocsState.irqRegs;
	return g_pocsState.modeRegs[(mode & 0xC) >> 2];
}

static inline uint32_t *pocsGetModeSpsrPtr(uint32_t mode)
{
	if ((mode & 3U) == 1U)
		return &g_pocsState.spsrFiq;
	if ((mode & 3U) == 2U)
		return &g_pocsState.spsrIrq;
	return &g_pocsState.spsrModes[(mode >> 2) & 3U];
}

/* 0x8102568c */
void pocsCpuSwitchMode(int saveOrRestoreSpsr, int targetMode)
{
	uint32_t oldCpsr = g_pocsState.cpsr;
	uint32_t oldModeGroup = oldCpsr & 3U;
	int enteringException = 1;
	uint32_t newModeGroup;

	g_pocsState.schedTargetCycle -= pocsScaleCpuCycles(g_pocsState.cpuCyclesLeft);
	g_pocsState.cpuCyclesLeft = 0;

	if (targetMode < 0) {
		if (oldModeGroup == 0U) {
			targetMode = 16;
			enteringException = 1;
		} else {
			uint32_t savedSpsr = *pocsGetModeSpsrPtr(oldCpsr);
			targetMode = (int)(savedSpsr & 0x1FU);
			enteringException = (targetMode == 16) ? 1 : 0;
		}
	}

	if ((int)(oldCpsr & 0x1FU) == targetMode)
		return;

	/* Swap out old banked registers */
	if (oldModeGroup == 1U) {
		int i;
		for (i = 0; i < 7; i++) {
			uint32_t saved = g_pocsState.regs[8 + i];
			g_pocsState.regs[8 + i] = g_pocsState.fiqRegs[i];
			g_pocsState.fiqRegs[i] = saved;
		}
	} else if (oldModeGroup == 2U || oldModeGroup == 3U) {
		uint32_t *bank = pocsGetModeSpLr((int)oldCpsr);
		uint32_t sp = g_pocsState.regs[13];
		uint32_t lr = g_pocsState.regs[14];
		g_pocsState.regs[13] = bank[0];
		bank[0] = sp;
		g_pocsState.regs[14] = bank[1];
		bank[1] = lr;
	}

	/* Swap in new banked registers */
	newModeGroup = (uint32_t)targetMode & 3U;
	if (newModeGroup == 1U) {
		int i;
		for (i = 0; i < 7; i++) {
			uint32_t saved = g_pocsState.regs[8 + i];
			g_pocsState.regs[8 + i] = g_pocsState.fiqRegs[i];
			g_pocsState.fiqRegs[i] = saved;
		}
	} else if (newModeGroup == 2U || newModeGroup == 3U) {
		uint32_t *bank = pocsGetModeSpLr(targetMode);
		uint32_t sp = g_pocsState.regs[13];
		uint32_t lr = g_pocsState.regs[14];
		g_pocsState.regs[13] = bank[0];
		bank[0] = sp;
		g_pocsState.regs[14] = bank[1];
		bank[1] = lr;
	}

	if (saveOrRestoreSpsr == 0)
		return;

	if (enteringException != 0 && newModeGroup != 0U) {
		uint32_t curCpsr = g_pocsState.cpsr;
		int wasThumb = (curCpsr & 0x20U) != 0U;

		if (newModeGroup == 1U)
			g_pocsState.spsrFiq = curCpsr;
		else if (newModeGroup == 2U)
			g_pocsState.spsrIrq = curCpsr;
		else
			g_pocsState.spsrModes[(oldCpsr >> 2) & 3U] = curCpsr;

		if (wasThumb)
			curCpsr -= 0x20U;
		g_pocsState.regs[14] = g_pocsState.regs[15] - (wasThumb ? 2U : 4U);
		g_pocsState.cpsr = curCpsr | 0x80U;
		return;
	}

	g_pocsState.cpsr = *pocsGetModeSpsrPtr(oldCpsr);
	pocsCpuCheckPendingIrq();
}

/* 0x8102585c */
int pocsCpuBlockTransfer(uint32_t instr, int preIndex)
{
	uint32_t regMask = instr & 0xFFFFU;
	int baseReg = (int)((instr >> 16) & 0xFU);
	int isLoad = (instr & 0x00100000U) != 0U;
	int isUp = (instr & 0x00800000U) != 0U;
	int step;
	int regIdx;
	int regDelta;
	uint32_t addr;

	if (isUp) {
		step = 4;
		regIdx = 0;
		regDelta = 1;
	} else {
		step = -4;
		regIdx = 15;
		regDelta = -1;
		regMask = pocsReverseBits32(regMask) >> 16;
	}

	g_pocsState.cpuCyclesLeft -= isLoad;
	addr = g_pocsState.regs[baseReg];
	if (preIndex == 0)
		addr -= (uint32_t)step;

	while (regMask != 0U) {
		if ((regMask & 1U) != 0U) {
			addr += (uint32_t)step;
			g_pocsState.cpuCyclesLeft--;
			if (isLoad)
				g_pocsState.regs[regIdx] = pocsBusRead32(addr);
			else
				pocsBusWrite32(addr, g_pocsState.regs[regIdx]);
		}
		regMask >>= 1;
		regIdx += regDelta;
	}

	if ((instr & 0x00200000U) != 0U) {
		if (preIndex == 0)
			addr += (uint32_t)step;
		g_pocsState.regs[baseReg] = addr;
	}

	if (isLoad && (instr & 0x00400000U) != 0U) {
		pocsCpuSwitchMode((instr & 0x8000U) != 0U, -1);
		return 1;
	}
	return (isLoad && (instr & 0x8000U) != 0U) ? 1 : 0;
}

/* 0x81025c7c */
uint32_t pocsCpuExceptionSwi(void)
{
	pocsCpuSwitchMode(1, 19);
	g_pocsState.cpsr = (g_pocsState.cpsr & 0xFFFFFF40U) | 0x93U;
	g_pocsState.cpuCyclesLeft -= 3;
	return 8U;
}

/* 0x81025ca8 */
int pocsCpuExceptionUndef(void)
{
	pocsCpuSwitchMode(1, 27);
	g_pocsState.cpsr = (g_pocsState.cpsr & 0xFFFFFF40U) | 0x9BU;
	g_pocsState.cpuCyclesLeft -= 3;
	return 4;
}

/* 0x81025cd4 */
int pocsCpuFinishPcWrite(int dstReg, int restoreSpsr, uint32_t instrPc)
{
	if (dstReg != 15)
		return 0;

	if (restoreSpsr != 0)
		pocsCpuSwitchMode(1, -1);

	if ((instrPc & 0x0E000000U) != 0U)
		g_pocsState.cpuCyclesLeft += ((g_pocsState.clockPendingReg & 8U) != 0U) ? -12 : -6;
	else
		g_pocsState.cpuCyclesLeft -= 3;

	return 1;
}

/* 0x81025d38 */
void pocsCpuPsrTransfer(uint32_t instr, int dstReg, uint32_t value)
{
	uint32_t op = (instr >> 21) & 3U;
	uint32_t modeGroup = g_pocsState.cpsr & 3U;

	switch (op) {
	case 0U: /* MRS Rd, CPSR */
		g_pocsState.regs[dstReg] = g_pocsState.cpsr;
		break;
	case 1U: { /* MSR CPSR, val */
		uint32_t mask = g_pocsState.nibbleMaskTable[(instr >> 16) & 0xFU];
		uint32_t nextCpsr = g_pocsState.cpsr ^ ((value ^ g_pocsState.cpsr) & mask);

		pocsCpuSwitchMode(0, (int)(nextCpsr & 0x1FU));
		g_pocsState.cpsr = nextCpsr;
		pocsCpuCheckPendingIrq();
		break;
	}
	case 2U: /* MRS Rd, SPSR */
		if (modeGroup != 0U)
			g_pocsState.regs[dstReg] = *pocsGetModeSpsrPtr(g_pocsState.cpsr);
		break;
	case 3U: /* MSR SPSR, val */
		if (modeGroup != 0U) {
			uint32_t mask = g_pocsState.nibbleMaskTable[(instr >> 16) & 0xFU];
			uint32_t *spsr = pocsGetModeSpsrPtr(g_pocsState.cpsr);
			*spsr ^= (value ^ *spsr) & mask;
		}
		break;
	}
}
