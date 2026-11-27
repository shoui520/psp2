/*
 * pocs_rtc_audio.c: ScePspemu PocketStation RTC, GPIO, 48 kHz Audio DAC,
 * and battery hardware devices (0x8102cf64..0x8102d637).
 */
#include "pocs_internal.h"

#define POCS_RTC_HALF_SEC_CYCLES    3997696
#define POCS_RTC_ADJUST_CYCLES      1998
#define POCS_RTC_IRQ_BIT            0x0200U

#define POCS_AUDIO_RING_SIZE        2048U
#define POCS_AUDIO_RING_MASK        (POCS_AUDIO_RING_SIZE - 1U)
#define POCS_AUDIO_CHUNK_SAMPLES    256U
#define POCS_AUDIO_SAMPLE_RATE      48000
#define POCS_AUDIO_DELAY_USEC       5333U

static SceKernelLwMutexWork g_pocsAudioMutex;
static int16_t g_pocsAudioRing[POCS_AUDIO_RING_SIZE];

static inline uint32_t pocsToBcd(uint32_t value)
{
	return ((value / 10U) << 4) | (value % 10U);
}

/* 0x8102cf64 */
PocsSchedEvent *pocsRtcEventCallback(PocsSchedEvent *event)
{
	int32_t delayCycles;
	int stepField = -1;

	pocsSpiLock();
	g_pocsState.intcRawStatus ^= POCS_RTC_IRQ_BIT;

	if ((g_pocsState.rtcControl & 1U) != 0U) {
		delayCycles = POCS_RTC_ADJUST_CYCLES;
		if ((g_pocsState.intcRawStatus & POCS_RTC_IRQ_BIT) != 0U) {
			if (g_pocsState.rtcAdjustStep != 0U) {
				g_pocsState.rtcAdjustStep = 0U;
				stepField = (g_pocsState.rtcControl >> 1) & 7;
			} else {
				stepField = 7;
			}
		}
	} else {
		delayCycles = POCS_RTC_HALF_SEC_CYCLES;
		if ((g_pocsState.intcRawStatus & POCS_RTC_IRQ_BIT) != 0U)
			stepField = 0;
	}

	if (stepField >= 0) {
		pocsRtcStepBcdField(stepField);
		g_pocsState.intcLatch |= POCS_RTC_IRQ_BIT;
	}

	pocsSpiUnlock();
	return pocsSchedAddEvent(event, delayCycles);
}

/* 0x8102cff4 */
int pocsSyncRtcFromHost(void)
{
	SceDateTime now;
	int ret = sceRtcGetCurrentClockLocalTime(&now);

	if (ret == 0) {
		uint8_t centuryBcd = (uint8_t)pocsToBcd(now.year / 100U);
		uint8_t yearLoBcd = (uint8_t)pocsToBcd(now.year % 100U);
		int dayOfWeek;

		g_pocsState.internalRam[POCS_RAM_OFF_CENTURY_BCD] = centuryBcd;
		g_pocsState.internalRam[POCS_RAM_OFF_YEAR_LO_BCD] = yearLoBcd;
		g_pocsState.rtcDateBcd = pocsToBcd(now.day) |
		                         (pocsToBcd(now.month) << 8) |
		                         ((uint32_t)yearLoBcd << 16);

		dayOfWeek = sceRtcGetDayOfWeek(now.year, now.month, now.day);
		g_pocsBiosRom[POCS_ROM_OFF_RTC_PATCH_B] = 0x82U;
		ret = dayOfWeek + 1;
		g_pocsBiosRom[POCS_ROM_OFF_RTC_PATCH_A] = 31U;
		g_pocsBiosRom[POCS_ROM_OFF_CENTURY_BCD] = centuryBcd;
		g_pocsState.rtcTimeBcd = pocsToBcd(now.second) |
		                         (pocsToBcd(now.minute) << 8) |
		                         (pocsToBcd(now.hour) << 16) |
		                         ((uint32_t)ret << 24);
	}
	return ret;
}

/* 0x8102d130 */
int pocsRtcRead(int regOffset)
{
	switch (regOffset) {
	case 0:
		return (int)g_pocsState.rtcControl;
	case 8:
		return (int)(g_pocsState.rtcTimeBcd & 0x073F7F7FU);
	case 12:
		return (int)(g_pocsState.rtcDateBcd & 0x00FF1F3FU);
	default:
		return 0;
	}
}

/* 0x8102d17c */
PocsSchedEvent *pocsRtcWrite(int regOffset, uint8_t value)
{
	if (regOffset == 0) {
		if ((g_pocsState.rtcControl & 1U) != 0U && (value & 1U) == 0U)
			pocsSyncRtcFromHost();
		g_pocsState.rtcControl = value & 0x0FU;
		if (g_pocsState.rtcEvent.prev != NULL)
			pocsSchedRemoveEvent(&g_pocsState.rtcEvent);
		return pocsRtcEventCallback(&g_pocsState.rtcEvent);
	}
	if (regOffset == 4)
		g_pocsState.rtcAdjustStep = value & 1U;
	return (PocsSchedEvent *)(uintptr_t)regOffset;
}

/* 0x8102d1e4 */
PocsSchedEvent *pocsRtcReset(void)
{
	g_pocsState.rtcDateBcd = 0x00980101U;
	g_pocsState.rtcTimeBcd = 0x04000000U;
	g_pocsState.rtcControl = 0U;
	pocsSyncRtcFromHost();
	g_pocsState.rtcEvent.callback = pocsRtcEventCallback;
	if (g_pocsState.rtcEvent.prev != NULL)
		pocsSchedRemoveEvent(&g_pocsState.rtcEvent);
	return pocsSchedAddEvent(&g_pocsState.rtcEvent, POCS_RTC_HALF_SEC_CYCLES);
}

/* 0x8102d234 */
int pocsGpioRead(int regOffset)
{
	if (regOffset == 0)
		return (int)g_pocsState.gpioDir;
	if (regOffset == 12)
		return (int)(g_pocsState.gpioData & 0x1FU);
	return 0;
}

/* 0x8102d260 */
int pocsGpioWrite(int regOffset, uint8_t value)
{
	if (regOffset == 0)
		g_pocsState.gpioDir = value & 0x0FU;
	else if (regOffset == 4)
		g_pocsState.gpioData |= (uint8_t)(value & 0x6FU);
	else if (regOffset == 8)
		g_pocsState.gpioData &= (uint8_t)~(value & 0x6FU);

	return pocsLcdNotifyDirty(((g_pocsState.gpioDir & 2U) & ~g_pocsState.gpioData) != 0U);
}

/* 0x8102d2e4 */
void pocsGpioReset(void)
{
	g_pocsState.gpioDir = 0U;
	g_pocsState.gpioData = 0U;
}

/* 0x8102d2f8 */
int pocsAudioUpdateRing(int sampleValue)
{
	uint32_t currentCycle = (uint32_t)pocsGetCurrentCycle();

	if (g_pocsState.audioLastCycle <= currentCycle) {
		uint32_t sampleCount = (uint32_t)(int)((float)(int)(currentCycle - g_pocsState.audioLastCycle) *
		                                       0.0060034934f);
		uint32_t writePos;
		uint32_t endPos;

		if (sampleCount >= 800U) {
			sampleValue = 0;
			g_pocsState.audioCurrentSample = 0;
		} else {
			sampleValue = (uint16_t)sampleValue;
		}

		writePos = g_pocsState.audioWriteIdx;
		g_pocsState.audioLastCycle += (uint32_t)(int)((float)sampleCount * 166.56967f);
		endPos = writePos + sampleCount;

		if (writePos < endPos) {
			int16_t holdSample = g_pocsState.audioCurrentSample;
			do {
				g_pocsAudioRing[writePos & POCS_AUDIO_RING_MASK] = holdSample;
				writePos++;
			} while (writePos != endPos);
		}

		g_pocsState.audioCurrentSample = (int16_t)sampleValue;
		g_pocsState.audioWriteIdx = (uint16_t)endPos;
	} else {
		g_pocsState.audioLastCycle = currentCycle;
	}

	return sampleValue;
}

/* 0x8102d3b8 */
static int pocsAudioDacThread(SceSize args, void *argp)
{
	int port;
	uint16_t readIdx = 0U;
	uint32_t minBuffered = 1024U;

	(void)args;
	(void)argp;

	port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, POCS_AUDIO_CHUNK_SAMPLES,
	                           POCS_AUDIO_SAMPLE_RATE, SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
	if (port < 0)
		return 0;

	for (;;) {
		uint32_t avail;

		for (;;) {
			sceKernelLockLwMutex(&g_pocsAudioMutex, 1, NULL);
			avail = (uint32_t)((uint16_t)(g_pocsState.audioWriteIdx - readIdx) & POCS_AUDIO_RING_MASK);
			if (avail >= minBuffered)
				break;
			minBuffered = 1024U;
			sceKernelUnlockLwMutex(&g_pocsAudioMutex, 1);
			sceKernelDelayThread(POCS_AUDIO_DELAY_USEC);
		}

		if (avail > 0x600U)
			readIdx = (uint16_t)((readIdx + POCS_AUDIO_CHUNK_SAMPLES) & POCS_AUDIO_RING_MASK);
		sceKernelUnlockLwMutex(&g_pocsAudioMutex, 1);

		if (g_pocsState.standaloneActive != 0U || popsHostGetPocsAutoBoot() != 0) {
			minBuffered = 512U;
			sceAudioOutOutput(port, &g_pocsAudioRing[readIdx]);
		} else {
			sceKernelDelayThread(POCS_AUDIO_DELAY_USEC);
		}

		readIdx = (uint16_t)((readIdx + POCS_AUDIO_CHUNK_SAMPLES) & POCS_AUDIO_RING_MASK);
	}
}

/* 0x8102d470 */
int pocsAudioFlush(void)
{
	sceKernelLockLwMutex(&g_pocsAudioMutex, 1, NULL);
	pocsAudioUpdateRing((uint16_t)g_pocsState.audioCurrentSample);
	return sceKernelUnlockLwMutex(&g_pocsAudioMutex, 1);
}

/* 0x8102d4a0 */
int pocsAudioRead(int regOffset)
{
	if (regOffset == 16)
		return (int)g_pocsState.audioEnabled;
	return 0;
}

/* 0x8102d4b4 */
int pocsAudioWrite(int regOffset, int value)
{
	int enabled;
	int targetSample;

	if (regOffset == 16) {
		value &= 1;
		if ((int)g_pocsState.audioEnabled == value)
			return regOffset;
		g_pocsState.audioEnabled = (uint8_t)value;
		enabled = value;
		if (value != 0)
			value = (uint16_t)g_pocsState.audioSampleLatched;
	} else if (regOffset == 20) {
		enabled = (int)g_pocsState.audioEnabled;
	} else {
		return regOffset;
	}

	g_pocsState.audioSampleLatched = (uint16_t)value;
	if (enabled != 0 && (g_pocsState.gpioData & 0x20U) == 0U)
		targetSample = value & 0xFFC0;
	else
		targetSample = 0;

	if ((int)(uint16_t)g_pocsState.audioCurrentSample != targetSample) {
		sceKernelLockLwMutex(&g_pocsAudioMutex, 1, NULL);
		pocsAudioUpdateRing(targetSample);
		return sceKernelUnlockLwMutex(&g_pocsAudioMutex, 1);
	}
	return (int)(g_pocsState.gpioData & 0x20U);
}

/* 0x8102d544 */
SceUID pocsAudioInit(void)
{
	SceUID ret = sceKernelCreateLwMutex(&g_pocsAudioMutex, "ScePocketStationAudioBufControl", 0, 0, NULL);
	if (ret < 0)
		return ret;

	ret = sceKernelCreateThread("ScePocketStationAudioDAC", pocsAudioDacThread,
	                            64, 0x8000U, 0, 0x20000, NULL);
	if (ret < 0)
		return ret;

	ret = sceKernelStartThread(ret, 0, NULL);
	return (ret < 0) ? ret : 0;
}

/* 0x8102d5a4 */
int pocsAudioReset(void)
{
	g_pocsState.audioEnabled = 0U;
	sceKernelLockLwMutex(&g_pocsAudioMutex, 1, NULL);
	g_pocsState.audioLastCycle = (uint32_t)pocsGetCurrentCycle();
	g_pocsState.audioCurrentSample = 0;
	g_pocsState.audioWriteIdx = 0U;
	return sceKernelUnlockLwMutex(&g_pocsAudioMutex, 1);
}

/* 0x8102d5fc */
int pocsBatteryRead(int regOffset)
{
	if (regOffset == 32)
		return (int)g_pocsState.batteryLevel;
	return 0;
}

/* 0x8102d610 */
int pocsBatteryWrite(int regOffset, uint8_t value)
{
	if (regOffset == 32)
		g_pocsState.batteryLevel = value & 7U;
	return regOffset;
}

/* 0x8102d628 */
void pocsBatteryReset(void)
{
	g_pocsState.batteryLevel = 3U;
}
