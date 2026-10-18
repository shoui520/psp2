/* Adhoc session initialization. */
#include <stdint.h>
#include <psp2kern/kernel/threadmgr.h>

extern SceUID gcAuthAdhocSemaphore;
extern uint64_t gcAuthAdhocCounter;
extern int gcAuthAdhocInitialized;
extern unsigned char gcAuthAdhocRandom[32];
int GcAuthGenerateRandom20(unsigned char *data);

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
