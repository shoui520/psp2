/*
 * fw1 runtime: time. The interval timer counts down from 0xffffffff once per
 * frame; time is measured from g_timeBase, and each task and frame is accounted
 * in the frame records and the statistics in DRAM.
 */
#include "runtime.h"

#define VIP_FRAME_PERIOD	0x200000	/* timer ticks */

VipTimes g_times;
VipFrameRecord g_frameRecords[VIP_FRAME_RECORDS];
uint32_t *g_frameCursor;
uint32_t g_timeBase VIP_LRAM_DATA;

/* Spin for about the given number of cycles (four per loop). */
void delay(uint32_t cycles)
{
	int32_t loops = cycles >> 2;

	if (loops < 7)
		return;
	loops -= 6;
	do
		__asm__ volatile ("");
	while (--loops);
}

/* Let the current task take at least its interval. */
void waitTaskInterval(void)
{
	uint32_t interval = g_taskIntervals[g_currentTaskId];
	uint32_t elapsed;

	if (!interval)
		return;
	elapsed = readTime() - g_times.taskStart;
	if (elapsed < interval)
		delay(interval - elapsed);
}

/* Wait for the end of the frame period. */
void waitForTimer(void)
{
	uint32_t time;

	while ((time = readTime()) < VIP_FRAME_PERIOD)
		delay(VIP_FRAME_PERIOD - time);
}

uint32_t readTime(void)
{
	return g_timeBase - vipReadControlBus(VIP_CB_TIMER_COUNT);
}

void clearWords(uint32_t *words, uint32_t count)
{
	uint32_t *end = words + count;

	for (; words < end; words++)
		*words = 0;
}

void clearStatistics(void)
{
	clearWords((uint32_t *)&g_times, sizeof(g_times) / 4);
	clearWords((uint32_t *)VIP_STATISTICS, 32);
}

void recordStatistic(uint32_t index, uint32_t value, VipStatistic *statistics)
{
	VipStatistic *statistic = &statistics[index];
	uint32_t total = statistic->total + value;

	if (total < statistic->total)
		statistic->wraps++;
	statistic->total = total;
	if (statistic->maximum < value)
		statistic->maximum = value;
	statistic->count++;
}

void recordTaskStatistic(uint32_t index, uint32_t value)
{
	recordStatistic(index, value, VIP_STATISTICS);
}

/* The current task yielded: record how long it ran. */
void recordTaskTime(void)
{
	uint32_t elapsed = readTime() - g_times.taskStart;

	*g_frameCursor++ = elapsed;
	recordTaskStatistic(g_currentTaskId, elapsed);
}

/* The frame's tasks are done: record how long they took. */
void recordFrameTime(void)
{
	uint32_t elapsed = readTime() - g_times.frameStart;

	*g_frameCursor++ = elapsed;
	recordTaskStatistic(VIP_TASK_COUNT, elapsed);
}

/* The frame period is over: record it. */
void recordFramePeriod(void)
{
	recordTaskStatistic(VIP_TASK_COUNT + 1, readTime() - g_times.frameStart);
}
