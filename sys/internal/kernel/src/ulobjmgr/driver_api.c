/*
 * SceUlobjMgrForDriver: direct access to a process's info array. Sony's
 * file name and the export names are unknown.
 */
#include "ulobjmgr_internal.h"

int sceUlobjMgrRetainInfoArray(SceUID pid, UlobjInfoArray **pInfoArray,
				  UlobjInfoArrayParam **pParam, UlobjProcCB **pProcCB)
{
	UlobjProcCB *procCB;

	if (pInfoArray == NULL || pParam == NULL || pProcCB == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	procCB = getAndLockUlobjInfoArrayForKernel(pid);
	if (procCB == NULL)
		return SCE_ERROR_ERRNO_ENOENT;
	*pInfoArray = procCB->infoArray;
	*pParam = &procCB->param;
	*pProcCB = procCB;
	return 0;
}

int sceUlobjMgrReleaseInfoArray(UlobjProcCB *procCB)
{
	if (procCB == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	unlockUlobjInfoArrayForKernelImpl(procCB);
	return 0;
}

int SceUlobjMgrForDriver_6F868221(SceUID pid)
{
	if (findUlobjInfoArray(pid) == NULL)
		return SCE_KERNEL_ERROR_UNSUP;
	return 0;
}

/* deci4p_dbgp sets and clears bits of a process's flag word through here */
int SceUlobjMgrForDriver_332F2E58(SceUID pid, void *dbgp, void *process, SceUInt32 set,
				  SceUInt32 clear)
{
	UlobjProcCB *procCB;
	int ret;

	procCB = getAndLockUlobjInfoArrayForKernel(pid);
	if (procCB == NULL)
		return SceSysrootForDriver_6219CC14(dbgp, process, set, clear, NULL);
	ret = SceSysrootForDriver_6219CC14(dbgp, process, set, clear,
					  &procCB->infoArray->ulobjmgrFlag);
	unlockUlobjInfoArrayForKernelImpl(procCB);
	return ret;
}
