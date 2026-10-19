/* Bluetooth delta timers; original source filename unavailable. */
#include <threadmgr/threadmgr.h>
#include "bt_internal.h"
#include "bt_transport.h"

int BtTimerThread(SceSize argumentSize, void *argument)
{
	BtTimer *timer, *position;
	SceInt64 before, timeout, elapsed;
	void (*callback)(void *argument);
	void *callbackArgument;

	(void)argumentSize;
	(void)argument;
	if (BtLockTransport() < 0)
		goto exit;
	for (;;) {
		while ((timer = gBtStack.pendingTimerHead) != NULL) {
			gBtStack.pendingTimerHead = timer->next;
			if (timer->next)
				timer->next->prev = NULL;
			else
				gBtStack.pendingTimerTail = NULL;
			position = gBtStack.activeTimerHead;
			while (position && position->relativeDelay < timer->relativeDelay) {
				timer->relativeDelay = (SceInt64)
					((uint64_t)timer->relativeDelay - position->relativeDelay);
				position = position->next;
			}
			if (position) {
				position->relativeDelay = (SceInt64)
					((uint64_t)position->relativeDelay - timer->relativeDelay);
				timer->prev = position->prev;
				if (position->prev)
					position->prev->next = timer;
				else
					gBtStack.activeTimerHead = timer;
				timer->next = position;
				position->prev = timer;
			} else {
				timer->prev = gBtStack.activeTimerTail;
				if (gBtStack.activeTimerTail)
					gBtStack.activeTimerTail->next = timer;
				else
					gBtStack.activeTimerHead = timer;
				timer->next = NULL;
				gBtStack.activeTimerTail = timer;
			}
		}
		while ((timer = gBtStack.activeTimerHead) != NULL &&
		       timer->relativeDelay < 1) {
			callback = timer->callback;
			callbackArgument = timer->argument;
			if (timer->id)
				*timer->id = 0;
			if (timer->next)
				timer->next->prev = timer->prev;
			else
				gBtStack.activeTimerTail = timer->prev;
			if (timer->prev)
				timer->prev->next = timer->next;
			else
				gBtStack.activeTimerHead = timer->next;
			timer->next = gBtStack.freeTimers;
			gBtStack.freeTimers = timer;
			if (callback)
				callback(callbackArgument);
		}
		before = BtGetSystemTimeWide();
		timeout = gBtStack.activeTimerHead ?
			gBtStack.activeTimerHead->relativeDelay : -1;
		BtUnlockTransport(0);
		if (BtWaitSignal(&gBtStack.timerSignal, timeout) < 0 ||
		    BtLockTransport() < 0)
			break;
		elapsed = (SceInt64)((uint64_t)BtGetSystemTimeWide() - before);
		position = gBtStack.activeTimerHead;
		if (timeout >= 0 && elapsed && position) {
			for (;;) {
				if (position->relativeDelay >= (int32_t)elapsed) {
					position->relativeDelay = (SceInt64)
						((uint64_t)position->relativeDelay - elapsed);
					break;
				}
				elapsed = (SceInt64)
					((uint64_t)elapsed - position->relativeDelay);
				position->relativeDelay = 0;
				position = position->next;
				if (!elapsed || !position)
					break;
			}
		}
	}
exit:
	BtExitThread();
	return 0;
}

unsigned int BtCancelTimer(unsigned int id)
{
	BtTimer *timer, *position;
	BtTimer **head;

	if (!id)
		return id;
	if (!gBtStack.timersInitialized)
		return 0x802f0b06;
	if (id > 64)
		return 0x802f0b05;
	timer = &gBtStack.timers[id - 1];
	position = gBtStack.pendingTimerHead;
	while (position && position != timer)
		position = position->next;
	if (position) {
		if (timer->next)
			timer->next->prev = timer->prev;
		else
			gBtStack.pendingTimerTail = timer->prev;
		head = &gBtStack.pendingTimerHead;
	} else {
		position = gBtStack.activeTimerHead;
		while (position && position != timer)
			position = position->next;
		if (!position)
			return 0;
		if (timer->next) {
			timer->next->relativeDelay = (SceInt64)
				((uint64_t)timer->next->relativeDelay + timer->relativeDelay);
			timer->next->prev = timer->prev;
		} else {
			gBtStack.activeTimerTail = timer->prev;
		}
		head = &gBtStack.activeTimerHead;
	}
	if (timer->prev)
		head = &timer->prev->next;
	*head = timer->next;
	timer->next = gBtStack.freeTimers;
	gBtStack.freeTimers = timer;
	return 0;
}

int BtQueueTimer(unsigned int *id, void (*callback)(void *argument),
		 void *argument, SceInt64 delay)
{
	BtTimer *timer;
	BtTimer **tail;
	int result;

	if (!gBtStack.timersInitialized)
		return 0x802f0b07;
	if (id) {
		if (*id)
			BtCancelTimer(*id);
		*id = 0;
	}
	timer = gBtStack.freeTimers;
	if (!timer)
		return 0x802f0b08;
	gBtStack.freeTimers = timer->next;
	timer->callback = callback;
	timer->argument = argument;
	timer->id = id;
	tail = gBtStack.pendingTimerTail ?
		&gBtStack.pendingTimerTail->next : &gBtStack.pendingTimerHead;
	timer->prev = gBtStack.pendingTimerTail;
	timer->relativeDelay = delay;
	*tail = timer;
	gBtStack.pendingTimerTail = timer;
	timer->next = NULL;
	result = BtSetSignal(&gBtStack.timerSignal);
	if (!result && id)
		*id = timer - gBtStack.timers + 1;
	return result;
}

int BtInitializeTimers(const char *threadName)
{
	BtTimer *timer = gBtStack.timers;
	int result;

	gBtStack.freeTimers = timer;
	do {
		timer++;
		timer[-1].next = timer;
	} while (timer != gBtStack.timers + 63);
	gBtStack.pendingTimerTail = NULL;
	gBtStack.timers[63].next = NULL;
	gBtStack.activeTimerHead = NULL;
	gBtStack.activeTimerTail = gBtStack.pendingTimerHead = NULL;
	result = BtCreateSignal(&gBtStack.timerSignal, 0, "SceBtTimer");
	if (result >= 0) {
		if ((gBtStack.timerThreadId =
		     BtCreateThread(BtTimerThread, 0, NULL, threadName, 18)) < 0) {
			BtDeleteSignal(&gBtStack.timerSignal);
			return gBtStack.timerThreadId;
		}
		gBtStack.timersInitialized = 1;
		return 0;
	}
	return result;
}
