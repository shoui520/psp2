/*
 * teleport_api.cpp: SceTeleportClient module entrypoints and public C API
 * (0x81000000-0x81000A87).
 */
#include "teleport_internal.h"

static SceUInt8 s_initialized = 0;
static TeleportClient s_client;

static inline SceInt32 sanitizeTeleportResult(SceInt32 res)
{
	SceUInt32 u = (SceUInt32)res;
	if ((u > 0x80108438u || u < 0x80108400u) && res != 0)
		return SCE_TELEPORT_ERROR_FATAL;
	if (u > 0x8010840Au && u < 0x80108430u)
		return SCE_TELEPORT_ERROR_FATAL;
	return res;
}

extern "C" {

int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

SceInt32 sceTeleportClientInitialize(SceTeleportClientInitializeInfo *info)
{
	if (sceKernelCheckModelCapability(TELEPORT_MODEL_CAPABILITY_CLIENT) != 1)
		return SCE_TELEPORT_ERROR_NOT_SUPPORTED_PLATFORM;
	if (info == NULL || info->_free == NULL || info->_malloc == NULL)
		return SCE_TELEPORT_ERROR_PARAM;
	for (unsigned int i = 0; i < sizeof(info->reserved); ++i) {
		if (info->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	if (s_initialized == 1)
		return SCE_TELEPORT_ERROR_ALREADY_INITIALIZED;

	SceInt32 ret = s_client.initialize(info);
	if (ret == 0)
		s_initialized = 1;
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientRegisterGetDeviceInfoCallback(SceTeleportClientGetDeviceInfoCallback func)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;
	if (func == NULL)
		return SCE_TELEPORT_ERROR_PARAM;

	SceInt32 ret = s_client.registerCallback(func);
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientStartSearchDevice(void)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;

	SceInt32 ret = s_client.startSearchDevice();
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientEndSearchDevice(void)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;

	SceInt32 ret = s_client.endSearchDevice();
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientFinalize(void)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;

	SceInt32 ret = s_client.finalize();
	s_initialized = 0;
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientGetDeviceDescription(const SceTeleportDeviceInfo *device,
					       SceTeleportDeviceDescription *desc,
					       SceUInt32 timeout)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;
	if (device == NULL || desc == NULL)
		return SCE_TELEPORT_ERROR_PARAM;
	for (unsigned int i = 0; i < sizeof(device->reserved); ++i) {
		if (device->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	for (unsigned int i = 0; i < sizeof(desc->reserved); ++i) {
		if (desc->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	if (timeout > 0x7FFFFFFFu)
		return SCE_TELEPORT_ERROR_PARAM;

	SceInt32 ret = s_client.getDeviceDescription(device, desc, timeout);
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientGetRemoteAppInfoNum(const SceTeleportDeviceInfo *device,
					      SceTeleportRemoteAppInfoNum *infoNum,
					      SceUInt32 timeout)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;
	if (device == NULL || infoNum == NULL)
		return SCE_TELEPORT_ERROR_PARAM;
	for (unsigned int i = 0; i < sizeof(device->reserved); ++i) {
		if (device->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	for (unsigned int i = 0; i < sizeof(infoNum->reserved); ++i) {
		if (infoNum->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	if (timeout > 0x7FFFFFFFu)
		return SCE_TELEPORT_ERROR_PARAM;

	SceInt32 ret = s_client.getRemoteAppInfoNum(device, infoNum, timeout);
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientGetRemoteAppInfo(const SceTeleportDeviceInfo *device,
					   SceUInt16 size,
					   SceTeleportRemoteAppInfo **info,
					   SceUInt32 timeout)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;
	if (device == NULL)
		return SCE_TELEPORT_ERROR_PARAM;
	for (unsigned int i = 0; i < sizeof(device->reserved); ++i) {
		if (device->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	if (info == NULL)
		return SCE_TELEPORT_ERROR_PARAM;

	for (int i = 0; i < size; ++i) {
		if (*info == NULL)
			return SCE_TELEPORT_ERROR_PARAM;
		const SceTeleportRemoteAppInfo *entry = &(*info)[i];
		for (unsigned int j = 0; j < sizeof(entry->reserved); ++j) {
			if (entry->reserved[j] != 0)
				return SCE_TELEPORT_ERROR_PARAM;
		}
	}

	if (timeout > 0x7FFFFFFFu)
		return SCE_TELEPORT_ERROR_PARAM;

	SceInt32 ret = s_client.getRemoteAppInfo(device, size, info, timeout);
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientLaunchRemoteApp(const SceTeleportDeviceInfo *device,
					  const SceTeleportRemoteAppInfo *appInfo,
					  const SceTeleportRemoteAppArg *argInfo,
					  SceTeleportLaunchResponse *res,
					  SceUInt32 timeout)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;
	if (device == NULL || appInfo == NULL || argInfo == NULL || res == NULL)
		return SCE_TELEPORT_ERROR_PARAM;

	for (unsigned int i = 0; i < sizeof(device->reserved); ++i) {
		if (device->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	for (unsigned int i = 0; i < sizeof(appInfo->reserved); ++i) {
		if (appInfo->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	for (unsigned int i = 0; i < sizeof(argInfo->reserved); ++i) {
		if (argInfo->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	for (unsigned int i = 0; i < sizeof(res->reserved); ++i) {
		if (res->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	if (timeout > 0x7FFFFFFFu)
		return SCE_TELEPORT_ERROR_PARAM;

	int argLen = 0;
	const char *p = asConstChar(argInfo->arg);
	while (*p != '\0') {
		++argLen;
		++p;
		if (argLen >= 10240)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	if (argLen == -1)
		return SCE_TELEPORT_ERROR_PARAM;

	SceSize prefixLen = sceClibStrnlen("videostreaming:", 16);
	if (sceClibStrncmp(asConstChar(appInfo->appId), "videostreaming:", prefixLen) != 0 &&
	    argLen > SCE_TELEPORT_LAUNCH_ARG_SIZE_MAX)
		return SCE_TELEPORT_ERROR_PARAM;

	prefixLen = sceClibStrnlen("videostreaming:", 16);
	(void)sceClibStrncmp(asConstChar(appInfo->appId), "videostreaming:", prefixLen);

	const SceUInt8 *argBytes = (const SceUInt8 *)argInfo->arg;
	if (argLen != 0) {
		SceUInt32 utf8Bytes = 0;
		SceUInt32 ucs4Char = 0;
		SceUInt32 ucs2Len = 0;
		SceCesUcsContext ucsCtx;
		if (sceCesUtf8ToUtf32(argBytes, (SceUInt32)argLen, &utf8Bytes, &ucs4Char) != 0)
			return SCE_TELEPORT_ERROR_PARAM;
		if (utf8Bytes == 3 && ucs4Char == 0xFEFFu)
			return SCE_TELEPORT_ERROR_PARAM;
		sceClibMemset(&ucsCtx, 0, sizeof(ucsCtx));
		if (sceCesUcsContextInit(&ucsCtx) != 0)
			return SCE_TELEPORT_ERROR_PARAM;
		if (sceCesUtf8StrGetUcs2Len(&ucsCtx, argBytes, (SceUInt32)argLen,
					    &utf8Bytes, &ucs2Len) != 0)
			return SCE_TELEPORT_ERROR_PARAM;
		argBytes = (const SceUInt8 *)argInfo->arg;
	}

	for (int i = 0; i < argLen - 1; ++i) {
		SceUInt8 ch = argBytes[i];
		if (ch <= 0x1Fu || ch == 0x7Fu)
			return SCE_TELEPORT_ERROR_PARAM;
		if (ch == 0xC2u && i + 1 < argLen &&
		    argBytes[i + 1] >= 0x80u && argBytes[i + 1] <= 0x9Fu)
			return SCE_TELEPORT_ERROR_PARAM;
	}

	SceInt32 ret = s_client.launchRemoteApp(device, appInfo, argInfo, res, argLen, timeout);
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientDeleteRemoteApp(const SceTeleportDeviceInfo *device,
					  const SceTeleportRemoteAppInfo *appInfo,
					  SceUInt32 timeout)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;
	if (device == NULL || appInfo == NULL) {
		if (sceRegMgrSystemParamSetInt(TELEPORT_REG_INITIAL_TARGET, 0) != 0)
			return SCE_TELEPORT_ERROR_FATAL;
		return 0;
	}

	for (unsigned int i = 0; i < sizeof(device->reserved); ++i) {
		if (device->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	for (unsigned int i = 0; i < sizeof(appInfo->reserved); ++i) {
		if (appInfo->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}
	if (timeout > 0x7FFFFFFFu)
		return SCE_TELEPORT_ERROR_PARAM;

	SceInt32 ret = s_client.deleteRemoteApp(device, appInfo, timeout);
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientWakeupLatestDevice(const SceTeleportWakeupInfo *info)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;
	if (info == NULL)
		return SCE_TELEPORT_ERROR_PARAM;
	for (unsigned int i = 0; i < sizeof(info->reserved); ++i) {
		if (info->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}

	SceInt32 ret = s_client.wakeupLatestDevice(info);
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientGetCacheInfo(SceTeleportCacheInfo *cacheInfo)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;
	if (cacheInfo == NULL)
		return SCE_TELEPORT_ERROR_PARAM;
	for (unsigned int i = 0; i < sizeof(cacheInfo->reserved); ++i) {
		if (cacheInfo->reserved[i] != 0)
			return SCE_TELEPORT_ERROR_PARAM;
	}

	SceInt32 ret = s_client.getCacheInfo(cacheInfo);
	return sanitizeTeleportResult(ret);
}

SceInt32 sceTeleportClientClearCacheInfo(void)
{
	if (s_initialized == 0)
		return SCE_TELEPORT_ERROR_NOT_INITIALIZED;

	SceInt32 ret = s_client.clearCacheInfo();
	return sanitizeTeleportResult(ret);
}

} /* extern "C" */
