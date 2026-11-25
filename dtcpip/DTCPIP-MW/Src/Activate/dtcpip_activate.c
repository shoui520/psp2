/*
 * swagner's activation state (DTCPIP-MW/Src/Activate/dtcpip_activate.c, from the trace
 * strings): the activation's lock and its cancel flag, which the activation's worker
 * checks between its requests. The init takes the lock the whole middleware shares; the
 * term releases it. TRACE prints the file, the line and the function, as Sony's does.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libhttp.h>
#include <libkernel/libkernel.h>

#define DTCPIP_ACTIVATE_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Activate\\dtcpip_activate.c"
#define DTCPIP_ACTIVATE_TRACE(line)						\
	sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(" #line ") : TRACE : %s()\n", __FUNCTION__)

/* the activation's lock, the request it is waiting on, and the flag its cancel raises */
SceKernelLwMutexWork g_dtcpIpActivateMutex;
int g_dtcpIpActivateCancelled;
SceUID g_dtcpIpActivateRequest;

__attribute__((noinline)) static int ActivateLock(void)
{
	return sceKernelLockLwMutex(&g_dtcpIpActivateMutex, 1, NULL);
}

__attribute__((noinline)) static int ActivateUnlock(void)
{
	return sceKernelUnlockLwMutex(&g_dtcpIpActivateMutex, 1);
}

int _sceDtcpIpActivateInit(void)
{
	g_dtcpIpActivateCancelled = 0;
	return sceKernelCreateLwMutex(&g_dtcpIpActivateMutex, "SceDtcpIpActivateLwMutex", 0, 0,
				      NULL);
}

int _sceDtcpIpActivateTerm(void)
{
	return sceKernelDeleteLwMutex(&g_dtcpIpActivateMutex);
}

int _sceDtcpIpCancelActivation(void)
{
	int result;

	DTCPIP_ACTIVATE_TRACE(629);
	result = ActivateLock();
	if (result >= 0) {
		g_dtcpIpActivateCancelled = 1;
		if (g_dtcpIpActivateRequest > 0) {
			int abortResult;

			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(641) : sceHttpAbortRequest()\n");
			abortResult = sceHttpAbortRequest(g_dtcpIpActivateRequest);
			if (abortResult < 0)
				sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
					      "(644) : sceHttpAbortRequest() error: 0x%08X\n",
					      abortResult);
		}
		result = ActivateUnlock();
		if (result < 0)
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(650) : Unlock() error: 0x%08X\n", result);
	} else {
		sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(635) : Lock() error: 0x%08X\n",
			      result);
	}
	return result;
}
