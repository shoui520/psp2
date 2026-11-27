/*
 * vshbridge_wrappers.c: SceShell's statically linked SceVshBridge, SceFios2Kernel,
 * and SceCompatForVsh user-side syscall wrappers (0x81417C68..0x81417E04,
 * 0x81425E50..0x81425EB7, 0x81449CC8..0x81449D17) and AEABI division-by-zero handlers.
 */
#include <stddef.h>
#include <scetypes.h>
#include <display/display.h>
#include <intrmgr/syscall_copy.h>

typedef struct VshEbootSigGenArgs {
	int swVersion;
	SceUInt32 reservedWords[3];
} VshEbootSigGenArgs;

typedef struct VshVerifyOutArgs {
	SceUInt32 outputWords[3];
} VshVerifyOutArgs;

typedef struct VshMlnpsnlAuthArgs {
	void *outputKey;
	SceUInt32 requestSize;
	SceUInt32 challengeSize;
	SceUInt32 responseSize;
	SceUInt32 keySize;
	SceUInt32 paddingWord;
} VshMlnpsnlAuthArgs;

/* Raw kernel syscall imports from SceVshBridge / SceFios2Kernel / SceCompatForVsh */
int _vshRtcSetCurrentTick(const void *tick);
int _vshRtcGetCurrentSecureTick(void *tick);
int _vshRtcSetAlarmTick(const void *tick, int flag);
int _vshRtcSetSecureAlarmTick(const void *tick);
int _vshRtcSetCurrentNetworkTick(const void *tick);
int _vshRtcSetCurrentSecureTick(const void *tick);
int _vshDisplayGetActualViewportConf(SceUInt32 head, SceUInt32 plane,
				     SceDisplayViewportConf *conf,
				     const SyscallSizeArgs *args);
int _vshSysconLogReadData(int id, void *buf, const SyscallSizeArgs *args);
int _vshSysconGetTemperatureLog(SceUInt8 *log);
int _vshSblSsGetNvsData(int area, void *buf, const SyscallSizeArgs *args);
int _vshSblSsDecryptWithPortability(int keyType, const void *iv, const void *inBuf, void *outBuf);
int _vshNpDrmEbootSigGenPsp(const char *ebootPath, const void *sha256, void *signature,
			    const VshEbootSigGenArgs *args);
int _vshNpDrmEbootSigGenPs1(const char *ebootPath, const void *sha256, void *signature,
			    const VshEbootSigGenArgs *args);
int _vshNpDrmEbootSigGenMultiDisc(const char *ebootPath, const void *discInfo, void *signature,
				  const VshEbootSigGenArgs *args);
int _vshNpDrmEbootSigVerify(const char *ebootPath, const void *signature,
			    VshVerifyOutArgs *args);
int _vshNpDrmPspEbootVerify(const char *ebootPath, const void *signature,
			    VshVerifyOutArgs *args);
int _vshSblAuthMgrVerifySpsfo(const char *path, void *sfo, SceSize size,
			      VshVerifyOutArgs *args);
int _vshKernelGetCompiledSdkVersionByPid(SceUID pid, SceUInt32 *outSdkVersion);
SceUInt32 _vshAppMgrBgdlSetQueueStatus(const void *status);
int _vshAppMgrUpdateRifInfo(const char *titleId);
int _vshAppMgrCloudDataSetupKey(void *key);
int _vshAppMgrSaveDataLocalBackupTargetRemoveItem(const char *saveDataTitleId);
int _vshAppMgrLocalBackupGetOfflineId(void *offlineId, SceUInt32 *checkWord);
int _vshAppMgrSystemParamDateTimeSetConf(const void *conf);
int _vshSblSsGenerateAppKey(const void *src, void *dst);
int _vshSblAimgrGetConsoleId(void *consoleId);
int _vshSblSsCreatePassPhrase(const void *inData, void *outPhrase, const SyscallSizeArgs *args);
int _vshTouchSetTouchEmulationData(const void *front, const void *back);
int _vshLedSetMode(int led, int mode, const void *param, const SyscallSizeArgs *args);
int _vshSblGcAuthMgrMlnpsnlAuth1(const void *request, const void *challenge,
				 void *response, const VshMlnpsnlAuthArgs *args);
int _sceFiosKernelOverlayResolveSync(SceUID pid, int resolveFlag, const char *inPath,
				     char *outPath, SceSize maxPath);
int SceCompatForVsh_9C107CC9(void);

/* 0x81417C68 */
int vshRtcSetCurrentTick(const void *tick)
{
	return _vshRtcSetCurrentTick(tick);
}

/* 0x81417C70 */
int vshRtcGetCurrentSecureTick(void *tick)
{
	return _vshRtcGetCurrentSecureTick(tick);
}

/* 0x81417C78 */
int vshRtcSetAlarmTick(const void *tick, int flag)
{
	return _vshRtcSetAlarmTick(tick, flag);
}

/* 0x81417C80 */
int vshRtcSetSecureAlarmTick(const void *tick)
{
	return _vshRtcSetSecureAlarmTick(tick);
}

/* 0x81417CA4 */
int vshDisplayGetActualViewportConf(SceUInt32 head, SceUInt32 plane, SceDisplayViewportConf *conf)
{
	SyscallSizeArgs args;

	args.size = conf->size;
	return _vshDisplayGetActualViewportConf(head, plane, conf, &args);
}

/* 0x81417CBC */
int vshSysconLogReadData(int id, void *buf, SceSize size)
{
	SyscallSizeArgs args;

	args.size = size;
	return _vshSysconLogReadData(id, buf, &args);
}

/* 0x81417CD0 */
int vshSysconGetTemperatureLog(SceUInt8 *log)
{
	return _vshSysconGetTemperatureLog(log);
}

/* 0x81417CF8 */
SceUInt32 vshSblSsGetNvsData(int area, void *buf, SceSize size)
{
	SyscallSizeArgs args;

	args.size = size;
	return (SceUInt32)_vshSblSsGetNvsData(area, buf, &args);
}

/* 0x81417D20 */
int vshSblSsDecryptWithPortability(int keyType, const void *iv, const void *inBuf, void *outBuf)
{
	return _vshSblSsDecryptWithPortability(keyType, iv, inBuf, outBuf);
}

/* 0x81417D28 */
int vshNpDrmEbootSigGenPsp(const char *ebootPath, const void *sha256, void *signature,
			   int swVersion)
{
	VshEbootSigGenArgs args;

	args.swVersion = swVersion;
	return _vshNpDrmEbootSigGenPsp(ebootPath, sha256, signature, &args);
}

/* 0x81417D3C */
int vshNpDrmEbootSigGenPs1(const char *ebootPath, const void *sha256, void *signature,
			   int swVersion)
{
	VshEbootSigGenArgs args;

	args.swVersion = swVersion;
	return _vshNpDrmEbootSigGenPs1(ebootPath, sha256, signature, &args);
}

/* 0x81417D50 */
int vshNpDrmEbootSigGenMultiDisc(const char *ebootPath, const void *discInfo, void *signature,
				 int swVersion)
{
	VshEbootSigGenArgs args;

	args.swVersion = swVersion;
	return _vshNpDrmEbootSigGenMultiDisc(ebootPath, discInfo, signature, &args);
}

/* 0x81417D64 */
int vshNpDrmEbootSigVerify(const char *ebootPath, const void *signature)
{
	VshVerifyOutArgs args;

	return _vshNpDrmEbootSigVerify(ebootPath, signature, &args);
}

/* 0x81417D74 */
int vshNpDrmPspEbootVerify(const char *ebootPath, const void *signature)
{
	VshVerifyOutArgs args;

	return _vshNpDrmPspEbootVerify(ebootPath, signature, &args);
}

/* 0x81417D84 */
int vshSblAuthMgrVerifySpsfo(const char *path, void *sfo, SceSize size)
{
	VshVerifyOutArgs args;

	return _vshSblAuthMgrVerifySpsfo(path, sfo, size, &args);
}

/* 0x81417D94 */
int vshKernelGetCompiledSdkVersionByPid(SceUID pid, SceUInt32 *outSdkVersion)
{
	return _vshKernelGetCompiledSdkVersionByPid(pid, outSdkVersion);
}

/* 0x81417D9C */
SceUInt32 vshAppMgrBgdlSetQueueStatus(const void *status)
{
	return _vshAppMgrBgdlSetQueueStatus(status);
}

/* 0x81417DA4 */
int vshAppMgrUpdateRifInfo(const char *titleId)
{
	return _vshAppMgrUpdateRifInfo(titleId);
}

/* 0x81417DAC */
int vshAppMgrCloudDataSetupKey(void *key)
{
	return _vshAppMgrCloudDataSetupKey(key);
}

/* 0x81417DC4 */
int vshAppMgrSaveDataLocalBackupTargetRemoveItem(const char *saveDataTitleId)
{
	return _vshAppMgrSaveDataLocalBackupTargetRemoveItem(saveDataTitleId);
}

/* 0x81417DCC */
int vshAppMgrLocalBackupGetOfflineId(void *offlineId, SceUInt32 *checkWord)
{
	return _vshAppMgrLocalBackupGetOfflineId(offlineId, checkWord);
}

/* 0x81417DFC */
int vshSblSsGenerateAppKey(const void *src, void *dst)
{
	return _vshSblSsGenerateAppKey(src, dst);
}

/* 0x81425E50 */
int vshTouchSetTouchEmulationData(const void *front, const void *back)
{
	return _vshTouchSetTouchEmulationData(front, back);
}

/* 0x81425E58 */
int vshLedSetMode(int led, int mode, const void *param)
{
	SyscallSizeArgs args;

	if (mode == 1 || mode == 4)
		args.size = 8;
	else if (mode == 2)
		args.size = 20;
	else
		args.size = 0;
	return _vshLedSetMode(led, mode, param, &args);
}

/* 0x81425E88 */
int vshSblGcAuthMgrMlnpsnlAuth1(const void *request, const void *challenge,
				void *response, void *outputKey)
{
	VshMlnpsnlAuthArgs args;

	args.outputKey = outputKey;
	args.requestSize = 0x20U;
	args.challengeSize = 0x08U;
	args.responseSize = 0x80U;
	args.keySize = 0x40U;
	args.paddingWord = 0U;
	return _vshSblGcAuthMgrMlnpsnlAuth1(request, challenge, response, &args);
}

/* 0x81449CC8 */
int vshRtcSetCurrentSecureTick(const void *tick)
{
	return _vshRtcSetCurrentSecureTick(tick);
}

/* 0x81449CD0 */
int vshRtcSetCurrentNetworkTick(const void *tick)
{
	return _vshRtcSetCurrentNetworkTick(tick);
}

/* 0x81449CD8 */
int vshSblAimgrGetConsoleId(void *consoleId)
{
	return _vshSblAimgrGetConsoleId(consoleId);
}

/* 0x81449CE0 */
int vshAppMgrSystemParamDateTimeSetConf(const void *conf)
{
	return _vshAppMgrSystemParamDateTimeSetConf(conf);
}

/* 0x81449D00 */
int vshSblSsCreatePassPhrase(const void *inData, void *outPhrase)
{
	SyscallSizeArgs args;

	args.size = 0x200U;
	return _vshSblSsCreatePassPhrase(inData, outPhrase, &args);
}

int sceFiosKernelOverlayResolveSync(SceUID pid, int resolveFlag, const char *inPath,
				    char *outPath, SceSize maxPath)
{
	return _sceFiosKernelOverlayResolveSync(pid, resolveFlag, inPath, outPath, maxPath);
}

int sceCompatGetPspSystemSoftwareVersion(void)
{
	return SceCompatForVsh_9C107CC9();
}

int __aeabi_idiv0(int returnValue)
{
	return returnValue;
}

long long __aeabi_ldiv0(long long returnValue)
{
	return returnValue;
}
