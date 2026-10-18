/*
 * SceGpuEs4User: SGX bridge calls (client info, contexts, render targets,
 * scene kicks, transfers).
 *
 * Every input record starts with a word the client leaves unset; most
 * continue with the device cookie. Every output record starts with the
 * kernel's result code.
 */

#include <libkernel/libkernel.h>
#include "gpu_es4_user.h"

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

int SceGpuEs4User_B000CA65(PVRSRV_DEV_DATA *dev, GpuClientInfo *info)
{
	struct GpuCookieIn in;
	struct {
		SGX_CLIENT_INFO sClientInfo;
		SceUInt32 error;
	} out;
	int ret;

	if (dev == NULL || info == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	PVRSRVMemSet(&out, 0, sizeof(out));
	in.hDevCookie = dev->hDevCookie;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_GETCLIENTINFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	ret = out.error;
	if (ret != GPU_OK)
		return ret;

	info->pvProcess = out.sClientInfo.pvProcess;
	PVRSRVMemCopy(&info->sMiscInfo, &out.sClientInfo.sMiscInfo, sizeof(info->sMiscInfo));
	info->asDevData[0] = out.sClientInfo.asDevData[4];
	info->asDevData[1] = out.sClientInfo.asDevData[5];
	info->asDevData[2] = out.sClientInfo.asDevData[6];
	info->asDevData[3] = out.sClientInfo.asDevData[7];
	info->asDevData[4] = out.sClientInfo.asDevData[8];
	info->asDevData[5] = out.sClientInfo.asDevData[9];
	return ret;
}

int SceGpuEs4User_5273D097(PVRSRV_DEV_DATA *dev, const GpuClientInfo *info)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SGX_CLIENT_INFO sClientInfo;
	} in;
	struct GpuErrorOut out;
	PVRSRV_CONNECTION *conn;

	if (dev == NULL || info == NULL || (conn = dev->psConnection) == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	in.hDevCookie = dev->hDevCookie;
	in.sClientInfo.pvProcess = info->pvProcess;
	PVRSRVMemCopy(&in.sClientInfo.sMiscInfo, &info->sMiscInfo, sizeof(in.sClientInfo.sMiscInfo));
	if (GpuBridgeCall(conn->services, SGX_BRIDGE_RELEASECLIENTINFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

/* name unknown: PVRSRV_CONNECTION.checkDevice of SGX devices, compares the kernel's DDK */
int SgxCheckDdk(PVRSRV_DEV_DATA *dev)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SGX_MISC_INFO *info;
	} in;
	struct GpuErrorOut out;
	SGX_MISC_INFO info;

	in.hDevCookie = dev->hDevCookie;
	in.info = &info;
	info.eRequest = SGX_MISC_INFO_REQUEST_DRIVER_SGXREV;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_GETMISCINFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	if (info.uData.sSGXFeatures.ui32BuildOptions != GPU_DDK_BUILD_OPTIONS)
		return GPU_ERROR_BUILD_OPTIONS;
	if (info.uData.sSGXFeatures.ui32DDKVersion != GPU_DDK_VERSION)
		return GPU_ERROR_DDK_VERSION;
	if (info.uData.sSGXFeatures.ui32DDKBuild != GPU_DDK_BUILD)
		return GPU_ERROR_DDK_VERSION;
	return GPU_OK;
}

int SceGpuEs4User_09BB98B2(PVRSRV_DEV_DATA *dev, SGX_MISC_INFO *info)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SGX_MISC_INFO *info;
	} in;
	struct GpuErrorOut out;
	const GpuRTDataSet *rtDataSet;

	if (info == NULL)
		return GPU_ERROR_INVALID_PARAMS;

	if (info->eRequest == SGX_MISC_INFO_REQUEST_SPM) {
		rtDataSet = info->uData.sSPM.hRTDataSet;
		if (rtDataSet == NULL)
			return GPU_ERROR_INVALID_PARAMS;
		info->uData.sSPM.ui32NumOutOfMemSignals = rtDataSet->spm->ui32NumOutOfMemSignals;
		info->uData.sSPM.ui32NumSPMRenders = rtDataSet->spm->ui32NumSPMRenders;
		return GPU_ERROR_INVALID_PARAMS;
	}

	in.hDevCookie = dev->hDevCookie;
	in.info = info;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_GETMISCINFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_8A997DBB(PVRSRV_DEV_DATA *dev, const SGX_CREATERENDERCONTEXT *params,
			   SceUInt32 *hRenderContext,
			   PVRSRV_CLIENT_MEM_INFO **visTestResultMemInfo)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SGX_CREATERENDERCONTEXT params;
	} in;
	struct GpuHandleOut out;

	in.hDevCookie = dev->hDevCookie;
	in.params = *params;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_CREATE_RENDER_CONTEXT, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	*hRenderContext = out.handle;
	*visTestResultMemInfo = NULL;
	return out.error;
}

int SceGpuEs4User_0BFE1685(PVRSRV_DEV_DATA *dev, SceUInt32 hRenderContext, SceUInt32 unused,
			   SceUInt32 forceCleanup)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hRenderContext;
		SceUInt32 forceCleanup;
	} in;
	struct GpuErrorOut out;

	in.hRenderContext = hRenderContext;
	in.forceCleanup = forceCleanup;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_DESTROY_RENDER_CONTEXT, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_0162A405(PVRSRV_DEV_DATA *dev, const SGX_ADDRENDTARG *target,
			   SceUInt32 *hRenderTarget)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SGX_ADDRENDTARG target;
	} in;
	struct GpuHandleOut out;

	in.hDevCookie = dev->hDevCookie;
	PVRSRVMemCopy(&in.target, target, sizeof(in.target));
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_ADD_RENDER_TARGET, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	*hRenderTarget = out.handle;
	return out.error;
}

int SceGpuEs4User_D90B0707(PVRSRV_DEV_DATA *dev, SceUInt32 hRenderContext, SceUInt32 hRenderTarget)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SceUInt32 hRenderContext;
		SceUInt32 hRenderTarget;
	} in;
	struct GpuErrorOut out;

	in.hDevCookie = dev->hDevCookie;
	in.hRenderContext = hRenderContext;
	in.hRenderTarget = hRenderTarget;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_REMOVE_RENDER_TARGET, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_F0CCE9DB(PVRSRV_DEV_DATA *dev, const SGX_KICKTA *kick, SGX_KICKTA_OUTPUT *output)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SGX_KICKTA kick;
	} in;
	struct {
		SceUInt32 error;
		SGX_KICKTA_OUTPUT output;
	} out;
	int ret;

	in.hDevCookie = dev->hDevCookie;
	PVRSRVMemCopy(&in.kick, kick, sizeof(in.kick));
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_DOKICK, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	ret = out.error;
	if (ret != GPU_OK)
		return ret;

	*output = out.output;
	return ret;
}

int SceGpuEs4User_BDE2B9E1(PVRSRV_DEV_DATA *dev, SceUInt32 reservedParameter,
			   const GpuTransferMemoryContext *memoryContext, SceUInt32 *hContext)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SceUInt32 reservedParameter;
		GpuTransferMemoryContext memoryContext;
	} in;
	struct GpuHandleOut out;

	in.hDevCookie = dev->hDevCookie;
	in.reservedParameter = reservedParameter;
	in.memoryContext = *memoryContext;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_CREATE_TRANSFER_QUEUE, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	*hContext = out.handle;
	return out.error;
}

int SceGpuEs4User_8ED7D698(PVRSRV_DEV_DATA *dev, SceUInt32 hContext, SceUInt32 forceCleanup)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hContext;
		SceUInt32 forceCleanup;
	} in;
	struct GpuErrorOut out;

	in.hContext = hContext;
	in.forceCleanup = forceCleanup;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_DESTROY_TRANSFER_QUEUE, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_5564C1BE(const SceUInt32 *commands, SceUInt32 numCommands, PVRSRV_DEV_DATA *dev,
			   SceUInt32 hContext, SceUInt32 hSyncInfo, SceUInt32 sourceSyncEnable,
			   SceUInt32 destinationSyncEnable, const GpuTransferStatus *status)
{
	struct {
		SceUInt32 bridgeFlags;
		SceUInt32 hDevCookie;
		SceUInt32 commands[GPU_MAX_TRANSFER_COMMANDS];
		SceUInt32 numCommands;
		SceUInt32 hContext;
		SceUInt32 hSyncInfo;
		SceUInt32 sourceSyncEnable;
		SceUInt32 destinationSyncEnable;
		GpuTransferStatus status;
		SceUInt32 statusEnabled;
	} in;
	struct GpuErrorOut out;
	SceUInt32 statusEnabled = 0;

	if (numCommands > GPU_MAX_TRANSFER_COMMANDS)
		return GPU_ERROR_INVALID_PARAMS;

	in.hDevCookie = dev->hDevCookie;
	GpuMemCopyCommands(in.commands, commands, numCommands * sizeof(in.commands[0]));
	in.numCommands = numCommands;
	in.hContext = hContext;
	in.hSyncInfo = hSyncInfo;
	in.sourceSyncEnable = sourceSyncEnable;
	in.destinationSyncEnable = destinationSyncEnable;
	if (status != NULL) {
		in.status = *status;
		statusEnabled = 1;
	}
	in.statusEnabled = statusEnabled;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_SUBMIT_TRANSFER_COMMANDS, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_7E965369(PVRSRV_DEV_DATA *dev, SceUInt32 hContext)
{
	struct GpuValueIn in;
	struct GpuErrorOut out;

	in.value = hContext;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_WAIT_TRANSFER_QUEUE_IDLE, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return out.error;
}

int SceGpuEs4User_9E8E2957(PVRSRV_DEV_DATA *dev, SceUInt32 arg, SceUInt32 *result)
{
	struct GpuValueIn in;
	struct GpuHandleOut out;

	in.value = arg;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_UNSUPPORTED, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	*result = out.handle;
	return out.error;
}

int SceGpuEs4User_804BA13D(PVRSRV_DEV_DATA *dev, SceUInt32 hRenderTarget, SceUID *dataMemblockUid,
			   SceUInt32 *useExternalUid)
{
	struct GpuValueIn in;
	struct {
		SceUInt32 error;
		SceUID dataMemblockUid;
		SceUInt32 useExternalUid;
	} out;

	in.value = hRenderTarget;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_GET_RENDER_TARGET_DRIVER_MEMBLOCK, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	if (out.error != GPU_OK)
		return out.error;

	*dataMemblockUid = out.dataMemblockUid;
	if (useExternalUid != NULL)
		*useExternalUid = out.useExternalUid;
	return GPU_OK;
}

int SceGpuEs4User_CCEE0336(PVRSRV_DEV_DATA *dev, SceUInt32 hRenderTarget,
			   SGX_RENDER_TARGET_DETAILS *details)
{
	struct GpuValueIn in;
	struct {
		SceUInt32 error;
		SGX_RENDER_TARGET_DETAILS details;
	} out;
	int ret;

	in.value = hRenderTarget;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_GET_RENDER_TARGET_DETAILS, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	ret = out.error;
	if (ret != GPU_OK)
		return ret;

	PVRSRVMemCopy(details, &out.details, sizeof(*details));
	return ret;
}

int SceGpuEs4User_83D972DF(PVRSRV_DEV_DATA *dev, SceUInt32 hRenderTarget,
			   SGX_RENDER_TARGET_INFO *info)
{
	struct GpuValueIn in;
	struct {
		SceUInt32 error;
		SGX_RENDER_TARGET_INFO info;
	} out;
	int ret;

	in.value = hRenderTarget;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_GET_RENDER_TARGET_INFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	ret = out.error;
	if (ret != GPU_OK)
		return ret;

	*info = out.info;
	return ret;
}

int SceGpuEs4User_768482F6(PVRSRV_DEV_DATA *dev, GpuTraceInfo *info)
{
	SceUInt32 in;			/* bridge flags only, not set */
	struct {
		SceUInt32 error;
		GpuTraceInfo info;
	} out;
	int ret;

	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_GET_TRACE_INFO, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	ret = out.error;
	if (ret != GPU_OK)
		return ret;

	*info = out.info;
	return ret;
}

int SceGpuEs4User_C0D16CE5(PVRSRV_DEV_DATA *dev, GpuTraceCounters *counters)
{
	SceUInt32 in;			/* the device cookie */
	struct {
		SceUInt32 error;
		GpuTraceCounters counters;
	} out;
	int ret;

	in = dev->hDevCookie;
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_GET_TRACE_COUNTERS, &in, sizeof(in),
			  &out, sizeof(out)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	ret = out.error;
	if (ret != GPU_OK)
		return ret;

	PVRSRVMemCopy(counters, &out.counters, sizeof(*counters));
	return ret;
}

int SceGpuEs4User_9D5C4326(PVRSRV_DEV_DATA *dev)
{
	struct GpuErrorOut data;	/* in and out */

	sceClibMemset(&data, 0, sizeof(data));
	if (GpuBridgeCall(DevServices(dev), SGX_BRIDGE_RESET_SYNC_WAITS, &data, sizeof(data),
			  &data, sizeof(data)) != GPU_OK)
		return GPU_ERROR_BRIDGE_CALL_FAILED;
	return data.error;
}
