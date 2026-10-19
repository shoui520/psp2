/* Adhoc session initialization. */
#include <stddef.h>
#include <stdint.h>
#include <threadmgr/threadmgr.h>
#include "drm_crypto.h"

extern SceUID gcAuthAdhocSemaphore;
extern uint64_t gcAuthAdhocCounter;
extern int gcAuthAdhocInitialized;
extern unsigned char gcAuthAdhocRandom[32];

int GcAuthAdhocInitializeState(void)
{
	int result;

	if (sceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return 0x808A010A;
	result = 0;
	if (!gcAuthAdhocInitialized) {
		gcAuthAdhocCounter = 0;
		result = 0x808A0101;
		if (GcAuthGenerateRandom20(gcAuthAdhocRandom) == 0) {
			result = 0;
			gcAuthAdhocInitialized = 1;
		}
	}
	if (sceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = 0x808A010A;
	return result;
}
