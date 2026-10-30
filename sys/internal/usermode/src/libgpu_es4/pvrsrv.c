/*
 * SceGpuEs4User: connection, devices, device memory and sync objects.
 */

#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include "gpu_es4_user.h"

PVRSRV_CONNECTION g_gpuConnection;
int g_gpuConnected;

static inline GpuServices *DevServices(const PVRSRV_DEV_DATA *dev)
{
	return dev->psConnection->services;
}

struct GpuCookieIn {
	SceUInt32 bridgeFlags;		/* not set */
	SceUInt32 hDevCookie;
};

struct GpuValueIn {
	SceUInt32 bridgeFlags;
	SceUInt32 value;
};

struct GpuErrorOut {			/* PVRSRV_BRIDGE_RETURN */
	SceUInt32 error;
	void *pvData;
};

struct GpuHandleOut {
	SceUInt32 error;
	SceUInt32 handle;
};

static inline __attribute__((always_inline)) int AcquireDeviceData(PVRSRV_CONNECTION *conn, SceUInt32 index,
							      PVRSRV_DEV_DATA *dev, SceUInt32 type)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 index;
		SceUInt32 type;
	} in;
	struct GpuHandleOut out;

	in.index = index;
	in.type = type;
	if (GpuBridgeCall(conn->services, PVRSRV_BRIDGE_ACQUIRE_DEVICEINFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	dev->psConnection = conn;
	dev->hDevCookie = out.handle;
	return out.error;
}

int SceGpuEs4User_228252A2(PVRSRV_CONNECTION **pconn, SceUInt32 flags)
{
	PVRSRV_CONNECTION *conn;
	PVRSRV_DEV_DATA dev;
	int ret;

	if (pconn == NULL)
		return GPU_ERROR_INIT_FAILURE;
	if (g_gpuConnected)
		return GPU_ERROR_INVALID_PARAMS;

	*pconn = &g_gpuConnection;
	PVRSRVMemSet(&g_gpuConnection, 0, sizeof(g_gpuConnection));
	if (*pconn == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	ret = GpuServicesConnect(&(*pconn)->services, flags);
	if (ret != GPU_OK)
		return ret;
	conn = *pconn;
	g_gpuConnected = 1;
	if (conn == NULL)
		return GPU_OK;
	conn->ui32SrvFlags = flags;

	conn = *pconn;
	ret = SceGpuEs4User_4447D217(conn, &conn->numDevices, &conn->device);
	if (ret != GPU_OK)
		return ret;
	if (conn->numDevices > GPU_MAX_DEVICES)
		return GPU_ERROR_INVALID_PARAMS;
	if (conn->numDevices == 0)
		return GPU_OK;

	if (conn->device.eDeviceType == PVRSRV_DEVICE_TYPE_SGX) {
		conn->checkDevice = SgxCheckDdk;
	} else {
		conn->checkDevice = NULL;
		conn->reserved = 0;
	}

	ret = AcquireDeviceData(*pconn, conn->device.ui32DeviceIndex, &dev, 0);
	if (ret != GPU_OK)
		return ret;
	if (conn->checkDevice != NULL) {
		ret = conn->checkDevice(&dev);
		if (ret != GPU_OK)
			return ret;
	}
	return GPU_OK;
}

int SceGpuEs4User_A5B14369(PVRSRV_CONNECTION *conn)
{
	int ret;

	if (conn == NULL || !g_gpuConnected)
		return GPU_ERROR_INVALID_PARAMS;

	ret = GpuServicesDisconnect(conn->services);
	if (ret != GPU_OK)
		return ret;
	g_gpuConnected = 0;
	return GPU_OK;
}

int SceGpuEs4User_4447D217(PVRSRV_CONNECTION *conn, SceUInt32 *numDevices,
			   PVRSRV_DEVICE_IDENTIFIER *device)
{
	struct {
		SceUInt32 error;
		SceUInt32 numDevices;
		PVRSRV_DEVICE_IDENTIFIER device;
	} out;
	int ret;

	if (numDevices == NULL || device == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	if (GpuBridgeCall(conn->services, PVRSRV_BRIDGE_ENUM_DEVICES, NULL, 0, &out,
			  sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	ret = out.error;
	if (ret != GPU_OK)
		return ret;
	if (out.numDevices > GPU_MAX_DEVICES)
		return GPU_ERROR_INVALID_PARAMS;

	*numDevices = out.numDevices;
	if (out.numDevices != 0)
		*device = out.device;
	return GPU_OK;
}

int SceGpuEs4User_70800B46(PVRSRV_CONNECTION *conn, SceUInt32 index, PVRSRV_DEV_DATA *dev, SceUInt32 type)
{
	return AcquireDeviceData(conn, index, dev, type);
}

int SceGpuEs4User_1E0A9C4A(PVRSRV_DEV_DATA *dev, SceUInt32 *hContext, SceUInt32 *numHeaps,
			   PVRSRV_HEAP_INFO *heaps)
{
	struct GpuCookieIn in;
	struct {
		SceUInt32 error;
		SceUInt32 hContext;
		SceUInt32 numHeaps;
		PVRSRV_HEAP_INFO heaps[GPU_MAX_HEAPS];
	} out;

	if (dev == NULL || hContext == NULL || numHeaps == NULL || heaps == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	in.hDevCookie = dev->hDevCookie;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_CREATE_DEVMEMCONTEXT, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	*hContext = out.hContext;
	*numHeaps = out.numHeaps;
	if (out.numHeaps != 0)
		PVRSRVMemCopy(heaps, out.heaps, out.numHeaps * sizeof(out.heaps[0]));
	return GPU_OK;
}

int SceGpuEs4User_B9178277(PVRSRV_DEV_DATA *dev, SceUInt32 hContext)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SceUInt32 hContext;
	} in;
	struct GpuErrorOut out;

	if (dev == NULL || hContext == 0)
		return GPU_ERROR_INVALID_PARAMS;

	in.hDevCookie = dev->hDevCookie;
	in.hContext = hContext;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_DESTROY_DEVMEMCONTEXT, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

struct GpuFreeDeviceMemIn {
	SceUInt32 bridgeFlags;
	SceUInt32 hDevCookie;
	SceUInt32 hKernelMemInfo;
	PVRSRV_CLIENT_MEM_INFO sClientMemInfo;
};

int SceGpuEs4User_071E62F0(PVRSRV_DEV_DATA *dev, SceUInt32 hHeap, SceUInt32 flags, SceUInt32 size,
			   SceUInt32 alignment, SceUInt32 hMemBlockProcRef,
			   PVRSRV_CLIENT_MEM_INFO **memInfo)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SceUInt32 hHeap;
		SceUInt32 flags;
		SceUInt32 size;
		SceUInt32 alignment;
		SceUInt32 hMemBlockProcRef;
	} in;
	struct {
		SceUInt32 error;
		SceUInt32 hKernelMemInfo;
		PVRSRV_CLIENT_MEM_INFO memInfo;
	} out;
	struct GpuFreeDeviceMemIn freeIn;
	struct GpuErrorOut freeOut;
	PVRSRV_CLIENT_MEM_INFO *info;

	if (dev == NULL || memInfo == NULL)
		return GPU_ERROR_INVALID_PARAMS;
	if (!(flags & GPU_ALLOC_REQUIRED))
		return GPU_ERROR_INVALID_FLAGS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.hDevCookie = dev->hDevCookie;
	in.hHeap = hHeap;
	in.flags = flags & ~GPU_ALLOC_NO_CPU_MAPPING;
	in.size = size;
	in.alignment = alignment;
	in.hMemBlockProcRef = hMemBlockProcRef;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_ALLOC_DEVICEMEM, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	info = SceGpuEs4User_7AF40138(sizeof(*info));
	if (info == NULL) {
		/* give the allocation back */
		freeIn.hDevCookie = dev->hDevCookie;
		freeIn.hKernelMemInfo = out.memInfo.hKernelMemInfo;
		GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_FREE_DEVICEMEM, &freeIn, sizeof(freeIn),
			      &freeOut, sizeof(freeOut));
		*memInfo = info;
		return GPU_ERROR_OUT_OF_MEMORY;
	}

	*info = out.memInfo;
	if (flags & GPU_ALLOC_NO_CPU_MAPPING)
		info->pvLinAddr = NULL;
	*memInfo = info;
	return GPU_OK;
}

int SceGpuEs4User_B6CFD213(PVRSRV_DEV_DATA *dev, PVRSRV_CLIENT_MEM_INFO *memInfo)
{
	struct GpuFreeDeviceMemIn in;
	struct {
		SceUInt32 error;
		void *pvData;
		SceUInt32 reserved;
	} out;

	if (dev == NULL || memInfo == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	if (memInfo->pvLinAddr != NULL)
		memInfo->pvLinAddr = NULL;
	in.hDevCookie = dev->hDevCookie;
	in.hKernelMemInfo = memInfo->hKernelMemInfo;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_FREE_DEVICEMEM, &in, sizeof(in),
			  &out, 8) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	SceGpuEs4User_1BE5985A(memInfo);
	return out.error;
}

int SceGpuEs4User_73E945A1(PVRSRV_CONNECTION *conn, PVRSRV_MISC_INFO *info)
{
	struct {
		SceUInt32 bridgeFlags;
		PVRSRV_MISC_INFO info;
	} in;
	struct {
		SceUInt32 error;
		PVRSRV_MISC_INFO info;
	} out;

	if (info == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	out.error = GPU_OK;
	if (info->ui32StateRequest != 0) {
		PVRSRVMemCopy(&in.info, info, sizeof(in.info));
		if (GpuBridgeCall(conn->services, PVRSRV_BRIDGE_GET_MISC_INFO, &in, sizeof(in),
				  &out, sizeof(out)) != GPU_OK)
			return GPU_ERROR_BRIDGE_CALL_FAILED;
		if (out.error != GPU_OK)
			return out.error;
		PVRSRVMemCopy(info, &out.info, sizeof(*info));
	}
	return GPU_OK;
}

int SceGpuEs4User_F4735A47(void)
{
	return GPU_OK;
}

int SceGpuEs4User_416C8813(PVRSRV_CONNECTION *conn, SceUInt32 *hModObj)
{
	struct GpuHandleOut out;

	if (conn == NULL || conn->services == NULL || hModObj == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	if (GpuBridgeCall(conn->services, PVRSRV_BRIDGE_CREATE_SYNC_INFO_MOD_OBJ, NULL, 0,
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error == GPU_ERROR_RETRY)
		return out.error;
	if (out.error != GPU_OK)
		return out.error;

	*hModObj = out.handle;
	return out.error;
}

int SceGpuEs4User_7DCCBCAF(PVRSRV_CONNECTION *conn, SceUInt32 hModObj)
{
	struct GpuValueIn in;
	struct GpuErrorOut out;

	if (conn == NULL || conn->services == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	in.value = hModObj;
	if (GpuBridgeCall(conn->services, PVRSRV_BRIDGE_DESTROY_SYNC_INFO_MOD_OBJ, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_0D27286F(PVRSRV_CONNECTION *conn, SceUInt32 hModObj, const SceUInt32 *hSyncInfos,
			   int count, SceUInt32 flags, SceUInt32 *writeOps, SceUInt32 *readOps)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hModObj;
		SceUInt32 hSyncInfos[GPU_MAX_SYNC_OPS];
		SceUInt32 flags;
		SceUInt32 count;
	} in;
	struct {
		SceUInt32 error;
		SceUInt32 writeOps;
		SceUInt32 readOps;
	} out;
	GpuServices *services;

	if (conn == NULL || (services = conn->services) == NULL || hSyncInfos == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	in.hModObj = hModObj;
	if (count > 0)
		PVRSRVMemCopy(in.hSyncInfos, hSyncInfos, count * sizeof(in.hSyncInfos[0]));
	in.flags = flags;
	in.count = count;
	if (GpuBridgeCall(services, PVRSRV_BRIDGE_MODIFY_PENDING_SYNC_OPS, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error == GPU_ERROR_RETRY)
		return out.error;
	if (out.error != GPU_OK)
		return out.error;

	if (writeOps != NULL)
		*writeOps = out.writeOps;
	if (readOps != NULL)
		*readOps = out.readOps;
	return GPU_OK;
}

int SceGpuEs4User_116449D2(PVRSRV_CONNECTION *conn, SceUInt32 hModObj)
{
	struct GpuValueIn in;
	struct GpuErrorOut out;

	if (conn == NULL || conn->services == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	in.value = hModObj;
	if (GpuBridgeCall(conn->services, PVRSRV_BRIDGE_MODIFY_COMPLETE_SYNC_OPS, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_AAD85460(PVRSRV_CONNECTION *conn, SceUInt32 hModObj, SceUInt32 wait)
{
	struct GpuValueIn in;
	struct GpuErrorOut out;

	if (conn == NULL || conn->services == NULL || wait != 0)
		return GPU_ERROR_INVALID_PARAMS;

	in.value = hModObj;
	if (GpuBridgeCall(conn->services, PVRSRV_BRIDGE_SYNC_OPS_FLUSH_TO_MOD_OBJ, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_5C3A6DFE(SceUInt32 hModObj, SceUInt32 timeout)
{
	struct GpuValueIn in;
	struct {
		SceUInt32 error;
		SceUID eventFlag;
		SceUInt32 pattern;
	} out;

	in.value = hModObj;
	if (GpuBridgeCall(g_gpuConnection.services, PVRSRV_BRIDGE_CHECK_SYNC_INFO_MOD_OBJ, &in,
			  sizeof(in), &out, sizeof(out)) != GPU_OK)
		return SCE_GPU_ERROR_BRIDGE;
	if (out.error != SCE_GPU_ERROR_WOULD_BLOCK)
		return out.error;

	return sceKernelWaitEventFlag(out.eventFlag, out.pattern,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL,
				      timeout != 0 ? &timeout : NULL);
}

int SceGpuEs4User_94286660(PVRSRV_DEV_DATA *dev, SceUInt32 *hSyncInfo)
{
	struct GpuCookieIn in;
	struct GpuHandleOut out;

	if (dev == NULL || hSyncInfo == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.hDevCookie = dev->hDevCookie;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_ALLOC_SYNC_INFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	*hSyncInfo = out.handle;
	return out.error;
}

int SceGpuEs4User_B26DF3D2(PVRSRV_DEV_DATA *dev, SceUInt32 hSyncInfo)
{
	struct GpuValueIn in;
	struct GpuErrorOut out;

	if (dev == NULL || hSyncInfo == 0)
		return GPU_ERROR_INVALID_PARAMS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.value = hSyncInfo;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_FREE_SYNC_INFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_56893E98(PVRSRV_DEV_DATA *dev, SceUInt32 hDevMemContext, SceUInt32 hDevMemHeap,
			   SceSize size, SceSize allocationSize, void *cpuAddress, SceUInt32 flags,
			   SceUInt32 *deviceAddress)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SceUInt32 hDevMemContext;
		SceUInt32 hDevMemHeap;
		void *cpuAddress;
		SceSize size;
		SceSize allocationSize;
		SceUInt32 flags;
	} in;
	struct GpuHandleOut out;

	if (dev == NULL || cpuAddress == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	in.hDevCookie = dev->hDevCookie;
	in.hDevMemContext = hDevMemContext;
	in.hDevMemHeap = hDevMemHeap;
	in.cpuAddress = cpuAddress;
	in.size = size;
	in.allocationSize = allocationSize;
	in.flags = flags;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_MAP_MEMORY_TO_GPU, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (deviceAddress != NULL)
		*deviceAddress = out.handle;
	return out.error;
}

int SceGpuEs4User_AD20D1A0(PVRSRV_DEV_DATA *dev, void *cpuAddress, SceUInt32 hDevMemHeap,
			   SceUInt32 waitForIdle)
{
	struct {
		SceUInt32 bridgeFlags;
		void *cpuAddress;
		SceUInt32 hDevMemHeap;
		SceUInt32 waitForIdle;
	} in;
	struct GpuErrorOut out;

	if (dev == NULL || cpuAddress == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	in.cpuAddress = cpuAddress;
	in.hDevMemHeap = hDevMemHeap;
	in.waitForIdle = waitForIdle;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_UNMAP_MEMORY_FROM_GPU, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4UserForVsh_3C09BFCA(PVRSRV_DEV_DATA *dev, SceUInt32 sharedKey, SceUInt32 hSyncInfo)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 sharedKey;
		SceUInt32 hSyncInfo;
	} in;
	struct GpuErrorOut out;

	if (dev == NULL || sharedKey == 0 || hSyncInfo == 0)
		return GPU_ERROR_INVALID_PARAMS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.sharedKey = sharedKey;
	in.hSyncInfo = hSyncInfo;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_CREATE_SHARED_SYNC_INFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4UserForVsh_8A27AB0A(PVRSRV_DEV_DATA *dev, SceUInt32 sharedKey, SceUInt32 *hSyncInfo)
{
	struct GpuValueIn in;
	struct GpuHandleOut out;

	if (dev == NULL || sharedKey == 0 || hSyncInfo == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.value = sharedKey;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_OPEN_SHARED_SYNC_INFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	*hSyncInfo = out.handle;
	return out.error;
}

int SceGpuEs4UserForVsh_0A9556D9(PVRSRV_DEV_DATA *dev, SceUInt32 sharedKey, SceUInt32 hSyncInfo)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 sharedKey;
		SceUInt32 hSyncInfo;
	} in;
	struct GpuErrorOut out;

	if (dev == NULL || sharedKey == 0 || hSyncInfo == 0)
		return GPU_ERROR_INVALID_PARAMS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.sharedKey = sharedKey;
	in.hSyncInfo = hSyncInfo;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_CLOSE_SHARED_SYNC_INFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4UserForVsh_010DA2CB(PVRSRV_DEV_DATA *dev, SceUInt32 sharedKey)
{
	struct GpuValueIn in;
	struct GpuErrorOut out;

	if (dev == NULL || sharedKey == 0)
		return GPU_ERROR_INVALID_PARAMS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.value = sharedKey;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_DELETE_SHARED_SYNC_INFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_0EA7458D(PVRSRV_DEV_DATA *dev, SceUID memBlockUid, SceUInt32 *blockGroupId,
			   SceUInt32 restoreMapping)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUID memBlockUid;
		SceUInt32 blockGroupId;
		SceUInt32 restoreMapping;
	} in;
	struct GpuHandleOut out;

	if (dev == NULL || memBlockUid == 0 || blockGroupId == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.memBlockUid = memBlockUid;
	in.blockGroupId = *blockGroupId;
	in.restoreMapping = restoreMapping;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_REGISTER_MEMBLOCK, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	*blockGroupId = out.handle;
	return out.error;
}

int SceGpuEs4User_156A6B70(PVRSRV_DEV_DATA *dev, SceUID memBlockUid)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUID memBlockUid;
		SceUInt32 reserved[2];
	} in;
	struct GpuErrorOut out;

	if (dev == NULL || memBlockUid == 0)
		return GPU_ERROR_INVALID_PARAMS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.memBlockUid = memBlockUid;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_UNREGISTER_MEMBLOCK, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4UserForVsh_3DC96093(PVRSRV_DEV_DATA *dev, SceUInt32 hDevMemContext,
				 SceUInt32 deviceAddress, SceSize size, SceUInt32 access)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevMemContext;
		SceUInt32 deviceAddress;
		SceSize size;
		SceUInt32 access;
	} in;
	struct GpuErrorOut out;

	if (dev == NULL || hDevMemContext == 0)
		return GPU_ERROR_INVALID_PARAMS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.hDevMemContext = hDevMemContext;
	in.deviceAddress = deviceAddress;
	in.size = size;
	in.access = access;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_CHECK_GPU_MAPPING, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_9A4A5DAE(PVRSRV_DEV_DATA *dev, void *cpuAddress, GpuMappedMemoryInfo *info)
{
	struct {
		SceUInt32 bridgeFlags;
		void *cpuAddress;
	} in;
	struct {
		SceUInt32 error;
		GpuMappedMemoryInfo info;
	} out;

	if (dev == NULL || info == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.cpuAddress = cpuAddress;
	if (GpuBridgeCall(DevServices(dev), PVRSRV_BRIDGE_GET_MAPPED_MEMORY_INFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	*info = out.info;
	return out.error;
}
