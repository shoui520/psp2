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
#include "robin/rom.h"
#include "robin/sbc.h"
#include "robin/soc.h"
#include "robin/socpower.h"

struct FmTuner fmTuner __attribute__((section(".itcm.rw"))) = {	/* 0x55694 */
	.reserved2 = { 0x0f, 0x17, 0x1f, 0x00 },
	.reserved3 = 1,
	.filters = (const uint32_t (*)[3])romFmTable,
	.searchLimit = 0x71,
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
	uint32_t clock;

	clock = fmSettings.referenceClock;
	if (socStrapIsSet() == 0)
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
		return uapActiveEntryGet();
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
	fmTuner.band = 30;
	btSbcRomAcquireEntry(30, &fmTuner.band);
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
	fmSettings.reserved1 = 15;
	fmTuner.reserved7[0] = 1;
	fmTuner.reserved7[1] = 0;
	fmTuner.reserved0 = 0;
	fmTuner.reserved13[0] = 20;
	fmTuner.reserved13[1] = 15;
	fmTuner.reserved13[2] = 10;
	fmTuner.reserved14 = 1;
	fmTuner.reserved10 = 2;
	fmTuner.reserved11 = 10;
	fmTuner.reserved15 = 1;
	return 0;
}
