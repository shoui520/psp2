/* SceCdram asynchronous secure-monitor initialization. */
#include "lowio_internal.h"
#include <psp2kern/kernel/threadmgr.h>
#include <sce/threadmgr.h>
#include <sce/touch_dummy.h>

uint32_t Smc117(uint32_t command);
volatile uint32_t s_cdramReady;

/* name unknown: runs SMC 0x117 once, from the work queue, on cores 1-2 */
static int CdramInitWorker(void *arg)
{
	int priority, affinity;

	(void)arg;
	priority = ksceKernelChangeThreadPriority(0, 16);
	if (priority < 0)
		priority = 0xbf;
	affinity = ksceKernelChangeThreadCpuAffinityMask(0, 6);
	if (affinity < 0)
		affinity = 0xf;
	Smc117(0);
	s_cdramReady = 1;
	ksceKernelChangeThreadCpuAffinityMask(0, affinity);
	ksceKernelChangeThreadPriority(0, priority);
	return 1;
}

int LowioCdramInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	ksceKernelEnqueueWorkQueue(0x10023, "SceCdramInit", CdramInitWorker, NULL);
	return 0;
}

uint32_t SceCdramForDriver_3EFA7540(void)
{
	while (s_cdramReady == 0)
		sceKernelDelayThread(1000);
	return 0;
}
