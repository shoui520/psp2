/*
 * SceNpCommonPs4: Cancelable (npext/handle/cancelable.cpp), a handle that can be cancelled. It
 * links to the Cancelable below it (m_subCancel) and the one above it (m_parentCancel).
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

void npCommonPs4Breakpoint(void);
int SceNpCommonPs4_A6C7D48D(struct NpCommonPs4Cancelable *cancelable, int errcode, int flags);
struct NpCommonPs4Cancelable *SceNpCommonPs4_48074E29(struct NpCommonPs4Cancelable *cancelable);
void SceNpCommonPs4_74001725(struct NpCommonPs4Cancelable *cancelable);

const void *g_cancelableVtable[] = {		/* 0x8102a6c0 */
	(const void *)SceNpCommonPs4_A6C7D48D,
	(const void *)SceNpCommonPs4_48074E29,
	(const void *)SceNpCommonPs4_74001725,
	(const void *)npCommonPs4Breakpoint,
	NULL,
	NULL,
};

int SceNpCommonPs4_86DF1834(struct NpCommonPs4Cancelable *cancelable, SceUInt8 enabled)
{
	struct NpCommonPs4Mutex *mutex;
	SceUInt8 previous;
	struct NpCommonPs4Cancelable *sub;

	mutex = &cancelable->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	previous = cancelable->cancelable;
	sub = cancelable->subCancel;
	cancelable->cancelable = enabled;
	if (sub != NULL)
		SceNpCommonPs4_86DF1834(sub, enabled);
	SceNpCommonPs4_6BA5A164(mutex);
	return previous;
}

int SceNpCommonPs4_E9F44EC1(struct NpCommonPs4Cancelable *cancelable, int a2, int a3, int a4)
{
	struct NpCommonPs4Mutex *mutex;
	struct NpCommonPs4Cancelable *sub;

	mutex = &cancelable->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	sub = cancelable->subCancel;
	cancelable->lockCount++;
	if (sub != NULL)
		SceNpCommonPs4_E9F44EC1(sub, a2, a3, a4);
	return SceNpCommonPs4_6BA5A164(mutex);
}

int SceNpCommonPs4_29034039(struct NpCommonPs4Cancelable *cancelable, int a2, int a3, int a4)
{
	struct NpCommonPs4Mutex *mutex;
	struct NpCommonPs4Cancelable *sub;

	mutex = &cancelable->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	sub = cancelable->subCancel;
	if (sub != NULL)
		SceNpCommonPs4_29034039(sub, a2, a3, a4);
	cancelable->lockCount--;
	return SceNpCommonPs4_6BA5A164(mutex);
}

int SceNpCommonPs4_DEAEDDB3(struct NpCommonPs4Cancelable *cancelable)
{
	return cancelable->cancelErrcode != 0;
}

int SceNpCommonPs4_C6AAF4C4(struct NpCommonPs4Cancelable *cancelable)
{
	int result;

	result = 0;
	if (cancelable->cancelable != 0)
		return cancelable->lockCount == 0;
	return result;
}

int SceNpCommonPs4_06575826(struct NpCommonPs4Cancelable *cancelable)
{
	struct NpCommonPs4Cancelable *top;
	struct NpCommonPs4Cancelable *next;
	struct NpCommonPs4Cancelable *i;
	int result;

	top = cancelable;
	if (cancelable->cancelable == 0)
		return 0;
	if (cancelable->lockCount != 0)
		return 0;
	result = cancelable->cancelErrcode;
	if (result == 0)
		return 0;
	if ((top->cancelFlags & 1) != 0) {
		for (i = top->parentCancel; i != NULL; i = i->parentCancel)
			top = i;
		if (top != NULL) {
			do {
				top->cancelErrcode = 0;
				next = top->subCancel;
				top->cancelFlags = 0;
				top = next;
			} while (next != NULL);
		}
	}
	return result;
}

int SceNpCommonPs4_3C8364C3(struct NpCommonPs4Cancelable *cancelable)
{
	struct NpCommonPs4Cancelable *top;
	struct NpCommonPs4Cancelable *next;
	struct NpCommonPs4Cancelable *i;
	struct NpCommonPs4Mutex *mutex;
	int errcode;
	int result;

	top = cancelable;
	mutex = &cancelable->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	if (top->cancelable != 0 && top->lockCount == 0 && (errcode = top->cancelErrcode) != 0) {
		if ((top->cancelFlags & 1) != 0) {
			for (i = top->parentCancel; i != NULL; i = i->parentCancel)
				top = i;
			if (top != NULL) {
				do {
					top->cancelErrcode = 0;
					next = top->subCancel;
					top->cancelFlags = 0;
					top = next;
				} while (next != NULL);
			}
		}
		result = errcode;
	} else {
		result = 0;
	}
	SceNpCommonPs4_6BA5A164(mutex);
	return result;
}

int SceNpCommonPs4_A6C7D48D(struct NpCommonPs4Cancelable *cancelable, int errcode, int flags)
{
	struct NpCommonPs4Mutex *mutex;
	struct NpCommonPs4Cancelable *sub;

	mutex = &cancelable->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	NP_COMMON_PS4_ASSERT(errcode != 0, "Cancel", "npext\\handle\\cancelable.cpp", 89, "errcode != 0");
	sub = cancelable->subCancel;
	cancelable->cancelErrcode = errcode;
	cancelable->cancelFlags = flags;
	if (sub != NULL)
		((void (*)(struct NpCommonPs4Cancelable *, int, int))sub->vtable[0])(sub, errcode, flags);
	if (cancelable->cancelable != 0 && cancelable->lockCount == 0)
		((void (*)(struct NpCommonPs4Cancelable *, int))cancelable->vtable[3])(cancelable, errcode);
	return SceNpCommonPs4_6BA5A164(mutex);
}

struct NpCommonPs4Cancelable *SceNpCommonPs4_178E883B(struct NpCommonPs4Cancelable *cancelable)
{
	cancelable->vtable = g_cancelableVtable;
	SceNpCommonPs4_4266D870(&cancelable->mutex);
	cancelable->cancelErrcode = 0;
	cancelable->cancelFlags = 0;
	cancelable->cancelable = 1;
	cancelable->lockCount = 0;
	cancelable->subCancel = NULL;
	cancelable->parentCancel = NULL;
	return cancelable;
}

struct NpCommonPs4Cancelable *SceNpCommonPs4_EB1FBB74(struct NpCommonPs4Cancelable *cancelable)
{
	cancelable->vtable = g_cancelableVtable;
	SceNpCommonPs4_4266D870(&cancelable->mutex);
	cancelable->cancelErrcode = 0;
	cancelable->cancelFlags = 0;
	cancelable->cancelable = 1;
	cancelable->lockCount = 0;
	cancelable->subCancel = NULL;
	cancelable->parentCancel = NULL;
	return cancelable;
}

struct NpCommonPs4Cancelable *SceNpCommonPs4_48074E29(struct NpCommonPs4Cancelable *cancelable)
{
	int lockCount;

	lockCount = cancelable->lockCount;
	cancelable->vtable = g_cancelableVtable;
	NP_COMMON_PS4_ASSERT(lockCount == 0, "~Cancelable", "npext\\handle\\cancelable.cpp", 35, "m_lockCount == 0");
	NP_COMMON_PS4_ASSERT(cancelable->subCancel == 0, "~Cancelable", "npext\\handle\\cancelable.cpp", 36, "m_subCancel == 0");
	NP_COMMON_PS4_ASSERT(cancelable->parentCancel == 0, "~Cancelable", "npext\\handle\\cancelable.cpp", 37, "m_parentCancel == 0");
	SceNpCommonPs4_294F0B1A(&cancelable->mutex);
	return cancelable;
}

void SceNpCommonPs4_74001725(struct NpCommonPs4Cancelable *cancelable)
{
	SceNpCommonPs4_48074E29(cancelable);
	operator delete(cancelable);
}

struct NpCommonPs4Cancelable *SceNpCommonPs4_C6E3FE21(struct NpCommonPs4Cancelable *cancelable)
{
	return SceNpCommonPs4_48074E29(cancelable);
}

int SceNpCommonPs4_2CA5AE6A(struct NpCommonPs4Cancelable *cancelable)
{
	int result;

	NP_COMMON_PS4_ASSERT(cancelable->lockCount == 0, "Init", "npext\\handle\\cancelable.cpp", 45, "m_lockCount == 0");
	NP_COMMON_PS4_ASSERT(cancelable->cancelErrcode == 0, "Init", "npext\\handle\\cancelable.cpp", 46, "m_cancelErrcode == 0");
	NP_COMMON_PS4_ASSERT(cancelable->cancelFlags == 0, "Init", "npext\\handle\\cancelable.cpp", 47, "m_cancelFlags == 0");
	NP_COMMON_PS4_ASSERT(cancelable->cancelable == 1, "Init", "npext\\handle\\cancelable.cpp", 48, "m_cancelable == true");
	NP_COMMON_PS4_ASSERT(cancelable->subCancel == 0, "Init", "npext\\handle\\cancelable.cpp", 49, "m_subCancel == 0");
	NP_COMMON_PS4_ASSERT(cancelable->parentCancel == 0, "Init", "npext\\handle\\cancelable.cpp", 50, "m_parentCancel == 0");
	result = SceNpCommonPs4_8AA7E995(&cancelable->mutex, "SceNpHandle", 1);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_9BCAE348(struct NpCommonPs4Cancelable *cancelable)
{
	NP_COMMON_PS4_ASSERT(cancelable->lockCount == 0, "Destroy", "npext\\handle\\cancelable.cpp", 70, "m_lockCount == 0");
	NP_COMMON_PS4_ASSERT(cancelable->subCancel == 0, "Destroy", "npext\\handle\\cancelable.cpp", 71, "m_subCancel == 0");
	NP_COMMON_PS4_ASSERT(cancelable->parentCancel == 0, "Destroy", "npext\\handle\\cancelable.cpp", 72, "m_parentCancel == 0");
	return SceNpCommonPs4_63EED478(&cancelable->mutex);
}

int SceNpCommonPs4_3AAA7DF7(struct NpCommonPs4Cancelable *cancelable)
{
	return SceNpCommonPs4_C78F5DF5(&cancelable->mutex);
}

int SceNpCommonPs4_E70C78DB(struct NpCommonPs4Cancelable *cancelable, struct NpCommonPs4Cancelable *sub)
{
	struct NpCommonPs4Cancelable *top;
	struct NpCommonPs4Cancelable *next;
	struct NpCommonPs4Cancelable *i;
	struct NpCommonPs4Mutex *mutex;
	int errcode;

	mutex = &cancelable->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	if (cancelable->cancelable == 0)
		goto link;
	if (cancelable->lockCount != 0)
		goto link;
	errcode = cancelable->cancelErrcode;
	if (errcode == 0)
		goto link;
	if ((cancelable->cancelFlags & 1) != 0) {
		top = cancelable;
		for (i = cancelable->parentCancel; i != NULL; i = i->parentCancel)
			top = i;
		if (top != NULL) {
			do {
				top->cancelErrcode = 0;
				next = top->subCancel;
				top->cancelFlags = 0;
				top = next;
			} while (next != NULL);
		}
	}
	if (errcode < 0) {
		SceNpCommonPs4_6BA5A164(mutex);
		return errcode;
	}
link:
	if (cancelable->subCancel != NULL || sub->parentCancel != NULL) {
		SceNpCommonPs4_6BA5A164(mutex);
		return -2141872625;
	}
	cancelable->subCancel = sub;
	sub->parentCancel = cancelable;
	SceNpCommonPs4_6BA5A164(mutex);
	return 0;
}

}
