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
#include <rtc/rtc.h>
#include <processmgr/processmgr.h>
#include <modulemgr/modulemgr.h>
#include <iofilemgr/iofilemgr.h>
#include <display/display.h>
#include <acmgr/acmgr.h>

#include <sysmem/pmmgr.h>
#include <sysmem/debug.h>
#include <ctrl/ctrl.h>
#include <msif/msif.h>
#include <touch/touch.h>
#include <motion/motion_dev.h>
#include <udcd/udcd.h>
#include <ss_mgr/ss_mgr.h>
#include <npdrm/npdrm.h>
#include <post_ss_mgr/sblpostssmgr.h>
#include <coredump/coredump.h>
#include <regmgr/regmgr.h>

/* sysroot, threads, modules */

/* SblACMgr, Aimgr, QafMgr, Ss, PostSsMgr, UtMgr, PmMgr */
int SceSblPostSsMgrForDriver_2646DE64(void *a, void *b);	/* generate an app key */
int SceSblPostSsMgrForDriver_BDF18922(const char *path, SceUInt32 *ctx);
int SceSblPostSsMgrForDriver_686B9461(SceUInt32 *ctx, void **data, SceSize *size);
int SceSblPostSsMgrForDriver_AD3B0078(SceUInt32 *ctx);
int sceSblGcAuthMgrMlnpsnlAuth1ForDriver(void *a, void *b, void *c, void *d);	/* Mlnpsnl auth 1 */
int sceSblGcAuthMgrMlnpsnlAuth2ForDriver(void *a, void *b, void *c);	/* Mlnpsnl auth 2 */

/* IdStorage */

/* Motion, Touch, Hdmi */
int sceHdmiCecEnableCec(void);
int SceHdmiForDriver_291F000B(void);			/* disable CEC */
int SceHdmiForDriver_6D2D10D8(void);			/* CEC forced polling message */

/* Io, Sdif, Msif */
struct SceMsInfo;

/* NpDrm */
int SceNpDrmForDriver_4E321BDE(void *a, void *b, void *c, void *d);
int SceNpDrmForDriver_90B1A6D3(void *a, void *b, void *c, void *d);
int SceNpDrmForDriver_6D9223E1(void *a, void *b, void *c, void *d);
int SceNpDrmForDriver_39A7A666(void *a, void *b, void *c, void *d);
int SceNpDrmForDriver_B6CA3A2C(void *a, void *b);
int SceNpDrmForDriver_FC84CA1A(void);
int SceNpDrmForDriver_116FC0D6(void *a, void *b);

/* AppMgr */
int SceAppMgrForDriver_193784A9(void *a);
int sceAppMgrUpdateRifInfo(const void *info);
int SceAppMgrForDriver_B53FB98E(const char *path, void *a);
int SceAppMgrForDriver_ED87F191(const void *a, int b, const void *c, void *d);
int SceAppMgrForDriver_55D48190(const void *header);
int SceAppMgrForDriver_2FAA6ABD(void *key);
int sceAppMgrCloudDataSrcMount(int a, const void *b, void *c);
int sceAppMgrCloudDataDstCreateMount(const void *a, void *b);
int sceAppMgrFakeSaveDataCreateMount(const char *path, void *b);
int SceAppMgrForDriver_0B94BB90(int a, const void *id);
int SceAppMgrForDriver_1286AF64(int a, void *id);
int SceAppMgrForDriver_EC408FEF(int a);
int sceAppMgrGetPfsProcessStatus(void);
int sceAppMgrIsExclusiveProcessRunning(const void *a);
int sceAppMgrAcInstGetAcdirParam(const void *a, const void *b, void *c);
int SceAppMgrForDriver_9B3F7D66(void *list);
int SceAppMgrForDriver_C4EF95BB(const void *item);
int SceAppMgrForDriver_61B13981(void *id, void *b);
int SceAppMgrForDriver_62E98C42(const void *header);
int sceAppMgrSaveDataNotifyBackupFinished(const void *a);

/* Coredump */
int SceCoredumpForDriver_9336009B(void);
int SceCoredumpForDriver_C90F61AF(void);
int SceCoredumpForDriver_7C8120C5(SceUID ctx, SceUInt32 algorithm);
int SceCoredumpForDriver_AE2C2793(SceUID ctx, const void *data, SceSize size);
int SceCoredumpForDriver_65AA4991(SceUID ctx, void *digest, SceSize size);
int SceCoredumpForDriver_E1BCBE8F(void *iv, SceSize size);
int SceCoredumpForDriver_07185515(SceUID ctx, void *a, void *b, void *c, void *d, void *e);
int SceCoredumpForDriver_FB7AEBFE(SceUID ctx, void *a, void *b, void *c);
int SceCoredumpForDriver_DF17420A(SceUID ctx, void *a, void *b);
