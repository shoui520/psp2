#include "mutex.h"
#include "../common/halt_assert.h"
#include "../std/new.h"
#include "../std/stdlib.h"
#include <kernel/error.h>
#include <stddef.h>

__attribute__((noinline))
Mutex *MutexConstruct(Mutex *self, const char *name, int32_t option)
{
	uint32_t attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO;

	if (option & MutexOption_FIFO)
		attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO;
	if (sceKernelCreateLwMutex(&self->m_mutex, name, attr, 0, NULL))
		on_error();
	return self;
}

typedef struct PafThreadCondition {
	SceKernelLwMutexWork *mutex;
	SceKernelLwCondWork work;
} PafThreadCondition;

typedef struct PafThreadQueueNode {
	struct PafThreadQueueNode *previous;
	struct PafThreadQueueNode *next;
	void *value;
} PafThreadQueueNode;

typedef struct PafThreadQueueCounter {
	Mutex mutex;
	PafThreadQueueNode *root;
	int32_t count;
} PafThreadQueueCounter;

_Static_assert(offsetof(PafThreadQueueCounter, root) == 32,
	       "queue counter root offset");
_Static_assert(offsetof(PafThreadQueueCounter, count) == 36,
	       "queue counter count offset");

static PafThreadQueueCounter *InitializeThreadQueueCounter(
		PafThreadQueueCounter *self, const char *name)
{
	PafThreadQueueNode *root;

	MutexConstruct(&self->mutex, name, 0);
	self->count = 0;
	root = PafNew(sizeof(*root));
	if (!root)
		ScePafStdc_D608246A();
	self->root = root;
	if (root) {
		root->previous = root;
		root->next = root;
	}
	return self;
}

PafThreadQueueCounter *ScePafThread_5A72B9FC(
		PafThreadQueueCounter *self, const char *name)
{
	return InitializeThreadQueueCounter(self, name);
}

PafThreadQueueCounter *ScePafThread_A87BC0AE(
		PafThreadQueueCounter *self, const char *name)
{
	return InitializeThreadQueueCounter(self, name);
}

static PafThreadCondition *ConstructThreadCondition(PafThreadCondition *self,
					SceKernelLwMutexWork *mutex,
					const char *name, int option)
{
	uint32_t attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO;

	self->mutex = mutex;
	if (option & MutexOption_FIFO)
		attr = SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO;
	if (sceKernelCreateLwCond(&self->work, name, attr, mutex, NULL))
		on_error();
	return self;
}

PafThreadCondition *ScePafThread_8291E6C4(PafThreadCondition *self,
					  SceKernelLwMutexWork *mutex,
					  const char *name, int option)
{
	return ConstructThreadCondition(self, mutex, name, option);
}

PafThreadCondition *ScePafThread_BA5BD169(PafThreadCondition *self,
					  SceKernelLwMutexWork *mutex,
					  const char *name, int option)
{
	return ConstructThreadCondition(self, mutex, name, option);
}

int ScePafThread_26225225(PafThreadCondition *self, int32_t milliseconds)
{
	uint32_t timeout;
	uint32_t *timeoutPtr = NULL;
	int result;

	if (milliseconds > 0) {
		timeout = (uint32_t)milliseconds * 1000U;
		timeoutPtr = &timeout;
	}
	result = sceKernelWaitLwCond(&self->work, timeoutPtr);
	if (result == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return 1;
	if (result)
		on_error();
	return 0;
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

__attribute__((noinline)) Mutex *MutexDestroy(Mutex *self)
{
	if (sceKernelDeleteLwMutex(&self->m_mutex))
		on_error();
	return self;
}

static PafThreadQueueCounter *DestroyThreadQueueCounter(
		PafThreadQueueCounter *self)
{
	PafThreadQueueNode *root = self->root;

	if (root) {
		PafThreadQueueNode *node = root->next;

		while (node != root) {
			PafThreadQueueNode *next = node->next;

			PafDelete(node);
			root = self->root;
			node = next;
		}
		PafDelete(root);
	}
	self->root = (PafThreadQueueNode *)-1;
	self->count = -1;
	MutexDestroy(&self->mutex);
	return self;
}

PafThreadQueueCounter *ScePafThread_6B1322CB(PafThreadQueueCounter *self)
{
	return DestroyThreadQueueCounter(self);
}

PafThreadQueueCounter *ScePafThread_BE9A9155(PafThreadQueueCounter *self)
{
	return DestroyThreadQueueCounter(self);
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

RMutex *ScePafThread_A48CABF3(RMutex *self, const char *name, int option)
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

SceUID *ScePafThread_323F3E4D(SceUID *self)
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

__attribute__((noinline)) int ScePafThread_651F4F81(uint32_t milliseconds)
{
	return sceKernelDelayThread(milliseconds * 1000U);
}

void ScePafThread_0164145D(PafProtectedCounter *self)
{
	for (;;) {
		int32_t remaining;

		MutexLock((Mutex *)self);
		remaining = self->count;
		MutexUnlock((Mutex *)self);
		if (remaining <= 0)
			break;
		ScePafThread_651F4F81(10);
	}
}

int ScePafThread_18AF6844(RMutex *self)
{
	int32_t result = sceKernelTryLockLwMutex(&self->m_mutex, 1);

	if (result == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN)
		return 1;
	if (result)
		on_error();
	return 0;
}

uint32_t *ScePafThread_EE889AAF(uint32_t *result)
{
	result[0] = 268435727U;
	result[1] = 268435732U;
	result[2] = 268435736U;
	return result;
}

static SceUID *InitializeSemaphore(SceUID *self, const char *name,
				   int32_t initial, int32_t maximum)
{
	SceUID sema = sceKernelCreateSema(name, 0x2000U, initial, maximum, NULL);

	*self = sema;
	if (sema < 0)
		on_error();
	return self;
}

SceUID *ScePafThread_1089526C(SceUID *self, const char *name,
				 int32_t initial, int32_t maximum)
{
	return InitializeSemaphore(self, name, initial, maximum);
}

SceUID *ScePafThread_D1D39853(SceUID *self, const char *name,
				 int32_t initial, int32_t maximum)
{
	return InitializeSemaphore(self, name, initial, maximum);
}

/* The native per-core bookkeeping table is indexed by the calling layer. */
static SceUID threadIdentity[4];

SceUID ScePafThread_C6F80BB3(uint32_t index)
{
	return threadIdentity[index];
}

SceUID ScePafThread_FE2DC4FE(uint32_t index)
{
	SceUID id = sceKernelGetThreadId();

	threadIdentity[index] = id;
	return id;
}

__attribute__((noinline)) int ScePafThread_D27FC881(uint32_t index)
{
	SceUID recorded = threadIdentity[index];

	return recorded == sceKernelGetThreadId();
}

typedef struct PafThreadEventRequest {
	SceUID event;
	int (*callback)(void *argument);
	void *argument;
	int32_t result;
} PafThreadEventRequest;

int ScePafThread_58876337(PafThreadEventRequest *self)
{
	int result = sceKernelPollEventFlag(self->event, 1, 4, NULL);

	if (result >= 0) {
		SceUID event;

		self->result = self->callback(self->argument);
		event = self->event;
		self->argument = NULL;
		self->callback = NULL;
		return sceKernelSetEventFlag(event, 2);
	}
	return result;
}

int ScePafThread_004445E7(PafThreadEventRequest *self,
			    int (*callback)(void *), void *argument)
{
	int result;

	if (ScePafThread_D27FC881(0))
		return callback(argument);
	sceKernelWaitEventFlag(self->event, 4, 4, NULL, NULL);
	self->callback = callback;
	self->argument = argument;
	self->result = 0;
	sceKernelSetEventFlag(self->event, 1);
	sceKernelWaitEventFlag(self->event, 2, 4, NULL, NULL);
	result = self->result;
	sceKernelSetEventFlag(self->event, 4);
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
