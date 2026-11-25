/*
 * location_stats.cpp: telemetry counters for liblocation WiFi history & query location
 * (0x8118E8AC-0x8118E9F4, sti 0x8118E984).
 */
#include "location_internal.h"

/* 0x8118E8AC */
int locationStatsCountWifiHistoryCalled(void)
{
	int ret = g_locationConfig->CountUp(175, 1);

	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "[STATS] Count up # of GetLocationWifiHistory called\n");
	return ret;
}

/* 0x8118E8E2 */
int locationStatsCountWifiHistoryFailed(void)
{
	int ret = g_locationConfig->CountUp(176, 1);

	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "[STATS] Count up # GetLocationWifiHistory failed\n");
	return ret;
}

/* 0x8118E918 */
int locationStatsCountQueryRequested(void)
{
	int ret = g_locationConfig->CountUp(173, 1);

	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "[STATS] Count up # QueryLocation requested\n");
	return ret;
}

/* 0x8118E94E */
int locationStatsCountQuerySucceeded(void)
{
	int ret = g_locationConfig->CountUp(174, 1);

	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "[STATS] Count up # QueryLocation succeeded\n");
	return ret;
}
