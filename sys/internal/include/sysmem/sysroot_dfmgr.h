#pragma once

#include <stddef.h>
#include <scetypes.h>

/* System-software tool progress services registered by NID 579CC758. */
typedef struct SysrootDfmgrOps {
	SceSize size;
	int (*updateProgressStarted)(SceInt32 kind, SceUInt32 total, const char *msg, int len);
	int (*updateProgressOngoing)(SceInt32 kind, SceUInt32 percent);
	int (*updateFinished)(SceUInt32 result, const char *msg, int len);
	int (*installProgressStarted)(SceInt32 kind, SceUInt32 total, const char *msg, int len);
	int (*installProgressOngoing)(SceInt32 kind, SceUInt32 percent);
	int (*installFinished)(SceUInt32 result, const char *msg, int len);
	int (*uninstallProgressStarted)(SceInt32 kind, SceUInt32 total, const char *msg,
					int len);
	int (*uninstallProgressOngoing)(SceInt32 kind, SceUInt32 percent);
	int (*uninstallFinished)(SceUInt32 result, const char *msg, int len);
	int (*saveDataInstallProgressStarted)(SceInt32 kind, SceUInt32 total,
					      const char *msg, int len);
	int (*saveDataInstallProgressOngoing)(SceInt32 kind, SceUInt32 percent);
	int (*saveDataInstallFinished)(SceUInt32 result, const char *msg, int len);
	int (*saveDataFormatProgressStarted)(SceInt32 kind, SceUInt32 total,
					     const char *msg, int len);
	int (*saveDataFormatProgressOngoing)(SceInt32 kind, SceUInt32 percent);
	int (*saveDataFormatFinished)(SceUInt32 result, const char *msg, int len);
} SysrootDfmgrOps;

#ifndef __cplusplus
_Static_assert(sizeof(SysrootDfmgrOps) == 0x40,
	       "Sysroot DECI4P progress service table size");
_Static_assert(offsetof(SysrootDfmgrOps, saveDataFormatFinished) == 0x3c,
	       "Sysroot DECI4P final progress service offset");
#endif
