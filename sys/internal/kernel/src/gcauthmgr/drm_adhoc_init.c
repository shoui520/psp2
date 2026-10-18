/* Adhoc BB semaphore lifecycle. */
#include <psp2kern/kernel/threadmgr.h>
#include <stdint.h>

uint64_t gcAuthAdhocCounter;
int gcAuthAdhocInitialized;
unsigned char gcAuthAdhocRandom[32];

extern SceUID gcAuthAdhocSemaphore;

int GcAuthInitializeAdhocBb(void)
{
	SceUID semaphore;

	semaphore = sceKernelCreateSema("SceAdhocBB", 0, 1, 1, NULL);
	gcAuthAdhocSemaphore = semaphore;
	return semaphore <= 0;
}
