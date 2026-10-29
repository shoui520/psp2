/* SceTouch: the syscon's sample reply parsed into one sample per panel, and its debug switches.
 * Private names without a recovered SDK name remain unknown.
 */
#include "private.h"

/* one report of the syscon's sample reply */
#define TOUCH_RAW_REPORT	6
#define TOUCH_RAW_FRONT		6	/* reports per panel in the reply */
#define TOUCH_RAW_BACK		4
#define TOUCH_RAW_PANEL		0x3c	/* bytes per panel */
#define TOUCH_RAW_INVALID	0x80	/* id byte of an unused report */

unsigned s_forceFilterMode = 0;		/* use the syscon's state for the force filter */
unsigned s_forceReport = 0;		/* report the raw force sum and spread (debug) */
static unsigned s_clampCoordinates = 1;

/* names unknown: debug switches (SceTouchForDriver_062A11A2 / _845E2115) */
void setForceReport(unsigned enable)
{
	s_forceReport = enable;
}

void setForceFilterMode(unsigned mode)
{
	s_forceFilterMode = mode;
}

void setClampCoordinates(unsigned clamp)
{
	s_clampCoordinates = clamp;
}

static inline unsigned rawX(const unsigned char *raw)
{
	return raw[1] + ((raw[2] & 7) << 8);
}

static inline unsigned rawY(const unsigned char *raw)
{
	return raw[3] + ((raw[4] & 7) << 8);
}

static inline void clearReserved(SceTouchReport *report)
{
	unsigned i;

	for (i = 0; i < sizeof(report->rsv); i++)
		report->rsv[i] = 0;
}

/* name unknown: raw force sum and spread (s_forceReport) into reports[0].reserved */
static inline void reportForceInfo(const unsigned char *panel, unsigned count, SceTouchData *out)
{
	unsigned short sum = (panel[0x34] + (panel[0x35] << 8)) << 5;
	unsigned short spread = 0;
	unsigned first, last;

	if (count) {
		first = panel[0x30] + ((panel[0x31] & 0xf) << 8);
		last = (panel[0x32] << 4) + (panel[0x31] >> 4);
		if (last < first)
			last += 0x1000;
		spread = last - first;
	}
	out->report[0].rsv[0] = sum;
	out->report[0].rsv[2] = spread;
	out->report[0].rsv[1] = sum >> 8;
	out->report[0].rsv[3] = spread >> 8;
}

/* name unknown: the syscon's sample reply into one sample per panel */
int parseSamples(unsigned kinds, const unsigned char *buffer, unsigned size,
			SceTouchData *front, SceTouchData *back)
{
	SceTouchReport *report;
	const unsigned char *raw;
	unsigned n, x, y;
	uint64_t time;
	int flags, busy;

	if (!front || !back)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	time = sceKernelGetSystemTimeWide();
	back->status = 0;
	back->reportNum = 0;
	front->timeStamp = time;
	front->status = 0;
	front->reportNum = 0;
	back->timeStamp = time;

	if (kinds & 1) {
		if (!buffer || size < TOUCH_RAW_PANEL)
			return SCE_TOUCH_ERROR_INVALID_ARG;
		n = 0;
		for (raw = buffer; raw != buffer + TOUCH_RAW_FRONT * TOUCH_RAW_REPORT;
		     raw += TOUCH_RAW_REPORT) {
			if (raw[0] & TOUCH_RAW_INVALID)
				continue;
			report = &front->report[n];
			report->id = raw[0];
			x = rawX(raw);
			y = rawY(raw);
			if (s_clampCoordinates) {
				if (x >= 1919)
					x = 1919;
				if (y >= 1087)
					y = 1087;
			}
			report->x = x;
			n++;
			report->y = y;
			report->force = raw[5];
			report->info = 0;
			clearReserved(report);
		}
		if (s_forceReport)
			reportForceInfo(buffer, n, front);
		front->reportNum = n;
		if (!(kinds & 2))
			goto filter;
		buffer += TOUCH_RAW_PANEL;
		size -= TOUCH_RAW_PANEL;
	} else if (!(kinds & 2)) {
		goto filter;
	}

	if (!buffer || size < TOUCH_RAW_PANEL)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	n = 0;
	back->status = 0;
	for (raw = buffer; raw != buffer + TOUCH_RAW_BACK * TOUCH_RAW_REPORT; raw += TOUCH_RAW_REPORT) {
		if (raw[0] & TOUCH_RAW_INVALID)
			continue;
		report = &back->report[n];
		report->id = raw[0];
		x = rawX(raw);
		y = rawY(raw);
		if (s_clampCoordinates) {
			/* the back panel covers display rows 108..889 */
			report->x = x < 1919 ? x : 1919;
			report->y = y > 781 ? 889 : y + 108;
		} else {
			report->x = x;
			report->y = y;
		}
		n++;
		report->force = raw[5];
		report->info = 0;
		clearReserved(report);
	}
	if (s_forceReport)
		reportForceInfo(buffer, n, back);
	back->reportNum = n;

filter:
	if (!isAltPanel(0))
		return 0;
	if (!s_forceFilterMode) {
		forceRange(0, front, 2);
		forceRange(1, back, 2);
		return 0;
	}
	busy = SceSysconForDriver_142D5E82();
	if (SceSysconForDriver_9DA2A5AB() == 0) {
		flags = busy != 0;
		forceSpread(0, front, flags);
		forceSpread(1, back, flags);
	} else {
		flags = 2;
	}
	forceRange(0, front, flags);
	forceRange(1, back, flags);
	return 0;
}
