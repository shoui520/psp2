/* Headset profile command callbacks; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>
#include <sysmem/sysclib.h>

int BtHspSetSpeakerGain(BtConnection *connection, int command,
			const char *text, const char *end)
{
	int32_t value;

	(void)command;
	(void)end;
	if ((signed char)*BtParseDecimalList(text, &value, 1) != '\r')
		return -1;
	BtNotifyEvent(19, 6, value, connection->deviceAddress);
	return BtSendHspResponse(connection, "\r\nOK\r\n");
}

int BtHspSetMicrophoneGain(BtConnection *connection, int command,
			   const char *text, const char *end)
{
	int32_t value;

	(void)command;
	(void)end;
	if ((signed char)*BtParseDecimalList(text, &value, 1) != '\r')
		return -1;
	BtNotifyEvent(19, 5, value, connection->deviceAddress);
	return BtSendHspResponse(connection, "\r\nOK\r\n");
}

int BtHspPressButton(BtConnection *connection, int command,
		     const char *text, const char *end)
{
	uint64_t now;
	uint64_t elapsed;
	int32_t value;

	(void)command;
	(void)end;
	if ((signed char)*BtParseDecimalList(text, &value, 1) != '\r' ||
	    value != 200)
		return -1;
	now = BtGetSystemTimeWide();
	elapsed = now - connection->hsp.connectedTime;
	if ((uint32_t)(elapsed >> 32) != 0 || (uint32_t)elapsed > 4999999 ||
	    connection->hsp.buttonCount != 0)
		BtNotifyEvent(19, 7, 0, connection->deviceAddress);
	connection->hsp.buttonCount++;
	return BtSendHspResponse(connection, "\r\nOK\r\n");
}

uintptr_t BtHspOpened(BtConnection *connection)
{
	connection->hsp.connectedTime = BtGetSystemTimeWide();
	return BtSetConnectedProfiles(connection, NULL, 0x10000);
}

int BtReceiveHsp(BtConnection *connection, const char *packet, int length)
{
	const BtHspCommand *command = gBtHspCommands;

	do {
		const char *prefix = command->prefix;
		size_t prefixLength = strnlen(prefix, 8);

		if (strncmp(packet, prefix, prefixLength) == 0) {
			if (command->handler(connection, 3, packet + prefixLength,
					     packet + length) == 0)
				return 0;
			break;
		}
		command++;
	} while (command < &gBtHspCommands[3]);
	BtSendHspResponse(connection, "\r\nERROR\r\n");
	return 0;
}
