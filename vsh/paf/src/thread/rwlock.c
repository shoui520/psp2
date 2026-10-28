#include "mutex.h"
#include <stddef.h>

RWLock *RWLockConstruct(RWLock *self, const char *name, int32_t option)
{
	self->m_counter = 0;
	self->m_evf = sceKernelCreateEventFlag(name, SCE_KERNEL_EVF_ATTR_MULTI,
					     3, NULL);
	return self;
}

RWLock *RWLockConstructBase(RWLock *self, const char *name, int32_t option)
{
	self->m_counter = 0;
	self->m_evf = sceKernelCreateEventFlag(name, SCE_KERNEL_EVF_ATTR_MULTI,
					     3, NULL);
	return self;
}

RWLock *RWLockDestroy(RWLock *self)
{
	sceKernelDeleteEventFlag(self->m_evf);
	return self;
}

RWLock *RWLockDestroyBase(RWLock *self)
{
	sceKernelDeleteEventFlag(self->m_evf);
	return self;
}

void RWLockLockShared(RWLock *self)
{
	sceKernelWaitEventFlag(self->m_evf, 2, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
			       NULL, NULL);
	self->m_counter = (int32_t)((uint32_t)self->m_counter + 1);
	if (self->m_counter == 1)
		sceKernelClearEventFlag(self->m_evf, ~1U);
	sceKernelSetEventFlag(self->m_evf, 2);
}

void RWLockUnlockShared(RWLock *self)
{
	sceKernelWaitEventFlag(self->m_evf, 2, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
			       NULL, NULL);
	self->m_counter = (int32_t)((uint32_t)self->m_counter - 1);
	if (self->m_counter == 0)
		sceKernelSetEventFlag(self->m_evf, 1);
	sceKernelSetEventFlag(self->m_evf, 2);
}

void RWLockLockExclusive(RWLock *self)
{
	sceKernelWaitEventFlag(self->m_evf, 3, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
			       NULL, NULL);
	sceKernelSetEventFlag(self->m_evf, 1);
}

void RWLockUnlockExclusive(RWLock *self)
{
	sceKernelSetEventFlag(self->m_evf, 2);
}
