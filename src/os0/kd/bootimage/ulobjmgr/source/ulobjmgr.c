/*
 * SceUlobjMgr: kernel support for libult's user-level objects (fibers,
 * user-level threads and their synchronisation objects).
 *
 * module_start and the process event handlers; Sony's file name for this
 * part is unknown.
 */
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/proc_event.h>
#include "ulobjmgr_internal.h"

static SceUID g_ulobjProcEventHandler;
static SceProcEventHandler g_ulobjProcEvents;

/* name unknown: the process is going away, drop its info array */
static int ulobjProcessExit(SceUID pid, SceProcEventInvokeParam1 *param, int unk)
{
	UlobjProcCB *procCB = getAndLockUlobjInfoArrayForKernel(pid);

	if (procCB != NULL) {
		freeInfoArray(procCB);
		freeAndUnlockUlobjInfoArrayForKernelImpl(procCB);
	}
	return 0;
}

/* name unknown */
static int ulobjProcessKill(SceUID pid, SceProcEventInvokeParam1 *param, int unk)
{
	UlobjProcCB *procCB = getAndLockUlobjInfoArrayForKernel(pid);

	if (procCB != NULL) {
		freeInfoArray(procCB);
		freeAndUnlockUlobjInfoArrayForKernelImpl(procCB);
	}
	return 0;
}

/* name unknown */
int registerUlobjProcessEvents(void)
{
	g_ulobjProcEvents = (SceProcEventHandler){
		.size = sizeof(SceProcEventHandler),
		.exit = ulobjProcessExit,
		.kill = ulobjProcessKill,
	};
	g_ulobjProcEventHandler = ksceKernelRegisterProcEventHandler("SceUlobjmgr",
								     &g_ulobjProcEvents, 0);
	return g_ulobjProcEventHandler & (g_ulobjProcEventHandler >> 31);
}

int module_start(SceSize args, void *argp)
{
	if (initializeUlobjProcCB() < 0)
		return SCE_KERNEL_START_FAILED;
	return SCE_KERNEL_START_SUCCESS;
}
