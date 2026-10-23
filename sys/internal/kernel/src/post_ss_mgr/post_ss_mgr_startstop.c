/*
 * post_ss_mgr_startstop.c: module start.
 */

#include "post_ss_mgr_internal.h"

#define POST_SS_MGR_HEAP_SIZE	0x1000

SceUID g_postSsMgrHeap;
SceKernelFastMutex g_postSsMgrKeysMutex;
SceKernelFastMutex g_postSsMgrSdMutex;
SceKernelFastMutex g_postSsMgrCloudMutex;

/* the work queued at start: what needs the file system */
static int postSsMgrInitWork(void *arg)	/* name unknown */
{
	sceSblLicMgrInit();
	sceSblUtMgrInit();
	return 1;
}

static int postSsMgrRandom(void *buf, SceSize size)	/* name unknown */
{
	return sceSblRngPseudoRandomNumber(buf, size);
}

int _sceSblPostSsMgrInitPhase1(SceSize args, void *argp)
{
	SceKernelHeapCreateOpt opt;

	opt.size = sizeof(opt);
	opt.attr = 1;
	g_postSsMgrHeap = sceKernelCreateHeap("SceSblPostSsMgr", POST_SS_MGR_HEAP_SIZE, &opt);
	if (g_postSsMgrHeap < 0) {
		SCE_DBG_PRINTF(0, 81, "sceKernelCreateHeap() failure\n");
		return 1;
	}
	sceKernelInitializeFastMutex(&g_postSsMgrKeysMutex, "SceSblSsMgr", 0, 0);
	sceKernelInitializeFastMutex(&g_postSsMgrSdMutex, "SceSblSsMgr", 0, 0);
	sceKernelInitializeFastMutex(&g_postSsMgrCloudMutex, "SceSblSsMgr", 0, 0);
	sceSblPmMgrInit();
	sceSblRtcMgrInit();
	sceSblFwLoaderInit();
	sceSblRsaSetRandom(postSsMgrRandom);
	sceKernelScheduleWorkTask(0x10023, "SceSblPostSsMgrInit", postSsMgrInitWork, NULL);
	return 0;
}

int module_start(SceSize args, void *argp) __attribute__((alias("_sceSblPostSsMgrInitPhase1")));
