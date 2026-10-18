/*
 * SceGpuEs4User: the services connection and the bridge into gpu_es4.skprx.
 */

#include <kernel/threadmgr.h>
#include "gpu_es4_user.h"

#define GPU_BRIDGE_RETRY_DELAY	201	/* microseconds */

GpuServices g_gpuServices;

struct GpuConnectServicesIn {
	SceUInt32 bridgeFlags;		/* not set */
	SceUInt32 flags;
};

struct GpuConnectServicesOut {
	SceUInt32 error;		/* not checked */
	SceUInt32 hServices;
};

/* name unknown: open the services and register this process */
int GpuServicesConnect(GpuServices **services, SceUInt32 flags)
{
	struct GpuConnectServicesIn in;
	struct GpuConnectServicesOut out;
	struct SceGpuBridgePackage pkg;
	int ret;

	if (PVRSRVOpen() < 0)
		return GPU_ERROR_INIT_FAILURE;

	g_gpuServices.hServices = 0;
	in.flags = flags;
	pkg.id = PVRSRV_BRIDGE_CONNECT_SERVICES;
	pkg.size = sizeof(pkg);
	pkg.in = &in;
	pkg.in_size = sizeof(in);
	pkg.out = &out;
	pkg.out_size = sizeof(out);
	pkg.h_services = 0;
	while ((ret = PVRSRV_BridgeDispatchKM(PVRSRV_BRIDGE_CONNECT_SERVICES, &pkg)) == GPU_ERROR_RETRY)
		sceKernelDelayThread(GPU_BRIDGE_RETRY_DELAY);
	if (ret != GPU_OK)
		return GPU_ERROR_INIT_FAILURE;

	g_gpuServices.hServices = out.hServices;
	*services = &g_gpuServices;
	return GPU_OK;
}

/* name unknown */
int GpuServicesDisconnect(GpuServices *services)
{
	struct GpuConnectServicesOut out;
	struct SceGpuBridgePackage pkg;

	if (services == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	pkg.id = PVRSRV_BRIDGE_DISCONNECT_SERVICES;
	pkg.size = sizeof(pkg);
	pkg.in = NULL;
	pkg.in_size = 0;
	pkg.out = &out;
	pkg.out_size = sizeof(out);
	pkg.h_services = services->hServices;
	while (PVRSRV_BridgeDispatchKM(PVRSRV_BRIDGE_DISCONNECT_SERVICES, &pkg) == GPU_ERROR_RETRY)
		sceKernelDelayThread(GPU_BRIDGE_RETRY_DELAY);
	PVRSRVRelease();
	return GPU_OK;
}

/* name unknown: one bridge call, repeated while the kernel is busy */
int GpuBridgeCall(GpuServices *services, SceUInt32 id, void *in, SceSize inSize, void *out,
		  SceSize outSize)
{
	struct SceGpuBridgePackage pkg;
	int ret;

	pkg.id = id;
	pkg.size = sizeof(pkg);
	pkg.in = in;
	pkg.in_size = inSize;
	pkg.out = out;
	pkg.out_size = outSize;
	pkg.h_services = services->hServices;
	while ((ret = PVRSRV_BridgeDispatchKM(id, &pkg)) == GPU_ERROR_RETRY)
		sceKernelDelayThread(GPU_BRIDGE_RETRY_DELAY);
	return ret;
}
