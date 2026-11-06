/*
 * SceFiber's exports beyond the SDK's (sce_fiber.h): the forms libult builds its threads on,
 * and SceUlobjDbg, the user-level object table the debugger reads (libfiber makes it; libult's
 * objects are entered in it too).
 */
#ifndef FIBER_FIBER_H
#define FIBER_FIBER_H

#include <scetypes.h>
#include <sce_fiber.h>
#include <ulobjmgr/ulobjmgr.h>

/* sceFiberInitialize with ULT_FIBER_FLAG_* options */
SceInt32 _sceFiberInitializeWithInternalOptionImpl(SceFiber *fiber, const char *name,
						   SceFiberEntry entry,
						   SceUInt32 argOnInitialize,
						   void *addrContext, SceUInt32 sizeContext,
						   const SceFiberOptParam *optParam,
						   SceUInt32 internalOption);

/* sceFiberRun and sceFiberSwitch giving a fiber made without one its context area */
SceInt32 _sceFiberAttachContextAndRun(SceFiber *fiber, void *addrContext, SceUInt32 sizeContext,
				      SceUInt32 argOnRunTo, SceUInt32 *argOnReturn);
SceInt32 _sceFiberAttachContextAndSwitch(SceFiber *fiber, void *addrContext,
					 SceUInt32 sizeContext, SceUInt32 argOnRunTo,
					 SceUInt32 *argOnRun);

/* the table (NULL when the kernel gave none) */
extern UlobjInfoArray *SceUlobjDbg_DE6CFF47;

/* enters an object (8-byte aligned) of a kind (ULOBJ_KIND_*); *id its entry, 0x1fffffff if none */
int SceUlobjDbg_D7F0F610(void *object, SceUInt32 kind, SceUInt32 *id);
/* frees an entry */
int SceUlobjDbg_F9C0F5DA(SceUInt32 id);

#endif
