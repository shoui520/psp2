/*
 * SceCoredump (os0:kd/coredump.skprx) driver interface, as used so far.
 */

#pragma once

#include <stddef.h>
#include <scetypes.h>

typedef struct SceCoredumpTriggerParam {
	SceSize size;
	SceUInt32 dumpLevel;
	SceUInt32 triggerDataType;
	SceSize triggerDataSize;
	const void *triggerData;
	SceSize titleid_len;
	const char *titleid;
	SceSize app_name_len;
	const char *app_name;
	int appVersion;
	int triggerReason;
	SceUID crash_thid;
	SceUInt32 triggerFlags;
} SceCoredumpTriggerParam;

#ifndef __cplusplus	/* C only; g++ 4.8 (C++98) has no static_assert */
_Static_assert(sizeof(SceCoredumpTriggerParam) == 52,
	"SceCoredumpTriggerParam");
_Static_assert(offsetof(SceCoredumpTriggerParam, triggerData) == 16,
	"SceCoredumpTriggerParam.triggerData");
_Static_assert(offsetof(SceCoredumpTriggerParam, appVersion) == 36,
	"SceCoredumpTriggerParam.appVersion");
_Static_assert(offsetof(SceCoredumpTriggerParam, triggerFlags) == 48,
	"SceCoredumpTriggerParam.triggerFlags");
#endif

typedef int (*SceKernelCoredumpStateUpdateCallback)(int task_id, SceUID pid,
	int progress);
typedef int (*SceKernelCoredumpStateFinishCallback)(int task_id, SceUID pid,
	int result, const char *path, SceSize path_len, int);

typedef SceInt32 (*SceCoredumpHandler)(void *pCommon);

#define SCE_COREDUMP_ERROR_PARAM			(-2146828288)
#define SCE_COREDUMP_ERROR_NOT_REGISTERED		(-2146828287)
#define SCE_COREDUMP_ERROR_ALREADY_REGISTERED	(-2146828286)
#define SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER	(-2146828285)

/* names unknown: a core dump file os0:kd/crashdump.skprx opens, writes and closes */
int SceCoredumpForDriver_13EF8516(SceSize payloadSize);
int SceCoredumpForDriver_12392973(int requestId, const void *data, SceSize size);
int SceCoredumpForDriver_10863B61(int requestId);

int SceCoredumpForDriver_097AA37D(void);
/* vitasdk: ksceCoredumpCreateDump */
int SceCoredumpForDriver_0C10313F(SceUID pid, const char *titleid, int titleidLength, const char *appName, int appNameLength, int appVersion, char *path, SceSize pathSize);
int SceCoredumpForDriver_AD070837(void);	/* vitasdk: ksceCoredumpDeleteCrashReportCaf */
int SceCoredumpForDriver_D064F6DC(SceUID pid, SceKernelCoredumpStateUpdateCallback update, SceKernelCoredumpStateFinishCallback finish, SceCoredumpTriggerParam *param);
SceUID sceCoredumpCafContextCreate(void);
int sceCoredumpCafContextDestroy(SceUID context);
int sceCoredumpRegisterCoredumpHandler(SceCoredumpHandler handler, SceSize stackSize, void *pCommon);
int sceCoredumpTrigger(SceUID pid, SceKernelCoredumpStateUpdateCallback update, SceKernelCoredumpStateFinishCallback finish, SceCoredumpTriggerParam *param);
int sceCoredumpUnregisterCoredumpHandler(void);
int sceCoredumpWriteUserData(const void *data, SceSize size);
int sceKernelCoredumpCancelForDriver(int requestId);

/* each thread of a faulting process, through Sysroot (SceSysrootForDriver_DABD4A47) */
struct CoredumpThreadSnapshot;
int SceCoredumpForDriver_EF20949F(const struct CoredumpThreadSnapshot *snapshot);

/* the CAF (crash report) transforms (caf.c) */
int SceCoredumpForDriver_9336009B(void);	/* vitasdk: ksceCoredumpCafInit */
int SceCoredumpForDriver_C90F61AF(void);	/* vitasdk: ksceCoredumpCafFinal */
/* vitasdk: ksceCoredumpCafHeaderInit */
int SceCoredumpForDriver_7C8120C5(SceUID context, SceUInt32 hashKeySelector);
/* vitasdk: ksceCoredumpCafHeaderTransform */
int SceCoredumpForDriver_AE2C2793(SceUID context, const void *data, SceSize size);
/* vitasdk: ksceCoredumpCafHeaderFinal */
int SceCoredumpForDriver_65AA4991(SceUID context, void *digest, SceSize size);
int SceCoredumpForDriver_E1BCBE8F(void *iv, SceSize size);	/* vitasdk: ksceCoredumpCafCreateIv */
/* vitasdk: ksceCoredumpCafSegmentInit */
int SceCoredumpForDriver_07185515(SceUID context, SceUInt32 hashKeySelector,
		SceUInt32 cipherMode, SceUInt32 cipherKeySelector, const void *iv, SceSize size);
/* vitasdk: ksceCoredumpCafSegmentTransform */
int SceCoredumpForDriver_FB7AEBFE(SceUID context, const void *source, void *destination,
		SceSize size);
/* vitasdk: ksceCoredumpCafSegmentFinal */
int SceCoredumpForDriver_DF17420A(SceUID context, void *digest, SceSize size);
