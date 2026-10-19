/*
 * Module start: the process local storage for each process's overlays, a
 * thread slot, the overlay lock and a handler that drops a process's
 * overlays and directory handles when it ends.
 */

#include <modulemgr/modulemgr.h>
#include <processmgr/processmgr.h>
#include <sysmem/procevent.h>
extern "C" {
#include <sysmem/debug.h>
}
#include "fios2_internal.h"

int g_fiosPlsKey = -1;
int g_fiosTlsSlot = -1;
static SceUID g_fiosProcEventId = -1;

/* name unknown */
static void fiosTlsDestructor(void *addr, uintptr_t argument)
{
}

/* the process is killed: its overlays and open directory handles go (name unknown) */
static int fiosProcessKill(SceUID pid, ProcEventParam *param, void *common)
{
	SceFiosKernelProcessStorage *storage;
	SceFiosKernelDH *dh;
	int i;

	sceFiosKernelOverlayRemoveAll(pid);
	storage = NULL;
	if (SceProcessmgrForDriver_AF80F39C(pid, g_fiosPlsKey, (void **)&storage, 0) == 0 &&
	    storage != NULL) {
		for (i = 0; i < FIOS_DH_MAX; i++) {
			dh = (SceFiosKernelDH *)storage->dh[i];
			if (dh != NULL && dh != (SceFiosKernelDH *)-1) {
				storage->dh[i] = NULL;
				sceFiosKernelDHFree(pid, dh);
			}
		}
	}
	return 0;
}

/* the process exits: the same (name unknown) */
static int fiosProcessExit(SceUID pid, ProcEventParam *param, void *common)
{
	SceFiosKernelProcessStorage *storage;
	SceFiosKernelDH *dh;
	int i;

	sceFiosKernelOverlayRemoveAll(pid);
	storage = NULL;
	if (SceProcessmgrForDriver_AF80F39C(pid, g_fiosPlsKey, (void **)&storage, 0) == 0 &&
	    storage != NULL) {
		for (i = 0; i < FIOS_DH_MAX; i++) {
			dh = (SceFiosKernelDH *)storage->dh[i];
			if (dh != NULL && dh != (SceFiosKernelDH *)-1) {
				storage->dh[i] = NULL;
				sceFiosKernelDHFree(pid, dh);
			}
		}
	}
	return 0;
}

extern "C" int sceFiosKernelInit(SceSize args, void *argp)
{
	SceKernelProcEventHandler handler;
	int ret;

	g_fiosPlsKey = sceKernelRegisterKPLS("SceFios",
							  sizeof(SceFiosKernelProcessStorage));
	if (g_fiosPlsKey < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 78, "%d\n", g_fiosPlsKey);
		g_fiosPlsKey = -1;
	}
	g_fiosTlsSlot = SceThreadmgrForDriver_B645C7EF(4, fiosTlsDestructor, 0);
	if (g_fiosTlsSlot < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 85, "%d\n", g_fiosTlsSlot);
		g_fiosTlsSlot = -1;
	}
	sceFiosKernelRwlockInit(&g_fiosOverlayLock);

	handler.size = sizeof(handler);
	handler.create = NULL;
	handler.exit = fiosProcessExit;
	handler.kill = fiosProcessKill;
	handler.suspend = NULL;
	handler.resume = NULL;
	handler.notify = NULL;
	ret = sceKernelRegisterProcEventHandler("SceFios", &handler, NULL);
	if (ret < 0) {
		sceFiosKernelRwlockTerm(&g_fiosOverlayLock);
		return SCE_KERNEL_START_FAILED;
	}
	g_fiosProcEventId = ret;
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_start(SceSize args, void *argp) __attribute__((alias("sceFiosKernelInit")));
