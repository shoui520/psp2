/* Bluetooth SDIO initialization; original source filename unavailable. */
#include <intrmgr/intrmgr.h>
#include <threadmgr/threadmgr.h>
#include <wlanbt/wlanbt.h>
#include <sysmem/atomic.h>
#include "bt_transport.h"
#include <sdif/sdif.h>

int BtReadLe16(const uint8_t *source);
int BtReadLe24(const uint8_t *source);
void BtPollTransport(void *argument);
int BtQueueTimer(unsigned int *timer, void (*callback)(void *argument),
		 void *argument, SceInt64 delay);
int BtHandleSdioInterrupt(void *subInterruptArgument, void *registerArgument,
			unsigned char priority);
int BtLockTransport(void);

int BtSetupSdioTransport(void)
{
	SdifSdioFunctionIdentifier identifier;
	uint8_t wakeup[1], registerData[4];
	unsigned int mode;
	int oldSubInterrupt, result;

	gBtTransport.sdio = SceSdifForDriver_6A8235FC(2);
	if (gBtTransport.sdio == NULL)
		return 0x802f1505;
	identifier.manufacturerId = 0x2df;
	identifier.productId = 0x911a;
	identifier.interfaceCode = ~0U;
	gBtTransport.function = SceSdifForDriver_0C66E36F(gBtTransport.sdio,
		&identifier);
	if (gBtTransport.function == NULL)
		return 0x802f1506;
	gBtTransport.stack->controllerType = 1;
	if (SceSdifForDriver_3428884D(gBtTransport.function) < 0)
		return 0x802f1507;
	mode = gBtTransport.stack->mode;
	if ((mode & 0x400) != 0) {
		gBtTransport.stack->mode = mode & ~0x600U;
		wakeup[0] = 2;
		result = SceSdifForDriver_3C4CDC8B(gBtTransport.function,
			0, 1, wakeup, 1);
		BtQueueTimer(&gBtTransport.wakeupTimer, BtPollTransport,
			&gBtTransport, 1000000);
		if (result < 0)
			goto disable_function;
	}
	oldSubInterrupt = gBtTransport.subInterrupt;
	gBtTransport.subInterrupt = gBtTransport.function->number + 1;
	if (oldSubInterrupt == 0) {
		result = sceKernelRegisterSubIntrHandler(222,
			gBtTransport.subInterrupt, "SceBtIntr",
			BtHandleSdioInterrupt, &gBtTransport);
		if (result < 0 && result != (int)0x80027109) {
			result = 0x802f1509;
			goto disable_function;
		}
	}
	if (sceKernelEnableSubIntr(222, gBtTransport.subInterrupt) < 0) {
		result = 0x802f150b;
		goto release_interrupt;
	}
	gBtTransport.interruptEnabled = 1;
	result = SceSdifForDriver_D0F78D9B(gBtTransport.function,
		0x60, 2, registerData, 1);
	if (result < 0)
		goto disable_interrupt;
	if (BtReadLe16(registerData) != 0xfedc) {
		result = 0x802f1501;
		goto disable_interrupt;
	}
	result = SceSdifForDriver_D0F78D9B(gBtTransport.function,
		0x78, 3, registerData, 1);
	if (result < 0)
		goto disable_interrupt;
	gBtTransport.ioPort = BtReadLe24(registerData);
	result = SceSdifForDriver_D0F78D9B(gBtTransport.function,
		0x63, 1, &gBtTransport.receiveUnitShift, 1);
	if (result < 0)
		goto disable_interrupt;
	result = SceSdifForDriver_D0F78D9B(gBtTransport.function,
		0x5c, 1, registerData, 1);
	if (result < 0)
		goto disable_interrupt;
	gBtTransport.cardRevision = registerData[0];
	result = SceSdifForDriver_D0F78D9B(gBtTransport.function,
		0x6c, 1, registerData, 1);
	if (result < 0)
		goto disable_interrupt;
	registerData[0] |= 0x10;
	result = SceSdifForDriver_3C4CDC8B(gBtTransport.function,
		0x6c, 1, registerData, 1);
	if (result < 0)
		goto disable_interrupt;
	registerData[0] = 3;
	result = SceSdifForDriver_3C4CDC8B(gBtTransport.function,
		1, 1, registerData, 1);
	if (result < 0)
		goto disable_interrupt;
	result = SceSdifForDriver_3C4CDC8B(gBtTransport.function,
		2, 1, registerData, 1);
	if (result < 0)
		goto disable_interrupt;
	result = SceSdifForDriver_733BC373(gBtTransport.function, 128);
	if (result < 0)
		goto disable_interrupt;
	return 0;
disable_interrupt:
	sceKernelDisableSubIntr(222, gBtTransport.subInterrupt);
	gBtTransport.interruptEnabled = 0;
release_interrupt:
	sceKernelReleaseSubIntrHandler(222, gBtTransport.subInterrupt);
disable_function:
	SceSdifForDriver_E80293EF(gBtTransport.function);
	return result;
}

int BtStartTransport(void)
{
	BtStack *stack = gBtTransport.stack;
	uint32_t mode = stack->mode;
	unsigned int events;
	int attempts, result, configuration;

	stack->lastError = 0;
	stack->mode = mode & ~0x8040U;
	if (sceKernelGetThreadId() == gBtTransport.lockOwner) {
		if (gBtTransport.lockDepth != 0) {
			gBtTransport.lockDepth--;
		} else {
			gBtTransport.lockOwner = 0;
			AtomicFullBarrier();
			do {
				events = AtomicLoadReserved32(&gBtTransport.lockSignal.events);
			} while (AtomicStoreReserved32(&gBtTransport.lockSignal.events,
				events | 1));
			if (gBtTransport.lockSignal.waiters != 0)
				sceKernelSetEventFlag(gBtTransport.lockSignal.eventFlag, 1);
		}
	}
	attempts = 100;
	for (;;) {
		result = sceWlanBtSetConfiguration(5);
		if (result != (int)0x80418007)
			break;
		sceKernelDelayThread(10000);
		if (--attempts == 0)
			goto failed;
	}
	if (result < 0)
		goto failed;
	attempts = 100;
	for (;;) {
		configuration = sceWlanBtGetConfiguration();
		if (configuration > 0 && (configuration & 4) != 0)
			break;
		sceKernelDelayThread(100000);
		if (--attempts == 0) {
			BtLockTransport();
			return 0x802f150d;
		}
	}
	BtLockTransport();
	if ((configuration & 0x100) != 0) {
		gBtTransport.asleep = 0;
		gBtTransport.stack->mode |= 0x8000;
	}
	return BtSetupSdioTransport();
failed:
	BtLockTransport();
	return result;
}
