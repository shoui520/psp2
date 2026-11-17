/*
 * SceNpCommonPs4: a timer thread that runs entries from a list sorted by due time.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

int SceNpCommonPs4_04EC8796(SceUID *pThread, SceKernelThreadEntry entry, SceSize argSize, void *pArgp,
	int initPriority, SceSize stackSize, int cpuAffinity, const char *name);
int SceNpCommonPs4_CFB92A12(SceUID thread, int *pExitStatus);
int SceNpCommonPs4_D37A4868(SceUID *pMutex);
int SceNpCommonPs4_262D1ACE(SceUID *pMutex);
int SceNpCommonPs4_69E858EA(SceUID *pCond, SceUInt32 timeout);

int npCommonPs4TimerThreadMain(SceSize argSize, void *argp)
{
	struct NpCommonPs4TimerThread *timer;
	struct NpCommonPs4TimerEntry *entry;
	SceUInt32 timeout;
	SceInt64 now;

	timer = *(struct NpCommonPs4TimerThread **)argp;
	SceNpCommonPs4_D37A4868(&timer->mutex);
	for (;;) {
		SceNpCommonPs4_F56B0EAB(&now);
		entry = timer->head;
		if (entry == NULL || (SceUInt64)now < (SceUInt64)entry->dueTime) {
			timeout = 0;
			if (entry != NULL)
				timeout = (SceUInt32)entry->dueTime - (SceUInt32)now;
			if (timer->stopping > 0)
				break;
			if (timeout == 0)
				SceNpCommonPs4_69E858EA(&timer->cond, 0);
			else
				SceNpCommonPs4_69E858EA(&timer->cond, timeout);
			if (timer->stopping > 0)
				break;
		} else {
			timer->head = entry->next;
			SceNpCommonPs4_262D1ACE(&timer->mutex);
			entry->callback(entry->argument);
			SceNpCommonPs4_D37A4868(&timer->mutex);
		}
	}
	SceNpCommonPs4_262D1ACE(&timer->mutex);
	return 0;
}

int npCommonPs4TimerThreadStart(struct NpCommonPs4TimerThread *timer, const char *name, SceSize stackSize,
	int priority, int cpuAffinity)
{
	struct NpCommonPs4TimerThread *self;
	int result;

	if (timer->running > 0)
		return NP_COMMON_PS4_ERROR_80558002;
	timer->stopping = 0;
	timer->head = NULL;
	self = timer;
	result = SceNpCommonPs4_EDBFFFE9(&timer->mutex, name, 1);
	if (result >= 0) {
		result = SceNpCommonPs4_34C15C21(&self->cond, &self->mutex, name);
		if (result >= 0) {
			result = SceNpCommonPs4_04EC8796(&self->thread, (SceKernelThreadEntry)npCommonPs4TimerThreadMain, 4, &self,
				priority, stackSize, cpuAffinity, name);
			if (result >= 0) {
				self->running = 1;
				return 0;
			}
			SceNpCommonPs4_AC89B1E7(&self->cond);
		}
		SceNpCommonPs4_2D74359F(&self->mutex);
	}
	return result;
}

int npCommonPs4TimerThreadStop(struct NpCommonPs4TimerThread *timer)
{
	int result;

	result = timer->running;
	if (result != 0) {
		timer->stopping = 1;
		SceNpCommonPs4_3D73D77E(&timer->cond);
		SceNpCommonPs4_CFB92A12(timer->thread, NULL);
		SceNpCommonPs4_AC89B1E7(&timer->cond);
		SceNpCommonPs4_2D74359F(&timer->mutex);
		timer->running = 0;
		return 0;
	}
	return result;
}

int npCommonPs4TimerThreadAdd(struct NpCommonPs4TimerThread *timer, struct NpCommonPs4TimerEntry *entry,
	SceUInt32 delay, int (*callback)(void *argument), void *argument)
{
	struct NpCommonPs4TimerEntry *current;
	struct NpCommonPs4TimerEntry *previous;
	struct NpCommonPs4TimerEntry *scan;
	SceInt64 now;

	if (timer->running == 0)
		return NP_COMMON_PS4_ERROR_80558001;
	sceClibMemset(entry, 0, 24);
	SceNpCommonPs4_F56B0EAB(&entry->dueTime);
	now = entry->dueTime;
	entry->argument = argument;
	entry->callback = callback;
	entry->dueTime = now + delay;
	SceNpCommonPs4_D37A4868(&timer->mutex);
	current = timer->head;
	for (scan = current; scan != NULL; scan = scan->next) {
		if (entry == scan) {
			SceNpCommonPs4_262D1ACE(&timer->mutex);
			return NP_COMMON_PS4_ERROR_80558006;
		}
	}
	previous = NULL;
	if (current != NULL) {
		previous = NULL;
		do {
			if ((SceUInt64)entry->dueTime <= (SceUInt64)current->dueTime)
				break;
			previous = current;
			current = current->next;
		} while (current != NULL);
	}
	entry->next = current;
	if (previous != NULL)
		previous->next = entry;
	else
		timer->head = entry;
	SceNpCommonPs4_3D73D77E(&timer->cond);
	SceNpCommonPs4_262D1ACE(&timer->mutex);
	return 0;
}

int npCommonPs4TimerThreadRemove(struct NpCommonPs4TimerThread *timer, struct NpCommonPs4TimerEntry *entry,
	int *pRemoved)
{
	struct NpCommonPs4TimerEntry *current;
	struct NpCommonPs4TimerEntry *previous;
	int removed;

	if (timer->running == 0)
		return NP_COMMON_PS4_ERROR_80558001;
	SceNpCommonPs4_D37A4868(&timer->mutex);
	current = timer->head;
	previous = NULL;
	while (current != NULL) {
		if (entry == current)
			break;
		previous = current;
		current = current->next;
	}
	if (current == NULL) {
		removed = 0;
	} else {
		if (previous != NULL)
			previous->next = entry->next;
		else
			timer->head = entry->next;
		removed = 1;
	}
	if (pRemoved != NULL)
		*pRemoved = removed;
	SceNpCommonPs4_262D1ACE(&timer->mutex);
	return 0;
}

}
