/* Adhoc session clearing. */
#include <stdint.h>
#include <psp2kern/kernel/threadmgr.h>

extern SceUID gcAuthAdhocSemaphore;
extern uint64_t gcAuthAdhocCounter;
extern int gcAuthAdhocInitialized;
extern unsigned char gcAuthAdhocRandom[32];

int GcAuthAdhocClearState(void)
{
	unsigned int index;

	if (ksceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return 0x808A010A;
	if (gcAuthAdhocInitialized) {
		for (index = 0; index < sizeof(gcAuthAdhocRandom); index++)
			gcAuthAdhocRandom[index] = 0;
		gcAuthAdhocInitialized = 0;
		gcAuthAdhocCounter = 0;
	}
	if (ksceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		return 0x808A010A;
	return 0;
}
