/* Bluetooth connection cleanup; original source filename unavailable. */
#include <string.h>
#include "bt_internal.h"

unsigned int BtClearHidConnection(BtConnection *connection);

BtConnection *BtClearConnection(BtConnection *connection, int status)
{
	BtStack *stack = connection->stack;
	uint32_t flags = connection->flags;

	if ((flags & 0x10000) && stack->exclusiveControllerCount) {
		stack->exclusiveControllerCount--;
		flags = connection->flags;
	}
	if ((flags & 0x20000) && stack->limitedConnectionCount) {
		stack->limitedConnectionCount--;
		flags = connection->flags;
	}
	if ((flags & 0x40000) && stack->audioConnectionCount) {
		stack->audioConnectionCount--;
		flags = connection->flags;
	}
	if ((flags & 0x80000) && stack->genericHidConnectionCount) {
		stack->genericHidConnectionCount--;
		flags = connection->flags;
	}
	connection->flags = flags | 0x80;
	BtReleaseOutstandingAclPackets(connection);
	BtClearAvrcpConnection(connection);
	BtClearAudioConnection(connection);
	BtClearHidConnection(connection);
	BtNotifyBnepDisconnected(connection);
	BtCancelBnepTimers(connection);
	connection->disconnectTimer = BtCancelTimer(connection->disconnectTimer);
	connection->profileTimer = BtCancelTimer(connection->profileTimer);
	connection->authenticationTimer = BtCancelTimer(connection->authenticationTimer);
	if (status >= 0)
		BtNotifyEvent(6, (uint8_t)status, 0, connection->deviceAddress);
	return memset(connection, 0, sizeof(*connection));
}

unsigned int BtClearHidConnection(BtConnection *connection)
{
	BtHidState *hid = &connection->hid;
	unsigned int timer;

	hid->featureTimer = BtCancelTimer(hid->featureTimer);
	hid->keepaliveTimer = BtCancelTimer(hid->keepaliveTimer);
	hid->statusTimer = BtCancelTimer(hid->statusTimer);
	timer = BtCancelTimer(hid->requestTimer);
	hid->requestTimer = timer;
	hid->control.readHead = NULL;
	hid->control.readTail = NULL;
	hid->control.sendHead = NULL;
	hid->control.sendTail = NULL;
	hid->interrupt.readHead = NULL;
	hid->interrupt.readTail = NULL;
	hid->interrupt.sendHead = NULL;
	hid->interrupt.sendTail = NULL;
	return timer;
}
