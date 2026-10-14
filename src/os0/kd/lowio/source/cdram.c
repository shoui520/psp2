/* SceCdram asynchronous secure-monitor initialization. */
#include "lowio_internal.h"
#include <psp2kern/kernel/threadmgr.h>
#include <sce/threadmgr.h>
#include <sce/touch_dummy.h>

uint32_t Smc117(uint32_t command);
volatile uint32_t s_cdramReady;

int CdramInitWorker(void *arg);

int LowioCdramInit(void)
{
	ksceKernelEnqueueWorkQueue(0x10023, "SceCdramInit", CdramInitWorker, NULL);
	return 0;
}

uint32_t SceCdramForDriver_3EFA7540(void)
{
	while (s_cdramReady == 0)
		sceKernelDelayThread(1000);
	return 0;
}
