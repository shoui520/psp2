/*
 * SceGpuEs4Init's entry: the init server's main (IMG srvinit's SrvInit). It
 * connects as the init process, runs the SGX initialisation for every SGX
 * device (devices/sgx/sgxinit.c) and finishes the services' initialisation.
 */

#include <modulemgr/modulemgr.h>
#include <sysmem/debug.h>
#include "../gpuinit_es4.h"

SceKernelModuleInfo moduleInfo __attribute__((aligned(16)));

int module_start(SceSize args, void *argp)
{
	static const char fallbackName[] = "SceGpuInit";
	PVRSRV_DEVICE_IDENTIFIER asDevID[1];
	PVRSRV_PER_PROCESS_DATA *process;
	IMG_UINT32 count, pid, i;
	PVRSRV_ERROR ret, final;

	if (SceModulemgrForDriver_1D9E0F7E((SceUInt32)&moduleInfo, &moduleInfo) < 0) {
		for (i = 0; i < sizeof(fallbackName); i++)
			moduleInfo.module_name[i] = fallbackName[i];
	}
	if (!OSProcHasPrivSrvInit() ||
	    PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_RUNNING) ||
	    PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_RAN))
		return PVRSRV_ERROR_INIT_FAILURE;

	pid = OSGetCurrentProcessIDKM();
	ret = PVRSRVPerProcessDataConnect(pid, 0);
	if (ret != PVRSRV_OK) {
		SceDebugForDriver_391B74B7("%s: Error %08x with %s\n", "SrvInit", ret, "PVRSRVPerProcessDataConnect");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	process = PVRSRVPerProcessData(pid);
	if (!process) {
		SceDebugForDriver_391B74B7("%s: Failed to get process data\n", "SrvInit");
		ret = PVRSRV_ERROR_INVALID_PERPROC;
	} else {
		process->bInitProcess = IMG_TRUE;
		ret = PVRSRVEnumerateDevicesKM(&count, asDevID);
		if (ret != PVRSRV_OK) {
			PVRSRVPerProcessDataDisconnect(pid);
			SceDebugForDriver_391B74B7("%s: Error %08x with %s\n", "SrvInit", ret, "PVRSRVEnumerateDevicesKM");
		} else {
			for (i = 0; i < count; i++) {
				if (asDevID[i].eDeviceClass != PVRSRV_DEVICE_CLASS_3D ||
				    asDevID[i].eDeviceType != PVRSRV_DEVICE_TYPE_SGX)
					continue;
				ret = SGXInitialisePart2KM(process, &asDevID[i]);
				if (ret != PVRSRV_OK)
					break;
			}
		}
	}
	/* Sony clears it even when there is no process data */
	process->bInitProcess = IMG_FALSE;
	PVRSRVSetInitServerState(PVRSRV_INIT_SERVER_RUNNING, IMG_FALSE);
	PVRSRVSetInitServerState(PVRSRV_INIT_SERVER_RAN, IMG_TRUE);
	final = PVRSRVFinaliseSystem(ret == PVRSRV_OK);
	PVRSRVSetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL, final == PVRSRV_OK && ret == PVRSRV_OK);
	return final != PVRSRV_OK ? SCE_KERNEL_START_FAILED : SCE_KERNEL_START_NO_RESIDENT;
}
