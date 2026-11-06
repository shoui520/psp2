/*
 * sce_fiber.h: the fiber library, under Sony's names and with Sony's definitions (SDK Release
 * 03.570.011, target/include/sce_fiber.h).
 */
#ifndef _SCE_FIBER_SCE_FIBER_H
#define _SCE_FIBER_SCE_FIBER_H

#include <scetypes.h>
#include <sceerror.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_FIBER_ERROR_NULL		(-2141650943)	/* 0x80590001 */
#define SCE_FIBER_ERROR_ALIGNMENT	(-2141650942)	/* 0x80590002 */
#define SCE_FIBER_ERROR_RANGE		(-2141650941)	/* 0x80590003 */
#define SCE_FIBER_ERROR_INVALID		(-2141650940)	/* 0x80590004 */
#define SCE_FIBER_ERROR_PERMISSION	(-2141650939)	/* 0x80590005 */
#define SCE_FIBER_ERROR_STATE		(-2141650938)	/* 0x80590006 */
#define SCE_FIBER_ERROR_BUSY		(-2141650937)	/* 0x80590007 */
#define SCE_FIBER_ERROR_AGAIN		(-2141650936)	/* 0x80590008 */
#define SCE_FIBER_ERROR_FATAL		(-2141650935)	/* 0x80590009 */

#define _SCE_FIBER_VERSION		(SCE_PSP2_SDK_VERSION & 0xffff0000)
#define SCE_FIBER_MAX_NAME_LENGTH	31

typedef void (*SceFiberEntry)(SceUInt32 argOnInitialize, SceUInt32 argOnRun)
	__attribute__((noreturn));

#define SCE_FIBER_SIZE		(128)
#define SCE_FIBER_ALIGNMENT	(8)
typedef struct SceFiber {
	SceUInt64 __padding__[SCE_FIBER_SIZE / sizeof(SceUInt64)];
} __attribute__((aligned(SCE_FIBER_ALIGNMENT))) SceFiber;

#define SCE_FIBER_OPT_PARAM_SIZE	(128)
#define SCE_FIBER_OPT_PARAM_ALIGNMENT	(8)
typedef struct SceFiberOptParam {
	SceUInt32 __padding32__;
	SceUInt32 debugOption;
	SceUInt64 __padding64__[SCE_FIBER_OPT_PARAM_SIZE / sizeof(SceUInt64) - 1];
} __attribute__((aligned(SCE_FIBER_OPT_PARAM_ALIGNMENT))) SceFiberOptParam;
#define SCE_FIBER_OPT_PARAM_DEBUG_OPTION_CHECK_CONTEXT_SIZE	(0x00000001u)

#define SCE_FIBER_CONTEXT_MINIMUM_SIZE	(512)
#define SCE_FIBER_CONTEXT_ALIGNMENT	(8)

#define SCE_FIBER_INFO_SIZE		(128)
#define SCE_FIBER_INFO_ALIGNMENT	(8)
typedef struct SceFiberInfo {
	SceFiberEntry entry;
	SceUInt32 argOnInitialize;
	void *addrContext;
	SceUInt32 sizeContext;
	char name[SCE_FIBER_MAX_NAME_LENGTH + 1];
	SceUInt32 sizeContextMargin;
	SceUInt8 __padding__[SCE_FIBER_INFO_SIZE - sizeof(SceFiberEntry) - sizeof(SceUInt32)
			     - sizeof(void *) - sizeof(SceUInt32)
			     - sizeof(char) * (SCE_FIBER_MAX_NAME_LENGTH + 1) - sizeof(SceUInt32)];
} __attribute__((aligned(SCE_FIBER_INFO_ALIGNMENT))) SceFiberInfo;

SceInt32 _sceFiberInitializeImpl(SceFiber *fiber, const char *name, SceFiberEntry entry,
				 SceUInt32 argOnInitialize, void *addrContext,
				 SceUInt32 sizeContext, const SceFiberOptParam *optParam,
				 SceUInt32 buildVersion);

static inline SceInt32 sceFiberInitialize(SceFiber *fiber, const char *name, SceFiberEntry entry,
					  SceUInt32 argOnInitialize, void *addrContext,
					  SceUInt32 sizeContext,
					  const SceFiberOptParam *optParam)
{
	return _sceFiberInitializeImpl(fiber, name, entry, argOnInitialize, addrContext,
				       sizeContext, optParam, _SCE_FIBER_VERSION);
}

SceInt32 sceFiberOptParamInitialize(SceFiberOptParam *optParam);
SceInt32 sceFiberFinalize(SceFiber *fiber);
SceInt32 sceFiberRun(SceFiber *fiber, SceUInt32 argOnRunTo, SceUInt32 *argOnReturn);
SceInt32 sceFiberSwitch(SceFiber *fiber, SceUInt32 argOnRunTo, SceUInt32 *argOnRun);
SceInt32 sceFiberGetSelf(SceFiber **fiber);
SceInt32 sceFiberReturnToThread(SceUInt32 argOnReturn, SceUInt32 *argOnRun);
SceInt32 sceFiberGetInfo(SceFiber *fiber, SceFiberInfo *fiberInfo);
SceInt32 sceFiberStartContextSizeCheck(SceUInt32 flags);
SceInt32 sceFiberStopContextSizeCheck(void);
SceInt32 sceFiberRenameSelf(const char *name);
SceInt32 sceFiberPushUserMarkerWithHud(const char *label, SceUInt32 color, SceUInt32 flags);
SceInt32 sceFiberPopUserMarkerWithHud(void);

#ifdef __cplusplus
}
#endif

#endif
