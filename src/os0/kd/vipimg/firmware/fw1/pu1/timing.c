/*
 * PU1: the pictures' times. A picture's decode time comes from its record's DTS,
 * else from its PTS when pictures are not reordered, else from the stream's
 * timing: pic_timing()'s cpb_removal_delay after the buffering period's first
 * picture, or the last picture's time and its duration (DeltaTfiDivisor). The
 * SPS's ticks (num_units_in_tick of time_scale) become 90 kHz units here, and
 * what is left over, in 1/time_scale of a unit, is kept in g_timeFraction. The
 * presentation time is the record's PTS, else the decode time and
 * dpb_output_delay.
 *
 * FW2's codec works its times out the same way, but it never holds a picture,
 * goes by the last picture's pic_struct when it has one too, does not mark a
 * picture without times for the output, and its caller can ask for no
 * presentation time where the decode time would be it.
 */
#ifdef VIP_FW2
#include "../../fw2/codec/codec.h"
#define TIMING_GLOBAL	VIP_GLOBAL
#define g_pictureHeld	0
#else
#include "codec.h"
#define TIMING_GLOBAL
#endif

#define PTS_CLOCK	90000	/* Hz */
#define NO_TIME		(~(uint64_t)0)

/* takePictureTime's option: VipOpenSettings.optionTaken */
#define OPTION_TIMING_FIRST	1	/* pic_timing() before the record's PTS */
#define OPTION_NO_REORDER	2	/* the PTS is the decode time (never given) */

uint64_t g_periodTime TIMING_GLOBAL;	/* the buffering period's first picture's decode time */
uint64_t g_pendingPeriodTime TIMING_GLOBAL;	/* a picture's with a buffering period, for the next */
uint64_t g_pictureTime TIMING_GLOBAL;	/* the last picture's decode time */
uint64_t g_timeFraction TIMING_GLOBAL;	/* signed, in 1/time_scale of a 90 kHz unit */
uint64_t g_periodTimeFraction TIMING_GLOBAL;
#ifdef VIP_FW2
int8_t g_timeStarted VIP_GLOBAL;
#endif

/* ticks of the SPS's clock in 90 kHz units */
static uint64_t ticksToPts(uint64_t ticks)
{
	return ticks * g_currentSps->numUnitsInTick * PTS_CLOCK / g_currentSps->timeScale;
}

/* and what of them is left over, in 1/time_scale of a unit */
static uint64_t ticksToPtsRemainder(uint64_t ticks)
{
	return ticks * g_currentSps->numUnitsInTick * PTS_CLOCK % g_currentSps->timeScale;
}

/*
 * DeltaTfiDivisor (Table E-6) of a pic_struct, but 2 for a field: the ticks a
 * picture lasts.
 */
static uint32_t deltaTfiDivisor(uint32_t picStruct)
{
	static const uint8_t divisors[] = { 2, 2, 2, 2, 2, 3, 3, 4, 6, 2 };

	if (picStruct > VIP_PIC_STRUCT_FRAME_TRIPLING)
		picStruct = VIP_PIC_STRUCT_FRAME_TRIPLING + 1;
	return divisors[picStruct];
}

static int hrdPresent(const VipSps *sps)
{
	return sps->nalHrdPresent || sps->vclHrdPresent;
}

/* the time after the last picture, which lasted by picStruct */
static uint64_t afterPicture(uint32_t picStruct, int64_t *fraction)
{
	uint32_t ticks = deltaTfiDivisor(picStruct);

	*fraction += ticksToPtsRemainder(ticks);
	return (g_pictureTime + ticksToPts(ticks)) & VIP_PTS_MASK;
}

/* the decode time after a buffering period's first picture, by cpb_removal_delay */
static uint64_t afterPeriod(int64_t *fraction)
{
	*fraction = g_periodTimeFraction + ticksToPtsRemainder(g_cpbRemovalDelay);
	return (g_periodTime + ticksToPts(g_cpbRemovalDelay)) & VIP_PTS_MASK;
}

/* A whole unit of g_timeFraction, either way, goes into the time. */
static uint64_t carryFraction(uint64_t time)
{
	int64_t scale = g_currentSps->timeScale;

	if ((int64_t)g_timeFraction >= scale) {
		g_timeFraction -= scale;
		return time + 1;
	}
	if ((int64_t)g_timeFraction <= -scale) {
		g_timeFraction += scale;
		return time - 1;
	}
	return time;
}

/*
 * A first field's or a frame's decode time when its record has no DTS; *untimed
 * when the stream has no timing and the time is the fallback.
 */
static uint64_t decodeTime(uint32_t option, uint32_t flags, uint32_t ptsLow,
	const VipTime *fallback, int64_t *fraction, int *untimed)
{
	const VipSps *sps = g_currentSps;
	int hasPts = (flags & VIP_TIME_PTS) != 0;

	if (hasPts && (option == OPTION_NO_REORDER || sps->picOrderCntType == 2))
		goto pts;
	if (!sps->timeScale) {
		if (g_periodTime == NO_TIME && hasPts)
			goto pts;
		*untimed = 1;
		*fraction = 0;
		return *fallback;
	}
	if (g_pictureTimingTaken && hrdPresent(sps)) {
		if (hasPts && g_bufferingPeriodTaken) {
			g_timeStarted = 1;
			*fraction = -(int64_t)ticksToPtsRemainder(g_dpbOutputDelay);
			return (recordPts(flags, ptsLow) - ticksToPts(g_dpbOutputDelay)) &
				VIP_PTS_MASK;
		}
		if (g_periodTime != NO_TIME &&
		    !(g_bufferingPeriodTaken && !g_cpbRemovalDelay))
			return afterPeriod(fraction);
#ifdef VIP_FW2
		return afterPicture(g_lastPicStruct, fraction);
#else
		return afterPicture(g_picStruct, fraction);
#endif
	}
	if (g_periodTime == NO_TIME && hasPts)
		goto pts;
	return afterPicture(g_lastPicStruct, fraction);

pts:
	/* without reordering, the PTS is the decode time */
	g_timeStarted = 1;
	*fraction = 0;
	return recordPts(flags, ptsLow) & VIP_PTS_MASK;
}

/*
 * A second field with a buffering period: its decode time, not its frame's,
 * is the period's start.
 */
static void takeSecondFieldTime(uint32_t flags, uint32_t ptsLow, uint32_t dtsLow,
	const VipTime *fallback)
{
	const VipSps *sps = g_currentSps;
	int64_t fraction = g_timeFraction;
	uint64_t time;
	int untimed = 0;

	if (!g_bufferingPeriodTaken)
		return;
	if (flags & VIP_TIME_DTS) {
		time = recordDts(flags, dtsLow) & VIP_PTS_MASK;
		fraction = 0;
	} else if ((flags & VIP_TIME_PTS) && sps->picOrderCntType == 2) {
		time = recordPts(flags, ptsLow) & VIP_PTS_MASK;
		fraction = 0;
	} else if (sps->timeScale && g_pictureTimingTaken && hrdPresent(sps)) {
		if (!g_cpbRemovalDelay)
			time = afterPicture(g_lastPicStruct, &fraction);
		else
			time = afterPeriod(&fraction);
	} else if (sps->timeScale && sps->fixedFrameRate) {
		time = afterPicture(g_lastPicStruct, &fraction);
	} else {
		untimed = 1;
		time = *fallback;
		fraction = 0;
	}
	if (g_pictureHeld)
		return;
	g_timeFraction = fraction;
	if (!untimed)
		time = carryFraction(time);
	g_pendingPeriodTime = time;
}

/*
 * The presentation time: the record's PTS, else (or first, with
 * OPTION_TIMING_FIRST) the decode time and dpb_output_delay. 0, or -1 when the
 * picture has none. FW2's noDecodeTimePts: the decode time alone is none.
 */
static int takePresentationTime(uint32_t noDecodeTimePts, uint32_t option, uint32_t flags,
	uint32_t ptsLow)
{
	const VipSps *sps = g_currentSps;
	VipPictureBuffer *buffer = g_currentBuffer;
	int timed = sps->timeScale && g_pictureTimingTaken && hrdPresent(sps);

	if ((option != OPTION_TIMING_FIRST || !timed) && (flags & VIP_TIME_PTS)) {
		buffer->presentationTime = recordPts(flags, ptsLow) & VIP_PTS_MASK;
		buffer->timeStarted = 1;
		buffer->ptsValid = 1;
		return 0;
	}
	if (timed) {
		buffer->presentationTime = (buffer->decodeTime +
			ticksToPts(g_dpbOutputDelay)) & VIP_PTS_MASK;
		buffer->timeStarted = g_timeStarted != 0;
		buffer->ptsValid = 1;
		return 0;
	}
	buffer->ptsValid = 0;
	buffer->timeStarted = 0;
	if (option != OPTION_TIMING_FIRST && sps->timeScale && !noDecodeTimePts) {
		buffer->presentationTime = buffer->decodeTime;
		return 0;
	}
	buffer->presentationTime = NO_TIME;
	return -1;
}

/*
 * The times of the picture starting, from its record's timestamps (flags: the
 * header's VIP_RECORD_* bits >> 12) or the stream's timing; fallback is the
 * time a stream without timing gets. 0, or -1 when it has no presentation time.
 */
#ifdef VIP_FW2
int takePictureTime(uint32_t noDecodeTimePts, uint32_t option, uint32_t flags,
	uint32_t ptsLow, uint32_t dtsLow, const VipTime *fallback)
#else
int takePictureTime(uint32_t option, uint32_t flags, uint32_t ptsLow, uint32_t dtsLow,
	const VipTime *fallback)
#endif
{
	VipPictureBuffer *buffer = g_currentBuffer;
	int64_t fraction = g_timeFraction;
	uint64_t time;
	int untimed = 0, result;

	if (g_secondFieldPending) {
		takeSecondFieldTime(flags, ptsLow, dtsLow, fallback);
		return 0;
	}

	if (flags & VIP_TIME_DTS) {
		time = recordDts(flags, dtsLow) & VIP_PTS_MASK;
		g_timeStarted = 1;
	} else {
		time = decodeTime(option, flags, ptsLow, fallback, &fraction, &untimed);
		if (!g_pictureHeld)
			g_timeFraction = fraction;
		if (!untimed)
			time = carryFraction(time);
	}
	if (!g_pictureHeld) {
		buffer->decodeTime = time & VIP_PTS_MASK;
		if (g_bufferingPeriodTaken)
			g_pendingPeriodTime = buffer->decodeTime;
	}

#ifdef VIP_FW2
	result = takePresentationTime(noDecodeTimePts, option, flags, ptsLow);
#else
	result = takePresentationTime(0, option, flags, ptsLow);
	if (!buffer->ptsValid && untimed)
		buffer->outputFlags |= VIP_OUTPUT_NO_PTS;
#endif
	return result;
}

/*
 * The SPS's time_scale changed from oldScale to newScale: the time fractions are
 * dropped. The firmware rescales them only when both scales are 0, which its
 * caller never asks for (the scales differ), and would then divide by 0.
 */
void rescaleClock(uint32_t oldScale, uint32_t newScale)
{
	if (newScale || oldScale) {
		g_timeFraction = 0;
		g_periodTimeFraction = 0;
		return;
	}
	g_timeFraction = (int64_t)(g_timeFraction * newScale) / (int64_t)oldScale;
	g_periodTimeFraction = (int64_t)(g_periodTimeFraction * newScale) /
		(int64_t)oldScale;
}
