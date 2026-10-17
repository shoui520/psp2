/*
 * SceVshBridge: see vshbridge.c.
 */

#pragma once

#include <psp2kern/types.h>
#include <kernel/error.h>

/* the caller is not a system program (errno 0x58) */

typedef struct SceKernelShutdownParam {
	SceSize size;				/* 4 or 8 */
	int mode;				/* size 8: 1, 2: power off */
} SceKernelShutdownParam;

typedef struct SceKernelSystemSwVersion {
	SceSize size;				/* sizeof(SceKernelSystemSwVersion) */
	char versionString[0x1c];		/* "X.YY" */
	SceUInt32 version;
	SceUInt32 versionExtra;			/* always 0 */
} SceKernelSystemSwVersion;

/* vshbridge.c */
SceUInt32 vshKernelShutdownSystemForUser(const SceKernelShutdownParam *param);
SceUInt32 vshKernelSendSysEventForUser(int event);
SceUInt32 vshKernelGetCompiledSdkVersionByPid(SceUID pid, SceUInt32 *version);
SceUInt32 vshKernelCheckModelCapability(int bit);
int vshKernelEnableAutoClockDown(void);
int vshKernelDisableAutoClockDown(void);
SceUInt32 vshRtcSetConf(void *a, void *b, void *c, void *d);
SceUInt32 vshRtcSetCurrentTick(const SceRtcTick *tick);
SceUInt32 vshRtcSetCurrentSecureTick(const SceRtcTick *tick);
SceUInt32 vshRtcGetCurrentSecureTick(SceRtcTick *tick);
SceUInt32 vshRtcSetCurrentNetworkTick(const SceRtcTick *tick);
SceUInt32 vshRtcSetCurrentDebugNetworkTick(const SceRtcTick *tick);
SceUInt32 vshRtcSetAlarmTick(const SceRtcTick *tick, int unk);
SceUInt32 vshRtcGetAlarmTick(SceRtcTick *tick, int *unk);
SceUInt32 vshRtcIsAlarmed(void);
SceUInt32 vshRtcRegisterCallback(SceUID cb);
SceUInt32 vshRtcUnregisterCallback(SceUID cb);
SceUInt32 vshRtcSetSecureAlarmTick(const SceRtcTick *tick);
SceUInt32 vshRtcGetSecureAlarmTick(SceRtcTick *tick);
SceUInt32 vshRtcIsSecureAlarmed(void);
SceUInt32 vshRtcRegisterSecureAlarmCallback(SceUID cb);
SceUInt32 vshRtcUnregisterSecureAlarmCallback(SceUID cb);
SceUInt32 vshDisplaySetInvertColors(int head, int enable);
SceUInt32 vshDisplaySetViewportConf(int head, const void *conf, int unk);
SceUInt32 vshDisplayGetActualViewportConf(int head, void *conf, int unk);
SceUInt32 vshDisplayRegisterFrameBufCallback(SceUID cb);
SceUInt32 vshCtrlSetIdleCancelKey(int a, int b, int c, int d);
SceUInt32 vshCtrlSetIdleCancelThreshold(int a, int b);
SceUInt32 vshCtrlSetVibrationSetting(int setting);
SceUInt32 vshCtrlSetTurnOffInterval(int interval);
SceUInt32 vshCtrlGetActiveControllerPort(void);
SceUInt32 vshCtrlIsBdRemoconConnected(void);
SceUInt32 vshCtrlChangePortAssign(int a, int b);
SceUInt32 vshCtrlRegisterNotifyCallBack(SceUID cb);
SceUInt32 vshCtrlUnregisterNotifyCallBack(SceUID cb);
SceUInt32 vshMotionNoiseFilterIsAvailable(void);
SceUInt32 vshTouchSetTouchEmulationData(const void *front, const void *back);
SceUInt32 vshLedSetMode(int led, int mode, const void *conf);
SceUInt32 vshPowerRequestColdReset(void);
SceUInt32 vshPowerSetPowerSwMode(int mode);
SceUInt32 vshPowerSetStandbyButtonPushTime(int time);
SceUInt32 vshPowerSetPsButtonPushTime(int time);
SceUInt32 vshPowerGetWakeupFactor(void);
SceUInt32 vshPowerSetBatteryFakeStatus(const void *status);
SceUInt32 vshPowerRequestHibernate(void);
SceUInt32 vshSysconGetManufacturesStatus(int *status);
SceUInt32 vshSysconGetManualChargeMode(int *mode);
SceUInt32 vshSysconCtrlManualChargeMode(int mode);
SceUInt32 vshSysconEnableHibernateIO(int enable);
SceUInt32 vshSysconGetLogInfo(void *info);
SceUInt32 vshSysconLogStart(void);
SceUInt32 vshSysconLogReadData(int a, void *buf, SceSize size);
SceUInt32 vshSysconLogStartWaiting(void);
SceUInt32 vshSysconGetTemperatureLog(SceUInt8 *log);
SceUInt32 vshSysconClearTemperatureLog(int unk);
SceUInt32 vshSysconHasWWAN(void);
SceUInt32 vshSysconIduModeSet(void);
SceUInt32 vshSysconIduModeClear(void);
SceUInt32 vshSysconIsIduMode(void);
SceUInt32 vshSysconShowModeSet(void);
SceUInt32 vshSysconShowModeClear(void);
SceUInt32 vshSysconIsShowMode(void);
SceUInt32 vshSysconIsMCEmuCapable(void);
SceUInt32 vshSysconIsDownLoaderMode(void);
SceUInt32 vshSysconGetUsbDetStatus(void *status);
SceUInt32 vshSysconGetHardwareInfo(SceUInt32 *info);
SceUInt32 vshSysconGetHardwareInfo2(void *info);
SceUInt32 vshSysconBeginConfigstorageTransaction(void);
SceUInt32 vshSysconEndConfigstorageTransaction(void);
SceUInt32 vshSysconCommitConfigstorageTransaction(void);
int vshSysconLoadConfigstorageScript(int a, const void *script, SceSize size);
int vshSysconVerifyConfigstorageScript(int a, const void *script, SceSize size);
SceUInt32 vshSysconGetBatteryCalibData(int *a, int *b, int *c, int *d);
SceUInt32 vshIdStorageGetLeafSize(void);
SceUInt32 vshIdStorageIsFormatted(void);
SceUInt32 vshIdStorageIsReadOnly(void);
SceUInt32 vshIdStorageIsDirty(void);
SceUInt32 vshIdStorageFormat(void);
SceUInt32 vshIdStorageUnformat(void);
SceUInt32 vshIdStorageReadLeaf(SceSize leaf, void *buf);
SceUInt32 vshIdStorageWriteLeaf(SceSize leaf, const void *buf);
SceUInt32 vshIdStorageCreateLeaf(SceSize leaf);
SceUInt32 vshIdStorageCreateAtomicLeaves(const SceUInt16 *leaves, int num);
SceUInt32 vshIdStorageDeleteLeaf(SceSize leaf);
SceUInt32 vshIdStorageGetFreeLeaves(void);
SceUInt32 vshIdStorageLookup(SceSize key, SceSize offset, void *buf, SceSize len);
SceUInt32 vshIdStorageUpdate(SceSize key, SceSize offset, const void *buf, SceSize len);
SceUInt32 vshIdStorageFlush(void);
SceUInt32 vshIdStorageRestart(int unk);
SceUInt32 vshSblAimgrIsTest(void);
SceUInt32 vshSblAimgrIsTool(void);
SceUInt32 vshSblAimgrIsDEX(void);
SceUInt32 vshSblAimgrIsCEX(void);
SceUInt32 vshSblAimgrIsVITA(void);
SceUInt32 vshSblAimgrIsDolce(void);
SceUInt32 vshSblAimgrIsGenuineVITA(void);
SceUInt32 vshSblAimgrIsGenuineDolce(void);
SceUInt32 vshSblAimgrGetConsoleId(void *id);
SceUInt32 vshSblAimgrGetPscode(void *code);
SceUInt32 vshSblAimgrGetPscode2(void *code);
SceUInt32 vshSblAimgrGetVisibleId(void *id);
SceUInt32 vshSblSsGetNvsData(SceUInt32 offset, void *buf, SceSize size);
SceUInt32 vshSblSsSetNvsData(SceUInt32 offset, const void *buf, SceSize size);
SceUInt32 vshSblSsEncryptWithPortability(SceUInt32 mode, const void *iv, const SceSize *inSize, 				   SceSize *outSize);
SceUInt32 vshSblSsDecryptWithPortability(SceUInt32 mode, const void *iv, const SceSize *inSize, 				   SceSize *outSize);
SceUInt32 vshSblQafMgrIsAllowNearTest(void);
SceUInt32 vshSblQafMgrIsAllowSystemAppDebug(void);
SceUInt32 vshSblQafMgrIsAllowMarlinTest(void);
SceUInt32 vshSblQafMgrIsAllowLoadMagicGate(void);
SceUInt32 vshSblQafMgrIsAllowLoadPSPEmuWhiteList(void);
SceUInt32 vshSblQafMgrIsAllowFakeACInstall(void);
SceUInt32 vshSblQafMgrIsAllowShowTitleUpgradeInfo(void);
SceUInt32 vshSblQafMgrIsAllowKeepCoreFile(void);
SceUInt32 vshSblQafMgrIsAllowDtcpIpReset(void);
SceUInt32 vshSblQafMgrIsAllowControlIduAutoUpdate(void);
SceUInt32 vshSblQafMgrIsAllowPSPEmuShowQAInfo(void);
SceUInt32 vshSblQafMgrIsAllowPSPEmuSelectNPEnv(void);
SceUInt32 vshSblQafMgrIsAllowPSPEmuDeleteParameter(void);
SceUInt32 vshSblQafMgrIsAllowPSPEmuScreenShot(void);
SceUInt32 vshSblQafMgrIsAllowLiblocationChangeModel(void);
SceUInt32 vshSblQafMgrIsAllowRemotePlayDebug(void);
SceUInt32 vshSblPmMgrSetSdModeOff(void);
SceUInt32 vshSblUtMgrHasComTestFlag(void);
SceUInt32 vshSblUtMgrHasStoreFlag(void);
SceUInt32 vshSblUtMgrHasNpTestFlag(void);
SceUInt32 vshSblSsIsDevelopmentMode(void);
SceUInt32 vshSblACMgrHasCapability(SceUID pid, int capability);
SceUInt32 vshSblGcAuthMgrMlnpsnlAuth1(void *a, void *b, void *c, void *d);
SceUInt32 vshSblGcAuthMgrMlnpsnlAuth2(void *a, void *b, void *c);
SceUInt32 vshIoMount(int id, const char *path, int permission, void *data,
		     SceUInt32 reservedLow, SceUInt32 reservedHigh);
SceUInt32 vshIoUmount(int id, int force, SceUInt32 reservedLow, SceUInt32 reservedHigh);
SceUInt32 vshIoCreateMountEvent(SceUInt32 mountPoint, SceUInt32 events);
SceUInt32 vshIoDeleteMountEvent(SceUID event);
SceUInt32 vshIoFlock(SceUID fd, int flags);
SceUInt32 vshIoCreateErrorEvent(int a, int b, int c);
SceUInt32 vshIoClearErrorEvent(SceUID event);
SceUInt32 vshIoDeleteErrorEvent(SceUID event);
SceUInt32 vshIoGetstat(const char *path, void *stat);
SceUInt32 vshIoChstat(const char *path, const void *stat, int bits);
SceUInt32 vshIoDread(SceUID fd, void *dirent);
SceUInt32 vshMemoryCardEnableSlowMode(void);
SceUInt32 vshMemoryCardGetCardInsertState(int card);
SceUInt32 vshMsifGetMsInfo(void *info);
SceUInt32 vshRemovableMemoryGetCardInsertState(void);
SceUInt32 vshSdGetCardInfo(void *info);
SceUInt32 vshIoSetProcessDefaultPriorityForSystem(int priority);
SceUInt32 vshIoGetProcessDefaultPriorityForSystem(void);
SceUInt32 vshHdmiCecCmdForcedPollingMsg(void);
SceUInt32 vshHdmiEnableCec(void);
SceUInt32 vshHdmiDisableCec(void);
SceUInt32 vshNpDrmGetLegacyDocKey(void *a, void *b, void *c, void *d);
SceUInt32 vshNpDrmEbootSigGenPsp(void *a, void *b, void *c, void *d);
SceUInt32 vshNpDrmEbootSigGenPs1(void *a, void *b, void *c, void *d);
SceUInt32 vshNpDrmEbootSigGenMultiDisc(void *a, void *b, void *c, void *d);
SceUInt32 vshNpDrmEbootSigConvert(const char *path, const void *oldSignature, void *newSignature);
SceUInt32 vshNpDrmEbootSigVerify(void *a, void *b);
SceUInt32 vshNpDrmPspEbootSigGen(void *a, void *b, void *c);
SceUInt32 vshNpDrmPspEbootVerify(void *a, void *b);
SceUInt32 vshNpDrmIsLooseAccountBind(void);
SceUInt32 vshNpDrmUpdateDebugSettings(void);
SceUInt32 vshNpDrmUpdateAccountId(void *a, void *b);
SceUID vshKernelSearchModuleByName(const char *name);
SceUInt32 vshSblAuthMgrVerifySpsfo(const char *path, void *buf, SceSize size);
SceUInt32 vshSDfCtlSetBootParameter(const char *param, int unused, int value);
SceUInt32 vshAppMgrSystemParamDateTimeSetConf(const void *conf);
SceUInt32 vshAppMgrBgdlSetQueueStatus(const void *status);
SceUInt32 vshAppMgrDebugSettingNotifyUpdate(int setting);
SceUInt32 vshAppMgrCheckContentInstallPeriod(void *a);
SceUInt32 vshAppMgrUpdateRifInfo(const void *info);
SceUInt32 vshAppMgrRegisterPath(const char *path, void *a);
SceUInt32 vshAppMgrCheckPfsMounted(const char *path);
SceUInt32 vshAppMgrCloudDataCreateHeader(const void *a, int b, const void *c, void *d);
SceUInt32 vshAppMgrCloudDataVerifyHeader(const void *header);
SceUInt32 vshAppMgrCloudDataSetupKey(void *key);
SceUInt32 vshAppMgrCloudDataSrcMount(int a, const void *b, void *c);
SceUInt32 vshAppMgrCloudDataDstCreateMount(const void *a, void *b);
SceUInt32 vshAppMgrFakeSaveDataCreateMount(const char *path, void *b);
SceUInt32 vshAppMgrCloudDataSetMcId(int a, const void *id);
SceUInt32 vshAppMgrCloudDataGetMcId(int a, void *id);
SceUInt32 vshAppMgrCloudDataClearMcId(int a);
SceUInt32 vshAppMgrGetPfsProcessStatus(void);
SceUInt32 vshAppMgrIsExclusiveProcessRunning(const void *a);
SceUInt32 vshAppMgrAcInstGetAcdirParam(const void *a, const void *b, void *c);
SceUInt32 vshAppMgrSaveDataLocalBackupTargetGetList(void *list);
SceUInt32 vshAppMgrSaveDataLocalBackupTargetRemoveItem(const void *item);
SceUInt32 vshAppMgrLocalBackupGetOfflineId(void *id, void *b);
SceUInt32 vshAppMgrLocalBackupVerifyOfflineHeader(const void *header);
SceUInt32 vshAppMgrSaveDataNotifyBackupFinished(const void *a);
SceUInt32 vshCoredumpCafInit(void);
SceUInt32 vshCoredumpCafFinal(void);
SceUInt32 vshCoredumpCafContextCreate(void);
SceUInt32 vshCoredumpCafContextDestroy(SceUID ctx);
SceUInt32 vshCoredumpCafHeaderInit(SceUID ctx, SceUInt32 algorithm);
SceUInt32 vshCoredumpCafHeaderTransform(SceUID ctx, const void *data, SceSize size);
SceUInt32 vshCoredumpCafHeaderFinal(SceUID ctx, void *digest, SceSize size);
SceUInt32 vshCoredumpCafCreateIv(void *iv, SceSize size);
SceUInt32 vshCoredumpCafSegmentInit(SceUID ctx, void *a, void *b, void *c, void *d, void *e);
SceUInt32 vshCoredumpCafSegmentTransform(SceUID ctx, void *a, void *b, void *c);
SceUInt32 vshCoredumpCafSegmentFinal(SceUID ctx, void *a, void *b);
SceUInt32 vshCoredumpDeleteCrashReportCaf(void);
SceUInt32 vshCoredumpCreateDump(int a, const void *b, int c, const void *d, int e, int f, void *g, int h);
SceUInt32 vshEventLogPut(int id, int index, int a3, const void *pBuf, SceSize bufSize);
SceUInt32 vshSblGetSystemSwVersion(SceKernelSystemSwVersion *info);
SceUInt32 vshSblAimgrGetSMI(void *smi);
SceUInt32 vshSblSsGenerateAppKey(void *a, void *b);
SceUInt32 vshSblSsCreatePassPhrase(void *a, void *b);

/* drmbridge.c */
int drmBridgeGetCurrentSecureTick(SceRtcTick *tick);
int drmBridgeIsAllowRemotePlayDebug(void);
int drmBridgeB81B597A(void);
int drmBridgeMlnpsnlAuth1(void *a, void *b, void *out1, void *out2);
int drmBridgeMlnpsnlAuth2(void *a, void *b, void *out);
