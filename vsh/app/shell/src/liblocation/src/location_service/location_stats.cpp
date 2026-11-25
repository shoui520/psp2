/*
 * location_stats.cpp: telemetry counters for liblocation WiFi history & query location
 * (0x8118E8AC-0x8118E9F4, sti 0x8118E984). The file and function names are ours (PLACEHOLDER).
 */
#include "location_service.h"

struct LocationStatsClient {
	virtual void vfn0();
	virtual void vfn1();
	virtual int CountUp(int id, int delta);
};

extern LocationStatsClient *g_locationStatsClient;	/* 0x815496C0 */
extern void *g_locationLogger;				/* 0x815496B8 */
extern void locationLoggerPrintf(void *logger, int level, const char *fmt, ...);

/* 0x8118E8AC */
int locationStatsCountWifiHistoryCalled(void)
{
	int ret = g_locationStatsClient->CountUp(175, 1);
	if (g_locationLogger != NULL)
		locationLoggerPrintf(g_locationLogger, 8,
				     "[STATS] Count up # of GetLocationWifiHistory called\n");
	return ret;
}

/* 0x8118E8E2 */
int locationStatsCountWifiHistoryFailed(void)
{
	int ret = g_locationStatsClient->CountUp(176, 1);
	if (g_locationLogger != NULL)
		locationLoggerPrintf(g_locationLogger, 8,
				     "[STATS] Count up # GetLocationWifiHistory failed\n");
	return ret;
}

/* 0x8118E918 */
int locationStatsCountQueryRequested(void)
{
	int ret = g_locationStatsClient->CountUp(173, 1);
	if (g_locationLogger != NULL)
		locationLoggerPrintf(g_locationLogger, 8,
				     "[STATS] Count up # QueryLocation requested\n");
	return ret;
}

/* 0x8118E94E */
int locationStatsCountQuerySucceeded(void)
{
	int ret = g_locationStatsClient->CountUp(174, 1);
	if (g_locationLogger != NULL)
		locationLoggerPrintf(g_locationLogger, 8,
				     "[STATS] Count up # QueryLocation succeeded\n");
	return ret;
}
