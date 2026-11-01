#ifndef NP_COMMON_INTERNAL_H
#define NP_COMMON_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <np/np_common.h>
#include <np/np_error.h>
#include <np/np_auth.h>
#include <modulemgr/module_info.h>
#include <regmgr/regmgr.h>

/* the imports SceNpCommon takes from libraries the tree has no header of yet */
extern SceUInt64 sceKernelGetSystemTimeWide(void);
extern int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);
extern int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *info);
extern int sceSblQafMgrIsAllowNpFullTest(void);
extern int sceSblQafMgrIsAllowNpTest(void);
extern SceUInt32 vshSblAimgrIsCEX(void);
extern SceUInt32 vshSblUtMgrHasStoreFlag(void);

/* Error codes the SDK headers do not name (names unknown). */
#define NP_COMMON_ERROR_NOT_INITIALIZED		-2141880319	/* 0x80558001 */
#define NP_COMMON_ERROR_ALREADY_INITIALIZED	-2141880318	/* 0x80558002 */
#define NP_COMMON_ERROR_INVALID_ARGUMENT	-2141880317	/* 0x80558003 */
#define NP_COMMON_ERROR_INVALID_DATA		-2141880316	/* 0x80558004 */
#define NP_COMMON_ERROR_ALREADY_QUEUED		-2141880314	/* 0x80558006 */

/* the NP settings a caller passes in, each the default for a registry setting */
typedef struct NpCommonSettings {
	int fullTest;			/* 0x00: default of system parameter 750541 */
	char serverName[40];		/* 0x04 */
	int test;			/* 0x2C: default of system parameter 656533 */
	int param749261;		/* 0x30 */
	int param858341;		/* 0x34 */
	int param1401993;		/* 0x38 */
} NpCommonSettings;

/* a timer of a callout thread, in its list sorted by time */
typedef struct NpCallout {
	struct NpCallout *next;		/* 0x00 */
	void (*func)(void *arg);	/* 0x04 */
	SceUInt64 time;			/* 0x08 */
	void *arg;			/* 0x10 */
	SceUInt32 reserved;		/* 0x14 */
} NpCallout;

/* a callout thread with its timer list */
typedef struct NpCalloutContext {
	SceUID thread;			/* 0x00 */
	SceUID mutex;			/* 0x04 */
	SceUID cond;			/* 0x08 */
	int initialized;		/* 0x0C */
	int stop;			/* 0x10 */
	NpCallout *list;		/* 0x14 */
} NpCalloutContext;

/* util.c */
int SceNpCommon_619FB198(SceSize size, void *memblock);
void SceNpCommon_622F004B(void);

#endif /* NP_COMMON_INTERNAL_H */
