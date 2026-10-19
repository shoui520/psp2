/* SDP connection policy; original source filename unavailable. */
#include "bt_internal.h"

void BtSdpClosed(BtConnection *connection, BtChannel *channel)
{
	BtDevice *device;
	unsigned int vendorProductId, mode;
	BtStack *stack;

	if (connection->flags & 0x40) {
		BtDisconnectConnection(connection->stack, connection);
		return;
	}
	if (channel->flags & 2)
		return;
	device = connection->device;
	vendorProductId = device->vendorProductId;
	stack = connection->stack;
	if (vendorProductId == 0x03290e6f)
		device->flags &= ~0xc1;
	mode = stack->mode;
	if ((mode & 0x10000) && vendorProductId == 0x05c4054c)
		goto flush;
	if ((device->profiles & 0xfffff) == 0x40 && !(device->flags & 0x40) &&
	    vendorProductId != 0x03290e6f) {
		if (!(mode & 0x11000)) {
			BtBeginDisconnect(connection);
			BtNotifyConnectionComplete(connection, 0, 0);
			return;
		}
		BtRegisterDevice(stack, connection->deviceAddress);
	} else {
flush:
		BtFlushConnectionDevice(connection);
	}
	BtAdvanceConnection(connection);
}
