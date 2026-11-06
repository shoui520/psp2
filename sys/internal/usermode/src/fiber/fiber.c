/*
 * SceFiber: fibers, user-level contexts a thread runs one at a time and switches between
 * (the file's name ours; Sony's libfiber names only ulobj_dbg.c).
 *
 * A fiber (UltFiber, the SDK's SceFiber) is checksummed and its entry, argument and name
 * stored masked, so that a corrupted one is refused. Its magic is the lock: SAVED while it
 * waits, RUNNING while a thread runs it. sceFiberRun starts the first fiber of a thread,
 * which then switches between fibers until one returns to the thread. A fiber with a context
 * area runs on its own stack and keeps its registers on it when it switches away; a fiber
 * without one runs on the thread's stack and starts over at its entry each time.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <intrinsics.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <libperf/razor.h>
#include <fiber/fiber.h>
#include "fiber_internal.h"

#define RAZOR_FIBER_PACKET		1
#define RAZOR_FIBER_CREATE		0x01
#define RAZOR_FIBER_DELETE		0x04
#define RAZOR_FIBER_SWITCH_OUT		0x43
#define RAZOR_FIBER_SWITCH_IN		0x82

static SceUInt32 sContextSizeCheck = 0;		/* sceFiberStartContextSizeCheck */
static SceUInt32 sRandomSeed = 0x12345678;
/*
 * &_pLibPerfCaptureFlagPtr, stored and read back so the weak import's address is tested; a
 * global, since Sony's code reads the fibers' fields only after the store
 */
const void *volatile gLibPerfCaptureFlag;

/* the fiber's name is stored XORed with this */
static const SceUInt8 sNameKey[SCE_FIBER_MAX_NAME_LENGTH + 1] = {
	0xaa, 0xaa, 0x55, 0xa5, 0xaa, 0x55, 0xaa, 0xa5,
	0x33, 0xc3, 0xcc, 0x33, 0xcc, 0xc3, 0x55, 0x55,
	0x99, 0x66, 0x99, 0x96, 0x33, 0x33, 0xcc, 0xcc,
	0x5a, 0x5a, 0x66, 0x66, 0x99, 0x99, 0x66, 0x96,
};

void fiberAssert(int condition, const char *expression, const char *file, SceUInt32 line)
{
	if (!condition) {
		sceClibPrintf("PANIC:line %u in %s : %s\n", line, file, expression);
		sceClibAbort();
	}
}

#define FIBER_RANDOM_MULTIPLIER	0xfedcba89
#define FIBER_RANDOM_INCREMENT	0x91827365

static inline SceUInt32 nextRandom(void)
{
	sRandomSeed = sRandomSeed * FIBER_RANDOM_MULTIPLIER + FIBER_RANDOM_INCREMENT;
	return sRandomSeed;
}

static inline SceUInt32 calcChecksum(const UltFiber *fiber)
{
	const SceUInt32 *word = &fiber->entry;
	SceUInt32 checksum = 0;
	int i;

	for (i = 0; i < 15; i++) {
		checksum = checksum ^ word[0] ^ word[1];
		word += 2;
	}
	return checksum;
}

/* a fiber is locked by setting its magic from SAVED to RUNNING; the magic it had */
static inline SceUInt32 lockFiber(UltFiber *fiber, SceUInt32 to)
{
	SceUInt32 magic;

	do {
		magic = __builtin_ldrex(&fiber->magic);
		if (magic != ULT_FIBER_MAGIC_SAVED)
			break;
	} while (__builtin_strex(to, &fiber->magic) != 0);
	return magic;
}

static inline void unlockFiber(UltFiber *fiber)
{
	__builtin_dmb();
	fiber->magic = ULT_FIBER_MAGIC_SAVED;
}

/* a fiber is traced while a capture runs, unless its flags say not */
#define RAZOR_CAPTURING(flags)							\
	(gLibPerfCaptureFlag = &_pLibPerfCaptureFlagPtr,			\
	 ((flags) & ULT_FIBER_FLAG_NO_RAZOR) == 0 && gLibPerfCaptureFlag != NULL	\
	 && *_pLibPerfCaptureFlagPtr != 0)

static inline FiberThreadContext **getThreadContext(void)
{
	return (FiberThreadContext **)sceKernelGetTLSAddr(FIBER_TLS_THREAD);
}

static inline UltFiberStackLimit *getStackLimit(void)
{
	return (UltFiberStackLimit *)sceKernelGetTLSAddr(FIBER_TLS_STACK_LIMIT);
}

/* no stack limit while the fibers switch */
static inline void clearStackLimit(void)
{
	UltFiberStackLimit *limit = getStackLimit();

	limit->top = 0xffffffff;
	limit->bottom = 0;
}

/* the fill of a context area's first word is gone: its stack overflowed */
static inline void checkContextArea(const UltFiber *fiber)
{
	if (fiber->addrContext != 0
	    && *(const SceUInt32 *)fiber->addrContext != ULT_FIBER_CONTEXT_FILL_WORD) {
		sceClibPrintf("PANIC:fiber's context area is corrupted.\n");
		sceClibAbort();
	}
}

static inline void setFiberContext(UltFiber *fiber, UltFiberContext *context)
{
	SceUIntPtr old = fiber->context;

	fiber->context = (SceUIntPtr)context;
	fiber->checksum = old ^ (SceUIntPtr)context ^ fiber->checksum;
}

static inline void getFiberName(const UltFiber *fiber, char *name)
{
	const SceUInt8 *encoded = (const SceUInt8 *)fiber->encodedName;
	SceUInt8 c;
	int i;

	i = 0;
	for (;;) {
		c = encoded[i] ^ sNameKey[i];
		name[i] = c;
		i++;
		if (c == '\0') {
			for (; i < SCE_FIBER_MAX_NAME_LENGTH + 1; i++)
				name[i] = '\0';
			return;
		}
		if (i == SCE_FIBER_MAX_NAME_LENGTH) {
			name[i] = '\0';
			return;
		}
	}
}

/* zeroes size bytes, a word at a time */
static inline void clearWords(void *p, SceSize size)
{
	SceUInt32 *word = p;

	while (size != 0) {
		size -= sizeof(*word);
		*word++ = 0;
	}
}

/* all of a context area is filled to measure the stack it uses, else its first word */
static inline void fillContextArea(SceUInt32 flags, void *addrContext, SceUIntPtr end)
{
	SceUInt64 *p;

	if ((flags & ULT_FIBER_FLAG_CONTEXT_SIZE_CHECK) != 0) {
		for (p = addrContext; (SceUIntPtr)p < end; p++)
			*p = ULT_FIBER_CONTEXT_FILL;
	} else {
		*(SceUInt32 *)addrContext = ULT_FIBER_CONTEXT_FILL_WORD;
	}
}

/* a context area for a fiber made without one: it must have none, and be one */
static inline SceInt32 checkAttachedContext(const UltFiber *fiber, void *addrContext,
					    SceUInt32 sizeContext)
{
	if (addrContext != NULL || sizeContext != 0) {
		if (sizeContext == 0)
			return SCE_FIBER_ERROR_INVALID;
		if (sizeContext < SCE_FIBER_CONTEXT_MINIMUM_SIZE)
			return SCE_FIBER_ERROR_RANGE;
		if (addrContext == NULL || (sizeContext & 7) != 0 || fiber->addrContext != 0)
			return SCE_FIBER_ERROR_INVALID;
	}
	return SCE_OK;
}

static inline void attachContext(UltFiber *fiber, void *addrContext, SceUInt32 sizeContext)
{
	SceUIntPtr end = (SceUIntPtr)addrContext + sizeContext;

	fiber->addrContext = (SceUIntPtr)addrContext;
	fiber->sizeContext = sizeContext;
	fiber->stackLimit.top = end;
	fiber->stackLimit.bottom = (SceUIntPtr)addrContext;
	fiber->checksum = calcChecksum(fiber);
	fillContextArea(fiber->flags, addrContext, end);
}

/* the thread runs a fiber again: what it was switched back with, and its stack limit */
static inline SceInt32 fiberResumed(SceUInt32 *argOnRun)
{
	FiberThreadContext **thread = getThreadContext();
	FiberThreadContext *threadContext;
	UltFiber *current;

	if (argOnRun != NULL)
		*argOnRun = (*thread)->argOnRun;
	threadContext = *thread;
	current = threadContext->currentFiber;
	if (current->addrContext != 0)
		*getStackLimit() = current->stackLimit;
	else
		*getStackLimit() = threadContext->stackLimit;
	return SCE_OK;
}

/* the fiber's entry returned: the thread resumes from sceFiberRun with an error */
void exitFiber(SceUInt32 returned)
{
	FiberThreadContext **thread = getThreadContext();

	if (RAZOR_CAPTURING((*thread)->currentFiber->flags))
		_sceRazorCpuWriteFiberUltPkt(RAZOR_FIBER_PACKET, RAZOR_FIBER_SWITCH_OUT, 0,
					     (*thread)->currentFiber, NULL);
	(*thread)->entryReturned = returned;
	restoreFiberContextAndRelease(&(*thread)->context, &(*thread)->currentFiber->magic);
	for (;;)
		;
}

/* random bytes, then the name over them up to its terminator */
static void setFiberName(const char *name, SceUInt64 *encodedName)
{
	const SceUInt8 *src = (const SceUInt8 *)name;
	SceUInt32 *word = (SceUInt32 *)encodedName;
	SceUInt8 *encoded = (SceUInt8 *)encodedName;
	int i;

	for (i = 0; i < 4; i++) {
		word[0] = nextRandom();
		word[1] = nextRandom();
		word += 2;
	}
	for (i = 0; src[i] != '\0'; i++) {
		if (i == SCE_FIBER_MAX_NAME_LENGTH) {
			encoded[i] = sNameKey[i];
			return;
		}
		encoded[i] = src[i] ^ sNameKey[i];
	}
	encoded[i] = src[i] ^ sNameKey[i];
}

static SceInt32 initializeFiber(UltFiber *fiber, const char *name, SceFiberEntry entry,
				SceUInt32 argOnInitialize, void *addrContext,
				SceUInt32 sizeContext, const SceFiberOptParam *optParam,
				SceUInt32 internalOption)
{
	SceUInt32 ulobjId;
	SceUInt32 seed;
	SceUIntPtr end;
	int i;

	if (fiber == NULL || entry == NULL || name == NULL)
		return SCE_FIBER_ERROR_NULL;
	if (((SceUIntPtr)fiber & 7) != 0 || ((SceUIntPtr)addrContext & 7) != 0
	    || (optParam != NULL && ((SceUIntPtr)optParam & 7) != 0))
		return SCE_FIBER_ERROR_ALIGNMENT;
	if (sizeContext != 0 && sizeContext < SCE_FIBER_CONTEXT_MINIMUM_SIZE)
		return SCE_FIBER_ERROR_RANGE;
	if ((addrContext == NULL && sizeContext != 0) || (addrContext != NULL && sizeContext == 0)
	    || (sizeContext & 7) != 0)
		return SCE_FIBER_ERROR_INVALID;
	if (optParam != NULL && optParam->__padding32__ != FIBER_OPT_PARAM_MAGIC)
		return SCE_FIBER_ERROR_INVALID;

	seed = sRandomSeed;
	for (i = 0; i < 10; i++) {
		seed = seed * FIBER_RANDOM_MULTIPLIER + FIBER_RANDOM_INCREMENT;
		fiber->randomFill[i] = seed;
	}
	sRandomSeed = seed;
	setFiberName(name, fiber->encodedName);
	fiber->addrContext = (SceUIntPtr)addrContext;
	fiber->sizeContext = sizeContext;
	fiber->context = 0;
	fiber->entry = (SceUInt32)entry ^ ULT_FIBER_ENTRY_KEY;
	fiber->argOnInitialize = argOnInitialize ^ ULT_FIBER_ARG_KEY;
	fiber->threadId = 0;
	fiber->flags = internalOption;
	clearWords(fiber->razorMarker, sizeof(fiber->razorMarker));
	if (sContextSizeCheck == 1)
		internalOption |= ULT_FIBER_FLAG_CONTEXT_SIZE_CHECK;
	fiber->flags = internalOption;
	if (addrContext != NULL) {
		end = (SceUIntPtr)addrContext + sizeContext;
		fiber->stackLimit.bottom = (SceUIntPtr)addrContext;
		fiber->stackLimit.top = end;
		fillContextArea(internalOption, addrContext, end);
	}
	if ((internalOption & ULT_FIBER_FLAG_NO_ULOBJ) != 0) {
		ulobjId = nextRandom();
	} else {
		SceUlobjDbg_D7F0F610(fiber, ULOBJ_KIND_FIBER, &ulobjId);
		ulobjId ^= ULT_FIBER_ULOBJ_ID_KEY;
	}
	fiber->ulobjId = ulobjId;
	fiber->checksum = calcChecksum(fiber);
	unlockFiber(fiber);
	if (RAZOR_CAPTURING(internalOption))
		_sceRazorCpuWriteFiberUltPkt(RAZOR_FIBER_PACKET, RAZOR_FIBER_CREATE, 0, fiber, NULL);
	return SCE_OK;
}

SceInt32 _sceFiberInitializeImpl(SceFiber *fiber, const char *name, SceFiberEntry entry,
				 SceUInt32 argOnInitialize, void *addrContext,
				 SceUInt32 sizeContext, const SceFiberOptParam *optParam,
				 SceUInt32 buildVersion)
{
	return initializeFiber((UltFiber *)fiber, name, entry, argOnInitialize, addrContext,
			       sizeContext, optParam, 0);
}

SceInt32 _sceFiberInitializeWithInternalOptionImpl(SceFiber *fiber, const char *name,
						   SceFiberEntry entry,
						   SceUInt32 argOnInitialize,
						   void *addrContext, SceUInt32 sizeContext,
						   const SceFiberOptParam *optParam,
						   SceUInt32 internalOption)
{
	return initializeFiber((UltFiber *)fiber, name, entry, argOnInitialize, addrContext,
			       sizeContext, optParam, internalOption);
}

SceInt32 sceFiberOptParamInitialize(SceFiberOptParam *optParam)
{
	if (optParam == NULL)
		return SCE_FIBER_ERROR_NULL;
	if (((SceUIntPtr)optParam & 7) != 0)
		return SCE_FIBER_ERROR_ALIGNMENT;
	clearWords(optParam, sizeof(*optParam));
	optParam->__padding32__ = FIBER_OPT_PARAM_MAGIC;
	return SCE_OK;
}

SceInt32 sceFiberFinalize(SceFiber *fiber)
{
	UltFiber *ultFiber = (UltFiber *)fiber;
	char name[SCE_FIBER_MAX_NAME_LENGTH + 1];
	SceUInt32 magic;

	if (fiber == NULL)
		return SCE_FIBER_ERROR_NULL;
	if (((SceUIntPtr)fiber & 7) != 0)
		return SCE_FIBER_ERROR_ALIGNMENT;
	magic = lockFiber(ultFiber, ULT_FIBER_MAGIC_FINALIZED);
	if (magic == ULT_FIBER_MAGIC_RUNNING)
		return SCE_FIBER_ERROR_STATE;
	if (magic != ULT_FIBER_MAGIC_SAVED)
		return SCE_FIBER_ERROR_INVALID;
	if (ultFiber->checksum != calcChecksum(ultFiber))
		return SCE_FIBER_ERROR_INVALID;
	if (RAZOR_CAPTURING(ultFiber->flags)) {
		getFiberName(ultFiber, name);
		_sceRazorCpuWriteFiberUltPkt(RAZOR_FIBER_PACKET, RAZOR_FIBER_DELETE, 1, ultFiber,
					     name);
	}
	__builtin_dmb();
	if ((ultFiber->flags & ULT_FIBER_FLAG_NO_ULOBJ) == 0)
		SceUlobjDbg_F9C0F5DA(ultFiber->ulobjId ^ ULT_FIBER_ULOBJ_ID_KEY);
	return SCE_OK;
}

/*
 * The first fiber of a thread: the thread's registers are kept in a FiberThreadContext on this
 * stack, and the thread resumes here when a fiber returns to it.
 */
static SceInt32 runFiber(UltFiber *fiber, void *addrContext, SceUInt32 sizeContext,
			 SceUInt32 argOnRunTo, SceUInt32 *argOnReturn)
{
	FiberThreadContext threadContext;
	FiberThreadContext **thread;
	SceUInt32 magic;
	SceUID threadId;
	SceInt32 ret;

	if (fiber == NULL)
		return SCE_FIBER_ERROR_NULL;
	if ((SceInt32)fiber % SCE_FIBER_ALIGNMENT != 0 || ((SceUIntPtr)addrContext & 7) != 0)
		return SCE_FIBER_ERROR_ALIGNMENT;
	thread = getThreadContext();
	if (*thread != NULL)
		return SCE_FIBER_ERROR_PERMISSION;
	if (addrContext != NULL || sizeContext != 0) {
		if (sizeContext == 0)
			return SCE_FIBER_ERROR_INVALID;
		if (sizeContext < SCE_FIBER_CONTEXT_MINIMUM_SIZE)
			return SCE_FIBER_ERROR_RANGE;
		if (addrContext == NULL || (sizeContext & 7) != 0 || fiber->addrContext != 0)
			return SCE_FIBER_ERROR_INVALID;
	}
	magic = lockFiber(fiber, ULT_FIBER_MAGIC_RUNNING);
	if (magic == ULT_FIBER_MAGIC_RUNNING)
		return SCE_FIBER_ERROR_STATE;
	if (magic != ULT_FIBER_MAGIC_SAVED)
		return SCE_FIBER_ERROR_INVALID;
	if (fiber->checksum != calcChecksum(fiber)) {
		unlockFiber(fiber);
		return SCE_FIBER_ERROR_INVALID;
	}
	if (addrContext != NULL || sizeContext != 0)
		attachContext(fiber, addrContext, sizeContext);
	threadId = sceKernelGetThreadId();
	fiber->checksum = (fiber->threadId ^ threadId) ^ fiber->checksum;
	fiber->threadId = threadId;
	if (RAZOR_CAPTURING(fiber->flags))
		_sceRazorCpuWriteFiberUltPkt(RAZOR_FIBER_PACKET, RAZOR_FIBER_SWITCH_IN, 0, fiber,
					     NULL);
	*thread = &threadContext;
	threadContext.currentFiber = fiber;
	(*thread)->entryReturned = 0;
	(*thread)->threadId = threadId;
	(*thread)->stackLimit = *getStackLimit();
	clearStackLimit();
	if (saveFiberContext(&(*thread)->context) == 0) {
		if (fiber->addrContext != 0) {
			if (fiber->context == 0) {
				FiberStartParam start;

				start.entry = fiber->entry ^ ULT_FIBER_ENTRY_KEY;
				start.argOnInitialize = fiber->argOnInitialize ^ ULT_FIBER_ARG_KEY;
				start.argOnRun = argOnRunTo;
				start.sp = (fiber->addrContext + fiber->sizeContext - 1) & ~7;
				start.stackLimit = getStackLimit();
				start.stackLimitFrom = &fiber->stackLimit;
				startFiber(&start);
			} else {
				(*thread)->argOnRun = argOnRunTo;
				restoreFiberContext((const UltFiberContext *)fiber->context);
			}
		} else {
			FiberStartParam start;

			start.entry = fiber->entry ^ ULT_FIBER_ENTRY_KEY;
			start.argOnInitialize = fiber->argOnInitialize ^ ULT_FIBER_ARG_KEY;
			start.argOnRun = argOnRunTo;
			start.sp = (*thread)->context.sp;
			start.stackLimit = getStackLimit();
			start.stackLimitFrom = &(*thread)->stackLimit;
			startFiber(&start);
		}
	}

	/* a fiber returned to the thread */
	fiber = (*thread)->currentFiber;
	(*thread)->currentFiber = NULL;
	*getStackLimit() = (*thread)->stackLimit;
	if ((*thread)->entryReturned != 0) {
		if (argOnReturn != NULL)
			*argOnReturn = (SceUInt32)fiber;
		ret = SCE_FIBER_ERROR_STATE;
	} else {
		if (argOnReturn != NULL)
			*argOnReturn = (*thread)->argOnReturn;
		ret = SCE_OK;
	}
	*thread = NULL;
	return ret;
}

SceInt32 sceFiberRun(SceFiber *fiber, SceUInt32 argOnRunTo, SceUInt32 *argOnReturn)
{
	return runFiber((UltFiber *)fiber, NULL, 0, argOnRunTo, argOnReturn);
}

SceInt32 _sceFiberAttachContextAndRun(SceFiber *fiber, void *addrContext, SceUInt32 sizeContext,
				      SceUInt32 argOnRunTo, SceUInt32 *argOnReturn)
{
	return runFiber((UltFiber *)fiber, addrContext, sizeContext, argOnRunTo, argOnReturn);
}

/* the next fiber runs, and the current one is released once it is off its stack */
static void switchToFiber(UltFiber *current, UltFiber *next, SceUInt32 argOnRunTo,
			  FiberThreadContext *thread)
{
	SceUInt32 *magic;

	thread->currentFiber = next;
	if (RAZOR_CAPTURING(next->flags))
		_sceRazorCpuWriteFiberUltPkt(RAZOR_FIBER_PACKET, RAZOR_FIBER_SWITCH_IN, 0, next,
					     NULL);
	magic = &current->magic;
	if (next->addrContext != 0) {
		if (next->context == 0) {
			FiberStartAndReleaseParam start;

			start.start.entry = next->entry ^ ULT_FIBER_ENTRY_KEY;
			start.start.argOnInitialize = next->argOnInitialize ^ ULT_FIBER_ARG_KEY;
			start.start.argOnRun = argOnRunTo;
			start.start.sp = (next->addrContext + next->sizeContext - 1) & ~7;
			start.start.stackLimit = getStackLimit();
			start.start.stackLimitFrom = &next->stackLimit;
			start.magic = magic;
			startFiberAndRelease(&start);
		} else {
			thread->argOnRun = argOnRunTo;
			restoreFiberContextAndRelease((const UltFiberContext *)next->context, magic);
		}
	} else {
		FiberStartAndReleaseParam start;

		start.start.entry = next->entry ^ ULT_FIBER_ENTRY_KEY;
		start.start.argOnInitialize = next->argOnInitialize ^ ULT_FIBER_ARG_KEY;
		start.start.argOnRun = argOnRunTo;
		start.start.sp = thread->context.sp;
		start.start.stackLimit = getStackLimit();
		start.start.stackLimitFrom = &thread->stackLimit;
		start.magic = magic;
		startFiberAndRelease(&start);
	}
	for (;;)
		;
}

SceInt32 sceFiberSwitch(SceFiber *fiber, SceUInt32 argOnRunTo, SceUInt32 *argOnRun)
{
	UltFiber *next = (UltFiber *)fiber;
	FiberThreadContext **thread;
	UltFiberContext context;
	UltFiber *current;
	SceUInt32 magic;
	SceUInt32 threadId;

	if (fiber == NULL)
		return SCE_FIBER_ERROR_NULL;
	if ((SceInt32)fiber % SCE_FIBER_ALIGNMENT != 0)
		return SCE_FIBER_ERROR_ALIGNMENT;
	thread = getThreadContext();
	if (*thread == NULL)
		return SCE_FIBER_ERROR_PERMISSION;
	current = (*thread)->currentFiber;
	magic = lockFiber(next, ULT_FIBER_MAGIC_RUNNING);
	if (magic == ULT_FIBER_MAGIC_RUNNING)
		return SCE_FIBER_ERROR_STATE;
	if (magic != ULT_FIBER_MAGIC_SAVED)
		return SCE_FIBER_ERROR_INVALID;
	if (next->checksum != calcChecksum(next)) {
		unlockFiber(next);
		return SCE_FIBER_ERROR_INVALID;
	}
	if (RAZOR_CAPTURING(current->flags))
		_sceRazorCpuWriteFiberUltPkt(RAZOR_FIBER_PACKET, RAZOR_FIBER_SWITCH_OUT, 0,
					     current, NULL);
	threadId = next->threadId;
	next->threadId = (*thread)->threadId;
	next->checksum = threadId ^ (*thread)->threadId ^ next->checksum;
	clearStackLimit();
	if (current->addrContext != 0) {
		if (saveFiberContext(&context) == 0) {
			setFiberContext(current, &context);
			checkContextArea((*thread)->currentFiber);
			switchToFiber(current, next, argOnRunTo, *thread);
		}
	} else {
		switchToFiber(current, next, argOnRunTo, *thread);
	}
	return fiberResumed(argOnRun);
}

SceInt32 _sceFiberAttachContextAndSwitch(SceFiber *fiber, void *addrContext,
					 SceUInt32 sizeContext, SceUInt32 argOnRunTo,
					 SceUInt32 *argOnRun)
{
	UltFiber *next = (UltFiber *)fiber;
	FiberThreadContext **thread;
	UltFiberContext context;
	UltFiber *current;
	SceUInt32 magic;
	SceUInt32 threadId;
	SceInt32 ret;

	if (fiber == NULL)
		return SCE_FIBER_ERROR_NULL;
	if ((SceInt32)fiber % SCE_FIBER_ALIGNMENT != 0 || ((SceUIntPtr)addrContext & 7) != 0)
		return SCE_FIBER_ERROR_ALIGNMENT;
	thread = getThreadContext();
	if (*thread == NULL)
		return SCE_FIBER_ERROR_PERMISSION;
	ret = checkAttachedContext(next, addrContext, sizeContext);
	if (ret != SCE_OK)
		return ret;
	current = (*thread)->currentFiber;
	magic = lockFiber(next, ULT_FIBER_MAGIC_RUNNING);
	if (magic == ULT_FIBER_MAGIC_RUNNING)
		return SCE_FIBER_ERROR_STATE;
	if (magic != ULT_FIBER_MAGIC_SAVED)
		return SCE_FIBER_ERROR_INVALID;
	if (next->checksum != calcChecksum(next)) {
		unlockFiber(next);
		return SCE_FIBER_ERROR_INVALID;
	}
	if (RAZOR_CAPTURING(current->flags))
		_sceRazorCpuWriteFiberUltPkt(RAZOR_FIBER_PACKET, RAZOR_FIBER_SWITCH_OUT, 0,
					     current, NULL);
	if (addrContext != NULL || sizeContext != 0)
		attachContext(next, addrContext, sizeContext);
	threadId = next->threadId;
	next->threadId = (*thread)->threadId;
	next->checksum ^= threadId ^ (*thread)->threadId;
	clearStackLimit();
	if (current->addrContext != 0) {
		if (saveFiberContext(&context) == 0) {
			setFiberContext(current, &context);
			checkContextArea((*thread)->currentFiber);
			switchToFiber(current, next, argOnRunTo, *thread);
		}
	} else {
		switchToFiber(current, next, argOnRunTo, *thread);
	}
	return fiberResumed(argOnRun);
}

SceInt32 sceFiberGetSelf(SceFiber **fiber)
{
	FiberThreadContext *thread;
	UltFiber *current;

	if (fiber == NULL)
		return SCE_FIBER_ERROR_NULL;
	thread = *getThreadContext();
	current = NULL;
	if (thread != NULL)
		current = thread->currentFiber;
	*fiber = (SceFiber *)current;
	return SCE_OK;
}

/* the thread's registers are restored, and the fiber released */
static inline void returnToThread(FiberThreadContext *thread, UltFiber *current,
				  SceUInt32 argOnReturn)
{
	if (RAZOR_CAPTURING(current->flags))
		_sceRazorCpuWriteFiberUltPkt(RAZOR_FIBER_PACKET, RAZOR_FIBER_SWITCH_OUT, 0,
					     current, NULL);
	thread->argOnReturn = argOnReturn;
	restoreFiberContextAndRelease(&thread->context, &current->magic);
	for (;;)
		;
}

SceInt32 sceFiberReturnToThread(SceUInt32 argOnReturn, SceUInt32 *argOnRun)
{
	FiberThreadContext **thread = getThreadContext();
	FiberThreadContext *threadContext = *thread;
	UltFiberContext context;
	UltFiber *current;

	if (threadContext == NULL)
		return SCE_FIBER_ERROR_PERMISSION;
	current = threadContext->currentFiber;
	if (current->addrContext != 0) {
		clearStackLimit();
		if (saveFiberContext(&context) == 0) {
			setFiberContext(current, &context);
			checkContextArea((*thread)->currentFiber);
			returnToThread(*thread, current, argOnReturn);
		}
	} else {
		returnToThread(threadContext, current, argOnReturn);
	}
	return fiberResumed(argOnRun);
}

SceInt32 sceFiberGetInfo(SceFiber *fiber, SceFiberInfo *fiberInfo)
{
	const UltFiber *ultFiber = (const UltFiber *)fiber;
	const SceUInt32 *bottom;
	const SceUInt64 *p;
	void *addrContext;
	SceUInt32 margin;
	SceUInt32 magic;

	if (fiber == NULL || fiberInfo == NULL)
		return SCE_FIBER_ERROR_NULL;
	if ((SceInt32)fiber % SCE_FIBER_ALIGNMENT != 0
	    || (SceInt32)fiberInfo % SCE_FIBER_INFO_ALIGNMENT != 0)
		return SCE_FIBER_ERROR_ALIGNMENT;
	magic = ultFiber->magic;
	if (magic != ULT_FIBER_MAGIC_RUNNING && magic != ULT_FIBER_MAGIC_SAVED)
		return SCE_FIBER_ERROR_INVALID;
	fiberInfo->entry = (SceFiberEntry)(ultFiber->entry ^ ULT_FIBER_ENTRY_KEY);
	fiberInfo->argOnInitialize = ultFiber->argOnInitialize ^ ULT_FIBER_ARG_KEY;
	addrContext = (void *)ultFiber->addrContext;
	fiberInfo->addrContext = addrContext;
	fiberInfo->sizeContext = ultFiber->sizeContext;
	getFiberName(ultFiber, fiberInfo->name);
	fiberInfo->sizeContextMargin = 0xffffffff;
	if ((ultFiber->flags & ULT_FIBER_FLAG_CONTEXT_SIZE_CHECK) != 0 && addrContext != NULL) {
		/* the fill left above the stack's deepest point */
		bottom = (const SceUInt32 *)ultFiber->stackLimit.bottom;
		if (*bottom == ULT_FIBER_CONTEXT_FILL_WORD) {
			for (p = (const SceUInt64 *)bottom + 1;
			     (SceUIntPtr)p < ultFiber->stackLimit.top; p++) {
				if (*p != ULT_FIBER_CONTEXT_FILL)
					break;
			}
			margin = (const char *)p - (const char *)((const SceUInt64 *)bottom + 1);
		} else {
			margin = 0;
		}
		fiberInfo->sizeContextMargin = margin;
	}
	return SCE_OK;
}

SceInt32 sceFiberStartContextSizeCheck(SceUInt32 flags)
{
	if (flags != 0)
		return SCE_FIBER_ERROR_INVALID;
	for (;;) {
		if (__builtin_ldrex(&sContextSizeCheck) != 0)
			return SCE_FIBER_ERROR_STATE;
		if (__builtin_strex(1, &sContextSizeCheck) == 0)
			return SCE_OK;
	}
}

SceInt32 sceFiberStopContextSizeCheck(void)
{
	SceUInt32 check;

	do {
		check = __builtin_ldrex(&sContextSizeCheck);
		if (check != 1)
			break;
	} while (__builtin_strex(0, &sContextSizeCheck) != 0);
	if (check != 1)
		return SCE_FIBER_ERROR_STATE;
	return SCE_OK;
}

SceInt32 sceFiberRenameSelf(const char *name)
{
	FiberThreadContext **thread;
	UltFiber *current;

	if (name == NULL)
		return SCE_FIBER_ERROR_NULL;
	thread = getThreadContext();
	if (*thread == NULL)
		return SCE_FIBER_ERROR_PERMISSION;
	current = (*thread)->currentFiber;
	if (current == NULL)
		return SCE_FIBER_ERROR_PERMISSION;
	setFiberName(name, current->encodedName);
	current->checksum = calcChecksum(current);
	return SCE_OK;
}

SceInt32 sceFiberPushUserMarkerWithHud(const char *label, SceUInt32 color, SceUInt32 flags)
{
	FiberThreadContext *thread;
	UltFiber *current;
	int ret;

	gLibPerfCaptureFlag = &_pLibPerfCaptureFlagPtr;
	if (gLibPerfCaptureFlag == NULL)
		return SCE_FIBER_ERROR_STATE;
	thread = *getThreadContext();
	if (thread == NULL)
		return SCE_FIBER_ERROR_STATE;
	current = thread->currentFiber;
	ret = _sceCpuRazorPushFiberUserMarker(label, current->razorMarker, current, color, flags);
	current->checksum = calcChecksum(current);
	if (ret != 0)
		return SCE_FIBER_ERROR_BUSY;
	return SCE_OK;
}

SceInt32 sceFiberPopUserMarkerWithHud(void)
{
	FiberThreadContext **thread;
	UltFiber *current;
	int ret;

	gLibPerfCaptureFlag = &_pLibPerfCaptureFlagPtr;
	if (gLibPerfCaptureFlag == NULL)
		return SCE_FIBER_ERROR_STATE;
	thread = getThreadContext();
	if (*thread == NULL)
		return SCE_FIBER_ERROR_STATE;
	current = (*thread)->currentFiber;
	ret = _sceCpuRazorPopFiberUserMarker(current->razorMarker, current);
	current->checksum = calcChecksum(current);
	if (ret != 0)
		return SCE_FIBER_ERROR_BUSY;
	return SCE_OK;
}
