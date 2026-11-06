/*
 * libfiber's own types: what a thread running fibers keeps (its TLS word 65 points to it, on
 * the stack of the sceFiberRun that started them), how a fiber is started, and the context
 * switch (fiber_context.S).
 */
#ifndef FIBER_FIBER_INTERNAL_H
#define FIBER_FIBER_INTERNAL_H

#include <scetypes.h>
#include <ulobjmgr/ulobjmgr.h>

#define FIBER_TLS_THREAD	65	/* FiberThreadContext * while fibers run */
#define FIBER_TLS_STACK_LIMIT	2	/* the thread's UltFiberStackLimit */

/* sceFiberOptParamInitialize marks the parameters with it */
#define FIBER_OPT_PARAM_MAGIC	0xbb40e64d

typedef struct FiberThreadContext {
	UltFiberContext context;	/* the thread's, while a fiber runs */
	UltFiber *currentFiber;
	SceUInt32 argOnRun;		/* sceFiberSwitch and sceFiberRun's argOnRunTo */
	SceUInt32 argOnReturn;		/* sceFiberReturnToThread's */
	SceUInt32 entryReturned;	/* a fiber's entry returned */
	SceUID threadId;
	SceUInt32 padding;
	UltFiberStackLimit stackLimit;	/* the thread's own */
} FiberThreadContext;

_Static_assert(sizeof(FiberThreadContext) == 0x90, "FiberThreadContext");

/* a fiber's start: its entry, the arguments, its stack, and its stack limit to set */
typedef struct FiberStartParam {
	SceUInt32 entry;
	SceUInt32 argOnInitialize;
	SceUInt32 argOnRun;
	SceUIntPtr sp;
	UltFiberStackLimit *stackLimit;
	const UltFiberStackLimit *stackLimitFrom;
} FiberStartParam;

/* and the fiber that ran, released once the new one has its stack */
typedef struct FiberStartAndReleaseParam {
	FiberStartParam start;
	SceUInt32 *magic;
} FiberStartAndReleaseParam;

void startFiber(const FiberStartParam *param);
void startFiberAndRelease(const FiberStartAndReleaseParam *param);
int saveFiberContext(UltFiberContext *context);
void restoreFiberContext(const UltFiberContext *context);
void restoreFiberContextAndRelease(const UltFiberContext *context, SceUInt32 *magic);

/* fiber.c */
void fiberAssert(int condition, const char *expression, const char *file, SceUInt32 line);
void exitFiber(SceUInt32 returned);

#endif
