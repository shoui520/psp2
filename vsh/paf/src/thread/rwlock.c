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

__attribute__((noinline)) void MutexLock(Mutex *self)
{
	if (sceKernelLockLwMutex(&self->m_mutex, 1, NULL))
		on_error();
}

__attribute__((noinline)) void MutexUnlock(Mutex *self)
{
	if (sceKernelUnlockLwMutex(&self->m_mutex, 1))
		on_error();
}

RMutex *ScePafThread_C4BD9753(RMutex *self, const char *name, int option)
{
	uint32_t attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO | 2;

	if (option & MutexOption_FIFO)
		attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO | 2;
	if (sceKernelCreateLwMutex(&self->m_mutex, name, attr, 0, NULL))
		on_error();
	return self;
}

RMutex *ScePafThread_8CCECAC9(RMutex *self)
{
	if (sceKernelDeleteLwMutex(&self->m_mutex))
		on_error();
	return self;
}

RMutex *ScePafThread_D1BEBE2D(RMutex *self)
{
	if (sceKernelDeleteLwMutex(&self->m_mutex))
		on_error();
	return self;
}

void ScePafThread_F9FFA0BE(RMutex *self)
{
	if (sceKernelLockLwMutex(&self->m_mutex, 1, NULL))
		on_error();
}

void ScePafThread_95F9C45F(RMutex *self)
{
	if (sceKernelUnlockLwMutex(&self->m_mutex, 1))
		on_error();
}

typedef struct PafProtectedCounter {
	uint8_t reserved[36];
	int32_t count;
} PafProtectedCounter;

int32_t ScePafThread_9F2131F9(PafProtectedCounter *self)
{
	int32_t count;

	MutexLock((Mutex *)self);
	count = self->count;
	MutexUnlock((Mutex *)self);
	return count;
}

void ScePafThread_733814FD(uint8_t *self)
{
	if (sceKernelSignalLwCond((SceKernelLwCondWork *)(self + 4)))
		on_error();
}

RWLock *ScePafThread_A8DB7042(RWLock *self)
{
	sceKernelDeleteEventFlag(self->m_evf);
	return self;
}

RWLock *ScePafThread_F17CAAB0(RWLock *self)
{
	sceKernelDeleteEventFlag(self->m_evf);
	return self;
}

void ScePafThread_12911164(uint8_t *self)
{
	if (sceKernelSignalLwCondAll((SceKernelLwCondWork *)(self + 4)))
		on_error();
}

uint8_t *ScePafThread_52F363A9(uint8_t *self)
{
	if (sceKernelDeleteLwCond((SceKernelLwCondWork *)(self + 4)))
		on_error();
	return self;
}

uint8_t *ScePafThread_E3D5D9BB(uint8_t *self)
{
	if (sceKernelDeleteLwCond((SceKernelLwCondWork *)(self + 4)))
		on_error();
	return self;
}

void ScePafThread_FB300461(SceUID *self)
{
	if (sceKernelWaitSema(*self, 1, NULL))
		on_error();
}

void ScePafThread_534F14D4(SceUID *self)
{
	if (sceKernelSignalSema(*self, 1))
		on_error();
}

SceUID *ScePafThread_E72CBBE1(SceUID *self)
{
	if (sceKernelDeleteSema(*self))
		on_error();
	return self;
}

static SceUID *InitializeEventFlag(SceUID *self, const char *name)
{
	self[1] = 0;
	self[2] = 0;
	self[3] = 0;
	self[0] = sceKernelCreateEventFlag(name, 0x1000U, 4, NULL);
	return self;
}

SceUID *ScePafThread_465970E2(SceUID *self, const char *name)
{
	return InitializeEventFlag(self, name);
}

SceUID *ScePafThread_94138DD2(SceUID *self, const char *name)
{
	return InitializeEventFlag(self, name);
}

uint32_t *ScePafThread_15F029A2(uint32_t *result)
{
	result[0] = 268435727U;
	result[1] = 268435732U;
	result[2] = 268435736U;
	return result;
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
