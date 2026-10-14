/*
 * SceCoredump (os0:kd/coredump.skprx) driver interface, as used so far.
 */

#pragma once

#include <stddef.h>
#include <psp2kern/types.h>

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

_Static_assert(sizeof(SceCoredumpTriggerParam) == 52,
	"SceCoredumpTriggerParam");
_Static_assert(offsetof(SceCoredumpTriggerParam, triggerData) == 16,
	"SceCoredumpTriggerParam.triggerData");
_Static_assert(offsetof(SceCoredumpTriggerParam, appVersion) == 36,
	"SceCoredumpTriggerParam.appVersion");
_Static_assert(offsetof(SceCoredumpTriggerParam, triggerFlags) == 48,
	"SceCoredumpTriggerParam.triggerFlags");

typedef int (*SceKernelCoredumpStateUpdateCallback)(int task_id, SceUID pid,
	int progress);
typedef int (*SceKernelCoredumpStateFinishCallback)(int task_id, SceUID pid,
	int result, const char *path, SceSize path_len, int);

typedef SceInt32 (*SceCoredumpHandler)(void *pCommon);

#define SCE_COREDUMP_ERROR_PARAM			(-2146828288)
#define SCE_COREDUMP_ERROR_NOT_REGISTERED		(-2146828287)
#define SCE_COREDUMP_ERROR_ALREADY_REGISTERED	(-2146828286)
#define SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER	(-2146828285)

/* names unknown: a core dump file being written (os0:kd/crashdump.skprx) */
SceUID SceCoredumpForDriver_13EF8516(SceSize size);				/* open */
int SceCoredumpForDriver_12392973(SceUID dump, const void *buf, SceSize size);	/* write */
int SceCoredumpForDriver_10863B61(SceUID dump);					/* close */
