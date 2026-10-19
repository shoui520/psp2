/* SceCdram asynchronous secure-monitor initialization. */
#include "lowio_internal.h"
#include <threadmgr/threadmgr.h>
#include <touch_dummy/touch_dummy.h>
#include <lowio/cdram.h>

uint32_t Smc117(uint32_t command);
static volatile uint32_t s_cdramReady;

/* name unknown: runs SMC 0x117 once, from the work queue, on cores 1-2 */
static int CdramInitWorker(void *arg)
{
	int priority, affinity;

	(void)arg;
	priority = sceKernelChangeThreadPriority2(0, 16);
	if (priority < 0)
		priority = 0xbf;
	affinity = sceKernelChangeThreadCpuAffinityMask(0, 6);
	if (affinity < 0)
		affinity = 0xf;
	Smc117(0);
	s_cdramReady = 1;
	sceKernelChangeThreadCpuAffinityMask(0, affinity);
	sceKernelChangeThreadPriority2(0, priority);
	return 1;
}

int LowioCdramInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	sceKernelScheduleWorkTask(0x10023, "SceCdramInit", CdramInitWorker, NULL);
	return 0;
}

uint32_t SceCdramForDriver_3EFA7540(void)
{
	while (s_cdramReady == 0)
		sceKernelDelayThread(1000);
	return 0;
}
