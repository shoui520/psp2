/*
 * fw1 runtime: the trace log. Two rings of 16-byte records in DRAM, one per core
 * (bit 0x20 of a channel picks core 1's), for the debugger on the ARM side.
 */
#include "runtime.h"

#define VIP_TRACE_CORE1		0x20	/* channel bit: core 1's ring */

typedef struct VipTraceRecord {
	uint32_t sequence;
	uint32_t channel;
	uint32_t first;
	uint32_t second;
} VipTraceRecord;

typedef struct VipTraceRing {
	VipTraceRecord *start;
	VipTraceRecord *last;		/* the cursor wraps after this record */
	uint32_t fill;			/* the first record's words until written */
} VipTraceRing;

/* the cursors of the two rings, in DRAM before them */
#define VIP_TRACE_CURSORS	((VipTraceRecord **)(VIP_IMAGE + 0xe6fe0))

static const VipTraceRing g_traceRings[2] = {
	{ (VipTraceRecord *)(VIP_IMAGE + 0xe7000), (VipTraceRecord *)(VIP_IMAGE + 0xeaff0),
	  0xaaaaaaaa },
	{ (VipTraceRecord *)(VIP_IMAGE + 0xeb000), (VipTraceRecord *)(VIP_IMAGE + 0xefff0),
	  0x55555555 },
};

static uint32_t g_traceSequence;

uint32_t getCoreNumber(void)
{
	return (vipReadControl(VIP_CR_ID) >> 16) & 0xff;
}

/* The task the channel's core runs, as its status block says. */
uint32_t getTraceTask(uint32_t channel)
{
	return VIP_STATUSES[(channel & VIP_TRACE_CORE1) ? 1 : 0].currentTask & 0xff;
}

static uint32_t traceRing(uint32_t channel)
{
	return (channel & VIP_TRACE_CORE1) ? 1 : 0;
}

static VipTraceRecord *getTraceCursor(uint32_t channel)
{
	return VIP_TRACE_CURSORS[traceRing(channel)];
}

static void advanceTrace(uint32_t channel)
{
	uint32_t ring = traceRing(channel);
	VipTraceRecord *cursor = VIP_TRACE_CURSORS[ring];

	if (cursor < g_traceRings[ring].last)
		VIP_TRACE_CURSORS[ring] = cursor + 1;
	else
		VIP_TRACE_CURSORS[ring] = g_traceRings[ring].start;
}

void writeTrace(uint32_t channel, uint32_t first, uint32_t second)
{
	VipTraceRecord *record = getTraceCursor(channel);

	record->sequence = g_traceSequence++;
	record->channel = channel;
	record->first = first;
	record->second = second;
	advanceTrace(channel);
}

void initializeTrace(void)
{
	uint32_t ring;

	for (ring = 0; ring < 2; ring++) {
		VipTraceRecord *record = g_traceRings[ring].start;

		record->sequence = g_traceRings[ring].fill;
		record->channel = g_traceRings[ring].fill;
		record->first = g_traceRings[ring].fill;
		record->second = g_traceRings[ring].fill;
		VIP_TRACE_CURSORS[ring] = record;
	}
	g_traceSequence = 0;
}
