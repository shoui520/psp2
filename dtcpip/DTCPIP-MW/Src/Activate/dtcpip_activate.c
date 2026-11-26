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

/* SceRegMgr's key reader (the SDK's libregmgr; the tree has no header for it yet) */
int sceRegMgrGetKeyBin(const char *category, const char *name, void *buffer, int size);

/* whether the device certificate is present: the activation's state (0x810110f6) */
int _sceDtcpIpCheckActivation(int *activated)
{
	unsigned char cert[3];
	int result;
	int i;

	DTCPIP_ACTIVATE_TRACE(660);
	result = 0;
	cert[0] = 0;
	cert[1] = 0;
	cert[2] = 0;
	i = 0;
	if (activated != NULL) {
		result = sceRegMgrGetKeyBin("/CONFIG/DTCPIP", "devcert", cert, 3);
		if (result >= 0) {
			for (i = 0; i < 3; ++i) {
				sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
					      "(679) : cert[%d] = %02d\n", i, (char)cert[i]);
				if (cert[i] != 0)
					break;
			}
			if (i < 3) {
				*activated = 1;
			} else {
				sceClibPrintf(DTCPIP_ACTIVATE_SOURCE "(686) : Need Activate\n");
				*activated = 0;
			}
		} else {
			sceClibPrintf(DTCPIP_ACTIVATE_SOURCE
				      "(673) : ERROR!! ret=0x%08x(%d)\n", result, result);
		}
	} else {
		result = -2;
	}
	return result;
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
