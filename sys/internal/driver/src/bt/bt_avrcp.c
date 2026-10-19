/* AVRCP requests; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>

void BtAvrcpRequestTimedOut(void *argument);
void BtAvrcpRepeatButton(void *argument);

uintptr_t BtOpenAvrcpBrowsing(BtConnection *connection, BtChannel *channel)
{
	(void)channel;
	return (uintptr_t)connection;
}

void BtAvrcpReleaseTranslatedButton(void *argument)
{
	BtChannel *channel = argument;
	BtConnection *connection = channel->connection;

	channel->protocol.avrcp.translatedButtonTimer = 0;
	if (connection)
		BtNotifyEvent(13, 0, channel->protocol.avrcp.translatedButton,
			      connection->deviceAddress);
}

void BtAvrcpReleaseButton(void *argument)
{
	BtChannel *channel = argument;
	int button = channel->protocol.avrcp.receivedButton;

	channel->protocol.avrcp.buttonReleaseTimer = 0;
	if (button) {
		channel->protocol.avrcp.receivedButton = button | 0x80;
		BtNotifyEvent(13, 0, button | 0x80,
			      channel->connection->deviceAddress);
	}
	channel->protocol.avrcp.receivedButton = -1;
}

void BtAvrcpRequestTimedOut(void *argument)
{
	BtChannel *channel = argument;
	unsigned int flags = channel->flags;

	channel->protocol.avrcp.requestTimer = 0;
	if (flags & 0x20000) {
		channel->flags = flags & ~0x30000;
		BtNotifyEvent(15, 0, 0, channel->connection->deviceAddress);
	} else {
		channel->flags = flags & ~0x10000;
	}
	channel->protocol.avrcp.sentButton = -1;
}

uintptr_t BtRequestAvrcpDisconnect(BtConnection *connection, BtChannel *channel)
{
	BtAvrcpChannelState *state = &channel->protocol.avrcp;
	int button = state->receivedButton;

	if (button >= 0) {
		if (state->translatedButton >= 0) {
			state->translatedButton |= 0x80;
			BtNotifyEvent(13, 0, state->translatedButton,
				      connection->deviceAddress);
		} else {
			state->receivedButton |= 0x80;
			BtNotifyEvent(13, 0, state->receivedButton,
				      connection->deviceAddress);
		}
		state->receivedButton = -1;
		state->translatedButton = -1;
	}
	state->requestTimer = BtCancelTimer(state->requestTimer);
	state->buttonReleaseTimer = BtCancelTimer(state->buttonReleaseTimer);
	state->buttonRepeatTimer = BtCancelTimer(state->buttonRepeatTimer);
	state->translatedButtonTimer = BtCancelTimer(state->translatedButtonTimer);
	return 0;
}

void BtAvrcpRepeatButton(void *argument)
{
	BtChannel *channel = argument;
	int button = channel->protocol.avrcp.sentButton;
	uint8_t label;

	channel->protocol.avrcp.buttonRepeatTimer = 0;
	if (button < 0)
		return;
	label = channel->protocol.avrcp.transactionLabel + 1;
	channel->protocol.avrcp.transactionLabel = label;
	BtSendL2capPacket(channel->connection, channel->remoteCid,
		"1H 111 11", (label << 4) & 0xf0, 0x110e, 0, 0x48,
		0x7c, button, 0);
	BtQueueTimer(&channel->protocol.avrcp.buttonRepeatTimer,
		     BtAvrcpRepeatButton, channel, 500000);
}

int BtBeginAvrcpRequest(BtChannel *channel)
{
	if (channel->flags & 0x10000)
		return 1;
	channel->flags |= 0x10000;
	BtQueueTimer(&channel->protocol.avrcp.requestTimer,
		     BtAvrcpRequestTimedOut, channel, 2000000);
	return 0;
}

void BtAvrcpClosed(BtConnection *connection, BtChannel *channel)
{
	BtClearConnectedProfiles(connection, NULL, channel->protocol.avrcp.profiles);
	BtDisconnectConnection(connection->stack, connection);
}

void BtReceiveAvrcpBrowsing(BtConnection *connection, BtChannel *channel,
				uint8_t *packet, int length)
{
	unsigned int flags;
	int responseLength;
	int payloadLength;
	int mtu;
	int remaining;
	int chunk;
	uint8_t *cursor;

	if (length <= 3)
		return;
	flags = packet[0];
	if (flags & 2)
		return;
	flags |= 2;
	packet[0] = flags;
	if (packet[1] != 0x11 || packet[2] != 0x0e) {
		flags |= 1;
		packet[0] = flags;
		responseLength = 3;
		payloadLength = 2;
	} else if (packet[3] != 0x71) {
		packet[4] = 0;
		packet[5] = 1;
		packet[6] = 0;
		responseLength = 7;
		payloadLength = 6;
	} else if (length <= 15 || packet[4] != 0 || packet[5] != 10) {
		packet[4] = 0;
		packet[5] = 1;
		packet[6] = 1;
		responseLength = 7;
		payloadLength = 6;
	} else if (packet[6] != 0) {
		packet[4] = 0;
		packet[5] = 1;
		packet[6] = 10;
		responseLength = 7;
		payloadLength = 6;
	} else if (packet[7] != 0 || packet[8] != 0 ||
		   packet[9] != 0 || packet[10] != 0) {
		packet[4] = 0;
		packet[5] = 1;
		packet[6] = 1;
		responseLength = 7;
		payloadLength = 6;
	} else {
		*(BtAvrcpPlayerListResponse *)(packet + 4) =
			gBtAvrcpPlayerListResponse;
		responseLength = 48;
		payloadLength = 47;
	}
	mtu = channel->mtu;
	if (responseLength <= mtu) {
		packet[0] = flags & 0xf3;
		BtSendL2capPacket(connection, channel->remoteCid,
					"#", packet, responseLength);
		return;
	}
	BtSendL2capPacket(connection, channel->remoteCid, "11#",
		(flags & 0xf3) | 4, payloadLength / (mtu - 1) + 1,
		packet + 1, mtu - 2);
	mtu = channel->mtu;
	remaining = payloadLength + 2 - mtu;
	cursor = packet + mtu - 1;
	while (remaining > 0) {
		chunk = mtu - 1;
		if (chunk >= remaining) {
			BtSendL2capPacket(connection, channel->remoteCid,
				"1#", (packet[0] & 0xf3) | 0x0c,
				cursor, remaining);
			return;
		}
		remaining -= chunk;
		BtSendL2capPacket(connection, channel->remoteCid,
			"1#", (packet[0] & 0xf3) | 8, cursor, chunk);
		cursor += chunk;
		if (remaining <= 0)
			return;
		mtu = channel->mtu;
	}
}

uintptr_t BtAvrcpOpened(BtConnection *connection, BtChannel *channel)
{
	BtDevice *device = connection->device;
	unsigned int profiles = device->profiles;
	unsigned int newProfiles = profiles & 0x10;
	unsigned int combinedProfiles = 0x20;
	uint8_t label;

	if (newProfiles) {
		if (connection->connectedProfiles & 0x10)
			newProfiles = 0;
		else
			combinedProfiles = 0x30;
	}
	if ((profiles & 0x20) && !(connection->connectedProfiles & 0x20)) {
		if ((device->avrcpFeatures & 2) && device->avrcpVersion >= 0x104) {
			if (!(channel->flags & 0x10000)) {
				channel->flags |= 0x10000;
				BtQueueTimer(&channel->protocol.avrcp.requestTimer,
					     BtAvrcpRequestTimedOut, channel, 2000000);
			}
			label = channel->protocol.avrcp.transactionLabel + 1;
			channel->protocol.avrcp.transactionLabel = label;
			BtSendL2capPacket(channel->connection, channel->remoteCid,
				"1H 11# 11H 1", (label << 4) & 0xf0,
				0x110e, 1, 0x48, &gBtAvrcpVendorHeader,
				4, 0x10, 0, 1, 3);
		}
		newProfiles = combinedProfiles;
	}
	if (newProfiles)
		return BtSetConnectedProfiles(connection, NULL, newProfiles);
	return (uintptr_t)device;
}

int BtInitializeAvrcp(BtConnection *connection, BtChannel *channel)
{
	unsigned int profiles;

	if (!channel) {
		channel = BtFindChannel(connection, 20, 23);
		if (!channel)
			return (int)0x802f0601;
	}
	channel->ops = &gBtAvrcpOps;
	profiles = connection->device->profiles;
	channel->protocol.avrcp.volume = -1;
	channel->protocol.avrcp.translatedButton = -1;
	channel->protocol.avrcp.sentButton = -1;
	channel->protocol.avrcp.receivedButton = -1;
	channel->protocol.avrcp.profiles = profiles & 0x30;
	channel->protocol.avrcp.translatePlayPause =
		strncmp("VGP-BRM1", connection->device->name, 121) != 0;
	if (channel->flags & 2)
		return 0;
	return BtSendL2capConnectionRequest(connection, channel);
}

int BtSendAvrcpButton(BtConnection *connection, int button)
{
	BtChannel *channel = BtFindChannel(connection, 4, 23);
	uint32_t flags;
	uint8_t label;

	if (!channel)
		return (int)0x802f0603;
	flags = channel->flags;
	if (flags & 0x20000)
		return (int)0x802f0605;
	if (button & 0x80) {
		if (channel->protocol.avrcp.sentButton != (button & 0x7f))
			return (int)0x802f0606;
	} else if (channel->protocol.avrcp.sentButton >= 0) {
		return (int)0x802f0607;
	}
	if (flags & 0x10000) {
		if (flags & 0x100000)
			return (int)0x802f0605;
		channel->protocol.avrcp.queuedButton = button;
		channel->flags = flags | 0x100000;
		return 0;
	}
	BtBeginAvrcpRequest(channel);
	channel->flags |= 0x20000;
	channel->protocol.avrcp.buttonRepeatTimer =
		BtCancelTimer(channel->protocol.avrcp.buttonRepeatTimer);
	if (button & 0x80) {
		channel->protocol.avrcp.sentButton = -1;
	} else {
		channel->protocol.avrcp.sentButton = button;
		BtQueueTimer(&channel->protocol.avrcp.buttonRepeatTimer,
			     BtAvrcpRepeatButton, channel, 500000);
	}
	label = channel->protocol.avrcp.transactionLabel + 1;
	channel->protocol.avrcp.transactionLabel = label;
	return BtSendL2capPacket(channel->connection, channel->remoteCid,
		"1H 111 11", (label << 4) & 0xf0, 0x110e, 0, 0x48,
		0x7c, button, 0);
}

int BtSendAvrcpVolume(BtConnection *connection, int volume)
{
	BtChannel *channel = BtFindChannel(connection, 4, 23);
	uint32_t flags;
	uint8_t label;

	if (!channel)
		return (int)0x802f0603;
	flags = channel->flags;
	if (flags & 0x10000) {
		int result = flags & 0x80000;

		if (!result) {
			channel->protocol.avrcp.volume = volume;
			channel->flags = flags | 0x80000;
		} else {
			result = (int)0x802f0605;
		}
		return result;
	}
	channel->flags = flags | 0x10000;
	BtQueueTimer(&channel->protocol.avrcp.requestTimer,
		     BtAvrcpRequestTimedOut, channel, 2000000);
	label = channel->protocol.avrcp.transactionLabel + 1;
	channel->protocol.avrcp.transactionLabel = label;
	return BtSendL2capPacket(channel->connection, channel->remoteCid,
		"1H 11# 11H 1", (label << 4) & 0xf0, 0x110e, 0, 0x48,
		&gBtAvrcpVendorHeader, 4, 0x50, 0, 1, volume & 0x7f);
}

int BtGetAvrcpVolume(BtConnection *connection)
{
	BtChannel *channel = BtFindChannel(connection, 4, 23);
	int volume;

	if (!channel)
		return (int)0x802f0603;
	volume = channel->protocol.avrcp.volume;
	if (volume < 0)
		return (int)0x802f0608;
	return volume;
}

int BtInitializeAvrcpBrowsing(BtConnection *connection, BtChannel *channel)
{
	if (!channel) {
		channel = BtFindChannel(connection, 20, 27);
		if (!channel)
			return (int)0x802f0601;
	}
	channel->ops = &gBtAvrcpBrowsingOps;
	if (channel->flags & 2)
		return 0;
	return BtSendL2capConnectionRequest(connection, channel);
}
