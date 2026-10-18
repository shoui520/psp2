/* Controller sleep completion; original source filename unavailable. */
#include <intrmgr/intrmgr.h>
#include <threadmgr/threadmgr.h>
#include <wlanbt/wlanbt.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include "bt_transport.h"

unsigned int BtCancelTimer(unsigned int timer);
int BtUnlockTransport(int result);
int BtLockTransport(void);
int BtApplyConfiguration(BtStack *stack);
uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
			unsigned int argument, uint64_t deviceAddress);
int BtSetupSdioTransport(void);
int BtSendHciCommand(BtStack *stack, unsigned int opcode,
		     const char *format, ...);
int BtQueueTimer(unsigned int *timer, void (*callback)(void *),
		 void *argument, SceInt64 delay);
void BtControllerResetTimedOut(void *argument);
int BtCompleteControllerSleep(BtStack *stack);

static inline void BtClearCommandQueue(BtStack *stack)
{
	stack->commandBusy = 0;
	stack->commandQueue.used = 0;
	stack->commandQueue.readPosition = 0;
	stack->commandQueue.writePosition = 0;
	stack->commandLength = 0;
}

static inline int BtWaitControllerConfiguration(unsigned int ready)
{
	int remaining = 100;

	while (!(ksceWlanBtGetConfiguration() & ready)) {
		sceKernelDelayThread(100000);
		if (--remaining == 0)
			return (int)0x802f150e;
	}
	return 0;
}

void BtControllerResetTimedOut(void *argument)
{
	BtStack *stack = argument;

	BtClearCommandQueue(gBtTransport.stack);
	gBtTransport.transmitActive = 0;
	stack->state = 80;
	BtCompleteControllerSleep(stack);
}

int BtCompleteControllerSleep(BtStack *stack)
{
	uint8_t interruptEnable = 0;
	unsigned int timer, mode;
	int configuration, result;

	result = SceSdifForDriver_3C4CDC8B(gBtTransport.function, 2, 1,
					 &interruptEnable, 1);
	if (result < 0)
		return result;
	if (sceKernelDisableSubIntr(222, gBtTransport.subInterrupt) < 0)
		return (int)0x802f150c;
	gBtTransport.interruptEnabled = 0;
	timer = BtCancelTimer(stack->resetTimer);
	mode = stack->mode;
	stack->resetTimer = timer;
	if (mode & 4)
		configuration = stack->autoAcceptDeviceCount > 0 ? 15 : 16;
	else
		configuration = 6;
	if ((mode & 0x20004) == 0x20004)
		configuration = 16;
	if (stack->state == 80)
		configuration = 18;
	BtUnlockTransport(0);
	result = ksceWlanBtSetConfiguration(configuration);
	BtLockTransport();
	mode = stack->mode;
	stack->state = 28;
	stack->mode = mode & ~0x28u;
	if (result < 0)
		return result;
	if (!(mode & 4) && BtApplyConfiguration(stack))
		BtNotifyEvent(21, 0, (uint8_t)stack->mode, 0);
	return 0;
}

int BtPowerOffTransport(void)
{
	BtStack *stack;
	unsigned int timer;
	uint8_t interruptEnable = 0;
	int result;

	timer = BtCancelTimer(gBtTransport.wakeupTimer);
	stack = gBtTransport.stack;
	gBtTransport.wakeupTimer = timer;
	if (!(stack->mode & 4)) {
		BtClearCommandQueue(stack);
		gBtTransport.transmitActive = 0;
		return BtCompleteControllerSleep(stack);
	}
	result = SceSdifForDriver_3C4CDC8B(gBtTransport.function, 2, 1,
					 &interruptEnable, 1);
	if (result < 0)
		return result;
	if (sceKernelDisableSubIntr(222, gBtTransport.subInterrupt) < 0)
		return (int)0x802f150c;
	gBtTransport.interruptEnabled = 0;
	BtUnlockTransport(0);
	result = ksceWlanBtSetConfiguration(10);
	if (result >= 0)
		result = BtWaitControllerConfiguration(8);
	BtLockTransport();
	if (result < 0)
		return result;
	stack = gBtTransport.stack;
	gBtTransport.transmitActive = 0;
	BtClearCommandQueue(stack);
	result = BtSetupSdioTransport();
	if (result < 0)
		return result;
	BtUnlockTransport(0);
	result = ksceWlanBtSetConfiguration(11);
	if (result >= 0)
		result = BtWaitControllerConfiguration(16);
	BtLockTransport();
	if (result < 0)
		return result;
	stack = gBtTransport.stack;
	if (stack->controllerType == 1) {
		gBtTransport.asleep = 0;
		BtQueueTimer(&stack->resetTimer, BtControllerResetTimedOut,
			     stack, 1000000);
		stack = gBtTransport.stack;
		stack->state = 70;
		return BtSendHciCommand(stack, 0xfc5b, "1", 0xf1);
	}
	return BtCompleteControllerSleep(stack);
}
