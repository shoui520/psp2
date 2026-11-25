/*
 * swagner's download state (DTCPIP-MW/Src/Mw/dtcpip_download.c, from the trace strings):
 * the download's lock and its cancel flag. The init takes the lock; the term prints its
 * trace and releases it, reporting a failed delete as Sony's does.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libhttp.h>
#include <libkernel/libkernel.h>

#define DTCPIP_DOWNLOAD_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\dtcpip_download.c"
#define DTCPIP_DOWNLOAD_TRACE(line)						\
	sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(" #line ") : TRACE : %s()\n", __FUNCTION__)

/* the download's lock, the request it is waiting on, and the flag its cancel raises */
SceKernelLwMutexWork g_dtcpIpDownloadMutex;
int g_dtcpIpDownloadCancelled;
SceUID g_dtcpIpDownloadRequest;

__attribute__((noinline)) static int DownloadLock(void)
{
	return sceKernelLockLwMutex(&g_dtcpIpDownloadMutex, 1, NULL);
}

__attribute__((noinline)) static int DownloadUnlock(void)
{
	return sceKernelUnlockLwMutex(&g_dtcpIpDownloadMutex, 1);
}

int _sceDtcpIpDownloadInit(void)
{
	g_dtcpIpDownloadCancelled = 0;
	return sceKernelCreateLwMutex(&g_dtcpIpDownloadMutex, "SceDtcpIpDownloadLwMutex", 0, 0,
				      NULL);
}

int _sceDtcpIpDownloadTerm(void)
{
	int result;

	DTCPIP_DOWNLOAD_TRACE(536);
	result = sceKernelDeleteLwMutex(&g_dtcpIpDownloadMutex);
	if (result < 0)
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
			      "(542) : sceKernelDeleteLwMutex() error: 0x%08X\n", result);
	return result;
}

int _sceDtcpIpCancelDownload(void)
{
	int result;

	DTCPIP_DOWNLOAD_TRACE(506);
	result = DownloadLock();
	if (result >= 0) {
		g_dtcpIpDownloadCancelled = 1;
		if (g_dtcpIpDownloadRequest > 0) {
			int abortResult;

			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(518) : sceHttpAbortRequest()\n");
			abortResult = sceHttpAbortRequest(g_dtcpIpDownloadRequest);
			if (abortResult < 0)
				sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
					      "(521) : sceHttpAbortRequest() error: 0x%08X\n",
					      abortResult);
		}
		result = DownloadUnlock();
		if (result < 0)
			sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE
				      "(527) : Unlock() error: 0x%08X\n", result);
	} else {
		sceClibPrintf(DTCPIP_DOWNLOAD_SOURCE "(512) : Lock() error: 0x%08X\n",
			      result);
	}
	return result;
}
