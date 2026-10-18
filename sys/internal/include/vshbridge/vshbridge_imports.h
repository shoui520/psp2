/*
 * The kernel services SceVshBridge (os0:kd/bootimage/vshbridge.skprx) bridges
 * to the system applications, as it calls them. Pointer arguments are the
 * caller's (user) buffers unless the bridge copies them. NID-named imports
 * have no known name.
 */

#pragma once

#include <psp2kern/types.h>
#include <idstorage/idstorage.h>
#include <sdif/sdif.h>
#include <lowio/pervasive.h>
#include <syscon/syscon.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <appmgr/appmgr_rdrfp.h>
#include <power/led.h>
#include <power/power.h>
#include <processmgr/processmgr.h>
#include <modulemgr/modulemgr.h>
#include <iofilemgr/iofilemgr.h>
#include <display/display.h>
#include <acmgr/acmgr.h>

#include <sysmem/pmmgr.h>
#include <sysmem/debug.h>
#include <ctrl/ctrl.h>
typedef SceUInt64 SceRtcTick;

/* sysroot, threads, modules */
int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);
int SceUdcdForDriver_2D91F8C3(int enable);		/* USB charging */

/* SblACMgr, Aimgr, QafMgr, Ss, PostSsMgr, UtMgr, PmMgr */
int ksceSblAimgrGetConsoleId(void *id);
int ksceSblAimgrGetPscode(void *code);
int ksceSblAimgrGetPscode2(void *code);
int ksceSblAimgrGetVisibleId(void *id);
int ksceSblSsGetNvsData(SceUInt32 offset, void *buf, SceSize size);
int ksceSblSsSetNvsData(SceUInt32 offset, const void *buf, SceSize size);
int ksceSblSsEncryptWithPortability(SceUInt32 mode, const void *iv, const SceSize *inSize,
				    SceSize *outSize);
int ksceSblSsDecryptWithPortability(SceUInt32 mode, const void *iv, const SceSize *inSize,
				    SceSize *outSize);
int ksceSblSsCreatePassPhrase(void *a, void *b);
int SceSblPostSsMgrForDriver_2646DE64(void *a, void *b);	/* generate an app key */
int ksceSblPostSsMgrInitializeSpfsoCtx(const char *path, SceUInt32 *ctx);
int ksceSblPostSsMgrVerifySpfsoCtx(SceUInt32 *ctx, void **data, SceSize *size);
int ksceSblPostSsMgrReleaseSpfsoCtx(SceUInt32 *ctx);
int _ksceSblPostSsMgrExecutePmSmF00dCommand(int command);
int ksceSblUtMgrHasComTestFlag(void);
int ksceSblUtMgrHasStoreFlag(void);
int ksceSblUtMgrHasNpTestFlag(void);
int SceSblGcAuthMgrMlnpsnlForDriver_DABC01F5(void *a, void *b, void *c, void *d);	/* Mlnpsnl auth 1 */
int SceSblGcAuthMgrMlnpsnlForDriver_296ABD68(void *a, void *b, void *c);	/* Mlnpsnl auth 2 */

/* IdStorage */
int ksceIdStorageGetLeafSize(void);
int ksceIdStorageIsFormatted(void);
int ksceIdStorageIsReadOnly(void);
int ksceIdStorageIsDirty(void);
int ksceIdStorageFormat(void);
int ksceIdStorageUnformat(void);
int ksceIdStorageReadLeaf(SceSize leaf, void *buf);
int ksceIdStorageWriteLeaf(SceSize leaf, const void *buf);
int ksceIdStorageCreateLeaf(SceSize leaf);
int ksceIdStorageCreateAtomicLeaves(const SceUInt16 *leaves, int num);
int ksceIdStorageDeleteLeaf(SceSize leaf);
int ksceIdStorageGetFreeLeaves(void);
int ksceIdStorageUpdate(SceSize key, SceSize offset, const void *buf, SceSize len);
int ksceIdStorageFlush(void);
int ksceIdStorageRestart(int unk);

/* Syscon, LED, power */
int ScePowerForDriver_0C6973B8(const void *status);

/* Rtc */
int SceRtcForDriver_ACFE5A9F(void *a, void *b, void *c, void *d);
int sceRtcSetCurrentTick(const SceRtcTick *tick);
int SceRtcForDriver_7D431C87(const SceRtcTick *tick);
int SceRtcForDriver_401C0954(SceRtcTick *tick);
int SceRtcForDriver_162E486E(const SceRtcTick *tick);
int SceRtcForDriver_AAB6053F(const SceRtcTick *tick);
int sceRtcSetAlarmTick(const SceRtcTick *tick, int unk);
int SceRtcForDriver_C838275A(SceRtcTick *tick, int *unk);
int SceRtcForDriver_CD295F0C(void);
int SceRtcForDriver_5220870D(SceUID cb);
int SceRtcForDriver_9546C68B(SceUID cb);
int SceRtcForDriver_8333790D(const SceRtcTick *tick);
int SceRtcForDriver_BD53731C(SceRtcTick *tick);
int SceRtcForDriver_E39F2ABE(void);
int SceRtcForDriver_CF84DFC0(SceUID cb);
int SceRtcForDriver_EDDC2325(SceUID cb);

/* Display, Ctrl, Motion, Touch, Hdmi */
int SceDisplayForDriver_EE5EB52D(int head, const void *conf, int unk);
int SceDisplayForDriver_40ACFE51(int head, void *conf, int unk);
int ksceMotionDevNoiseFilterIsAvailable(void);
int ksceTouchSetTouchEmulationData(const void *front, const void *back);
int sceHdmiCecEnableCec(void);
int SceHdmiForDriver_291F000B(void);			/* disable CEC */
int SceHdmiForDriver_6D2D10D8(void);			/* CEC forced polling message */

/* Io, Sdif, Msif */
int SceSdifForDriver_F37CF8E5(void);			/* SD slow mode */
int ksceSdifGetCardInsertState1(int card);
int ksceSdifGetCardInsertState2(int card);
int ksceMsifEnableSlowMode(void);
struct SceMsInfo;
int ksceMsifGetMsInfo(struct SceMsInfo *info);

/* NpDrm */
int ksceNpDrmGetLegacyDocKey(void *a, void *b, void *c, void *d);
int ksceNpDrmEbootSigGenPsp(void *a, void *b, void *c, void *d);
int ksceNpDrmEbootSigGenPs1(void *a, void *b, void *c, void *d);
int ksceNpDrmEbootSigGenMultiDisc(void *a, void *b, void *c, void *d);
int ksceNpDrmEbootSigConvert(const char *path, const void *oldSignature, void *newSignature);
int ksceNpDrmEbootSigVerify(void *a, void *b);
int ksceNpDrmPspEbootSigGen(void *a, void *b, void *c);
int ksceNpDrmPspEbootVerify(void *a, void *b);
int ksceNpDrmIsLooseAccountBind(void);
int ksceNpDrmUpdateDebugSettings(void);
int ksceNpDrmUpdateAccountId(void *a, void *b);

/* AppMgr */
int ksceAppMgrCheckContentInstallPeriod(void *a);
int ksceAppMgrUpdateRifInfo(const void *info);
int ksceAppMgrRegisterPath(const char *path, void *a);
int ksceAppMgrCloudDataCreateHeader(const void *a, int b, const void *c, void *d);
int ksceAppMgrCloudDataVerifyHeader(const void *header);
int ksceAppMgrCloudDataSetupKey(void *key);
int ksceAppMgrCloudDataSrcMount(int a, const void *b, void *c);
int ksceAppMgrCloudDataDstCreateMount(const void *a, void *b);
int ksceAppMgrFakeSaveDataCreateMount(const char *path, void *b);
int ksceAppMgrCloudDataSetMcId(int a, const void *id);
int ksceAppMgrCloudDataGetMcId(int a, void *id);
int ksceAppMgrCloudDataClearMcId(int a);
int ksceAppMgrGetPfsProcessStatus(void);
int ksceAppMgrIsExclusiveProcessRunning(const void *a);
int ksceAppMgrAcInstGetAcdirParam(const void *a, const void *b, void *c);
int ksceAppMgrSaveDataLocalBackupTargetGetList(void *list);
int ksceAppMgrSaveDataLocalBackupTargetRemoveItem(const void *item);
int ksceAppMgrLocalBackupGetOfflineId(void *id, void *b);
int ksceAppMgrLocalBackupVerifyOfflineHeader(const void *header);
int ksceAppMgrSaveDataNotifyBackupFinished(const void *a);

/* Coredump */
int ksceCoredumpCafInit(void);
int ksceCoredumpCafFinal(void);
SceUID ksceCoredumpCafContextCreate(void);
int ksceCoredumpCafContextDestroy(SceUID ctx);
int ksceCoredumpCafHeaderInit(SceUID ctx, SceUInt32 algorithm);
int ksceCoredumpCafHeaderTransform(SceUID ctx, const void *data, SceSize size);
int ksceCoredumpCafHeaderFinal(SceUID ctx, void *digest, SceSize size);
int ksceCoredumpCafCreateIv(void *iv, SceSize size);
int ksceCoredumpCafSegmentInit(SceUID ctx, void *a, void *b, void *c, void *d, void *e);
int ksceCoredumpCafSegmentTransform(SceUID ctx, void *a, void *b, void *c);
int ksceCoredumpCafSegmentFinal(SceUID ctx, void *a, void *b);
int ksceCoredumpDeleteCrashReportCaf(void);
int ksceCoredumpCreateDump(int a, const void *b, int c, const void *d, int e, int f, void *g, int h);
