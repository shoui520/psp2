/*
 * SceNetCtl periodical scan: a thread that alternately starts a wireless scan
 * and, two seconds later, collects up to ten results into a table the
 * process reads with SceNetCtl_BBA99D34.
 */

#include "netctl_internal.h"

#define NETCTL_SCAN_RESULT_MAX		10
#define NETCTL_SCAN_COLLECT_DELAY	2000000		/* us between scan start and collection */
#define NETCTL_SCAN_PERIOD		13000000	/* us between collection and next scan */
#define NETCTL_SCAN_EVENT_STOP		1

/* One scan result as SceNetCtl_1A254169 reports it (0x60 bytes). */
typedef struct NetCtlScanInfo {
	char unk00[0x10];
	char bssid[6];
	char unk16[2];
	char ssid[NETCTL_SSID_SIZE];
	char unk39[3];
	int unk3C;
	int unk40;
	int unk44;
	int unk48;
	int unk4C;
	SceUInt64 unk50;
	char unk58[8];
} NetCtlScanInfo;

/* One entry of the periodical scan table (0x70 bytes). */
typedef struct NetCtlScanResult {
	SceUInt64 unk00;
	char bssid[6];
	char unk0E[2];
	char ssid[NETCTL_SSID_SIZE];
	char unk31[3];
	int unk34;
	int unk38;
	int unk3C;
	int unk40;
	int unk44;
	char unk48[0x28];
} NetCtlScanResult;

static SceUInt32 g_scanResultCount;
static SceKernelLwMutexWork *g_scanMutex;
static SceUID g_scanEventFlag = -1;
static SceUID g_scanThread = -1;
static int g_scanEnabled;
static int g_scanRunning;
static NetCtlScanResult g_scanResults[NETCTL_SCAN_RESULT_MAX];

int SceNetCtl_A840C74C(void *buf, int count);
int SceNetCtl_1A254169(void *bssid, void *info, void *extra, int *extraSize);
int SceNetCtl_6A56FC3A(const char **ssids, SceUInt32 count, const NetCtlScanParam *param);
static int netCtlScanThread(SceSize args, void *argp);

int netCtlScanSetMutex(SceKernelLwMutexWork *mutex)
{
	g_scanMutex = mutex;
	g_scanEnabled = 1;
	return 0;
}

void netCtlScanClearMutex(void)
{
	g_scanEnabled = 0;
	g_scanMutex = NULL;
}

/* name unknown: starts the periodical scan thread */
int SceNetCtl_586C4DD7(int priority, int cpuAffinity, const SceSize *option)
{
	int ret;

	if (!g_scanEnabled)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (option != NULL && *option != 0x30)
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	ret = sceKernelLockLwMutex(g_scanMutex, 1, NULL);
	if (ret < 0)
		return ret;
	sceClibMemset(g_scanResults, 0, sizeof(g_scanResults));
	g_scanResultCount = 0;
	ret = sceKernelCreateEventFlag("SceNetCtlPeriodicalScan", 0, 0, NULL);
	if (ret >= 0) {
		g_scanEventFlag = ret;
		ret = sceKernelCreateThread("SceNetCtlPeriodicalScan", netCtlScanThread, priority,
					    0x4000, 0, cpuAffinity, NULL);
		if (ret >= 0) {
			g_scanThread = ret;
			ret = sceKernelStartThread(ret, 0, NULL);
			if (ret >= 0) {
				g_scanRunning = 1;
				sceKernelUnlockLwMutex(g_scanMutex, 1);
				return 0;
			}
		}
	}
	if (g_scanThread >= 0) {
		sceKernelDeleteThread(g_scanThread);
		g_scanThread = -1;
	}
	if (g_scanEventFlag >= 0) {
		sceKernelDeleteEventFlag(g_scanEventFlag);
		g_scanEventFlag = -1;
	}
	sceKernelUnlockLwMutex(g_scanMutex, 1);
	return ret;
}

/* name unknown: stops the periodical scan thread */
int SceNetCtl_67926E00(void)
{
	int ret;

	if (!g_scanEnabled)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	ret = sceKernelLockLwMutex(g_scanMutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (!g_scanRunning) {
		sceKernelUnlockLwMutex(g_scanMutex, 1);
		return 0;
	}
	sceKernelSetEventFlag(g_scanEventFlag, NETCTL_SCAN_EVENT_STOP);
	sceKernelUnlockLwMutex(g_scanMutex, 1);
	if (g_scanThread >= 0) {
		sceKernelWaitThreadEnd(g_scanThread, NULL, NULL);
		sceKernelDeleteThread(g_scanThread);
		g_scanThread = -1;
	}
	if (g_scanEventFlag >= 0) {
		sceKernelDeleteEventFlag(g_scanEventFlag);
		g_scanEventFlag = -1;
	}
	g_scanRunning = 0;
	return 0;
}

/* name unknown: copies up to max entries of the scan table, returns the count */
int SceNetCtl_BBA99D34(NetCtlScanResult *results, SceUInt32 max)
{
	int ret;

	if (!g_scanEnabled)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (results == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (max == 0)
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	ret = sceKernelLockLwMutex(g_scanMutex, 1, NULL);
	if (ret >= 0) {
		ret = max;
		if (g_scanResultCount < max)
			ret = g_scanResultCount;
		sceClibMemcpy_safe(results, g_scanResults, ret * sizeof(NetCtlScanResult));
		sceKernelUnlockLwMutex(g_scanMutex, 1);
	}
	return ret;
}

static int netCtlScanThread(SceSize args, void *argp)
{
	int ret;
	int count;
	SceUInt32 i;
	SceUInt32 found;
	SceUInt32 phase = 0;
	SceUInt32 bits = 0;
	SceUInt32 outBits;
	SceUInt32 timeout;
	NetCtlScanResult *result;
	char bssids[NETCTL_SCAN_RESULT_MAX][6];
	NetCtlScanInfo info;

	do {
		if (phase == 1) {
			if (sceKernelLockLwMutex(g_scanMutex, 1, NULL) >= 0) {
				count = SceNetCtl_A840C74C(bssids, NETCTL_SCAN_RESULT_MAX);
				if (count >= 0) {
					sceClibMemset(g_scanResults, 0, sizeof(g_scanResults));
					g_scanResultCount = 0;
					found = 0;
					result = g_scanResults;
					for (i = 0; i < (SceUInt32)count; i++) {
						if (SceNetCtl_1A254169(bssids[i], &info, NULL, NULL) < 0)
							continue;
						result->unk00 = info.unk50;
						sceClibMemcpy_safe(result->bssid, info.bssid, sizeof(result->bssid));
						sceClibMemcpy_safe(result->ssid, info.ssid, sizeof(result->ssid));
						result->unk34 = info.unk3C;
						result->unk38 = info.unk40;
						result->unk3C = info.unk44;
						result->unk40 = info.unk48;
						result->unk44 = info.unk4C;
						found++;
						result++;
					}
					g_scanResultCount = found;
				}
				sceKernelUnlockLwMutex(g_scanMutex, 1);
			}
			timeout = NETCTL_SCAN_PERIOD;
			phase = 0;
		} else {
			SceNetCtl_6A56FC3A(NULL, 0, NULL);
			timeout = NETCTL_SCAN_COLLECT_DELAY;
			phase = 1;
		}
		outBits = bits;
		ret = sceKernelWaitEventFlag(g_scanEventFlag, NETCTL_SCAN_EVENT_STOP,
					     SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, &outBits, &timeout);
	} while (ret != 0 && ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT);
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}
