/* BNEP network driver callbacks; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>
#include "bt_transport.h"

BtBnepInterface gBtBnepInterface;

int BtControlBnepInterface(struct NetDriver *driver, unsigned int command,
			 int *argument, unsigned int size)
{
	(void)driver;
	(void)command;
	(void)argument;
	(void)size;
	return (int)0x80418006;
}

int BtOpenBnepInterface(struct NetDriver *driver, int argument)
{
	BtBnepInterface *interface = driver->private;
	BtConnection *existing;
	BtConnection *connection;
	BtDevice *device;
	BtStack *stack;
	uint64_t deviceAddress;
	unsigned int flags;
	int result;

	(void)argument;
	result = BtLockTransport();
	if (result)
		return result;
	device = interface->stack->deviceHead;
	while (device && !(device->profiles & 0x300))
		device = device->next;
	if (!device)
		return BtUnlockTransport((int)0x802f1107);
	existing = BtFindConnection(interface->stack, 0x21,
				    device->deviceAddress);
	if (existing) {
		if (!interface->connected) {
			interface->pendingConnection = 1;
			return BtUnlockTransport(0);
		}
		BtUnlockTransport(0);
		driver->notify(driver,
			NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
					 NET_DRIVER_STATUS_STARTED), NULL, 0);
		result = BtLockTransport();
		return BtUnlockTransport(result);
	}
	if (interface->stack->state != 28)
		return BtUnlockTransport((int)0x802f1106);
	connection = BtFindConnection(interface->stack, 0x11,
				      device->deviceAddress);
	if (!connection)
		return BtUnlockTransport(result);
	flags = connection->flags | 1;
	stack = interface->stack;
	deviceAddress = device->deviceAddress;
	connection->device = device;
	stack->connectingAddress = deviceAddress;
	interface->deviceAddress = deviceAddress;
	stack->state = 40;
	connection->flags = flags;
	interface->pendingConnection = 1;
	result = BtSendHciCommand(stack, 0x405, "621-21", deviceAddress,
				 0xcc18, device->pageScanRepetitionMode,
				 device->clockOffset, existing);
	return BtUnlockTransport(result);
}

int BtCloseBnepInterface(struct NetDriver *driver, int argument)
{
	int result = BtLockTransport();

	(void)argument;
	if (result)
		return result;
	BtUnlockTransport(0);
	driver->notify(driver,
		NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
				 NET_DRIVER_STATUS_STOPPED), NULL, 0);
	BtLockTransport();
	return BtUnlockTransport(0);
}

int BtKickBnepTransmit(struct NetDriver *driver, int argument)
{
	BtBnepInterface *interface = driver->private;
	BtConnection *connection;
	struct NetDriverPacket *packet;
	int result;

	(void)argument;
	result = BtLockTransport();
	if (result)
		return result;
	if (!interface->connected)
		return BtUnlockTransport((int)0x802f1109);
	connection = BtFindConnection(interface->stack, 1,
				      interface->deviceAddress);
	if (!connection)
		return BtUnlockTransport((int)0x802f1101);
	if (!interface->currentPacket) {
		packet = interface->driver.dequeueTx(&interface->driver);
		interface->currentPacket = packet;
		if (packet)
			result = BtSendBnepFrame(connection, packet->buffer->data,
						(int)packet->length, 1);
	}
	return BtUnlockTransport(result);
}

uintptr_t BtNotifyBnepDisconnected(BtConnection *connection)
{
	uint64_t deviceAddress = connection->deviceAddress;
	uintptr_t result = (uint32_t)deviceAddress;

	if (gBtBnepInterface.deviceAddress != deviceAddress)
		return result;
	if (gBtBnepInterface.connected) {
		BtUnlockTransport(0);
		gBtBnepInterface.driver.notify(&gBtBnepInterface.driver,
			NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
					 NET_DRIVER_STATUS_STOPPED), NULL, 0);
		result = BtLockTransport();
	}
	gBtBnepInterface.pendingConnection = 0;
	gBtBnepInterface.connected = 0;
	gBtBnepInterface.deviceAddress = 0;
	return result;
}

int BtNotifyBnepConnected(BtConnection *connection)
{
	BtBnepInterface *interface = &gBtBnepInterface;
	int pending;

	for (;;) {
		if (interface < &gBtBnepInterface + 1) {
			if (interface->deviceAddress != connection->deviceAddress) {
				interface++;
				continue;
			}
		} else {
			if (gBtBnepInterface.deviceAddress)
				return 0;
			interface = &gBtBnepInterface;
			gBtBnepInterface.deviceAddress = connection->deviceAddress;
		}
		pending = interface->pendingConnection;
		interface->connected = 1;
		if (pending)
			break;
		interface++;
	}
	interface->pendingConnection = 0;
	BtUnlockTransport(0);
	interface->driver.notify(&interface->driver,
		NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
				 NET_DRIVER_STATUS_STARTED), NULL, 0);
	return BtLockTransport();
}

uintptr_t BtContinueSerialPacket(BtConnection *connection)
{
	uint64_t deviceAddress = gBtBnepInterface.deviceAddress;
	struct NetDriverPacket *packet;
	uintptr_t result = (uint32_t)deviceAddress;

	if (deviceAddress != connection->deviceAddress)
		return result;
	packet = gBtBnepInterface.currentPacket;
	if (packet) {
		gBtBnepInterface.currentPacket = NULL;
		gBtBnepInterface.driver.releaseTx(&gBtBnepInterface.driver, packet);
	}
	connection = BtFindConnection(gBtBnepInterface.stack, 1,
				      gBtBnepInterface.deviceAddress);
	result = (uintptr_t)connection;
	if (connection && !gBtBnepInterface.currentPacket) {
		packet = gBtBnepInterface.driver.dequeueTx(&gBtBnepInterface.driver);
		gBtBnepInterface.currentPacket = packet;
		result = (uintptr_t)packet;
		if (packet)
			return BtSendBnepFrame(connection, packet->buffer->data,
					       (int)packet->length, 1);
	}
	return result;
}

int BtReceiveNetworkPacket(BtStack *stack, uint64_t deviceAddress,
			   const void *data, unsigned int length)
{
	struct NetDriver *driver = &gBtBnepInterface.driver;
	struct NetDriverPacket *packet;

	(void)stack;
	if (gBtBnepInterface.deviceAddress != deviceAddress)
		return (int)0x802f1103;
	if (!gBtBnepInterface.connected)
		return (int)0x802f1108;
	packet = driver->allocateRx(driver, 2048, 0);
	if (!packet)
		return (int)0x802f1102;
	memcpy(packet->buffer->data, data, length);
	packet->buffer->end += length;
	packet->length = length;
	driver->deliverRx(driver, packet);
	BtUnlockTransport(0);
	driver->notify(driver,
		NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
				 NET_DRIVER_STATUS_RECEIVED), NULL, 0);
	return BtLockTransport();
}

int BtInitializeNetworkInterface(BtStack *stack, uint64_t localAddress)
{
	struct NetDriver *driver = &gBtBnepInterface.driver;
	int result;

	if (gBtBnepInterface.localAddress)
		return 0;
	driver->open = BtOpenBnepInterface;
	driver->close = BtCloseBnepInterface;
	driver->control = BtControlBnepInterface;
	driver->capabilities = 0x60000;
	driver->type = 3;
	driver->kickTx = BtKickBnepTransmit;
	BtPack(driver->mac.data, 6, "E", localAddress);
	driver->private = &gBtBnepInterface;
	driver->mtu = 1500;
	driver->prefix = 0;
	result = SceNetPsForDriver_1ABF937D(driver);
	if (result < 0)
		return result;
	gBtBnepInterface.stack = stack;
	gBtBnepInterface.localAddress = localAddress;
	return 0;
}
