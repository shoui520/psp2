/* SCE device-node accessors; these originals carry no source-file record. */
#include <gpu/device_sce.h>

static PVRSRV_DEVICE_NODE *psGpuDeviceNode;

PVRSRV_DEVICE_NODE *SetGpuDeviceNode(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	psGpuDeviceNode = psDeviceNode;
	return psDeviceNode;
}

PVRSRV_DEVICE_NODE *GetGpuDeviceNode(void)
{
	return psGpuDeviceNode;
}
