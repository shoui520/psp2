/* Power events; original source filename unavailable. */
#include <sysmem/debug.h>
#include <regmgr/regmgr.h>
#include <sysmem/sysevent.h>
#include <wlanbt/wlanbt.h>
#include "bt_internal.h"
#include "bt_transport.h"

extern uint64_t gBtBusyDumpTime;
/* BtWakeTransport as this file's callers use it: they pass on what it leaves in r0 */
uintptr_t BtWakeTransportResult(void) __asm__("BtWakeTransport");
void BtConfigureAsynchronously(void *argument);
void BtPowerOffTimedOut(void *argument);

static inline void bt_busy_dump(int result)
{
	uint64_t time;

	if (!gBtBusyDumpTime) {
		gBtBusyDumpTime = (uint64_t)BtGetSystemTimeWide() + 10000000;
	} else {
		time = BtGetSystemTimeWide();
		if (time >= gBtBusyDumpTime) {
			if (result)
				SceDebugForDriver_391B74B7("%s: r=%d mode=0x%x state=%d last_error=0x%x\n",
						 __func__, result, gBtStack.mode,
						 gBtStack.state, gBtStack.lastError);
			gBtBusyDumpTime = time + 10000000;
		}
	}
	if (!result)
		gBtBusyDumpTime = 0;
}

int BtSystemEvent(int resume, int eventId, void *args, void *opt)
{
	uint32_t mode;
	int result;

	(void)opt;
	if (resume) {
		if (resume != 1)
			return 0;
		if (eventId == 0x100001) {
			if (!(gBtStack.mode & 4) || BtLockTransport() < 0)
				return 0;
			mode = gBtStack.mode;
			if (mode & 4) {
				gBtStack.mode = mode & ~4u;
				if (gBtStack.deviceRegistryState > 0 && !(mode & 0x40))
					BtQueueTimer(NULL, BtConfigureAsynchronously, NULL, 0);
			}
			BtUnlockTransport(0);
			return 0;
		}
		if (eventId != 0x100002)
			return 0;
		mode = gBtStack.mode;
		if (mode & 0x40)
			result = 0;
		else if ((mode & 9) == 1)
			result = -1;
		else if (mode & 0x10)
			result = -2;
		else
			result = 0;
		bt_busy_dump(result);
		return result;
	}
	if (eventId == 0x401) {
		struct PowerSuspendEvent *event = args;

		if ((gBtStack.mode & 4) || BtLockTransport() < 0)
			return 0;
		mode = gBtStack.mode;
		if (!(mode & 4)) {
			mode |= 4;
			gBtStack.mode = mode;
			if (event->sleepMode)
				gBtStack.mode = mode | 0x20000;
			if (gBtStack.deviceRegistryState > 0 && !(mode & 0x40)) {
				sceRegMgrSetKeyInt("/CONFIG/BT", "bt_config", (mode >> 7) & 1);
				BtQueueTimer(NULL, BtConfigureAsynchronously, NULL, 0);
			}
		}
		BtUnlockTransport(0);
		return 0;
	}
	if (eventId != 0x402)
		return 0;
	mode = gBtStack.mode;
	if (mode & 0x40)
		result = 0;
	else if (mode & 8)
		result = -1;
	else if (mode & 0x20)
		result = -2;
	else
		result = 0;
	bt_busy_dump(result);
	return result;
}

uintptr_t BtAdvanceShutdown(BtStack *stack)
{
	BtConnection *connection;
	int result;

	if (stack->scanEnable) {
		int previousState = stack->state;

		stack->state = 60;
		stack->previousState = previousState;
		stack->scanEnable = 0;
		return BtSendHciCommand(stack, 0x0c1a, "1", 0);
	}
	if (stack->state == 30) {
		stack->state = 61;
		return BtSendHciCommand(stack, 0x0402, gBtEmptyHciFormat);
	} else if (stack->state == 31) {
		stack->state = 61;
		return (uintptr_t)stack;
	} else if (stack->state == 32) {
		stack->state = 62;
		return BtSendHciCommand(stack, 0x041a, gBtDeviceAddressHciFormat,
					stack->inquiryAddress);
	} else if (stack->state == 40) {
		stack->state = 63;
		return BtSendHciCommand(stack, 0x0408, gBtDeviceAddressHciFormat,
					stack->connectingAddress);
	} else if (stack->state == 50) {
		stack->state = 64;
		return BtSendHciCommand(stack, 0x041a, gBtDeviceAddressHciFormat,
					stack->connectingAddress);
	} else if (stack->state == 66) {
		result = BtIsAsleep();
		if (result == 0) {
			stack->state = 67;
			return result;
		}
	} else if (stack->state != 67) {
		for (connection = stack->connections;
		     connection < stack->connections + 7; connection++) {
			if (!connection->deviceAddress)
				continue;
			stack->state = 65;
			if (!connection->aclHandle) {
				if (connection->flags & 1)
					return BtSendHciCommand(&gBtStack, 0x0408,
						gBtDeviceAddressHciFormat,
						connection->deviceAddress);
				connection->flags |= 0x200000;
			} else if (!(connection->flags & 0x40)) {
				connection->flags |= 0x40;
				BtQueueTimer(&connection->disconnectTimer,
					     BtDisconnectTimedOut, connection, 3000000);
				if (connection->linkMode == 2) {
					BtSendHciCommand(&gBtStack, 0x0804, "2",
							 connection->aclHandle);
					connection->interval = 0;
				}
				return BtDisconnectConnection(&gBtStack, connection);
			}
			return (uintptr_t)stack;
		}
		stack->shutdownTimer = BtCancelTimer(stack->shutdownTimer);
		if (stack->controllerType == 1) {
			stack->state = 66;
			BtSendHciCommand(&gBtStack, 0xfc23, "12", 2, 0);
			return BtQueueTimer(&stack->powerOffTimer,
					BtPowerOffTimedOut, stack, 1000000);
		}
	}
	stack->state = 68;
	stack->powerOffTimer = BtCancelTimer(stack->powerOffTimer);
	result = BtPowerOffTransport();
	if (result < 0) {
		uint32_t mode = stack->mode;

		stack->lastError = result;
		stack->state = 28;
		mode = (mode & ~0x20u) | 0x40;
		stack->mode = mode;
		return BtNotifyEvent(21, 0, mode & ~0x20u, 0);
	}
	return result;
}

void BtShutdownTimedOut(void *argument)
{
	BtStack *stack = argument;
	BtConnection *connection = stack->connections;

	stack->shutdownTimer = 0;
	do {
		BtConnection *current;

		while (!connection->deviceAddress) {
			if (++connection >= stack->connections + 7)
				goto finished;
		}
		current = connection++;
		BtClearConnection(current, -1);
	} while (connection < stack->connections + 7);
finished:
	BtAdvanceShutdown(stack);
}

static inline int BtApplyConfigurationCore(BtStack *stack)
{
	uint32_t mode = stack->mode;
	int result;
	int retries;

	if ((mode & 5) == 1) {
		if (mode & 0x38) {
			stack->mode = mode | 8;
			return 1;
		}
		mode |= 0x18;
		stack->mode = mode;
		BtNotifyEvent(21, 0, (unsigned char)mode, 0);
		if (stack->mode & 0x100) {
			BtWakeTransport();
			return 0;
		}
		result = BtStartTransport();
		if (result < 0)
			goto failed;
		if (stack->controllerType == 1 && (gBtStack.mode & 0x8000)) {
			stack->state = 1;
			BtSendHciCommand(stack, 0xfc5b, "1", 0xf1);
		} else {
			stack->state = 2;
			BtSendHciCommand(stack, 0x1009, gBtEmptyHciFormat);
		}
		return result;
	}
	stack->mode = mode & ~8u;
	if ((mode & 0x38) == 8) {
		mode = (mode & ~8u) | 0x20;
		stack->mode = mode;
		BtNotifyEvent(21, 0, mode & 0xf7, 0);
		BtAdvanceShutdown(stack);
		BtQueueTimer(&stack->shutdownTimer, BtShutdownTimedOut,
			     stack, 5000000);
		return 0;
	}
	if (!(mode & 4))
		return 1;
	BtUnlockTransport(0);
	result = sceWlanBtSetConfiguration(10);
	if (result < 0) {
		BtLockTransport();
		goto failed;
	}
	retries = 100;
	while (!(sceWlanBtGetConfiguration() & 8)) {
		BtDelayThread(100000);
		if (--retries == 0) {
			BtLockTransport();
			result = (int)0x802f150f;
			goto failed;
		}
	}
	result = sceWlanBtSetConfiguration(16);
	BtLockTransport();
	if (result < 0)
		goto failed;
	return result;
failed:
	stack->lastError = result;
	stack->mode |= 0x40;
	return result;
}

int BtApplyConfiguration(BtStack *stack)
{
	return BtApplyConfigurationCore(stack);
}

void BtConfigureAsynchronously(void *argument)
{
	(void)argument;
	BtApplyConfigurationCore(&gBtStack);
}

void BtPowerOffTimedOut(void *argument)
{
	BtStack *stack = argument;
	int scanEnable = stack->scanEnable;
	int result;

	stack->powerOffTimer = 0;
	stack->state = 67;
	if (scanEnable) {
		stack->previousState = 67;
		stack->state = 60;
		stack->scanEnable = 0;
		BtSendHciCommand(stack, 0x0c1a, "1", 0);
		return;
	}
	stack->state = 68;
	stack->powerOffTimer = BtCancelTimer(0);
	result = BtPowerOffTransport();
	if (result < 0) {
		uint32_t mode = stack->mode;

		stack->lastError = result;
		stack->state = 28;
		mode = (mode & ~0x20u) | 0x40;
		stack->mode = mode;
		BtNotifyEvent(21, 0, mode & ~0x20u, 0);
	}
}

uintptr_t BtCompleteConfiguration(BtStack *stack)
{
	uint32_t mode = stack->mode;
	uintptr_t result;
	int started;
	int retries;

	stack->mode = mode & ~0x10u;
	if ((mode & 5) == 1) {
		if (mode & 0x28) {
			stack->mode = (mode & ~0x10u) | 8;
			goto notify;
		}
		stack->mode = (mode & ~0x10u) | 0x18;
		BtNotifyEvent(21, 0, (unsigned char)stack->mode, 0);
		if (stack->mode & 0x100)
			return BtWakeTransportResult();
		started = BtStartTransport();
		if (started < 0)
			goto failed;
		if (stack->controllerType == 1 && (gBtStack.mode & 0x8000)) {
			stack->state = 1;
			result = BtSendHciCommand(stack, 0xfc5b, "1", 0xf1);
		} else {
			stack->state = 2;
			result = BtSendHciCommand(stack, 0x1009, gBtEmptyHciFormat);
		}
		if (started == 0)
			return result;
		goto notify;
	}
	stack->mode = mode & ~0x18u;
	if ((mode & 0x28) == 8) {
		stack->mode = (mode & ~0x18u) | 0x20;
		BtNotifyEvent(21, 0, stack->mode & 0xe7, 0);
		BtAdvanceShutdown(stack);
		return BtQueueTimer(&stack->shutdownTimer, BtShutdownTimedOut,
				    stack, 5000000);
	}
	if (!(mode & 4))
		goto notify;
	BtUnlockTransport(0);
	started = sceWlanBtSetConfiguration(10);
	if (started < 0) {
		BtLockTransport();
		goto failed;
	}
	retries = 100;
	while (!(sceWlanBtGetConfiguration() & 8)) {
		BtDelayThread(100000);
		if (--retries == 0) {
			BtLockTransport();
			started = (int)0x802f150f;
			goto failed;
		}
	}
	started = sceWlanBtSetConfiguration(16);
	result = BtLockTransport();
	if (started >= 0) {
		if (started == 0)
			return result;
		goto notify;
	}
failed:
	stack->lastError = started;
	stack->mode |= 0x40;
notify:
	return BtNotifyEvent(21, 0, (unsigned char)stack->mode, 0);
}
