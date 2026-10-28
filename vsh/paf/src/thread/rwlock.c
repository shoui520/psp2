#include "mutex.h"
#include "../common/halt_assert.h"
#include <kernel/error.h>
#include <stddef.h>

Mutex *MutexConstruct(Mutex *self, const char *name, int32_t option)
{
	uint32_t attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO;

	if (option & MutexOption_FIFO)
		attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO;
	if (sceKernelCreateLwMutex(&self->m_mutex, name, attr, 0, NULL))
		on_error();
	return self;
}

Mutex *MutexConstructBase(Mutex *self, const char *name, int32_t option)
{
	uint32_t attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO;

	if (option & MutexOption_FIFO)
		attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO;
	if (sceKernelCreateLwMutex(&self->m_mutex, name, attr, 0, NULL))
		on_error();
	return self;
}

Mutex *MutexDestroy(Mutex *self)
{
	if (sceKernelDeleteLwMutex(&self->m_mutex))
		on_error();
	return self;
}

Mutex *MutexDestroyBase(Mutex *self)
{
	if (sceKernelDeleteLwMutex(&self->m_mutex))
		on_error();
	return self;
}

void MutexLock(Mutex *self)
{
	if (sceKernelLockLwMutex(&self->m_mutex, 1, NULL))
		on_error();
}

void MutexUnlock(Mutex *self)
{
	if (sceKernelUnlockLwMutex(&self->m_mutex, 1))
		on_error();
}

unsigned char MutexTryLock(Mutex *self)
{
	int32_t result = sceKernelTryLockLwMutex(&self->m_mutex, 1);

	if (result == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN)
		return 1;
	if (result)
		on_error();
	return 0;
}

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
