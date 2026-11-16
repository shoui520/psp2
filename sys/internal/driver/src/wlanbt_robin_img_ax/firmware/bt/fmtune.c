/*
 * The FM tuner: its event sender, its register defaults and tuning
 * (include/robin/fm.h). Marvell published no source; names are inferred from
 * use.
 */
#include <stdint.h>
#include "robin/btpower.h"
#include "robin/btrom.h"
#include "robin/clock.h"
#include "robin/eventpower.h"
#include "robin/fm.h"
#include "robin/macdefaults.h"
#include "robin/rom.h"
#include "robin/sbc.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/threadx.h"

struct FmTuner fmTuner __attribute__((section(".itcm.rw"))) = {	/* 0x55694 */
	.filterCodes = { 0x0f, 0x17, 0x1f, 0x00 },
	.idle = 1,
	.filters = (const uint32_t (*)[3])romFmTable,
	.searchLimit = 0x71,
};

struct FmGainStep fmGainSteps[FM_GAIN_STEPS] __attribute__((section(".itcm.rw"))) = {	/* 0x556d8 */
	{ 0x00, 0x50, 0x7f, 0 }, { 0x51, 0x53, 0x7b, 0 }, { 0x54, 0x55, 0x78, 0 },
	{ 0x56, 0x58, 0x74, 0 }, { 0x59, 0x5b, 0x6f, 0 }, { 0x5c, 0x5d, 0x6c, 0 },
	{ 0x5e, 0x60, 0x69, 0 }, { 0x61, 0x65, 0x64, 0 }, { 0x66, 0x6c, 0x5c, 0 },
	{ 0x6d, 0x73, 0x4e, 0 }, { 0x74, 0x77, 0x3a, 0 }, { 0x78, 0x79, 0x2c, 0 },
	{ 0x7a, 0x7c, 0x25, 0 }, { 0x7d, 0x7d, 0x1d, 0 }, { 0x7e, 0x80, 0x14, 0 },
	{ 0x81, 0x81, 0x0a, 0 }, { 0x82, 0xff, 0x00, 0 },
};

struct FmFrequencyStep fmFrequencySteps[FM_FREQUENCY_STEPS] __attribute__((section(".itcm.rw"))) = {	/* 0x5571c */
	{ 0x2710, 0, 14, 0 }, { 0x2904, 0, 12, 0 }, { 0x2af8, 0, 10, 0 },
	{ 0x2cec, 0, 8, 0 }, { 0x2ee0, 0, 6, 0 }, { 0x30d4, 0, 4, 0 },
	{ 0x32c8, 0, 2, 0 }, { 0x34bc, 0, 0, 0 }, { 0x3539, 0, 0, 0 },
};

uint8_t btFmLevelThreshold __attribute__((section(".itcm.rw")));	/* 0x55740 */

/* btSbcManageBuffer()'s copy of the channels' bytes (wlan/macdefaults.c); it follows here. */
uint8_t wlanChannelBytes[WLAN_CHANNELS] __attribute__((section(".itcm.rw"))) = {	/* 0x55741 */
	0x02, 0x04, 0x0b, 0x16, 0x0c, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c, 0x2c, 0x90,
};

const struct FmClockCode fmClockCodes[FM_CLOCK_CODES] = {
	{ 0x253b, 8, 0 }, { 0x25be, 6, 0 }, { 0x263b, 7, 0 }, { 0x26be, 6, 0 },
	{ 0x26c5, 8, 0 }, { 0x273b, 7, 0 }, { 0x27bf, 6, 0 }, { 0x282f, 8, 0 },
	{ 0x283c, 7, 0 }, { 0x28bf, 6, 0 }, { 0x2916, 7, 0 }, { 0x293c, 8, 0 },
	{ 0x29bf, 6, 0 }, { 0x2a3c, 7, 0 }, { 0x2abf, 6, 0 }, { 0x2af1, 8, 0 },
	{ 0x2b3c, 7, 0 }, { 0x2bc0, 6, 0 }, { 0x2bd9, 7, 0 }, { 0x2c3d, 8, 0 },
	{ 0x2cc0, 6, 0 }, { 0x2d37, 7, 0 }, { 0x2d3d, 8, 0 }, { 0x2dc0, 6, 0 },
	{ 0x2e3d, 7, 0 }, { 0x2ec0, 6, 0 }, { 0x2f18, 8, 0 }, { 0x2f3d, 7, 0 },
	{ 0x2fc1, 6, 0 }, { 0x3018, 7, 0 }, { 0x303e, 8, 0 }, { 0x30c1, 6, 0 },
	{ 0x313e, 7, 0 }, { 0x31bb, 6, 0 }, { 0x31da, 8, 0 }, { 0x323e, 7, 0 },
	{ 0x32bb, 6, 0 }, { 0x32fa, 8, 0 }, { 0x3338, 7, 0 }, { 0x33bb, 6, 0 },
	{ 0x341f, 7, 0 }, { 0x3438, 8, 0 }, { 0x34bc, 6, 0 },
};

const struct FmLevelStep fmLevelSteps[FM_LEVEL_HIGH - FM_LEVEL_LOW + 1] = {	/* 0x543f0 */
	{ 0x9b96918c, 0x014576fb },	/* 125 */
	{ 0xa5a09b96, 0x014476b9 },
	{ 0x96918c87, 0x01447656 },
	{ 0xa09b9691, 0x014375f4 },
	{ 0x8c87827d, 0x01435552 },
	{ 0xa09b9691, 0x0142540c },	/* 120 */
	{ 0x8c87827d, 0x0142540c },
	{ 0xaaa5a09b, 0x01412c0c },
	{ 0x96918c87, 0x01412c06 },
	{ 0x87827d78, 0x01412c06 },
	{ 0x78736e69, 0x01412c06 },	/* 115 */
	{ 0xaaa5a09b, 0x00a02c06 },
	{ 0x96918c87, 0x00a02c06 },
	{ 0x89847f7a, 0x00a02c06 },
	{ 0x7d78736e, 0x00502c06 },
	{ 0x6e69645f, 0x00282c06 },	/* 110 */
	{ 0x625d5853, 0x00282c06 },
	{ 0x55504b46, 0x00282c06 },
	{ 0x4d4a4542, 0x00282c06 },
	{ 0x41403b3a, 0x00282c06 },
	{ 0x37363130, 0x00281406 },	/* 105 */
	{ 0x32312d2c, 0x00281406 },
	{ 0x2f2e2b2a, 0x00281406 },
	{ 0x28252423, 0x00281406 },
	{ 0x2322201f, 0x00281406 },
	{ 0x201f1c1b, 0x00280c06 },	/* 100 */
	{ 0x1c1b1817, 0x00280c06 },	/* 99 */
};

/* Sends an FM reply or event once the secondary core is out of its alternate mode. */
int btSbcComputeEntry(struct FmReply *reply, unsigned int length, unsigned int kind)
{
	sysSecondaryWaitAltMode();
	return btSbcRomNotifyContext(reply, length, kind);
}

void btSbcQueryEntry(void)
{
	fmEventSend = btSbcComputeEntry;
}

/* Stops the FM block: drops two features, and pulses its reset. */
__attribute__((noinline)) int btSbcResetEntry(void)
{
	fmSettings.features &= ~0x3000u;
	SOC_CONTROL &= ~8u;
	SOC_CONTROL |= 8;
	return 0;
}

/* The FM block's register defaults. */
void btSbcSyncEntry(void)
{
	FM_REGISTER(0x15c) = 0x89;
	FM_REGISTER(0x254) = 0x20;
	FM_REGISTER(0x3b8) = 0xa81;
	FM_REGISTER(0x03c) = 0x5fb3;
	FM_REGISTER(0x2c8) = 0x3500;
	FM_REGISTER(0x3c8) = 0x1c8;
	FM_REGISTER(0x3c4) = 0x62;
	FM_REGISTER(0x0b8) = 0x7055;
	FM_REGISTER(0x008) = 0x1ee2;
	FM_REGISTER(0x2c0) = 0x109;
	FM_REGISTER(0x3c0) = 6;
	FM_REGISTER(0x35c) = 0xee0;
	FM_REGISTER(0x3e0) = 0xffc3;
	FM_REGISTER(0x334) = 0xd555;
	FM_REGISTER(0x010) = 0x1f;
	FM_REGISTER(0x2e0) = 0x51e;
	if (FM_REGISTER(0x000) >> 7 > 7) {
		FM_REGISTER(0x174) = 0x4916;
		FM_REGISTER(0x098) = 0x1002;
	}
}

/* Waits a number of event clock ticks. */
__attribute__((noinline)) int btSbcApplyEntry(uint32_t ticks)
{
	uint32_t start = SOC_EVENT_CLOCK;

	while (SOC_EVENT_CLOCK - start < ticks)
		;
	return 0;
}

/*
 * Waits for a tune (or a search) to complete, then retunes the block to the
 * channel: FM_TUNE_TIMEOUT when it never completes.
 */
int btSbcResolveEntry(int search)
{
	uint16_t polls = 0;
	int result;

	do {
		btSbcApplyEntry(66);
		polls++;
	} while (fmTuner.tuneDone == 0 && polls < FM_TUNE_POLLS);
	fmTuner.tuneDone = 0;
	if (polls == FM_TUNE_POLLS) {
		FM_REGISTER(0x000) |= 1;
		return FM_TUNE_TIMEOUT;
	}
	if (search != 0) {
		if ((fmSettings.features >> 4 & 4) != 0 && (FM_REGISTER(0x00c) >> 5 & 7) == 3) {
			FM_REGISTER(0x3a8) |= 0x1000;
			FM_REGISTER(0x3a8) &= ~0x1000;
		}
		btSbcRomAdjustContext(0x8000, 1);
		btFmSearchHook(fmSettings.frequency);
	} else {
		if ((fmSettings.features & 0x2000) != 0) {
			result = btFmTuneHook(fmSettings.frequency);
			if (result != 0)
				return result;
		}
		btSbcRomAdjustContext(0x8000, 0);
		btSbcUpdateBuffer(fmSettings.frequency);
	}
	FM_REGISTER(0x008) |= 0x10;
	return 0;
}

/* Enables or disables the stereo decoder's outputs, as the settings ask. */
void btSbcScheduleEntry(int enable)
{
	if ((fmSettings.policy >> 4 & 0xf) == 1) {
		btFmEnableHook(enable, 3);
		FM_REGISTER(0x3bc) |= 0x2000;
	}
	FM_REGISTER(0x3bc) = (FM_REGISTER(0x3bc) & ~0x600) | (((fmSettings.policy >> 23 & 3) != 0) << 9);
	if (enable != 0) {
		FM_REGISTER(0x3bc) |= 1;
		FM_REGISTER(0x03c) &= ~0x1000;
		if ((fmSettings.features & 0x80) != 0) {
			FM_REGISTER(0x3bc) |= 0x10;
			FM_REGISTER(0x3c0) |= 1;
		} else if ((fmSettings.features >> 4 & 4) != 0) {
			FM_REGISTER(0x3bc) &= ~0x10;
			FM_REGISTER(0x3c0) |= 1;
		} else {
			FM_REGISTER(0x3bc) |= 0x10;
			FM_REGISTER(0x3c0) &= ~1;
		}
	} else {
		FM_REGISTER(0x3bc) &= ~1;
		FM_REGISTER(0x3bc) &= ~0x10;
		FM_REGISTER(0x3c0) &= ~1;
		FM_REGISTER(0x03c) |= 0x1000;
	}
}

int btSbcNotifyEntry(int enable)
{
	unsigned int mode = fmSettings.policy >> 4 & 0xf;

	if (mode == 0)
		btSbcRecordTable(enable);
	else if (mode == 1)
		btSbcScheduleEntry(enable);
	return 0;
}

/* The front end's filter words for a frequency (Hz), from the ROM table. */
int btSbcFormatEntry(uint32_t frequency)
{
	const uint32_t *filter;
	int index;

	index = ((((int)(frequency - 63504000) >> 15) + 1) >> 1) + 7;
	if (index < 0)
		index = 0;
	else if (index > 15)
		index = 15;
	filter = fmTuner.filters[index];
	FM_REGISTER(0x26c) = filter[0] >> 16;
	FM_REGISTER(0x270) = filter[0];
	FM_REGISTER(0x274) = filter[1] >> 16;
	FM_REGISTER(0x278) = filter[1];
	FM_REGISTER(0x27c) = filter[2] >> 16;
	FM_REGISTER(0x280) = filter[2];
	FM_REGISTER(0x284) = 0x3f5d;
	FM_REGISTER(0x288) = 0x17c2;
	return 0;
}

/* A value in reference clock periods per 1000. */
__attribute__((noinline)) uint16_t btSbcInspectEntry(uint32_t value)
{
	int strap = socStrapIsSet();
	uint32_t clock = fmSettings.referenceClock;

	if (strap == 0)
		clock >>= 12;
	return clock * value / 1000;
}

void btSbcTrackEntry(uint32_t value)
{
	uint16_t control;

	btSbcRomHandleEntry(value);
	FM_REGISTER(0x060) = btSbcInspectEntry(value);
	control = FM_REGISTER(0x05c) | 3;
	if (socStrapIsSet() != 0)
		control &= ~8;
	else
		control |= 8;
	FM_REGISTER(0x05c) = control;
	btSbcRomAdjustContext(0x20, 1);
}

/*
 * 1 when the oscillator frequency keeps 4 MHz (scaled by the station's
 * frequency) from the nearest harmonic of the controller's clock; inverted
 * for 103.2 MHz.
 */
int btSbcManageEntry(uint32_t vco)
{
	uint32_t step;
	uint64_t harmonic;
	uint32_t distance;
	int clear = 1;

	step = vco / (btPower.control * 10);
	harmonic = romMul64x32(((uint64_t)step + step + 10000) / 20000, 10000);
	if (harmonic >= step)
		distance = (uint32_t)harmonic - step;
	else
		distance = step - (uint32_t)harmonic;
	if (distance * fmSettings.frequency < 4000000)
		clear = 0;
	if (fmSettings.frequency == 103200)
		clear = clear == 0;
	return clear;
}

/*
 * Tunes to a frequency (kHz): picks the side of the station the local
 * oscillator sits on, away from the clock's harmonics, and sets up the PLL.
 */
int btSbcAcquireEntry(uint32_t frequency)
{
	uint32_t config;
	uint32_t tuned;
	uint32_t vco;
	uint32_t oscillator;
	uint32_t divider;
	unsigned int mode = 100;
	int result;

	oscillator = 1000 * (fmSettings.frequencyOffset + frequency);
	btFmInjectionHook(frequency, &config);
	if ((fmSettings.features & 0x80) != 0)
		config = (uint16_t)config & 0x7fff;
	FM_REGISTER(0x004) = (FM_REGISTER(0x004) & ~0x40) | ((uint16_t)config >> 15) << 6;
	if ((fmSettings.features >> 4 & 4) == 0) {
		if ((config & 0x4000) != 0)
			oscillator -= FM_INTERMEDIATE;
		else
			oscillator += FM_INTERMEDIATE;
		if (btSbcManageEntry(FM_VCO_LIMIT / (oscillator * 4) * oscillator * 4) == 0) {
			oscillator = 1000 * (fmSettings.frequencyOffset + frequency);
			if ((config & 0x4000) != 0) {
				config = (uint16_t)config & ~0x4000;
				oscillator += FM_INTERMEDIATE;
			} else {
				config = (uint16_t)config | 0x4000;
				oscillator -= FM_INTERMEDIATE;
			}
		}
		FM_REGISTER(0x204) = (FM_REGISTER(0x204) & ~3) | ((uint16_t)config >> 14 & 1);
		mode = 6;
	}
	divider = FM_VCO_LIMIT / (oscillator * 4);
	vco = divider * oscillator * 4;
	result = btSbcRomCheckEntry(vco);
	if (result != 0)
		return result;
	result = btFmPllSetupHook(vco, ((uint16_t)config >> 15) ^ 1, &tuned);
	if (result != 0)
		return result;
	result = btFmPllDividerHook(tuned);
	if (result != 0)
		return result;
	result = btFmPllFractionHook(tuned);
	if (result != 0)
		return result;
	result = btFmFilterHook(tuned);
	if (result != 0)
		return result;
	result = btFmPllLockHook(tuned);
	if (result != 0)
		return result;
	FM_REGISTER(0x174) = (FM_REGISTER(0x174) & 0x3fff) | (divider - 2) << 14;
	btFmMixerHook(oscillator, mode);
	fmSettings.frequency = frequency;
	return 0;
}

/* Hands the tuner's routines to the ROM's FM code. */
int btSbcReleaseEntry(void)
{
	btFmTuneHook = btSbcAcquireEntry;
	btFmClockHook = btSbcInspectEntry;
	btFmTimerHook = btSbcTrackEntry;
	btFmFilterHook = btSbcFormatEntry;
	btFmStereoHook = btSbcNotifyEntry;
	btFmWaitHook = btSbcResolveEntry;
	btFmSearchHook = btSbcCheckBuffer;
	btFmApplyHook = btSbcApplyBuffer;
	return 0;
}

/* The reference clock (Hz) the host gives, or the board's. */
uint32_t btSbcSetupEntry(uint32_t clock)
{
	if (socStrapIsSet() != 0)
		return sleepClockRateGet();
	FM_REGISTER(0x3e4) = 2;
	if (clock != 0)
		return clock;
	return rfCalibrationTable[3 + sysRefClockProfile()] * 100000;
}

/* Resets the FM block and the tuner's state for a reference clock. */
int btSbcClearEntry(uint8_t clock)
{
	btSbcResetEntry();
	btSbcRomSetupContext(btSbcSetupEntry(clock));
	fmTuner.levelBase = 30;
	btSbcRomAcquireEntry(30, &fmTuner.levelBase);
	sbcPcmStreamHook = btSbcPcmStreamHandler;
	btSbcRomReleaseContext();
	btSbcSyncEntry();
	if (fmSettings.referenceClock > 0x8000)
		FM_REGISTER(0x178) |= 0x80;
	else
		FM_REGISTER(0x178) &= ~0x80;
	btSbcRomAdjustContext(0xffff, 0);
	FM_REGISTER(0x058) = 0xffff;
	FM_REGISTER(0x2cc) &= 0x7f00;
	btSbcRomAdjustContext(0x10, 1);
	FM_REGISTER(0x378) &= ~7;
	btFmLevelThreshold = 125;
	fmSettings.features |= 0x1000;
	fmSettings.searchThreshold = 15;
	fmTuner.antennaScan = 1;
	fmTuner.antennaOffset = 0;
	fmTuner.quietSearch = 0;
	fmTuner.measureTolerance = 20;
	fmTuner.measureSpread = 15;
	fmTuner.calibratedTolerance = 10;
	fmTuner.searchCalibrate = 1;
	fmTuner.measureGain = 2;
	fmTuner.measureTime = 10;
	fmTuner.measureScale = 1;
	return 0;
}

/* A tune or search completed (the FM interrupt): finishes it and marks it done. */
void btSbcConvertEntry(void)
{
	uint32_t config;
	uint16_t value;
	unsigned int mode;
	uint16_t status;

	if ((FM_REGISTER(0x008) & 4) == 0) {
		if (FM_REGISTER(0x000) >> 7 > 7) {
			btFmTuneHook(fmSettings.frequency);
			if ((FM_REGISTER(0x174) >> 14) + 2 == 3 && fmSettings.frequency <= 104000)
				FM_REGISTER(0x170) |= 0x200;
			else
				FM_REGISTER(0x170) &= ~0x200;
		}
		btFmInjectionHook(fmSettings.frequency, &config);
		value = FM_REGISTER(0x02c) & 0x8007;
		if ((uint16_t)config >> 15 != 0)
			value += 0x4718;
		else
			value += 0x1188;
		FM_REGISTER(0x02c) = value;
	} else {
		value = FM_REGISTER(0x078);
		FM_REGISTER(0x078) = value & ~0xc0;
		FM_REGISTER(0x078) = value;
	}
	mode = FM_REGISTER(0x00c) >> 5 & 7;
	status = FM_REGISTER(0x004);
	if (mode == 2 && (status & 0x1000) != 0) {
		FM_REGISTER(0x008) |= 0x10;
		return;
	}
	fmTuner.tuneDone = 1;
}

/*
 * An RDS group arrived (the FM interrupt): its four blocks go to the RDS
 * buffer when blocks A and B match what the host asked for.
 */
void btSbcVerifyEntry(void)
{
	uint16_t status;
	uint16_t blockA;
	uint16_t blockB;
	uint16_t blockC;
	uint16_t blockD;
	uint32_t now;
	uint32_t elapsed;
	uint32_t match;
	unsigned int size;

	status = FM_REGISTER(0x2e0);
	if ((status & 0x2000) != 0) {
		FM_REGISTER(0x2e0) = status & ~0x2000;
		btSbcFilterTable();
		return;
	}
	fmSettings.rdsFlags |= FM_RDS_RECEIVING;
	now = SOC_EVENT_CLOCK;
	if (now > fmTuner.rdsLastClock)
		elapsed = now - fmTuner.rdsLastClock;
	else
		elapsed = now - fmTuner.rdsLastClock - 1;
	fmTuner.rdsLastClock = now;
	if (elapsed > FM_RDS_TIMEOUT) {
		/* The ROM routine takes no arguments; the original leaves the limit and the time in r0 and r1. */
		btSbcRomVerifyEntry();
		btSbcFilterTable();
		return;
	}
	if (FM_RDS_MODE(status) == 2 && FM_RDS_MODE(fmSettings.rdsFlags) == 1)
		return;
	blockA = FM_REGISTER(0x2ec);
	blockB = FM_REGISTER(0x2f0);
	blockC = FM_REGISTER(0x2f4);
	blockD = FM_REGISTER(0x2f8);
	match = fmSettings.rdsBlockAMatch;
	if ((match & match >> 16) == (match >> 16 & blockA))
		btSbcRomConvertContext(0x80);
	match = fmSettings.rdsBlockBMatch;
	if ((match & match >> 16) != (match >> 16 & blockB))
		return;
	btSbcRomConvertContext(0x40);
	if (fmSettings.rdsIndex.read == fmSettings.rdsIndex.write && (fmSettings.rdsFlags & FM_RDS_PENDING) != 0) {
		fmSettings.rdsIndex.read += 4;
		fmSettings.rdsIndex.read %= FM_RDS_SIZE(fmSettings.rdsFlags);
	}
	fmRdsBlocks[fmSettings.rdsIndex.write++] = blockA;
	fmRdsBlocks[fmSettings.rdsIndex.write++] = blockB;
	fmRdsBlocks[fmSettings.rdsIndex.write++] = blockC;
	fmRdsBlocks[fmSettings.rdsIndex.write++] = blockD;
	size = FM_RDS_SIZE(fmSettings.rdsFlags);
	fmSettings.rdsIndex.write %= size;
	fmSettings.rdsFlags |= FM_RDS_PENDING;
	if (btSbcRomConvertEntry(0x100) == 0)
		return;
	btSbcRomConvertContext(2);
	if ((fmSettings.eventMask & 2) != 0)
		tx_event_flags_set((TX_EVENT_FLAGS_GROUP *)fmFlags, 8, TX_OR);
}

/* The RDS interrupt: counts, during a search, whether it saw a group. */
void btSbcSelectEntry(void)
{
	if ((fmSettings.features >> 4 & 4) != 0) {
		btSbcRomTrackEntry();
		return;
	}
	btSbcVerifyEntry();
	if (fmTuner.searchCount == 0 || fmTuner.searchCount > fmTuner.searchLimit)
		return;
	if ((fmSettings.rdsFlags & FM_RDS_RECEIVING) != 0)
		fmTuner.rdsReceived++;
	else
		fmTuner.rdsMissed++;
}

/* The level interrupt: wakes the FM task for the levels that crossed. */
void btSbcAdjustEntry(uint32_t status)
{
	if ((status & 0x80) != 0 || (status & 0x40) != 0)
		tx_event_flags_set((TX_EVENT_FLAGS_GROUP *)fmFlags, 0x200, TX_OR);
	if ((status & 0x400) != 0)
		tx_event_flags_set((TX_EVENT_FLAGS_GROUP *)fmFlags, 0x800, TX_OR);
	if ((status & 0x800) != 0)
		tx_event_flags_set((TX_EVENT_FLAGS_GROUP *)fmFlags, 0x400, TX_OR);
}

/* The stereo interrupt. */
void btSbcRecordEntry(void)
{
	if ((FM_REGISTER(0x378) & 0x400) != 0)
		tx_event_flags_set((TX_EVENT_FLAGS_GROUP *)fmFlags, 0x2000, TX_OR);
	btSbcRomConvertContext(0x200);
}

/* The search interrupt. */
void btSbcFilterEntry(void)
{
	if ((fmSettings.features >> 4 & 4) != 0) {
		btSbcResolveBuffer();
		btSbcScheduleBuffer();
		return;
	}
	btSbcRomConvertContext(4);
	tx_event_flags_set((TX_EVENT_FLAGS_GROUP *)fmFlags, 2, TX_OR);
	tx_event_flags_set((TX_EVENT_FLAGS_GROUP *)fmFlags, 0x100, TX_OR);
}

/* Starts a search from a frequency, up or down. */
int btSbcHandleTable(uint32_t frequency, int up)
{
	int result;

	fmSettings.frequency = frequency;
	if (up != 0)
		FM_REGISTER(0x004) |= 8;
	else
		FM_REGISTER(0x004) &= ~8;
	result = btFmWaitHook(0);
	if (result != 0)
		return result;
	result = btFmWaitHook(1);
	if (result != 0)
		return result;
	return btSbcRomFilterContext();
}

/*
 * Holds the two level registers at what the block measures now (enable), or
 * gives them back their saved values.
 */
__attribute__((noinline)) void btSbcProcessTable(int enable)
{
	unsigned int level;
	uint16_t value;

	if (enable == 0) {
		FM_REGISTER(0x134) = fmTuner.savedLevels[0];
		FM_REGISTER(0x138) = fmTuner.savedLevels[1];
		return;
	}
	level = FM_REGISTER(0x12c) >> 7 & 0xf;
	fmTuner.savedLevels[0] = FM_REGISTER(0x134);
	value = (fmTuner.savedLevels[0] & ~0xf0) | level << 4;
	FM_REGISTER(0x134) = (value & ~0xf) | level;
	FM_REGISTER(0x130) = (FM_REGISTER(0x130) & ~0x1e00) | level << 9;
	FM_REGISTER(0x078) &= ~4;
	FM_REGISTER(0x078) |= 4;
	btSbcApplyEntry(0xa4);
	level = FM_REGISTER(0x12c) >> 11;
	fmTuner.savedLevels[1] = FM_REGISTER(0x138);
	value = (fmTuner.savedLevels[1] & ~0x3e0) | level << 5;
	FM_REGISTER(0x138) = (value & ~0x1f) | level;
}

#define FM_MEASURE_POINTS	3
#define FM_MEASURE_STEP		35	/* kHz between the points */

/*
 * Measures the level at three points around a frequency, 35 kHz apart.
 * Each must be within the tolerance, their mean too, and each near the mean:
 * returns 1 and the mean then, 0 otherwise.
 */
__attribute__((noinline)) int btSbcUpdateTable(uint32_t frequency, int tolerance, int16_t *mean)
{
	int16_t levels[FM_MEASURE_POINTS];
	int16_t upper, lower, high, low;
	unsigned int gain, i;
	uint16_t value;
	int8_t offset;
	int result;

	offset = -FM_MEASURE_STEP;
	result = 1;
	upper = fmTuner.measureSpread + tolerance;
	lower = -upper;
	value = FM_REGISTER(0x03c);
	gain = value >> 2 & 7;
	FM_REGISTER(0x03c) = (value & ~0x1c) | fmTuner.measureGain << 2;
	*mean = 0;
	for (i = 0; i < FM_MEASURE_POINTS; i = (uint8_t)(i + 1)) {
		btFmFrequencyHook(frequency + offset);
		value = FM_REGISTER(0x038);
		FM_REGISTER(0x038) = value | 4;
		FM_REGISTER(0x038) = value & ~4;
		btSbcApplyEntry(((uint32_t)fmTuner.measureTime << 15) / 1000 + 1);
		levels[i] = FM_REGISTER(0x044);
		if (fmTuner.measureScale != 0)
			levels[i] = levels[i] * 0x5b3 >> 16;
		else
			levels[i] = levels[i] * 0x422 >> 16;
		levels[i] = (uint16_t)levels[i] - offset;
		if (levels[i] > upper || levels[i] < lower) {
			result = 0;
			goto out;
		}
		*mean += levels[i];
		offset += FM_MEASURE_STEP;
	}
	if (result == 0)
		goto out;
	*mean = *mean / FM_MEASURE_POINTS;
	if (*mean > tolerance || *mean < -tolerance) {
		result = 0;
		goto out;
	}
	high = *mean + fmTuner.measureSpread;
	low = *mean - fmTuner.measureSpread;
	for (i = 0; i < FM_MEASURE_POINTS; i = (uint8_t)(i + 1)) {
		if (levels[i] > high || levels[i] < low) {
			result = 0;
			goto out;
		}
	}
out:
	FM_REGISTER(0x03c) = (FM_REGISTER(0x03c) & ~0x1c) | gain << 2;
	return result;
}

/*
 * Calibrates the frequency offset at a frequency: measures without the offset
 * and takes the mean it finds, or none. Returns 60 when the measurement fails.
 */
__attribute__((noinline)) int btSbcConfigureTable(uint32_t frequency)
{
	int16_t mean;
	uint32_t calibrated;
	int result;

	result = 0;
	if ((fmSettings.features >> 4 & 4) != 0)
		return result;
	calibrated = fmSettings.tuneFlags >> 31;
	fmSettings.frequencyOffset = 0;
	fmSettings.tuneFlags &= ~FM_TUNE_OFFSET;
	if (btSbcUpdateTable(frequency, fmTuner.measureTolerance, &mean) != 0) {
		fmSettings.frequencyOffset = -mean;
	} else {
		result = 60;
		fmSettings.frequencyOffset = 0;
	}
	fmSettings.tuneFlags = (fmSettings.tuneFlags & ~FM_TUNE_OFFSET) | calibrated << 31;
	btFmFrequencyHook(frequency);
	return result;
}

/* Waits for the block's state to settle: 18 with bit 13 of the features, else 0. */
int btSbcCheckTable(void)
{
	unsigned int state;
	uint16_t polls;
	int result;

	polls = 0;
	result = 0;
	btSbcRomRecordContext(0);
	if ((fmSettings.features & 0x2000) != 0)
		state = 0x12;
	else
		state = 0;
	do {
		btSbcApplyEntry(0x21);
		polls++;
		if ((FM_REGISTER(0x00c) & 0x1f) == state)
			break;
	} while (polls < FM_TUNE_POLLS);
	if (polls == FM_TUNE_POLLS)
		result = FM_TUNE_TIMEOUT;
	FM_REGISTER(0x004) &= ~0x1000;
	return result;
}

/* Tunes to a frequency in the band, calibrating the offset there. */
int btSbcValidateTable(uint32_t frequency)
{
	uint32_t features;
	unsigned int mode;
	int result;

	features = fmSettings.features;
	if ((features & 0x1000) == 0)
		return 21;
	if (romFmBands[features & 0xf].high < frequency || romFmBands[features & 0xf].low > frequency)
		return 60;
	if ((features & 0x2000) == 0) {
		fmSettings.frequency = frequency;
		return 0;
	}
	if (fmSettings.frequency == frequency)
		return 0;
	result = btSbcCheckTable();
	if (result != 0)
		return result;
	mode = fmSettings.policy & 0xf;
	btSbcRomConfigureEntry(mode | 1);
	fmSettings.tuneFlags &= ~FM_TUNE_OFFSET;
	btSbcRomVerifyContext(0x20);
	btSbcRomAdjustContext(0xcc0, 0);
	result = btFmFrequencyHook(frequency);
	if (result != 0)
		return result;
	fmSettings.tuneFlags |= FM_TUNE_OFFSET;
	btSbcApplyEntry(0xa4);
	btSbcProcessTable(1);
	btSbcConfigureTable(frequency);
	btSbcProcessTable(0);
	btSbcRomConvertContext(0x20);
	btSbcBuildBuffer();
	btSbcRomRecordContext(1);
	btSbcRomConfigureEntry(mode);
	return result;
}

/* Sets the audio level and the registers that go with the mode (1 or not). */
int btSbcPrepareTable(int mode)
{
	unsigned int rearm;
	uint32_t level;

	rearm = fmSettings.policy >> 4 & 0xf;
	level = fmSettings.audioLevel & ~0x1fe0000;
	if (mode == 1) {
		FM_REGISTER(0x178) &= ~0x4000;
		FM_REGISTER(0x1d8) = (FM_REGISTER(0x1d8) | 6) & ~1;
		if (rearm == 0) {
			if (FM_REGISTER(0x000) >> 7 > 7)
				fmSettings.audioLevel = level + 0x1900000;
			else
				fmSettings.audioLevel = level + 0x1400000;
			FM_REGISTER(0x330) = 0x4a;
		} else {
			fmSettings.audioLevel = level + 0x1000000;
			FM_REGISTER(0x330) = 0xc5;
		}
		FM_REGISTER(0x1dc) = 0xadb;
	} else {
		FM_REGISTER(0x178) |= 0x4000;
		FM_REGISTER(0x1d8) = ((FM_REGISTER(0x1d8) & ~6) + 2) | 1;
		if (rearm == 0) {
			fmSettings.audioLevel = level + 0x1200000;
			FM_REGISTER(0x330) = 0x4a;
		} else {
			fmSettings.audioLevel = level + 0xc00000;
			FM_REGISTER(0x330) = 0xc5;
		}
		FM_REGISTER(0x1dc) = 0x49b;
	}
	btSbcRomProcessEntry(fmSettings.audioLevel << 15 >> 16);
	return 0;
}

/* Sends a 4-byte value as an FM event. */
int btSbcDispatchTable(uint32_t value)
{
	fmReply.status = 0;
	fmReply.command = FM_EVENT_VALUE;
	fmReply.data[2] = value >> 24;
	fmReply.data[1] = value >> 16;
	fmReply.data[0] = value >> 8;
	fmReply.result = value;
	return fmEventSend(&fmReply, 6, FM_REPLY_EVENT);
}

/* The level the block measures. */
__attribute__((noinline)) uint16_t btSbcParseTable(void)
{
	uint16_t level;

	level = FM_REGISTER(0x230) & 0x1ff;
	if ((fmSettings.levelFlags & 0x4000) != 0)
		level -= 0x38;
	return level;
}

/* Sets the measurement's mode, bits 3..0 of register 0x22c. */
__attribute__((noinline)) void btSbcBuildTable(unsigned int value)
{
	uint16_t control;

	control = FM_REGISTER(0x22c) & ~0xf;
	FM_REGISTER(0x22c) = control;
	FM_REGISTER(0x22c) = (control & ~0xf) | value;
}

/* Measures the level at a frequency. */
int btSbcComputeTable(uint32_t frequency)
{
	int result;

	result = btFmFrequencyHook(frequency);
	if (result != 0)
		return result;
	btSbcBuildTable(7);
	btSbcApplyEntry(0x42);
	return btSbcParseTable();
}

/* The measurement's limits, set and read by the vendor commands. */
void btSbcQueryTable(int16_t tolerance, int16_t calibratedTolerance, uint16_t spread, uint32_t calibrate)
{
	fmTuner.measureTolerance = tolerance;
	fmTuner.measureSpread = spread;
	fmTuner.calibratedTolerance = calibratedTolerance;
	fmTuner.searchCalibrate = calibrate;
}

void btSbcResetTable(uint8_t *tolerance, uint8_t *calibratedTolerance, uint8_t *spread, uint8_t *calibrate)
{
	*tolerance = fmTuner.measureTolerance;
	*spread = fmTuner.measureSpread;
	*calibratedTolerance = fmTuner.calibratedTolerance;
	*calibrate = fmTuner.searchCalibrate;
}

void btSbcSyncTable(uint8_t gain, uint8_t time, uint32_t scale)
{
	fmTuner.measureGain = gain;
	fmTuner.measureTime = time;
	fmTuner.measureScale = scale;
}

void btSbcApplyTable(uint8_t *gain, uint8_t *time, uint8_t *scale)
{
	*gain = fmTuner.measureGain;
	*time = fmTuner.measureTime;
	*scale = fmTuner.measureScale;
}

/* Turns the level measurement on, or off unless bit 28 of the features keeps it. */
__attribute__((noinline)) void btSbcResolveTable(int enable)
{
	if (enable != 0) {
		FM_REGISTER(0x234) |= 0x800;
		FM_REGISTER(0x230) |= 0x200;
		return;
	}
	if ((fmSettings.features & 0x10000000) == 0)
		FM_REGISTER(0x234) &= ~0x800;
	FM_REGISTER(0x230) &= ~0x200;
}

/*
 * Searches the band from a frequency for a station, a channel by the
 * spacing at a time, wrapping at the band's ends, until it finds one or has
 * been round the band. A station stands above its neighbours (the levels of
 * the last FM_SEARCH_RING channels are kept) and passes btSbcUpdateTable();
 * the first one may calibrate the offset. With mode 0 the search stops
 * there, else it reports each station and goes on. The quiet search looks
 * for the channel with the lowest level instead. Returns 60 when nothing was
 * found.
 */
int btSbcScheduleTable(uint32_t frequency, uint32_t *station, uint8_t mode)
{
	uint16_t levels[FM_SEARCH_RING + 1];
	uint8_t probes[FM_SEARCH_PROBES] = { 0, 1, FM_SEARCH_RING - 2, FM_SEARCH_RING - 1 };
	int16_t offsets[FM_SEARCH_PROBES];
	uint32_t features, startFrequency, freq;
	unsigned int tuneMode, hardware, step, current, i, k;
	unsigned int savedFilter, savedGain, savedMode, savedPolicy;
	uint16_t steps, maxSteps, level, bestLevel, value;
	int16_t mean;
	int tolerance, calibrated, stop, found, result;

	result = 60;
	freq = frequency;
	features = fmSettings.features;
	tuneMode = features >> 8 & 0xf;
	stop = 0;
	hardware = features >> 13 & 1;
	steps = 0;
	bestLevel = 0xffff;
	tolerance = fmTuner.measureTolerance;
	startFrequency = fmSettings.frequency;
	__aeabi_memclr4(levels, sizeof(levels));
	current = 4;
	if ((features & 0x1000) == 0)
		return 21;
	if (romFmBands[features & 0xf].high < frequency) {
		freq = romFmBands[features & 0xf].low;
		btSbcRomConvertContext(8);
	}
	if (romFmBands[fmSettings.features & 0xf].low > frequency) {
		freq = romFmBands[fmSettings.features & 0xf].high;
		btSbcRomConvertContext(8);
	}
	step = (uint8_t)(fmSettings.spacing / 50);
	if ((fmSettings.features & 0x8000) != 0) {
		offsets[0] = 200;
		offsets[1] = 150;
		offsets[2] = -150;
		offsets[3] = -200;
	} else {
		offsets[0] = -200;
		offsets[1] = -150;
		offsets[2] = 150;
		offsets[3] = 200;
	}
	calibrated = fmTuner.searchCalibrate == 0;
	value = FM_REGISTER(0x13c);
	savedFilter = value;
	FM_REGISTER(0x13c) = value & ~7;
	FM_REGISTER(0x14c) |= 1;
	value = FM_REGISTER(0x15c);
	savedGain = value;
	FM_REGISTER(0x15c) = (value & ~0x1c0) + 0x40;
	FM_REGISTER(0x000) &= ~1;
	btSbcCheckTable();
	if (hardware != 0 && (fmSettings.features >> 4 & 4) != 0)
		btFmResetHook();
	btSbcRomVerifyContext(0x20);
	btSbcRomHandleEntry(btSbcRomAdjustContext(0xcc0, 0));
	value = FM_REGISTER(0x22c);
	savedMode = value & 0xf;
	FM_REGISTER(0x22c) = (value & ~0xf) + 7;
	value = FM_REGISTER(0x2a8);
	savedPolicy = value & 0xf;
	FM_REGISTER(0x2a8) = value & ~0xf;
	fmSettings.tuneFlags &= ~FM_TUNE_OFFSET;
	fmSettings.frequencyOffset = 0;
	btSbcResolveTable(1);
	maxSteps = (romFmBands[fmSettings.features & 0xf].high - romFmBands[fmSettings.features & 0xf].low) /
		   fmSettings.spacing;
	btSbcPrepareTable(0);
	if ((fmSettings.features & 0x2000) == 0) {
		FM_REGISTER(0x004) |= 4;
		btFmTuneModeHook(4, freq);
		btSbcApplyEntry(0x290);
	}
	for (;;) {
		found = 0;
		btFmFrequencyHook(freq);
		btSbcBuildTable(7);
		btSbcApplyEntry(0x42);
		btSbcProcessTable(1);
		level = btSbcParseTable();
		levels[current] = level;
		if (((fmSettings.features >> 4 & 4) != 0 && hardware != 0) ||
		    (fmTuner.quietSearch == 1 && hardware == 0)) {
			if (btSbcManageEntry(((FM_REGISTER(0x174) >> 14) + 2) * freq * 4000) != 0 && level < bestLevel) {
				bestLevel = level;
				*station = freq;
				result = 0;
			}
		} else if (FM_LEVEL_FLOOR(fmSettings.levelFlags) < level) {
			for (i = 0; i < FM_SEARCH_PROBES; i = (uint8_t)(i + 1)) {
				k = probes[i];
				if (levels[k] == 0)
					levels[k] = btSbcComputeTable(offsets[i] + freq);
				if ((uint16_t)fmSettings.searchThreshold + levels[k] <= level)
					break;
			}
			if (i == FM_SEARCH_PROBES && found == 0)
				goto next;
			found = btSbcUpdateTable(freq, tolerance, &mean);
			if (found == 1 && calibrated == 0) {
				fmSettings.frequencyOffset = -mean;
				if (mode != 0) {
					btFmFrequencyHook(freq);
					FM_REGISTER(0x234) |= 0xc;
					FM_REGISTER(0x234) &= ~0xc;
					if (btSbcRomParseEntry(btSbcApplyEntry(0xa4)) != 0) {
						calibrated = 1;
						tolerance = fmTuner.calibratedTolerance;
					} else {
						fmSettings.frequencyOffset = 0;
					}
				}
			}
			__aeabi_memclr4(levels, sizeof(levels));
		}
next:
		btSbcProcessTable(0);
		if (found != 0) {
			result = 0;
			if (mode != 0)
				btSbcDispatchTable(freq);
			else
				stop = 1;
		}
		if (fmCoexCheckState() != 0)
			stop = 1;
		steps++;
		k = probes[0];
		for (i = 0; i < step; i = (uint8_t)(i + 1)) {
			levels[k] = 0;
			k = (uint8_t)(k + 1);
			if (k == FM_SEARCH_RING)
				k = 0;
		}
		current = (uint8_t)(current + step);
		if (current >= FM_SEARCH_RING)
			current = (uint8_t)(current - FM_SEARCH_RING);
		for (i = 0; i < FM_SEARCH_PROBES; i = (uint8_t)(i + 1)) {
			k = (uint8_t)(probes[i] + step);
			if (k >= FM_SEARCH_RING)
				k = (uint8_t)(k - FM_SEARCH_RING);
			probes[i] = k;
		}
		if (steps > maxSteps || stop != 0)
			break;
		if ((fmSettings.features & 0x8000) != 0) {
			freq -= fmSettings.spacing;
			if (romFmBands[fmSettings.features & 0xf].low > freq) {
				btSbcRomConvertContext(8);
				freq = romFmBands[fmSettings.features & 0xf].high;
				__aeabi_memclr4(levels, sizeof(levels));
			}
		} else {
			freq += fmSettings.spacing;
			if (romFmBands[fmSettings.features & 0xf].high < freq) {
				btSbcRomConvertContext(8);
				freq = romFmBands[fmSettings.features & 0xf].low;
				__aeabi_memclr4(levels, sizeof(levels));
			}
		}
	}
	if ((fmSettings.features >> 4 & 4) == 0) {
		if (mode != 0) {
			*station = startFrequency;
			result = 0;
		} else if (found != 0) {
			*station = freq;
		} else {
			*station = startFrequency;
		}
	}
	FM_REGISTER(0x14c) &= ~1;
	FM_REGISTER(0x13c) = savedFilter;
	FM_REGISTER(0x15c) = savedGain;
	FM_REGISTER(0x22c) = (FM_REGISTER(0x22c) & ~0xf) | savedMode;
	FM_REGISTER(0x2a8) = (FM_REGISTER(0x2a8) & ~0xf) | savedPolicy;
	btSbcResolveTable(0);
	if ((fmSettings.features >> 4 & 4) != 0 || hardware == 0)
		btFmResetHook();
	fmSettings.tuneFlags |= FM_TUNE_OFFSET;
	if (hardware != 0) {
		if ((fmSettings.features >> 4 & 4) != 0) {
			FM_REGISTER(0x004) |= 4;
			btFmTuneModeHook(tuneMode, *station);
		} else {
			btFmFrequencyHook(*station);
			if (mode == 1 || (mode | found) == 0) {
				btSbcApplyEntry(0x290);
				btSbcProcessTable(1);
				btSbcConfigureTable(*station);
				btSbcProcessTable(0);
			}
		}
	} else {
		fmSettings.frequency = *station;
		FM_REGISTER(0x000) |= 1;
	}
	btSbcBuildBuffer();
	btSbcRomConvertContext(0x20);
	btSbcRomRecordContext(1);
	btSbcTrackEntry(1000);
	return result;
}

/* Sets up the PCM port for the FM audio from the policy. */
void btSbcNotifyTable(uint32_t *config)
{
	uint32_t policy, word;

	policy = fmSettings.policy;
	word = config[0] & ~1;
	if ((policy >> 4 & 0xf) == 4)
		word |= 2;
	else
		word &= ~2;
	word &= ~0x7c;
	word = (word & ~0x10000000) | (uint32_t)((policy >> 17 & 1) == 0) << 28;
	config[1] = 0;
	word &= ~0x1800;
	word = (word & ~0xff00000) | ((policy >> 18 & 1) + 0x80) << 20;
	if ((policy >> 19 & 3) == 3) {
		word &= ~0xc0000;
	} else {
		word = (word & ~0xc0000) + 0x40000;
		word = (word & ~0x780) | (policy >> 19 & 3) << 7;
	}
	config[0] = word;
	config[2] = policy >> 8 & 0xf;
	config[0] = (config[0] & ~0x3e000) | (fmTuner.filterCodes[policy >> 23 & 3] & 0x1f) << 13;
}

/*
 * Starts the radio for a mode (FM_MODE_*), or stops it (0). Leaving the
 * transmit mode gives back the policy bits and the output register it held.
 */
int btSbcFormatTable(uint8_t mode)
{
	uint32_t config[3];
	unsigned int kind, rate;
	int result;

	if ((fmSettings.features & 0x1000) == 0)
		return 21;
	result = btSbcCheckTable();
	if (result != 0)
		return result;
	if ((fmSettings.features & 0x2000) != 0) {
		result = btFmResetHook();
		if (result != 0)
			return result;
	}
	if ((fmTuner.mode & FM_MODE_TRANSMIT) != 0) {
		FM_REGISTER(0x318) = fmTuner.savedOutput;
		fmSettings.policy = (((((fmSettings.policy & ~0xf0) | (fmTuner.savedRate & 0xf) << 4) & ~0x40000) |
				      (fmTuner.savedLevel & 1) << 18) & ~0x180000) | (fmTuner.savedMode & 3) << 19;
	}
	fmSettings.features = (fmSettings.features & ~0xf0) | (mode & 0xf) << 4;
	fmTuner.mode = mode;
	if (mode == 0) {
		FM_REGISTER(0x000) |= 1;
		btSbcScheduleEntry(0);
		FM_REGISTER(0x394) &= ~1;
		goto done;
	}
	fmSettings.audioSetting = 99;
	btFmApplyHook(99, 1);
	if ((fmSettings.policy >> 4 & 0xf) == 1) {
		btSbcNotifyTable(config);
		config[0] = (config[0] & ~0xff00000) | (uint32_t)((fmSettings.policy >> 18 & 1) == 0) << 20;
		sbcPcmStreamHook(config);
		btAudioPathDataInvert(fmSettings.policy >> 18 & 1);
	}
	if ((mode & FM_MODE_TRANSMIT) != 0) {
		kind = 6;
		FM_REGISTER(0x1d8) = (FM_REGISTER(0x1d8) | 6) & ~1;
		FM_REGISTER(0x1dc) = 0xadb;
		fmTuner.savedOutput = FM_REGISTER(0x318);
		FM_REGISTER(0x318) = 0x1010;
		fmTuner.savedRate = fmSettings.policy >> 4 & 0xf;
		fmTuner.savedLevel = fmSettings.policy >> 18 & 1;
		fmTuner.savedMode = fmSettings.policy >> 19 & 3;
		btSbcRecordTable(1);
		btSbcRomQueryEntry(1, 1, 0);
	} else if ((mode & FM_MODE_RECEIVE) != 0) {
		kind = 3;
		if ((fmSettings.policy >> 4 & 0xf) != 0)
			kind = 2;
		btSbcPrepareTable(1);
		fmSettings.frequencyOffset = 0;
		if ((mode & FM_MODE_SCHEDULED) != 0 && (fmSettings.features >> 4 & 2) == 0) {
			kind = 3;
			btSbcRomScheduleEntry(1);
		}
		FM_REGISTER(0x394) |= 1;
	} else {
		kind = 1;
		if ((fmSettings.policy >> 4 & 0xf) != 0)
			kind = 0;
		fmSettings.tuneFlags &= ~FM_TUNE_OFFSET;
		btSbcPrepareTable(0);
		FM_REGISTER(0x394) &= ~1;
	}
	FM_REGISTER(0x000) &= ~1;
	rate = fmSettings.policy & 0xf;
	btSbcRomConfigureEntry(rate | 1);
	FM_REGISTER(0x004) |= 4;
	btFmStereoHook((fmSettings.features >> 4 & 2) != 0);
	result = btFmTuneModeHook(kind, fmSettings.frequency);
	if (result != 0)
		return result;
	if ((mode & FM_MODE_TRANSMIT) == 0) {
		if ((mode & FM_MODE_RECEIVE) != 0) {
			/* The original passes the mode's bits 2..0 from bit 29 as the second argument. */
			btSbcRomReleaseEntry(mode & 1, (uint32_t)mode << 29);
			btSbcSyncBuffer(125, 0);
			btSbcTrackEntry(100);
		} else {
			btSbcRomClearEntry();
			btSbcTrackEntry(1000);
			fmSettings.tuneFlags |= FM_TUNE_OFFSET;
			btSbcApplyEntry(0x290);
			btSbcProcessTable(1);
			btSbcConfigureTable(fmSettings.frequency);
			btSbcProcessTable(0);
			btSbcBuildBuffer();
		}
	}
	btSbcRomConfigureEntry(rate);
done:
	btSbcRomRecordContext(1);
	return 0;
}

/* Vendor command 0x5a: its value and flag; retunes when the block is the hardware tuner. */
int btSbcInspectTable(uint32_t value, uint8_t mode)
{
	int result;

	result = 0;
	fmSettings.vendorSetting = value;
	fmSettings.levelFlags = (fmSettings.levelFlags & ~FM_LEVEL_VENDOR) | (uint32_t)mode << 31;
	if ((fmSettings.features & 0x2000) != 0) {
		result = btSbcCheckTable();
		if (result == 0) {
			btFmFrequencyHook(fmSettings.frequency);
			btSbcRomRecordContext(1);
		}
	}
	return result;
}

/* The level of the channel, and whether it is below the floor. */
int btSbcTrackTable(uint16_t *value, uint8_t *active)
{
	if ((fmSettings.features & 0x2000) == 0)
		return 41;
	if ((fmSettings.features >> 4 & 4) != 0)
		return 25;
	*value = btSbcParseTable();
	btSbcRomResetEntry();
	if (*value < FM_LEVEL_FLOOR(fmSettings.levelFlags)) {
		*active = 1;
		btSbcRomConvertContext(1);
	} else {
		*active = 0;
		btSbcRomVerifyContext(1);
	}
	return 0;
}

/* Measures the second level, bits 11..0 of register 0x2b4. */
__attribute__((noinline)) uint16_t btSbcManageTable(void)
{
	uint16_t value;

	FM_REGISTER(0x290) |= 2;
	btSbcApplyEntry(0x42);
	value = FM_REGISTER(0x2b4) & 0xfff;
	FM_REGISTER(0x290) &= ~2;
	return value;
}

int btSbcAcquireTable(uint16_t *value, uint8_t *active)
{
	if ((fmSettings.features & 0x2000) == 0)
		return 41;
	if ((fmSettings.features >> 4 & 4) != 0)
		return 25;
	*value = btSbcManageTable();
	*active = 0;
	return 0;
}

/* Tunes a channel up, wrapping to the band's start (62). */
int btSbcReleaseTable(uint32_t *value)
{
	int result, error;

	result = 0;
	if ((fmSettings.features & 0x1000) == 0)
		return 21;
	*value = fmSettings.frequency + fmSettings.spacing;
	if (*value > romFmBands[fmSettings.features & 0xf].high) {
		result = 62;
		*value = romFmBands[fmSettings.features & 0xf].low;
	}
	error = btSbcValidateTable(*value);
	if (error != 0)
		return error;
	return result;
}

/* Tunes a channel down, wrapping to the band's end (63). */
int btSbcSetupTable(uint32_t *value)
{
	int result, error;

	result = 0;
	if ((fmSettings.features & 0x1000) == 0)
		return 21;
	*value = fmSettings.frequency - fmSettings.spacing;
	if (*value < romFmBands[fmSettings.features & 0xf].low) {
		result = 63;
		*value = romFmBands[fmSettings.features & 0xf].high;
	}
	error = btSbcValidateTable(*value);
	if (error != 0)
		return error;
	return result;
}

/*
 * Checks whether a frequency carries a station: the level must rise when a
 * search starts there; then, with block A matching set, waits for an RDS group
 * to match it. Tunes there when it does, else back (60).
 */
int btSbcClearTable(uint32_t value, uint32_t *result, uint8_t mode)
{
	unsigned int rate;
	uint16_t before, after;
	int error;

	(void)mode;
	if ((fmSettings.features & 0x1000) == 0)
		return 21;
	if ((fmSettings.features >> 4 & 1) == 0)
		return 23;
	if (romFmBands[fmSettings.features & 0xf].high < value || romFmBands[fmSettings.features & 0xf].low > value)
		return 20;
	error = btSbcCheckTable();
	if (error != 0)
		return error;
	fmSettings.tuneFlags &= ~FM_TUNE_OFFSET;
	*result = fmSettings.frequency;
	before = btSbcParseTable();
	error = btSbcHandleTable(value, 1);
	if (error != 0)
		return error;
	after = btSbcParseTable();
	error = btSbcHandleTable(*result, 0);
	if (error != 0)
		return error;
	if (after <= before) {
		error = 60;
	} else {
		rate = fmSettings.policy & 0xf;
		btSbcRomConfigureEntry(rate | 1);
		error = btFmFrequencyHook(value);
		if (error != 0)
			return error;
		btSbcRomVerifyContext(0x80);
		if (fmSettings.rdsBlockAMatch >> 16 != 0) {
			tx_thread_sleep(25);
			if ((fmSettings.eventStatus & 0x80) == 0) {
				error = btFmFrequencyHook(*result);
				if (error != 0)
					return error;
				error = 60;
				goto restore;
			}
		}
		*result = value;
restore:
		btSbcRomConfigureEntry(rate);
	}
	fmSettings.tuneFlags |= FM_TUNE_OFFSET;
	btSbcRomRecordContext(1);
	return error;
}

/* Marks the FM task idle (1) or busy; going idle releases the hardware tuner. */
void btSbcConvertTable(int enable)
{
	btSbcCheckTable();
	if (enable == 1 && (fmSettings.features & 0x2000) != 0)
		btFmResetHook();
	fmTuner.idle = enable;
	btSbcRomRecordContext(1);
}

int btSbcVerifyTable(void)
{
	return fmTuner.idle;
}

/*
 * The PCM port's code for a frequency, and 2. The original returns them in
 * the two low bytes of a 4-byte struct whose other bytes it leaves unset.
 */
uint32_t btSbcSelectTable(uint32_t frequency)
{
	const struct FmClockCode *entry;

	for (entry = fmClockCodes; entry->limit < frequency >> 3; entry++)
		;
	return entry->code | 2 << 8;
}

void btSbcAdjustTable(uint8_t value)
{
	fmTuner.quietSearch = value;
}

/* Turns bits 12..11 of register 0x008 on or off. */
__attribute__((noinline)) void btSbcRecordTable(int enable)
{
	if (enable != 0)
		FM_REGISTER(0x008) |= 0x1800;
	else
		FM_REGISTER(0x008) &= ~0x1800;
}

/* Ends RDS reception. */
void btSbcFilterTable(void)
{
	tx_event_flags_set((TX_EVENT_FLAGS_GROUP *)fmFlags, 0x10, TX_OR);
	btSbcRomConvertContext(0x10);
	fmSettings.rdsFlags &= ~FM_RDS_RECEIVING;
}

/*
 * Sets the receiver's gain from the level register 0x2b0 reads, less
 * levelBase: mode 0 the highest gain, mode 3 none, otherwise the gain step the
 * level falls in, whose ends the ROM then watches.
 */
int btSbcHandleBuffer(int mode)
{
	const struct FmGainStep *step;
	unsigned int level, high, low;

	level = (uint8_t)(FM_REGISTER(0x2b0) - fmTuner.levelBase);
	for (step = fmGainSteps; step->high < level; step++)
		;
	if (mode == 0 || mode == 3) {
		btSbcRomComputeEntry(mode == 0 ? 0x7f : 0);
	} else {
		btSbcRomComputeEntry(step->gain);
		high = (uint8_t)(step->high + fmTuner.levelBase + 1);
		if (step->high > high)
			high = 0xff;
		low = (uint8_t)(step->low + fmTuner.levelBase - 1);
		if (step->low > low)
			low = 0xff;
		btSbcRomPrepareEntry(low, high, step->low);
	}
	return 0;
}

__attribute__((noinline)) const struct FmFrequencyStep *btSbcProcessBuffer(uint32_t frequency)
{
	const struct FmFrequencyStep *step;

	for (step = fmFrequencySteps; step->limit < frequency >> 3; step++)
		;
	return step;
}

/* Sets register 0x160 for a frequency. */
void btSbcUpdateBuffer(uint32_t frequency)
{
	btSbcResetBuffer(btSbcProcessBuffer(frequency)->setting + fmTuner.antennaOffset);
}

void btSbcConfigureBuffer(uint8_t scan, uint8_t offset)
{
	fmTuner.antennaScan = scan;
	if (offset > 17)
		offset = 17;
	fmTuner.antennaOffset = offset;
}

/*
 * After a search (btFmSearchHook): with the offset calibrated, a strong
 * enough station and the scan on, tries the 32 settings of register 0x160 and
 * keeps the mean of those that read the highest level.
 */
void btSbcCheckBuffer(uint32_t frequency)
{
	uint16_t sum = 0, best = 0, ties = 0, index, saved;
	unsigned int enabled, rate, level, setting;

	(void)frequency;
	enabled = FM_REGISTER(0x05c) & 1;
	btSbcRomHandleEntry(enabled);
	if ((fmSettings.tuneFlags & FM_TUNE_OFFSET) != 0 && fmTuner.antennaScan != 0) {
		saved = FM_REGISTER(0x22c) & 0xf;
		FM_REGISTER(0x22c) = (FM_REGISTER(0x22c) & ~0xf) + 8;
		rate = FM_REGISTER(0x12c) >> 7 & 0xf;
		if (btSbcParseTable() > 50) {
			for (index = 0; index < 32; index++) {
				FM_REGISTER(0x160) = (((FM_REGISTER(0x160) & ~0x1f) | index) & ~0x3e0) | index << 5;
				btSbcApplyEntry(33);
				level = btSbcParseTable();
				if (level > best) {
					best = level;
					sum = index;
					ties = 1;
				} else if (level == best) {
					sum += index;
					ties++;
				}
			}
			setting = sum / ties;
			if (rate != 8 && setting < 30)
				setting = (uint16_t)(setting + 2);
			btSbcResetBuffer(setting);
		}
		FM_REGISTER(0x22c) = (FM_REGISTER(0x22c) & ~0xf) | saved;
	}
	btSbcApplyEntry(33);
	btSbcParseTable();
	btSbcRomResetEntry();
	if (enabled != 0)
		btSbcTrackEntry(1000);
}

/* Vendor command: 1 turns the level events off, 2 fixes the gain, others go to the ROM. */
int btSbcValidateBuffer(uint8_t value)
{
	int mode;

	fmSettings.policy |= 0x20000;
	fmTuner.levelMode = 0;
	if (value == 1) {
		fmSettings.policy &= ~0x20000;
		mode = 0;
	} else if (value == 2) {
		fmTuner.levelMode = 1;
		mode = 3;
	} else {
		mode = btSbcRomParseEntry(value);
	}
	btSbcHandleBuffer(mode);
	return 0;
}

/* What btSbcValidateBuffer() was given: 0 (the ROM's), 1 or 2. */
uint8_t btSbcPrepareBuffer(void)
{
	return fmTuner.levelMode * 2 + 1 - (fmSettings.policy >> 17 & 1);
}

int btSbcDispatchBuffer(uint8_t value)
{
	int mode = 0;

	if (value < 2)
		value = 2;
	fmTuner.levelBase = value;
	if (fmTuner.levelMode != 0)
		mode = 3;
	else if ((fmSettings.policy & 0x20000) != 0)
		mode = btSbcRomParseEntry(0);
	btSbcHandleBuffer(mode);
	return 0;
}

uint8_t btSbcParseBuffer(void)
{
	return fmTuner.levelBase;
}

/* Sets the gain up again after the receiver starts. */
void btSbcBuildBuffer(void)
{
	int mode = 0;

	btSbcRomDispatchEntry(0x5f, 0x5f);
	btSbcRomAdjustContext(0xcc0, 1);
	if (fmTuner.levelMode != 0)
		mode = 3;
	else if ((fmSettings.policy & 0x20000) != 0)
		mode = btSbcRomParseEntry(fmSettings.policy << 14);
	btSbcHandleBuffer(mode);
}

/* Starts counting the searches that see an RDS group, up to limit. */
void btSbcComputeBuffer(uint32_t limit)
{
	fmTuner.searchCount = 0;
	fmTuner.rdsMissed = 0;
	fmTuner.rdsReceived = 0;
	fmTuner.searchLimit = limit;
}

/*
 * The RDS count's result: the groups received and missed, the groups
 * expected and the percentage received. 23 without RDS, 1 while counting.
 */
/* A 16-bit value at any byte, the high byte first. */
#define FM_PUT16(p, v)	do { (p)[1] = (v) >> 8; (p)[0] = (v); } while (0)

/* The RDS block counts: received, missed and expected (16-bit each), and the percentage received. */
int btSbcQueryBuffer(uint8_t *data, uint8_t *length)
{
	uint16_t expected;

	expected = fmTuner.searchLimit * 10000 / 875;
	*length = 0;
	if ((fmSettings.features >> 4 & 1) == 0)
		return 23;
	if (fmTuner.searchCount <= fmTuner.searchLimit)
		return 1;
	FM_PUT16(data, fmTuner.rdsReceived);
	FM_PUT16(data + 2, fmTuner.rdsMissed);
	FM_PUT16(data + 4, expected);
	data[6] = fmTuner.rdsReceived * 100 / expected;
	*length = 7;
	return 0;
}

/* Register 0x160: the setting in bits 9..5 and, from 2 up, the setting less 2 in bits 4..0. */
__attribute__((noinline)) void btSbcResetBuffer(unsigned int setting)
{
	uint16_t value;

	value = (FM_REGISTER(0x160) & ~0x3e0) | setting << 5;
	if (setting >= 2)
		value = (value & ~0x1f) | (setting - 2);
	else
		value &= ~0x1f;
	FM_REGISTER(0x160) = value;
}

/* Register 0x394's step for a transmit level, one higher when raising. */
void btSbcSyncBuffer(uint32_t value, int raise)
{
	unsigned int step;

	step = btSbcInspectBuffer(value)->settings & 0x1f;
	if (raise != 0)
		step++;
	FM_REGISTER(0x394) = (FM_REGISTER(0x394) & ~0x3e) | step << 1;
}

/*
 * Sets the transmitter's registers for a level (btFmApplyHook). Register
 * 0x398 takes one output byte before and the other after the rest, in the
 * order swap gives.
 */
void btSbcApplyBuffer(uint32_t value, int swap)
{
	const struct FmLevelStep *step;

	step = btSbcInspectBuffer(value);
	if (swap != 0)
		FM_REGISTER(0x398) = (FM_REGISTER(0x398) & 0xff) | (step->outputs >> 24) << 8;
	else
		FM_REGISTER(0x398) = (FM_REGISTER(0x398) & 0xff00) | (step->outputs & 0xff);
	FM_REGISTER(0x39c) = (step->outputs >> 8 & 0xff) | (step->outputs >> 16 & 0xff) << 8;
	FM_REGISTER(0x38c) = (FM_REGISTER(0x38c) & ~0x3f) | (step->settings >> 10 & 0x3f);
	FM_REGISTER(0x390) = (FM_REGISTER(0x390) & ~0x7) | (step->settings >> 16 & 0x7);
	FM_REGISTER(0x390) = (FM_REGISTER(0x390) & 0x7ff) | (step->settings >> 5 & 0x1f) << 11;
	if (swap != 0)
		FM_REGISTER(0x398) = (FM_REGISTER(0x398) & 0xff00) | (step->outputs & 0xff);
	else
		FM_REGISTER(0x398) = (FM_REGISTER(0x398) & 0xff) | (step->outputs >> 24) << 8;
}

/* Moves the level threshold until register 0x394's bit 6 changes. */
void btSbcResolveBuffer(void)
{
	if ((FM_REGISTER(0x394) & 0x40) != 0) {
		if (btFmLevelThreshold < FM_LEVEL_HIGH) {
			do {
				btSbcSyncBuffer(++btFmLevelThreshold, 1);
				btSbcApplyEntry(1);
			} while ((FM_REGISTER(0x394) & 0x40) != 0);
			btSbcSyncBuffer(--btFmLevelThreshold, 0);
		}
	} else {
		do {
			btSbcSyncBuffer(--btFmLevelThreshold, 0);
			btSbcApplyEntry(1);
		} while ((FM_REGISTER(0x394) & 0x40) == 0);
	}
}

/*
 * Steps the transmit level: down to the threshold, down when the output
 * overloads, up towards audioLimit while it is quiet.
 */
void btSbcScheduleBuffer(void)
{
	unsigned int level, limit, count;
	int raise = 0;

	level = fmSettings.audioSetting;
	limit = btFmLevelThreshold;
	if (level > limit) {
		fmSettings.audioSetting = limit;
	} else if ((FM_REGISTER(0x3b8) & 0x4000) != 0) {
		if (level <= 100)
			return;
		fmSettings.audioSetting = level - 1;
	} else {
		if ((FM_REGISTER(0x38c) >> 6 & 0x3f) > 40) {
			count = FM_REGISTER(0x3a4) & 0x7f;
			if (count > 120) {
				fmSettings.audioSetting = level - 1;
				goto apply;
			}
			if (count >= 100)
				return;
		}
		if (fmSettings.audioLimit < limit)
			limit = fmSettings.audioLimit;
		if (level >= limit)
			return;
		raise = 1;
		fmSettings.audioSetting = level + 1;
	}
apply:
	btFmApplyHook(fmSettings.audioSetting, raise);
}

/*
 * Vendor command: the highest transmit level, 99..125 (117 with level flag
 * 8). A transmitter that is on moves to it at once.
 */
int btSbcNotifyBuffer(uint8_t value)
{
	unsigned int limit = value;
	int raise;

	if (limit < FM_LEVEL_LOW)
		limit = FM_LEVEL_LOW;
	else if (limit > FM_LEVEL_HIGH)
		limit = FM_LEVEL_HIGH;
	if ((fmSettings.levelFlags & 0x100) != 0 && limit > 117)
		limit = 117;
	if (fmSettings.audioLimit != limit) {
		btSbcRomHandleEntry(fmSettings.audioLimit);
		fmSettings.audioLimit = limit;
		if ((fmSettings.features & 0x2000) != 0 && (fmSettings.features & 0x40) != 0) {
			if (fmSettings.audioSetting >= fmSettings.audioLimit) {
				fmSettings.audioSetting = fmSettings.audioLimit;
				raise = 0;
			} else {
				limit = btFmLevelThreshold;
				if (fmSettings.audioLimit <= limit)
					limit = fmSettings.audioLimit;
				raise = 1;
				fmSettings.audioSetting = limit;
			}
			btFmApplyHook(fmSettings.audioSetting, raise);
			FM_REGISTER(0x3a8) |= 0x1000;
			FM_REGISTER(0x3a8) &= ~0x1000;
		}
		btSbcTrackEntry(100);
	}
	return 0;
}

/*
 * Vendor command: the transmitter's deviations. Their total (the tone's
 * counts 5.67 times) is kept to 22500..100000; the gain that brings the
 * largest to full scale, 50..255, and each deviation's share of it, rounded,
 * go to registers 0x318..0x320. A transmitter that is on is retuned.
 */
int btSbcFormatBuffer(uint32_t first, uint32_t second, uint32_t third, uint32_t tone)
{
	uint32_t total, largest, gain, scale, shares[3], toneShare;
	uint16_t level;
	int error;

	total = tone * 567 / 100 + first + second + third;
	if (total > 100000)
		total = 100000;
	else if (total < 22500)
		total = 22500;
	largest = 0;
	if (first == 0 || second == 0 || third == 0 || tone == 0)
		largest = total;
	if (largest <= first)
		largest = first;
	if (largest <= second)
		largest = second;
	if (largest <= third)
		largest = third;
	if (largest <= tone)
		largest = tone;
	gain = (largest << 7) / total;
	scale = tone * 72576 / 100 / total;
	level = gain > scale ? gain : scale;
	if (level > 0xff)
		gain = 0xff;
	else if (level < 50)
		gain = 50;
	else
		gain = level;
	scale = total * gain;
	shares[0] = romMulAdd32x32(scale, first, 0xff00) / (2 * scale);
	shares[1] = romMulAdd32x32(scale, second, 0xff00) / (2 * scale);
	shares[2] = romMulAdd32x32(scale, third, 0xff00) / (2 * scale);
	toneShare = romMulAdd32x32(100 * scale, tone, 0x234c900) / (200 * scale);
	FM_REGISTER(0x300) |= 0x40;
	FM_REGISTER(0x318) = shares[0] << 8 | shares[1];
	level = (FM_REGISTER(0x31c) & 0xff) | shares[2] << 8;
	FM_REGISTER(0x31c) = (level & ~0xff) | toneShare;
	FM_REGISTER(0x320) = (FM_REGISTER(0x320) & 0xff00) | gain;
	fmSettings.deviation = total;
	if ((fmSettings.features & 0x2000) != 0) {
		error = btSbcCheckTable();
		if (error != 0)
			return error;
		btFmFrequencyHook(fmSettings.frequency);
		btSbcRomRecordContext(1);
	}
	return 0;
}

/* The level step of a transmit level, clamped to 99..125. */
__attribute__((noinline)) const struct FmLevelStep *btSbcInspectBuffer(uint32_t level)
{
	if (level < FM_LEVEL_LOW)
		level = FM_LEVEL_LOW;
	else if (level > FM_LEVEL_HIGH)
		level = FM_LEVEL_HIGH;
	return &fmLevelSteps[FM_LEVEL_HIGH - level];
}
