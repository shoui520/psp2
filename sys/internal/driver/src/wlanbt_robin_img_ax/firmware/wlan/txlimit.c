/*
 * The transmit power compensation for the temperature (0x3e09c - 0x3e86a): the SoC's temperature
 * sensor is sampled when a channel is set and while the radio runs, a calibration entry of the
 * channel says how far the sample moves the power limit, and the limit goes to the radio's bus.
 * The firmware's placeholder names, "wlanPs*Header" and "wlanPs*Descriptor", are kept; see
 * robin/txlimit.h.
 */
#include <stddef.h>
#include "robin/calibration.h"
#include "robin/ie.h"
#include "robin/phy.h"
#include "robin/rf.h"
#include "robin/rfbus.h"
#include "robin/rom.h"
#include "robin/socpower.h"
#include "robin/softtimer.h"
#include "robin/sys.h"
#include "robin/txlimit.h"

#define WLAN_LIMIT_MAX		504
#define WLAN_LIMIT_NONE		0xffff
#define WLAN_LIMIT_CALLBACKS	2
#define WLAN_LIMIT_SCALE_BASE	256

extern const uint16_t romRfChannelTable5G[];	/* ROM 0x03f4214c: 5 GHz frequencies, by wlanChannelIndex */
extern uint8_t wlanChannelIndex[256];		/* 0x04000568 */
unsigned int macTxNotifySelectPacket(void);	/* 0x32d30 */

/* The compensation's state (0x551e0). */
static struct {
	uint8_t tolerance;		/* the samples that count as the same */
	uint8_t stable;			/* the last sample that held */
	uint8_t count;			/* the callbacks registered */
	uint8_t reserved3;
	uint32_t period;		/* the sampling period, 10 ms times ten */
	uint32_t frame;			/* a sample was asked for by wlanPsScheduleHeader() */
	uint32_t reserved12;
	uint64_t timestamp;		/* the ROM timer when the sample was taken */
	void (*callbacks[WLAN_LIMIT_CALLBACKS])(int sample, int frame);
	uint8_t queue;			/* bit 0: the limit changed and is written to the radio */
	uint8_t limit;			/* the value last written to the radio */
	uint8_t reserved34[2];
	uint8_t active;			/* wlanPsCheckDescriptor() put the compensation on the sampling */
	uint8_t reserved37[3];
	struct WlanLimitState *state;	/* 0x596f4 */
	uint8_t bandScale[4];		/* 256ths of the limit by the part of the 5 GHz band */
	uint8_t curveLoaded;		/* the calibration entry of the reference channel is in curve */
	uint8_t reserved49[3];
	uint32_t slot;			/* nonzero: the sample is not tracked */
	struct WlanLimitCurve tracking;	/* what the host's calibration gave (wlanPsValidateDescriptor) */
	struct WlanLimitCurve curve;	/* the entry of the reference channel (wlanPsConvertHeader) */
	uint8_t channelScale[14];	/* 256ths, by 2.4 GHz channel */
} wlanLimit __attribute__((section(".itcm.rw"))) = {
	.tolerance = 2,
	.period = 500,
	.state = (struct WlanLimitState *)0x596f4,
	.bandScale = { 0xb7, 0x6f, 0xb7, 0xe0 },
	.channelScale = { 0xa4, 0xa8, 0xac, 0xaf, 0xb3, 0xb7, 0xbb, 0xbf, 0xc3, 0xc6, 0xca, 0xce, 0xd2, 0xd2 },
};

_Static_assert(offsetof(typeof(wlanLimit), state) == 0x28, "limit state pointer");
_Static_assert(offsetof(typeof(wlanLimit), tracking) == 0x38, "limit tracking curve");
_Static_assert(offsetof(typeof(wlanLimit), channelScale) == 0x48, "limit channel scale");

static struct WlanSoftTimer wlanLimitTimer __attribute__((section(".itcm.bss")));	/* 0x596dc */

static unsigned int wlanLimitClamp(int limit)
{
	if (limit > WLAN_LIMIT_MAX)
		return WLAN_LIMIT_MAX;
	if (limit < 0)
		return 0;
	return limit;
}

/* The sample once it has held for two reads. */
int wlanPsSyncHeader(void)
{
	int sample;

	for (sample = socSensorSample();
	     wlanLimit.stable - wlanLimit.tolerance > sample || wlanLimit.stable + wlanLimit.tolerance < sample;
	     sample = socSensorSample())
		wlanLimit.stable = sample;
	wlanLimit.stable = sample;
	return sample;
}

/* Takes a sample and hands it to the callbacks. */
__attribute__((noinline)) int wlanPsApplyHeader(void)
{
	int sample = wlanPsSyncHeader();
	unsigned int i;

	for (i = 0; i < WLAN_LIMIT_CALLBACKS; i = (uint8_t)(i + 1)) {
		if (wlanLimit.callbacks[i] != NULL)
			wlanLimit.callbacks[i](sample, wlanLimit.frame);
	}
	wlanLimit.timestamp = romGetTimerTimestamp();
	wlanLimit.frame = 0;
	return 0;
}

void wlanPsResolveHeader(uint32_t argument)
{
	wlanPsApplyHeader();
}

/* The next sample is the first. */
__attribute__((noinline)) void wlanPsScheduleHeader(void)
{
	wlanLimit.stable = 0;
	wlanLimit.frame = 1;
}

/* Starts the periodic sampling. */
void wlanPsNotifyHeader(void)
{
	wlanLimit.timestamp = romGetTimerTimestamp();
	wlanPsScheduleHeader();
	wlanPsApplyHeader();
	wlanSoftTimerInit(&wlanLimitTimer);
	wlanSoftTimerStartPeriodic(&wlanLimitTimer, 1, wlanPsResolveHeader, 0, wlanLimit.period / 10);
}

/* Registers a callback for the samples; the first one starts the sampling. */
int wlanPsFormatHeader(void (*callback)(int sample, int frame))
{
	unsigned int count;
	unsigned int i;

	if (callback != NULL) {
		count = wlanLimit.count;
		if (count != 2) {
			for (i = 0; wlanLimit.callbacks[i] != NULL;) {
				i = (uint8_t)(i + 1);
				if (i >= WLAN_LIMIT_CALLBACKS)
					return (int)(uintptr_t)callback;
			}
			wlanLimit.callbacks[i] = callback;
			if (count == 0)
				wlanPsNotifyHeader();
			return wlanLimit.count++ + 1;
		}
	}
	return (int)(uintptr_t)callback;
}

/* Removes a callback; the last one stops the sampling. */
int wlanPsInspectHeader(void (*callback)(int sample, int frame))
{
	unsigned int i;
	unsigned int count;

	if (callback != NULL && wlanLimit.count != 0) {
		for (i = 0; wlanLimit.callbacks[i] != callback;) {
			i = (uint8_t)(i + 1);
			if (i >= WLAN_LIMIT_CALLBACKS)
				return (int)(uintptr_t)callback;
		}
		count = (uint8_t)(wlanLimit.count - 1);
		wlanLimit.callbacks[i] = NULL;
		wlanLimit.count = count;
		if (count == 0)
			return wlanSoftTimerStop(&wlanLimitTimer);
		return count;
	}
	return (int)(uintptr_t)callback;
}

/* The limit the curve gives for the sample, 0 to 63. */
int wlanPsTrackHeader(const struct WlanLimitCurve *curve, int sample)
{
	int reference = curve->reference;
	int delta = sample - reference;
	int bend = curve->corner - reference;
	int outer;
	int inner;
	int result;
	int part;

	if (bend < 0) {
		inner = 0;
		outer = delta;
		if (delta < bend) {
			inner = delta - bend;
			outer = bend;
		}
	} else {
		outer = 0;
		inner = delta;
		if (delta > bend) {
			outer = delta - bend;
			inner = bend;
		}
	}
	part = curve->gain * outer / curve->divisorAbove;
	result = curve->offset + curve->gain * inner / curve->divisorBelow + part;
	if (result >= 0) {
		if (result > 63)
			result = 63;
	} else {
		result = 0;
	}
	return (uint8_t)result;
}

/* The change of the limit between two samples: eighths of the gain, per divisor, either side of the corner. */
__attribute__((noinline)) int wlanPsManageHeader(int sample, int reference, int corner, int gain, int divisorBelow, int divisorAbove)
{
	int bend = corner - reference;
	int delta = sample - reference;
	int excess = delta - bend;
	int outer;
	int inner;
	int part;

	if (bend < 0) {
		inner = 0;
		outer = delta;
		if (delta < bend) {
			inner = excess;
			outer = bend;
		}
	} else {
		outer = 0;
		inner = delta;
		if (delta > bend) {
			outer = excess;
			inner = bend;
		}
	}
	part = 8 * outer * gain / divisorAbove;
	return 8 * inner * gain / divisorBelow + part;
}

/* The change of the limit from the reference sample to this one, by the host's curve (mirrored: its divisors swap). */
int wlanPsAcquireHeader(int sample, int reference)
{
	return wlanPsManageHeader(sample, reference, wlanLimit.tracking.corner, wlanLimit.tracking.gain,
				  wlanLimit.tracking.divisorAbove, wlanLimit.tracking.divisorBelow);
}

/* Starts the compensation with nothing known. */
void wlanPsReleaseHeader(void)
{
	struct WlanLimitState *state = wlanLimit.state;

	__aeabi_memclr4(state, sizeof(*state));
	state = wlanLimit.state;
	state->kind = 1;
	state->limitApplied = WLAN_LIMIT_NONE;
	state->target = WLAN_LIMIT_NONE;
}

/* How the compensation stands: 1 off, 0 nothing to do, 2 no target moving, 3 rising, 4 falling. */
int wlanPsSetupHeader(void)
{
	unsigned int phase;

	if (wlanCalibrationType60Loaded() == 0 || wlanLimit.state->kind == 0)
		return 1;
	if (wlanLimit.state->target == WLAN_LIMIT_NONE)
		return 0;
	phase = wlanLimit.state->phase;
	if ((phase & 0xf) == WLAN_LIMIT_PHASE_NONE)
		return 2;
	phase &= 0xf;
	if (phase == WLAN_LIMIT_PHASE_RISING)
		return 3;
	if (phase != WLAN_LIMIT_PHASE_FALLING)
		return 0;
	return 4;
}

/* The limit scaled for the 5 GHz channel's part of the band; other bands keep it. */
int wlanPsClearHeader(unsigned int band, unsigned int channel, unsigned int bandwidth, int limit)
{
	unsigned int frequency;
	int scale;

	if (band == 1) {
		frequency = romRfChannelTable5G[wlanChannelIndex[wlanHt40CenterChannel(1, channel, bandwidth)]];
		if (frequency > 5100) {
			if (frequency > 5350) {
				if (frequency > 5720)
					scale = wlanLimit.bandScale[3];
				else
					scale = wlanLimit.bandScale[2];
			} else {
				scale = wlanLimit.bandScale[1];
			}
		} else {
			scale = wlanLimit.bandScale[0];
		}
		return scale * limit / WLAN_LIMIT_SCALE_BASE;
	}
	return limit;
}

/* Reads the calibration entry of channel 1 of the 2.4 GHz band. */
int wlanPsConvertHeader(void)
{
	struct WlanCalibrationKey key = { .band = 0, .channel = 1 };

	wlanLimit.curveLoaded = wlanCalibrationLookupBlock(key, &wlanLimit.curve) != 0;
	return wlanLimit.curveLoaded;
}

/* Counts down the target by the time since the last sample and drops it when it is over. */
void wlanPsVerifyHeader(void)
{
	struct WlanLimitState *state;
	unsigned int elapsed;
	unsigned int stamp;
	unsigned int countdown;

	if (wlanCalibrationType60Loaded() == 0)
		return;
	if (wlanLimit.state->kind == 0)
		return;
	if (wlanLimit.state->target == WLAN_LIMIT_NONE)
		return;
	stamp = romGetMicroseconds();
	elapsed = (unsigned int)wlanTimeDelta(stamp, wlanLimit.state->stamp) / 10000;
	state = wlanLimit.state;
	state->stamp = stamp;
	countdown = state->countdown;
	if (countdown <= elapsed) {
		state->countdown = 0;
	} else {
		state->countdown = countdown - elapsed;
		if (state->countdown != 0)
			return;
	}
	state->target = WLAN_LIMIT_NONE;
	state->phase = 16 * (state->phase >> 4);
}

/* A channel is being set: the reference channel starts a phase, another channel drops a target the sensor has left. */
void wlanPsSelectHeader(unsigned int band, uint32_t channel)
{
	struct WlanLimitState *state;
	unsigned int phase;
	unsigned int sample;
	unsigned int kept;

	if (wlanCalibrationType60Loaded() == 0)
		return;
	state = wlanLimit.state;
	if (state->kind == 0)
		return;
	phase = state->phase;
	if ((phase >> 4) != 0)
		return;
	if (band == 0 && channel == 1) {
		state->phase = 16 * (phase >> 4) + WLAN_LIMIT_PHASE_START;
		return;
	}
	if (state->target != WLAN_LIMIT_NONE && (phase & 0xf) != 0) {
		sample = socSensorSample();
		state = wlanLimit.state;
		kept = state->sampleKept;
		if ((sample >= kept && (int)(sample - kept) > 5) || (sample < kept && (int)(kept - sample) > 5)) {
			wlanLimit.state->target = WLAN_LIMIT_NONE;
			state->sampleKept = 0xff;
			state->phase = 16 * (state->phase >> 4);
		}
	}
	state = wlanLimit.state;
	if (state->kind == 1 && state->target == WLAN_LIMIT_NONE) {
		state->phase = 16;
		rfSetChannel(0, 1, 1);
		state = wlanLimit.state;
		state->phase = state->phase & 0xf;
	}
}

/* Keeps the sample and the limit of the reference channel and says how far the limit is from where the sample puts it. */
int wlanPsAdjustHeader(unsigned int sample, unsigned int limit)
{
	struct WlanLimitState *state = wlanLimit.state;
	unsigned int tracked;
	int result;

	state->sampleKept = sample;
	state->target = limit;
	if (wlanLimit.curveLoaded == 0) {
		result = wlanPsConvertHeader();
		if (result == 0)
			return result;
	}
	tracked = (uint16_t)(8 * wlanPsTrackHeader(&wlanLimit.curve, sample));
	state = wlanLimit.state;
	state->tracked = tracked;
	state->stamp = romGetMicroseconds();
	if (limit <= tracked || (int)(limit - tracked) <= 16) {
		if (limit >= tracked || (int)(tracked - limit) <= 16) {
			state->countdown = 1080000;
			state->phase = 16 * (state->phase >> 4);
		} else {
			state->countdown = 360000;
			state->phase = 16 * (state->phase >> 4) + WLAN_LIMIT_PHASE_FALLING;
		}
	} else {
		state->countdown = 360000;
		state->phase = 16 * (state->phase >> 4) + WLAN_LIMIT_PHASE_RISING;
	}
	return state->phase & 0xf;
}

/* Writes the limit to the radio's bus. */
__attribute__((noinline)) void wlanPsRecordHeader(unsigned int value)
{
	int i = 0;

	wlanLimit.limit = value;
	do {
		rfBusWriteIndexed(i++, wlanLimit.limit);
	} while (i < 1);
}

/*
 * A channel has been set up: works out the limit of the channel from the sensor and the channel's
 * calibration entry. The reference channel (2.4 GHz channel 1) keeps what it measures; the other
 * channels follow it (their limit is scaled by the channel's own factor).
 */
void wlanPsFilterHeader(unsigned int band, uint32_t channel, unsigned int bandwidth)
{
	struct WlanLimitState *state;
	struct WlanLimitLookup lookup;
	struct WlanCalibrationKey key;
	unsigned int posture;
	unsigned int kind;
	unsigned int phase;
	unsigned int position;
	unsigned int origin;
	unsigned int sample;
	int offset;
	int limit;
	int delta;
	int bound;

	if (wlanCalibrationType60Loaded() == 0)
		return;
	if (wlanLimit.state->kind == 0)
		return;
	posture = sysIrqDisable();
	state = wlanLimit.state;
	kind = state->kind;
	if (kind == 5) {
		state->positionSampled = macTxNotifySelectPacket();
		wlanLimit.state->sampleSampled = socSensorSample();
		sysIrqRestore(posture);
		return;
	}
	state->limitApplied = WLAN_LIMIT_NONE;
	phase = state->phase;
	if ((phase & 0xf) == 0 && (phase >> 4) == 0) {
		sysIrqRestore(posture);
		return;
	}
	if (band == 1) {
		limit = wlanPsClearHeader(1, channel, bandwidth, state->target - state->tracked);
		state = wlanLimit.state;
		state->limit = limit;
		state->limitApplied = limit;
		state->sample = socSensorSample();
		sysIrqRestore(posture);
		return;
	}
	if (kind == 1) {
		key.band = band;
		key.channel = channel;
		key.reserved[0] = 0;
		key.reserved[1] = 0;
		state->gain = 0;
		state->divisorBelow = 0;
		state->divisorAbove = 0;
		if (wlanCalibrationLookupInterpolated(key, (uint8_t *)&lookup) == 0) {
			sysIrqRestore(posture);
			return;
		}
		state = wlanLimit.state;
		state->gain = lookup.gain;
		state->divisorBelow = lookup.divisorBelow;
		state->divisorAbove = lookup.divisorAbove;
		state->corner = lookup.corner;
		state->reference = lookup.reference;
		state->step = lookup.step;
		state->origin = lookup.origin;
	}
	if (wlanLimit.state->divisorBelow == 0 || wlanLimit.state->divisorAbove == 0) {
		sysIrqRestore(posture);
		return;
	}
	state = wlanLimit.state;
	state->position = macTxNotifySelectPacket();
	sample = socSensorSample();
	state = wlanLimit.state;
	state->sample = sample;
	position = state->position;
	origin = state->origin;
	delta = position - origin;
	if (position < origin)
		delta = delta + 512;
	else
		delta = 512 - delta;
	if (delta < 0) {
		if (wlanLimit.active == 1) {
			wlanPsScheduleHeader();
			wlanPsApplyHeader();
		}
		sysIrqRestore(posture);
		return;
	}
	state->position = (uint16_t)(position + state->step);
	limit = 256 - ((state->position & 0xffff) - origin) +
		wlanPsManageHeader(sample, state->reference, state->corner, state->gain, state->divisorBelow,
				   state->divisorAbove);
	limit = wlanLimitClamp(limit);
	wlanLimit.state->limit = limit;
	if (band == 0 && channel == 1) {
		wlanPsAdjustHeader(sample, (uint16_t)limit);
		phase = wlanLimit.state->phase;
		if ((phase & 0xf) == 0 || (phase >> 4) == 1) {
			sysIrqRestore(posture);
			return;
		}
		if (sysEventPathIsEnabled(0) == 0) {
			offset = limit - 256;
			phase = wlanLimit.state->phase & 0xf;
			if (phase == WLAN_LIMIT_PHASE_RISING ? offset > 0 : phase == WLAN_LIMIT_PHASE_FALLING && offset < 0)
				offset = wlanLimit.channelScale[0] * offset / WLAN_LIMIT_SCALE_BASE;
			else if (phase == WLAN_LIMIT_PHASE_RISING || phase == WLAN_LIMIT_PHASE_FALLING)
				offset = (offset << 8) / wlanLimit.channelScale[0];
			limit = offset + 256;
		}
	} else if (sysEventPathIsEnabled(band) == 0) {
		state = wlanLimit.state;
		offset = state->target - 256;
		phase = state->phase & 0xf;
		if (phase == WLAN_LIMIT_PHASE_RISING ? offset > 0 : phase == WLAN_LIMIT_PHASE_FALLING && offset < 0)
			offset = wlanLimit.channelScale[channel - 1] * offset / WLAN_LIMIT_SCALE_BASE;
		else if (phase == WLAN_LIMIT_PHASE_RISING || phase == WLAN_LIMIT_PHASE_FALLING)
			offset = (offset << 8) / wlanLimit.channelScale[channel - 1];
		bound = offset + 256;
		if (phase == WLAN_LIMIT_PHASE_RISING ? limit > bound : phase == WLAN_LIMIT_PHASE_FALLING && limit < bound) {
			limit = bound;
			state->sample = state->sampleKept;
		}
	}
	limit = wlanLimitClamp(limit);
	wlanLimit.state->limitApplied = limit;
	if (wlanLimit.active == 1)
		wlanPsRecordHeader((unsigned int)((limit + 4) << 21) >> 24);
	sysIrqRestore(posture);
}

/* The compensation has a limit to apply. */
int wlanPsHandleDescriptor(void)
{
	return wlanCalibrationType60Loaded() != 0 && wlanLimit.state->kind != 0 &&
	       wlanLimit.state->limitApplied != WLAN_LIMIT_NONE;
}

/* The value for the radio from the limit and a new sample. */
unsigned int wlanPsProcessDescriptor(unsigned int band, unsigned int sample)
{
	int limit = wlanLimit.state->limitApplied;
	int change;

	if (band == 1) {
		limit = (int16_t)limit;
		change = 8 * wlanPsTrackHeader(&wlanLimit.tracking, sample);
	} else {
		unsigned int last = wlanLimit.state->sample;

		if (last == sample)
			goto done;
		change = wlanPsAcquireHeader(sample, last);
	}
	limit = wlanLimitClamp(limit + change);
done:
	wlanLimit.state->usedSample = sample;
	wlanLimit.state->usedLimit = limit;
	return (unsigned int)((limit + 4) << 21) >> 24;
}

/* Queues the value for the radio when it is not the one written last. */
__attribute__((noinline)) void wlanPsUpdateDescriptor(unsigned int value)
{
	if (value != wlanLimit.limit) {
		wlanLimit.limit = value;
		wlanLimit.queue |= 1;
	}
}

/* The sampling's callback: works out the value of a sample and writes it when it changed. */
void wlanPsConfigureDescriptor(int sample, int reset)
{
	unsigned int value;

	wlanPsVerifyHeader();
	if (reset != 0) {
		wlanLimit.queue = 0xff;
		wlanLimit.limit = 0;
	}
	if (wlanLimit.slot == 0) {
		if (wlanPsHandleDescriptor())
			value = wlanPsProcessDescriptor(rfGetBand(), sample);
		else
			value = wlanPsTrackHeader(&wlanLimit.tracking, sample);
		wlanPsUpdateDescriptor(value);
		if (wlanLimit.queue != 0) {
			wlanPsRecordHeader(value);
			wlanLimit.queue = 0;
		}
	}
}

/* Puts the compensation on the sampling, or takes it off and gives the radio its default. */
void wlanPsCheckDescriptor(int enable)
{
	int i;

	if (enable != 0) {
		if (wlanLimit.active == 0) {
			wlanPsFormatHeader(wlanPsConfigureDescriptor);
			wlanLimit.active = 1;
			wlanPsScheduleHeader();
		}
	} else if (wlanLimit.active != 0) {
		wlanPsInspectHeader(wlanPsConfigureDescriptor);
		wlanLimit.active = 0;
		for (i = 0; i < 1; ++i)
			rfBusWriteIndexed(i, 32);
	}
}

/* The curve the host's calibration gave. */
void wlanPsValidateDescriptor(const void *block)
{
	if (block != NULL)
		__aeabi_memcpy(&wlanLimit.tracking, block, sizeof(wlanLimit.tracking));
}
