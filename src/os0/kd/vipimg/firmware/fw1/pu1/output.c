/*
 * PU1: the picture output (output.h). PU0 carries the same code, unused.
 */
#include "../common/debug.h"
#include "output.h"

#define CLOCK_SEMAPHORE_TRIES	10

/* the frame durations, in 720 kHz units, by frame rate code */
#define DURATION_23_976		30030
#define DURATION_24		30000
#define DURATION_25		28800
#define DURATION_29_97		24024
#define DURATION_30		24000
#define DURATION_50		14400
#define DURATION_59_94		12012
#define DURATION_60		12000

static inline uint64_t ptsDifference(uint64_t value)
{
	if (value & VIP_PTS_SIGN)
		value |= VIP_PTS_EXTEND;
	return value;
}

static inline uint32_t pack16(uint32_t high, uint32_t low)
{
	return high << 16 | low;
}

/* Tell the debugger when pictures come further apart than their frame rate. */
void reportFrameInterval(VipOutputState *state)
{
	uint32_t elapsed = 0;

	if (state->intervalStarted) {
		if (state->intervalFrame < VIP_STATUS->frameCount)
			elapsed = VIP_STATUS->frameCount - state->intervalFrame;
		else
			elapsed = state->intervalFrame - VIP_STATUS->frameCount;
	}
	switch (state->rate) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if (elapsed >= 3)
			notifyDebugger(0, VIP_EVENT_FRAME_LATE);
		break;
	case 6:
	case 7:
	case 8:
		if (elapsed >= 2)
			notifyDebugger(0, VIP_EVENT_FRAME_LATE);
		break;
	}
	state->intervalStarted = 1;
	state->intervalFrame = VIP_STATUS->frameCount;
}

/* the display information that changes the report */
static uint32_t pictureChanged(const VipFrameInfo *last, const VipFrameInfo *info)
{
	uint32_t changed = 0;

	if (last->width != info->width || last->height != info->height ||
	    last->cropTop != info->cropTop || last->cropBottom != info->cropBottom ||
	    last->cropLeft != info->cropLeft || last->cropRight != info->cropRight)
		changed = 1;
	if (last->offset[0].x != info->offset[0].x || last->offset[1].x != info->offset[1].x ||
	    last->offset[2].x != info->offset[2].x || last->offset[0].y != info->offset[0].y ||
	    last->offset[1].y != info->offset[1].y || last->offset[2].y != info->offset[2].y)
		changed = 1;
	if (last->display[0].width != info->display[0].width ||
	    last->display[0].height != info->display[0].height)
		changed = 1;
	if ((last->flags & 0xffc00000) != (info->flags & 0xffc00000) ||
	    last->colour != info->colour)
		changed = 1;
	return changed;
}

static uint32_t pictureScan(const VipFrameInfo *info)
{
	uint32_t scan = VIP_SCAN_INTERLACED;

	if (info->flags >> VIP_FRAME_PROGRESSIVE_SEQUENCE & 1 ||
	    info->flags >> VIP_FRAME_PROGRESSIVE_FRAME & 1)
		scan = VIP_SCAN_PROGRESSIVE;
	return (info->flags >> VIP_FRAME_PROGRESSIVE_FRAME & 1) |
		(info->flags >> (VIP_FRAME_REPEAT_FIRST_FIELD - VIP_SCAN_FIELD_FLAGS_SHIFT) & 6) |
		(info->flags >> (VIP_FRAME_TYPE_SHIFT - VIP_SCAN_TYPE_SHIFT) & 0x700) |
		scan;
}

/* the matrix of a picture whose stream gives none */
static uint32_t pictureMatrix(const VipFrameInfo *info, uint32_t standard,
	const VipDecoderArea *area)
{
	uint32_t matrix = info->colour >> VIP_FRAME_MATRIX_SHIFT & 0xff;

	if (matrix != VIP_MATRIX_UNSPECIFIED)
		return matrix;
	if ((standard & VIP_STANDARD_MASK) == VIP_STANDARD_SD)
		return VIP_MATRIX_BT601;
	if ((standard & VIP_STANDARD_MASK) == VIP_STANDARD_HD)
		return VIP_MATRIX_BT709;
	return area->matrix;
}

static void reportPicture(volatile VipOutputPicture *picture, const VipFrameInfo *info,
	uint32_t standard, const VipDecoderArea *area)
{
	picture->size = pack16(info->width, info->height);
	picture->cropVertical = pack16(info->cropTop, info->cropBottom);
	picture->cropHorizontal = pack16(info->cropLeft, info->cropRight);
	picture->aspectRate = (picture->aspectRate & (3 << VIP_RATE_KIND_SHIFT)) |
		(info->flags >> VIP_FRAME_RATE_SHIFT & 0xff);
	picture->progressive = info->flags >> (VIP_FRAME_PROGRESSIVE_SEQUENCE - 8) & 0x100;
	picture->panScan0 = info->offset[0].x << 16 | info->offset[0].y;
	picture->panScan1 = info->offset[1].x << 16 | info->offset[1].y;
	picture->panScan2 = info->offset[2].x << 16 | info->offset[2].y;
	picture->displaySize = pack16(info->display[0].width, info->display[0].height);
	picture->matrix = pictureMatrix(info, standard, area);
}

/*
 * Report the picture's display information to the ARM side when it changed, or
 * when the key did (a new stream).
 */
int publishPictureInfo(VipOutputState *state, VipFrameHandle *frame, VipDecoderArea *area,
	uint32_t standard, uint32_t key)
{
	VipFrameInfo info;
	uint32_t changed;

	if (getFrameInfo(frame, &info) < 0)
		return -1;
	changed = state->infoKey != key;
	changed |= pictureChanged(&state->info, &info);
	area->picture.scan = pictureScan(&info);
	if (!changed)
		return 0;
	if (takeDebugSemaphore(0) < 0)
		return -1;
	memcpy(&state->info, &info, sizeof(info));
	state->infoKey = key;
	area->width = info.width;
	area->height = info.height;
	area->rate = info.flags >> VIP_FRAME_RATE_SHIFT & 0xf;
	reportPicture(&area->picture, &info, standard, area);
	area->picture.key = key;
	area->secondPicture.key = key;
	releaseDebugSemaphore(0);
	notifyDebugger(0, VIP_EVENT_PICTURE);
	return 0;
}

/* The same for the second view, which has no key. */
int publishSecondPictureInfo(VipOutputState *state, VipFrameHandle *frame,
	VipDecoderArea *area, uint32_t standard)
{
	VipFrameInfo info;

	if (getFrameInfo(frame, &info) < 0)
		return -1;
	area->secondPicture.scan = pictureScan(&info);
	if (!pictureChanged(&state->secondInfo, &info))
		return 0;
	if (takeDebugSemaphore(0) < 0)
		return -1;
	memcpy(&state->secondInfo, &info, sizeof(info));
	reportPicture(&area->secondPicture, &info, standard, area);
	releaseDebugSemaphore(0);
	notifyDebugger(0, VIP_EVENT_SECOND_PICTURE);
	return 0;
}

static uint8_t controlRate(uint32_t control, uint8_t progressive)
{
	uint32_t rate;

	if (progressive) {
		rate = control >> VIP_CONTROL_PROGRESSIVE_SHIFT & VIP_CONTROL_RATE_MASK;
		return rate ? rate + 5 : 0;
	}
	rate = control & VIP_CONTROL_RATE_MASK;
	return rate ? rate + 2 : 0;
}

/*
 * Apply VipDecoderArea.control to the picture: its scan, and its frame rate when it
 * has none or the control overrides it.
 */
void applyRateControl(VipOutputState *state, VipFrameHandle *frame, VipDecoderArea *area,
	uint32_t unused, const uint64_t *dts, uint32_t ptsValid, const uint64_t *pts)
{
	uint8_t rate, progressiveSequence, type, progressiveFrame;
	uint32_t control, kind = VIP_RATE_STREAM;

	/* the decode passes the picture's times too, unused */
	(void)state;
	(void)unused;
	(void)dts;
	(void)ptsValid;
	(void)pts;
	getFrameRate(frame, &rate);
	getFrameProgressiveSequence(frame, &progressiveSequence);
	getFrameType(frame, &type);
	getFrameProgressiveFrame(frame, &progressiveFrame);
	(void)type;
	control = area->control;
	if (control & VIP_CONTROL_FORCE_SCAN) {
		if (control & VIP_CONTROL_PROGRESSIVE) {
			progressiveSequence = 1;
			if (control & VIP_CONTROL_PROGRESSIVE_RATE) {
				rate = controlRate(control, 1);
				kind = VIP_RATE_OVERRIDE;
			}
		} else {
			progressiveSequence = 0;
			if (control & VIP_CONTROL_RATE) {
				rate = controlRate(control, 0);
				if (rate && progressiveFrame)
					setFrameTopFieldFirst(frame, 1);
				kind = VIP_RATE_OVERRIDE;
			}
		}
	} else if (control & VIP_CONTROL_PROGRESSIVE_RATE) {
		if (control & VIP_CONTROL_RATE) {
			rate = controlRate(control, progressiveSequence);
			kind = VIP_RATE_OVERRIDE;
		} else if (progressiveSequence) {
			rate = controlRate(control, 1);
			kind = VIP_RATE_OVERRIDE;
		}
	} else if ((control & VIP_CONTROL_RATE) && !progressiveSequence) {
		rate = controlRate(control, 0);
		kind = VIP_RATE_OVERRIDE;
	}
	if (!rate) {
		kind = VIP_RATE_DEFAULT;
		rate = controlRate(control, progressiveSequence);
	}
	setFrameProgressiveSequence(frame, progressiveSequence);
	setFrameRate(frame, rate);
	area->picture.aspectRate = (area->picture.aspectRate & 0xff) |
		kind << VIP_RATE_KIND_SHIFT;
	area->secondPicture.aspectRate = (area->secondPicture.aspectRate & 0xff) |
		kind << VIP_RATE_KIND_SHIFT;
}

/*
 * Take the clock's semaphore, with interrupts off until it is released; -1 if it
 * stays taken. Reading the semaphore takes it when it reads 0.
 */
static int takeClockSemaphore(uint32_t mode)
{
	uint32_t tries;

	if (mode)
		return -1;
	mep_di();
	for (tries = 0; *VIP_OUTPUT_CLOCK->semaphore; tries++) {
		if (tries >= CLOCK_SEMAPHORE_TRIES) {
			mep_ei();
			return -1;
		}
		__asm__ volatile ("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
				  "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop");
	}
	return 0;
}

static void releaseClockSemaphore(uint32_t mode)
{
	if (mode)
		return;
	*VIP_OUTPUT_CLOCK->semaphore = 0;
	mep_ei();
}

void resetOutputState(VipOutputState *state)
{
	memset(state, 0, sizeof(*state));
}

void closeOutputState(VipOutputState *state)
{
	(void)state;
}

static uint64_t readTimer(uint32_t high, uint32_t low)
{
	uint64_t value = (uint64_t)VIP_TIMER(high) << 24;

	return value | VIP_TIMER(low);
}

/* the time the display shows: the STC, less the latency of the display */
static void readOutputTime(uint64_t *time)
{
	uint64_t base = readTimer(VIP_TIMER_BASE_HIGH, VIP_TIMER_BASE_LOW);
	uint64_t stc = readTimer(VIP_TIMER_STC_HIGH, VIP_TIMER_STC_LOW);
	uint64_t start = readTimer(VIP_TIMER_START_HIGH, VIP_TIMER_START_LOW);

	*time = base + stc - start - VIP_OUTPUT_LATENCY;
}

/*
 * Start a picture: take the command and the clock's requests from the ARM side.
 * -1 if the clock has not been started.
 */
int beginOutput(VipOutputState *state, uint32_t areaKind, uint32_t mode, uint32_t reset)
{
	VipDecoderArea *area;
	uint64_t offset = 0, now;
	uint32_t command;

	state->unclocked = 0;
	state->restart = 0;
	getDebugArea(areaKind, (uint32_t *)&area);
	command = area->command;
	state->command = command;
	if (state->lastCommand != command)
		state->repeatCount = 0;
	if (reset) {
		state->started = 0;
		state->stopped = 0;
	}
	if (mode == VIP_OUTPUT_FREE)
		goto unclocked;
	if (takeClockSemaphore(0) >= 0) {
		state->clockRequest = *VIP_OUTPUT_CLOCK->request;
		if (reset)
			*VIP_OUTPUT_CLOCK->acknowledge = VIP_CLOCK_HOLD;
		if (state->clockRequest & VIP_CLOCK_RESET) {
			VIP_OUTPUT_CLOCK->driftHigh = 0;
			VIP_OUTPUT_CLOCK->driftMiddle = 0;
			VIP_OUTPUT_CLOCK->driftLow = 0;
			*VIP_OUTPUT_CLOCK->request = VIP_CLOCK_RUNNING;
			*VIP_OUTPUT_CLOCK->acknowledge = VIP_CLOCK_RESET;
		}
		if (state->clockRequest & VIP_CLOCK_OFFSET) {
			offset = (uint64_t)(VIP_OUTPUT_CLOCK->offsetHigh & 0x1ff) << 24 |
				VIP_OUTPUT_CLOCK->offsetLow;
			*VIP_OUTPUT_CLOCK->acknowledge = VIP_CLOCK_OFFSET;
		} else {
			offset = 0;
		}
		releaseClockSemaphore(0);
	}
	switch (command >> 16) {
	case VIP_COMMAND_STOP:
	case VIP_COMMAND_STEP_I:
	case VIP_COMMAND_STEP_IP:
	case VIP_COMMAND_STEP_I_COUNT:
	case VIP_COMMAND_STEP_IP_COUNT:
		goto unclocked;
	}
	if (mode == VIP_OUTPUT_STILL)
		goto unclocked;
	if (!state->clockRequest) {
		state->cadence = 0;
		area->flags |= VIP_AREA_NO_CLOCK;
		return -1;
	}
	if (state->clockRequest & VIP_CLOCK_HOLD) {
		if (!reset) {
			state->restart = 1;
			state->unclocked = 1;
			area->flags &= VIP_AREA_FLAGS & ~VIP_AREA_NO_CLOCK;
			return 0;
		}
		state->clockRequest &= ~VIP_CLOCK_HOLD;
	}
	area->flags &= VIP_AREA_FLAGS & ~VIP_AREA_NO_CLOCK;
	if (state->clockRequest & VIP_CLOCK_OFFSET) {
		state->offset = (state->offset + offset) & VIP_PTS_MASK;
		state->clockRequest &= ~VIP_CLOCK_OFFSET;
	}
	if (state->clockRequest & VIP_CLOCK_RESET) {
		readOutputTime(&now);
		now &= VIP_PTS_MASK;
		state->adjustment = 0;
		state->publishedAdjustment = 0;
		state->commandTime = now;
		state->commandAdjustment = 0;
		state->offset = 0;
		state->clockRequest = (state->clockRequest & ~VIP_CLOCK_RESET) |
			VIP_CLOCK_RUNNING;
	}
	return 0;

unclocked:
	state->restart = 1;
	state->unclocked = 1;
	return 0;
}

/*
 * Whether the picture's field cadence fits the display's, for the 2:3 pulldown of
 * 24 and 30 frame pictures; VIP_OUTPUT_CADENCE when the cadence changed.
 */
static int checkCadence(VipOutputState *state, uint32_t mode, uint32_t field,
	uint8_t rate, uint8_t topFieldFirst, uint8_t repeat)
{
	uint8_t count;

	if (mode != VIP_OUTPUT_FREE && state->rate < 6 && (state->cadence & 0x10) &&
	    (state->cadence & 0xf) != field) {
		state->rate = rate;
		return VIP_OUTPUT_CADENCE;
	}
	state->cadence = field;
	if ((rate == 8 || rate == 7 || rate == 6) && repeat &&
	    (mode == VIP_OUTPUT_STILL || mode == VIP_OUTPUT_STARTING)) {
		count = ++state->repeatCount;
		if (state->topFieldFirst ? count < 3 : count < 2)
			return VIP_OUTPUT_DROP;
		state->repeatCount = 0;
		state->topFieldFirst = topFieldFirst;
	}
	return 0;
}

static uint64_t unpackDrift(void)
{
	return (uint32_t)(VIP_OUTPUT_CLOCK->driftHigh << 28 |
		(VIP_OUTPUT_CLOCK->driftMiddle & 0xffffff) << 4 |
		(VIP_OUTPUT_CLOCK->driftLow & 0xf));
}

/*
 * Publish the picture's time to the ARM side, with how far the clock's adjustment
 * moved since the last one, added to the drift the ARM side has not taken yet.
 */
static void publishClock(int result, VipDecoderArea *area, const uint64_t *target,
	const uint64_t *pts, const uint64_t *dts, uint32_t command, VipOutputState *state)
{
	uint64_t change, drift;
	uint32_t negative, driftNegative, last;

	if (takeClockSemaphore(0) < 0)
		return;
	VIP_OUTPUT_CLOCK->ptsHigh = *target >> 24 & 0x1ff;
	VIP_OUTPUT_CLOCK->ptsLow = *target & 0xffffff;
	if (result == VIP_OUTPUT_SHOW) {
		if (!state->restart) {
			area->ptsHigh = (*pts >> 24 & 0x1ff) | ((int64_t)*pts < 0 ? 0x80000000 : 0);
			area->dtsHigh = (*dts >> 24 & 0x1ff) | ((int64_t)*dts < 0 ? 0x80000000 : 0);
		}
		area->ptsLow = *pts & 0xffffff;
		area->dtsLow = *dts & 0xffffff;
	}
	VIP_OUTPUT_CLOCK->stcHigh = VIP_TIMER(VIP_TIMER_STC_HIGH);
	VIP_OUTPUT_CLOCK->stcLow = VIP_TIMER(VIP_TIMER_STC_LOW);
	VIP_OUTPUT_CLOCK->command = command;
	if (state->adjustment != state->publishedAdjustment) {
		negative = (uint32_t)(state->adjustment >> 37) & 0xff;
		last = (uint32_t)(state->publishedAdjustment >> 37) & 0xff;
		if (negative != last) {
			change = state->publishedAdjustment + state->adjustment;
		} else if (state->adjustment > state->publishedAdjustment) {
			change = state->adjustment - state->publishedAdjustment;
		} else {
			change = state->publishedAdjustment - state->adjustment;
			negative = last;
		}
		drift = unpackDrift();
		driftNegative = VIP_OUTPUT_CLOCK->driftHigh >> VIP_DRIFT_SIGN_SHIFT & 0xff;
		if (negative == driftNegative) {
			change += drift;
		} else if (change > drift) {
			change -= drift;
		} else {
			change = drift - change;
			negative = driftNegative;
		}
		VIP_OUTPUT_CLOCK->driftHigh = negative << VIP_DRIFT_SIGN_SHIFT |
			(uint32_t)(change >> 28 & 0x1ff);
		VIP_OUTPUT_CLOCK->driftMiddle = (uint32_t)(change >> 4) & 0xffffff;
		VIP_OUTPUT_CLOCK->driftLow = (uint32_t)change & 0xf;
	}
	releaseClockSemaphore(0);
	state->publishedAdjustment = state->adjustment;
}

/* the PTS a command stepped to, for the ARM side */
static void reportStep(VipDecoderArea *area, const uint64_t *pts)
{
	area->stepPtsHigh = *pts >> 24 & 0x1ff;
	area->stepPtsLow = *pts & 0xffffff;
}

/* the time difference of a step: bit 33 when the picture is behind */
static uint64_t stepDifference(const VipOutputState *state, const uint64_t *pts,
	uint32_t round)
{
	uint64_t difference = state->lastTarget - *pts;

	if ((int64_t)difference < 0 || (difference & VIP_PTS_SIGN))
		return (*pts - state->lastTarget + round) & VIP_PTS_MASK;
	return ((difference - round) & VIP_PTS_MASK) | VIP_DIFFERENCE_NEGATIVE;
}

/* the pictures VIP_COMMAND_STEP_I_COUNT and _IP_COUNT count */
static int stepCount(VipOutputState *state, uint8_t count, int shown, int skipped)
{
	if (state->repeatCount == 0) {
		if (++state->repeatCount == count)
			state->repeatCount = 0;
		return shown;
	}
	if (++state->repeatCount == count)
		state->repeatCount = 0;
	return skipped;
}

static int stepSkipped(VipOutputState *state, uint8_t count)
{
	if (state->repeatCount == 0 && ++state->repeatCount == count)
		state->repeatCount = 0;
	return VIP_OUTPUT_SKIP;
}

static inline int isIntra(uint8_t type)
{
	return type == VIP_FRAME_TYPE_I || type == VIP_FRAME_TYPE_IDR;
}

static inline int isIntraOrP(uint8_t type)
{
	return isIntra(type) || type == VIP_FRAME_TYPE_P;
}

/*
 * Decide what to do with the picture at *pts, by the clock (pts given by the
 * stream when ptsValid, else predicted, and then stored there): VIP_OUTPUT_*.
 */
int scheduleFrame(VipOutputState *state, uint32_t ptsValid, uint64_t *pts,
	const uint64_t *dts, uint32_t areaKind, uint32_t flags, uint32_t mode,
	VipFrameHandle *frame, VipFrameHandle *field)
{
	VipFrameHandle *source = field ? field : frame;
	VipDecoderArea *area;
	uint8_t rate, topFieldFirst, repeat, type, progressiveSequence, progressiveFrame;
	uint8_t randomAccess, reference, count;
	uint32_t command, kind, displayField, duration, displayDuration, n;
	uint64_t now, target, elapsed, delta, base, difference, threshold;
	uint64_t ptsCopy, dtsCopy, targetCopy;
	int result, negative, baseNegative, starting;

	if (state->stopped)
		return VIP_OUTPUT_DROP;
	getFrameRate(source, &rate);
	getFrameTopFieldFirst(source, &topFieldFirst);
	getFrameRepeatFirstField(source, &repeat);
	getFrameProgressiveFrame(source, &progressiveFrame);
	getDebugArea(areaKind, (uint32_t *)&area);
	command = state->command;
	if (mode == VIP_OUTPUT_FREE) {
		state->unclocked = 0;
		if (ptsValid) {
			state->nextPts = *pts << 1;
			state->remainder = 0;
		}
		state->rate = rate;
		getFrameType(frame, &type);
		result = VIP_OUTPUT_SHOW;
		kind = command >> 16;
		if (kind == VIP_COMMAND_STEP_I || kind == VIP_COMMAND_STEP_IP) {
			state->rebase = 1;
			if (kind == VIP_COMMAND_STEP_I)
				result = isIntra(type) ? VIP_OUTPUT_SHOW : VIP_OUTPUT_SKIP;
			else
				result = isIntraOrP(type) ? VIP_OUTPUT_SHOW :
					VIP_OUTPUT_SKIP;
			notifyDebugger(0, VIP_EVENT_STEP);
		}
		state->lastCommand = command;
		if (result == VIP_OUTPUT_SHOW)
			state->presentTime = *pts;
		return result;
	}

	getFrameProgressiveSequence(frame, &progressiveSequence);
	displayField = vipReadControlBus(VIP_CB_DISPLAY_FIELD);
	if (!state->rate && rate)
		state->rate = rate;
	result = checkCadence(state, mode, displayField, rate, topFieldFirst, repeat);
	if (result)
		return result;

	/* the frame's duration, the display's, and the fields of a command's step */
	switch (rate) {
	case 1:
	case 2:
		if (!progressiveFrame && topFieldFirst == displayField)
			goto wrongField;
		duration = rate == 1 ? DURATION_23_976 : DURATION_24;
		displayDuration = rate == 1 ? DURATION_29_97 : DURATION_30;
		break;
	case 3:
	case 4:
	case 5:
		if (progressiveSequence) {
			if (displayField)
				goto wrongField;
		} else if (topFieldFirst == displayField && (mode != VIP_OUTPUT_CLOCKED || !progressiveFrame)) {
			goto wrongField;
		}
		duration = rate == 3 ? DURATION_25 : rate == 4 ? DURATION_29_97 : DURATION_30;
		displayDuration = duration;
		break;
	case 6:
	case 7:
	case 8:
		duration = rate == 6 ? DURATION_50 : rate == 7 ? DURATION_59_94 : DURATION_60;
		displayDuration = duration;
		count = command & VIP_COMMAND_COUNT;
		goto timed;
	default:
		duration = DURATION_29_97;
		displayDuration = DURATION_29_97;
		break;
	}
	count = (command & VIP_COMMAND_COUNT) << 1;

timed:
	if (repeat)
		duration += (duration >> 1) + 1;
	readOutputTime(&now);
	target = (state->offset + now) & VIP_PTS_MASK;
	if (ptsValid) {
		state->nextPts = *pts << 1;
		state->remainder = 0;
	} else {
		*pts = state->nextPts >> 1;
	}
	if (state->rebase) {
		state->rebase = 0;
		state->commandTime = target;
		state->commandAdjustment = state->adjustment;
	} else if (state->lastCommand != state->command) {
		state->commandTime = target;
		state->commandAdjustment = state->adjustment;
	}

	/* how far the command moved the clock since it came */
	negative = 0;
	elapsed = ((target - state->commandTime) & VIP_PTS_MASK) << VIP_ADJUSTMENT_SHIFT;
	kind = command >> 16;
	n = command & VIP_COMMAND_COUNT;
	delta = elapsed;
	switch (kind) {
	case VIP_COMMAND_FAST_FRACTION:
		delta = elapsed / (n ? n : 1);
		break;
	case VIP_COMMAND_FAST_EVEN:
		delta = elapsed * (2 * (n ? n : 1) - 1);
		break;
	case VIP_COMMAND_FAST:
		delta = elapsed * ((n ? n : 1) - 1);
		break;
	case VIP_COMMAND_SLOW_FRACTION:
		delta = elapsed - elapsed * n / (n + 1);
		negative = 1;
		break;
	case VIP_COMMAND_SLOW_POWER:
		delta = elapsed - elapsed / (uint64_t)(int64_t)(int32_t)(2 << ((n ? n : 1) - 1));
		negative = 1;
		break;
	case VIP_COMMAND_SLOW:
		delta = elapsed - elapsed / (n ? n : 1);
		negative = 1;
		break;
	case VIP_COMMAND_SEEK:
		displayDuration >>= (progressiveFrame ? 1 : 0) + 3;
		if (*pts > target) {
			delta = *pts - displayDuration - target + 1;
		} else {
			delta = target - *pts + displayDuration - 1;
			negative = 1;
		}
		delta <<= VIP_ADJUSTMENT_SHIFT;
		state->commandAdjustment = 0;
		area->command = (command & 0xffff) | 0x80000000;
		break;
	case VIP_COMMAND_PAUSE:
		negative = 1;
		break;
	default:
		delta = 0;
		goto adjusted;
	}
	if (delta)
		notifyDebugger(0, VIP_EVENT_CLOCK);

adjusted:
	/* add the adjustment the command started from */
	base = state->commandAdjustment & VIP_ADJUSTMENT_MASK;
	baseNegative = (uint32_t)(state->commandAdjustment >> 37) & 0xff;
	if (negative == baseNegative) {
		delta += base;
	} else if (delta > base) {
		delta -= base;
	} else {
		negative = baseNegative;
		delta = base - delta;
	}
	state->adjustment = (uint64_t)negative << 37 | delta;
	state->lastTarget = target;

	/* the clock: the time, moved by the adjustment */
	delta = state->adjustment >> VIP_ADJUSTMENT_SHIFT & VIP_PTS_MASK;
	if (state->adjustment & VIP_ADJUSTMENT_NEGATIVE)
		target = target - delta - ((state->adjustment & 0xf) ? 1 : 0);
	else
		target = target + delta;
	target &= VIP_PTS_MASK;
	state->lastCommand = command;
	state->presentTime = target;
	if (kind)
		area->flags |= VIP_AREA_COMMAND;
	else
		area->flags &= VIP_AREA_FLAGS & ~VIP_AREA_COMMAND;

	starting = !state->inSync && mode == VIP_OUTPUT_STARTING;
	getFrameType(frame, &type);
	threshold = VIP_DEBUG->syncThreshold;
	difference = ptsDifference(state->presentTime - *pts);
	getFrameRandomAccess(frame, &randomAccess);
	if (state->waitRandomAccess) {
		if (randomAccess) {
			state->waitRandomAccess = 0;
		} else if (type == VIP_FRAME_TYPE_P ||
			   type == VIP_FRAME_TYPE_B) {
			state->rate = rate;
			return VIP_OUTPUT_SKIP;
		}
	}

	if (state->unclocked) {
		/* the picture goes out by its command, and the clock follows it */
		delta = stepDifference(state, pts, 0);
		result = VIP_OUTPUT_SHOW;
		switch (kind) {
		case VIP_COMMAND_STOP:
			if (isIntra(type))
				state->stopped = 1;
			else
				result = VIP_OUTPUT_SKIP;
			goto publish;
		case VIP_COMMAND_STEP_I:
			state->rebase = 1;
			result = isIntra(type) ? VIP_OUTPUT_SHOW : VIP_OUTPUT_SKIP;
			break;
		case VIP_COMMAND_STEP_IP:
			state->rebase = 1;
			result = isIntraOrP(type) ? VIP_OUTPUT_SHOW :
				VIP_OUTPUT_SKIP;
			break;
		case VIP_COMMAND_STEP_I_COUNT:
			state->rebase = 1;
			if (isIntra(type))
				result = stepCount(state, count, VIP_OUTPUT_SHOW, VIP_OUTPUT_DROP);
			else
				result = stepSkipped(state, count);
			break;
		case VIP_COMMAND_STEP_IP_COUNT:
			state->rebase = 1;
			if (isIntraOrP(type))
				result = stepCount(state, count, VIP_OUTPUT_SHOW, VIP_OUTPUT_DROP);
			else
				result = stepSkipped(state, count);
			break;
		default:
			goto publish;
		}
		state->adjustment = delta << VIP_ADJUSTMENT_SHIFT;
		target = *pts;
		state->presentTime = *pts;
		while (takeClockSemaphore(0))
			;
		reportStep(area, pts);
		releaseClockSemaphore(0);
		notifyDebugger(0, VIP_EVENT_STEP);
		goto publish;
	}

	if ((int64_t)difference >= 0) {
		/* early */
		if ((int64_t)difference > (int64_t)threshold)
			goto resync;
		if (difference <= duration >> 3) {
			state->inSync = 1;
			result = VIP_OUTPUT_SHOW;
		} else {
			getFrameNalReference(frame, &reference);
			if (!reference) {
				result = VIP_OUTPUT_HOLD;
			} else if (difference <= (duration >> 1) + (duration >> 2)) {
				result = VIP_OUTPUT_SHOW;
			} else {
				state->waitRandomAccess = 1;
				result = isIntra(type) ? VIP_OUTPUT_SHOW : VIP_OUTPUT_SKIP;
			}
		}
	} else {
		/* late */
		if ((int64_t)((*pts - state->presentTime) & VIP_PTS_MASK) > (int64_t)threshold)
			goto resync;
		result = VIP_OUTPUT_DROP;
	}
	goto checked;

resync:
	/* too far off: resync the clock to the picture, if the ARM side allows it */
	while (takeClockSemaphore(0))
		;
	if (flags & VIP_SCHEDULE_RESYNC) {
		if (!area->resync) {
			result = VIP_OUTPUT_DROP;
			*VIP_OUTPUT_CLOCK->acknowledge = VIP_CLOCK_STOPPED;
		} else {
			state->adjustment = stepDifference(state, pts, 1) << VIP_ADJUSTMENT_SHIFT;
			state->rebase = 1;
			notifyDebugger(0, VIP_EVENT_CLOCK);
			result = VIP_OUTPUT_SHOW;
		}
		notifyDebugger(0, VIP_EVENT_STEP);
	} else {
		state->restart = 1;
		if (rate < 3)
			result = displayField ? VIP_OUTPUT_DROP : VIP_OUTPUT_SHOW;
		else if (rate < 6 && !progressiveSequence && topFieldFirst == displayField)
			result = VIP_OUTPUT_DROP;
		else
			result = VIP_OUTPUT_SHOW;
	}
	reportStep(area, pts);
	releaseClockSemaphore(0);

checked:
	if (!state->inSync && starting)
		result = VIP_OUTPUT_DROP;
	if (state->inSync && mode == VIP_OUTPUT_STARTING)
		result = VIP_OUTPUT_SHOW;

publish:
	targetCopy = target;
	ptsCopy = *pts;
	dtsCopy = *dts;
	publishClock(result, area, &targetCopy, &ptsCopy, &dtsCopy, command, state);
	state->scheduled = 1;
	state->rate = rate;
	if (result >= 0)
		state->cadence += 0x10;
	if (result == VIP_OUTPUT_DROP && !state->repeatCount)
		area->flags |= VIP_AREA_REPEAT;
	else
		area->flags &= VIP_AREA_FLAGS & ~VIP_AREA_REPEAT;
	if (result == VIP_OUTPUT_SHOW)
		state->unclocked = 0;
	if (state->repeatCount) {
		getFrameNalReference(frame, &reference);
		if (!reference && (kind == VIP_COMMAND_REPEAT_PICTURE ||
				   kind == VIP_COMMAND_REPEAT_FIELD || kind == VIP_COMMAND_REPEAT))
			result = VIP_OUTPUT_HOLD;
	}
	return result;

wrongField:
	state->rate = rate;
	return VIP_OUTPUT_DROP;
}

/* Advance the predicted PTS by the picture's duration. */
void advancePts(VipOutputState *state, VipFrameHandle *frame, VipFrameHandle *field)
{
	uint8_t rate, repeat;
	uint32_t duration;

	if (field) {
		getFrameRate(field, &rate);
		getFrameRepeatFirstField(field, &repeat);
	} else {
		getFrameRate(frame, &rate);
		repeat = 0;
	}
	switch (rate) {
	case 1:
		duration = DURATION_23_976;
		break;
	case 2:
		duration = DURATION_24;
		break;
	case 3:
		duration = DURATION_25;
		break;
	case 4:
		duration = repeat ? DURATION_29_97 * 3 / 2 : DURATION_29_97;
		break;
	case 5:
		duration = repeat ? DURATION_30 * 3 / 2 : DURATION_30;
		break;
	case 6:
		duration = DURATION_50;
		break;
	case 7:
		duration = DURATION_59_94;
		break;
	case 8:
		duration = DURATION_60;
		break;
	default:
		duration = DURATION_29_97;
		break;
	}
	state->remainder += duration & 3;
	state->nextPts += duration >> 2;
	if (state->remainder & 4) {
		state->remainder &= ~4;
		state->nextPts++;
	}
}

/* whether the display shows the wrong field for the picture */
static int wrongField(uint8_t rate, uint32_t mode, uint8_t progressiveSequence, uint8_t topFieldFirst)
{
	uint32_t displayField = vipReadControlBus(VIP_CB_DISPLAY_FIELD);

	if ((rate != 5 && rate != 4 && rate != 3) || mode == VIP_OUTPUT_FREE)
		return 0;
	if (progressiveSequence)
		return displayField != 0;
	return topFieldFirst == displayField;
}

/*
 * Check a picture the decoder repeats or shows without scheduleFrame:
 * VIP_OUTPUT_HOLD while a repeat command holds it.
 */
int checkFrame(VipOutputState *state, uint32_t ptsValid, uint64_t *pts, uint32_t unused,
	uint32_t mode, VipFrameHandle *frame, uint32_t hold)
{
	uint8_t progressiveSequence, type, topFieldFirst, rate, randomAccess;
	uint64_t lead, behind;
	int result;

	(void)unused;
	if (!ptsValid)
		*pts = state->nextPts >> 1;
	getFrameProgressiveSequence(frame, &progressiveSequence);
	getFrameType(frame, &type);
	getFrameTopFieldFirst(frame, &topFieldFirst);
	getFrameRate(frame, &rate);
	/* 0, unless the frame store is missing */
	result = getFrameRandomAccess(frame, &randomAccess);
	switch (state->command >> 16) {
	case VIP_COMMAND_STOP:
	case VIP_COMMAND_STEP_I:
	case VIP_COMMAND_STEP_IP:
	case VIP_COMMAND_STEP_I_COUNT:
	case VIP_COMMAND_STEP_IP_COUNT:
		state->started = 0;
		return VIP_OUTPUT_SKIP;
	case VIP_COMMAND_REPEAT_PICTURE:
		if (!state->repeatCount)
			state->repeatCount = 1;
		else
			result = isIntraOrP(type) ? 0 : VIP_OUTPUT_HOLD;
		break;
	case VIP_COMMAND_REPEAT_FIELD:
		if (!state->repeatCount)
			state->repeatCount = 1;
		else
			result = isIntra(type) ? 0 : VIP_OUTPUT_HOLD;
		break;
	case VIP_COMMAND_REPEAT:
		if (!state->repeatCount)
			state->repeatCount = 1;
		else
			result = VIP_OUTPUT_HOLD;
		break;
	default:
		result = 0;
		break;
	}
	if (randomAccess && !hold)
		state->started = 0;
	lead = ptsDifference((state->nextPts >> 1) - *pts + 10);

	if (result) {
		if (state->started)
			goto done;
		if (state->restart) {
			state->restart = 0;
			if ((int64_t)lead < 0)
				result = VIP_OUTPUT_DROP;
			else
				state->started = 1;
		} else {
			behind = ptsDifference(state->presentTime - *pts);
			if ((int64_t)behind < 0 || (int64_t)lead < 0)
				result = VIP_OUTPUT_DROP;
			else
				state->started = 1;
		}
		goto done;
	}
	if (hold) {
		if (wrongField(rate, mode, progressiveSequence, topFieldFirst))
			result = VIP_OUTPUT_DROP;
		goto done;
	}
	if (!state->started) {
		if (state->restart) {
			state->restart = 0;
			if ((int64_t)lead < 0)
				result = VIP_OUTPUT_DROP;
			else
				state->started = 1;
		} else {
			behind = ptsDifference(state->presentTime - *pts);
			if ((int64_t)behind < 0 || (int64_t)lead < 0)
				result = VIP_OUTPUT_DROP;
			else
				state->started = 1;
		}
	}
	if (!state->scheduled && wrongField(rate, mode, progressiveSequence, topFieldFirst))
		result = VIP_OUTPUT_DROP;

done:
	state->scheduled = 0;
	return result;
}
