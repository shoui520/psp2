/* AVRCP command and response handling; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>

int BtBeginAvrcpRequest(BtChannel *channel);
int BtCancelTimer(unsigned int timer);
int BtQueueTimer(unsigned int *timer, void (*callback)(void *argument),
		 void *argument, SceInt64 delay);
void BtAvrcpReleaseButton(void *argument);
void BtAvrcpReleaseTranslatedButton(void *argument);
void BtAvrcpRepeatButton(void *argument);
int BtSendL2capPacket(BtConnection *connection, int channelId,
		     const char *format, ...);
int BtPack(uint8_t *destination, int remaining, const char *format, ...);
int BtUnpack(const uint8_t *source, int remaining, const char *format, ...);
BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			 int identifier);
uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
			unsigned int argument, uint64_t deviceAddress);

uint32_t gBtAvrcpTitleLength;
int gBtAvrcpPlayStatusKnown;
int gBtAvrcpPlayStatus;
char gBtAvrcpTitle[1025];

void BtReceiveAvrcp(BtConnection *connection, BtChannel *channel,
			uint8_t *packet, int length)
{
	BtAvrcpChannelState *state = &channel->protocol.avrcp;
	BtAvrcpFragment *fragment;
	BtAvcHeader *avc;
	BtAvrcpVendorPacket *vendor;
	BtAvrcpPassthroughPacket *passthrough;
	uint32_t flags, parameterLength;
	uint64_t identifier;
	uint8_t header, label, *operands;
	int ctype, pdu, command, button, translated, mtu, remaining;
	int count, index;
	uint8_t *position;

	if (length <= 1)
		return;
	header = packet[0];
	fragment = header & 2 ? &connection->avrcpResponse :
		&connection->avrcpCommand;
	switch (header & 0xc) {
	case 0:
		fragment->length = 0;
		header = packet[0];
		break;
	case 4:
		if ((unsigned int)(length - 3) <= 254) {
			fragment->data[0] = header & 0xf3;
			memcpy(fragment->data + 1,
						 packet + 2, length - 2);
			fragment->length = length - 1;
		}
		return;
	case 8:
	case 12:
		if (fragment->length + length >= 258)
			return;
		memcpy(fragment->data + fragment->length,
					 packet + 1, length - 1);
		length = length - 1 + fragment->length;
		fragment->length = length;
		if ((packet[0] & 0xc) == 8)
			return;
		packet = fragment->data;
		header = packet[0];
		fragment->length = 0;
		break;
	}
	avc = (BtAvcHeader *)packet;
	vendor = (BtAvrcpVendorPacket *)packet;
	passthrough = (BtAvrcpPassthroughPacket *)packet;
	operands = (uint8_t *)(avc + 1);
	if (header & 2) {
		if (length <= 5 || avc->transport.profileIdBe[0] != 0x11 ||
		    avc->transport.profileIdBe[1] != 0x0e ||
		    avc->subunit != 0x48 || length <= 7)
			return;
		if (avc->opcode == 0x7c)
			goto completeRequest;
		if (length == 8 || avc->opcode || vendor->companyId[0] ||
		    vendor->companyId[1] != 0x19 ||
		    vendor->companyId[2] != 0x58)
			return;
		if (length > 15) {
			ctype = avc->ctype;
			if (ctype == 12 && vendor->parameters[0] == 3) {
				count = vendor->parameters[1];
				if (length > count + 14) {
					for (index = 0; index < count; index++) {
						if (vendor->parameters[index + 2] == 13)
							goto registerVolume;
					}
					goto completeRequest;
				}
			}
		} else if (length == 15) {
			ctype = avc->ctype;
		} else {
			if (length != 14)
				return;
			goto volumeResponse;
		}
		pdu = vendor->pduId;
		if ((ctype & 0xfd) == 13 && pdu == 0x31) {
			if (vendor->parameterLengthBe[0] ||
			    vendor->parameterLengthBe[1] != 2 ||
			    vendor->parameters[0] != 13)
				return;
			state->volume = vendor->parameters[1] & 0x7f;
			BtNotifyEvent(14, ctype != 15, state->volume,
					       connection->deviceAddress);
			if (avc->ctype == 13) {
				flags = channel->flags;
				if (flags & 0x10000) {
					channel->flags = flags | 0x40000;
					return;
				}
				BtBeginAvrcpRequest(channel);
				goto registerVolume;
			}
			goto completeRequest;
		}
volumeResponse:
		if (vendor->pduId != 0x50 || vendor->parameterLengthBe[0] ||
		    vendor->parameterLengthBe[1] != 1)
			return;
		if (avc->ctype == 9) {
			state->volume = vendor->parameters[0] & 0x7f;
			BtNotifyEvent(14, 2, state->volume,
				      connection->deviceAddress);
		}
completeRequest:
		channel->flags &= ~0x10000;
		state->requestTimer = BtCancelTimer(state->requestTimer);
		flags = channel->flags;
		if (flags & 0x40000) {
			channel->flags = flags & ~0x40000;
			BtBeginAvrcpRequest(channel);
registerVolume:
			label = state->transactionLabel + 1;
			state->transactionLabel = label;
			{
				BtSendL2capPacket(channel->connection,
				channel->remoteCid, "1H 11# 11H 1W",
				(label << 4) & 0xf0, 0x110e, 3, 0x48,
				&gBtAvrcpVendorHeader, 4, 0x31, 0, 5, 13, 0);
				return;
			}
		}
		if (flags & 0x100000) {
			channel->flags = flags & ~0x100000;
			BtBeginAvrcpRequest(channel);
			button = state->queuedButton;
			channel->flags |= 0x20000;
			state->buttonRepeatTimer = BtCancelTimer(state->buttonRepeatTimer);
			if (button & 0x80) {
				state->sentButton = -1;
			} else {
				state->sentButton = button;
				BtQueueTimer(&state->buttonRepeatTimer,
					     BtAvrcpRepeatButton, channel, 500000);
			}
			label = state->transactionLabel + 1;
			state->transactionLabel = label;
			{
				BtSendL2capPacket(channel->connection,
				channel->remoteCid, "1H 111 11",
				(label << 4) & 0xf0, 0x110e, 0, 0x48,
				0x7c, button, 0);
				return;
			}
		}
		if (flags & 0x80000) {
			channel->flags = flags & ~0x80000;
			BtBeginAvrcpRequest(channel);
			button = state->volume & 0x7f;
			label = state->transactionLabel + 1;
			state->transactionLabel = label;
			{
				BtSendL2capPacket(channel->connection,
				channel->remoteCid, "1H 11# 11H 1",
				(label << 4) & 0xf0, 0x110e, 0, 0x48,
				&gBtAvrcpVendorHeader, 4, 0x50, 0, 1, button);
				return;
			}
		}
		if (flags & 0x20000) {
			channel->flags = flags & ~0x20000;
			BtNotifyEvent(15, 0, 0, connection->deviceAddress);
		}
		return;
	}
	if (length <= 2)
		return;
	avc->transport.header = header | 2;
	if (avc->transport.profileIdBe[0] != 0x11 ||
	    avc->transport.profileIdBe[1] != 0x0e) {
		length = 3;
		avc->transport.header = header | 3;
		goto sendResponse;
	}
	if (length <= 7)
		goto rejectAvc;
	ctype = avc->ctype;
	if (!ctype && avc->subunit == 0x48 && avc->opcode == 0x7c)
		goto receiveButton;
	if (length > 10) {
		if (ctype == 1 && avc->subunit == 0xff) {
			if (avc->opcode == 0x30) {
				operands[0] = 7;
				operands[1] = 0x18;
				operands[2] = 0;
				operands[3] = 0x19;
				operands[4] = 0x58;
				avc->ctype = 12;
				goto sendResponse;
			}
			if (avc->opcode == 0x31) {
				operands[0] = 7;
				operands[1] = 0x18;
				avc->ctype = 12;
				goto sendResponse;
			}
			goto vendorCommand;
		}
	} else if (length <= 9) {
		goto rejectAvc;
	}
vendorCommand:
	if (avc->subunit != 0x48 || avc->opcode ||
	    vendor->companyId[0] || vendor->companyId[1] != 0x19 ||
	    vendor->companyId[2] != 0x58) {
		length = 9;
		goto rejectAvc;
	}
	command = (ctype << 8) | vendor->pduId;
	switch (command) {
	case 0x111:
		if (length > 11 && !vendor->packetType &&
		    !vendor->parameterLengthBe[0] &&
		    !vendor->parameterLengthBe[1]) {
			vendor->parameterLengthBe[1] = 1;
			vendor->parameters[0] = 0;
			avc->ctype = 12;
			goto sendResponse;
		}
		goto invalidParameter;
	case 0x120:
		if (length <= 21 || vendor->packetType)
			goto invalidParameter;
		BtUnpack(vendor->parameters, 8, "8", &identifier);
		if (identifier)
			goto invalidParameter;
		BtUnpack(vendor->parameterLengthBe, 2, "H", &parameterLength);
		count = vendor->parameters[8];
		if (count * 4 + 9 != parameterLength)
			goto invalidParameter;
		if (count) {
			for (position = vendor->parameters + 9;
			     position < packet + length; position += 4) {
				BtUnpack(position, 4, "W", &parameterLength);
				if (parameterLength == 1)
					goto titleResponse;
			}
			goto invalidParameter;
		}
titleResponse:
		parameterLength = gBtAvrcpTitleLength;
		if (parameterLength <= 493) {
			length = parameterLength + 22;
		} else {
			length = 515;
			parameterLength = 493;
			vendor->packetType |= 1;
			state->continuationPosition = 493;
		}
		BtPack(vendor->parameterLengthBe, parameterLength + 11,
		       "H1WHH#", parameterLength + 9, 1, 1, 111,
		       gBtAvrcpTitleLength, gBtAvrcpTitle, parameterLength);
		avc->ctype = 12;
		goto sendResponse;
	case 0x130:
		length = 22;
		BtPack(vendor->parameters, 9, "WW1", -1, -1,
		       gBtAvrcpPlayStatus);
		vendor->parameterLengthBe[1] = 9;
		avc->ctype = 12;
		goto sendResponse;
	case 0x331:
		if (length <= 14 || vendor->packetType ||
		    vendor->parameterLengthBe[0] ||
		    vendor->parameterLengthBe[1] != 5)
			goto invalidParameter;
		if (vendor->parameters[0] == 1) {
			length = 15;
			vendor->parameterLengthBe[1] = 2;
			avc->ctype = 15;
			vendor->parameters[1] = gBtAvrcpPlayStatus;
			state->playStatusRegistered = 1;
			state->playStatusLabel = avc->transport.header;
			goto sendResponse;
		}
		if (vendor->parameters[0] == 2) {
			vendor->parameterLengthBe[1] = 9;
			length = 22;
			memset(vendor->parameters + 1,
			       gBtAvrcpPlayStatusKnown ? 0 : 255, 8);
			avc->ctype = 15;
			state->trackRegistered = 1;
			state->trackLabel = avc->transport.header;
			goto sendResponse;
		}
		goto invalidParameter;
	case 0x41:
		if (length <= 12 || vendor->packetType ||
		    vendor->parameterLengthBe[0] ||
		    vendor->parameterLengthBe[1] != 1 ||
		    vendor->parameters[0] != 0x20 || state->continuationPosition <= 0)
			goto invalidParameter;
		state->continuationPosition = vendor->parameterLengthBe[0];
		length = 13;
		avc->ctype = 9;
		goto sendResponse;
	case 0x40:
		if (length <= 12 || vendor->packetType ||
		    vendor->parameterLengthBe[0] ||
		    vendor->parameterLengthBe[1] != 1 ||
		    vendor->parameters[0] != 0x20 || state->continuationPosition <= 0)
			goto invalidParameter;
		vendor->pduId = 0x20;
		parameterLength = gBtAvrcpTitleLength - state->continuationPosition;
		if (parameterLength > 502) {
			vendor->packetType = 2;
			parameterLength = 502;
			length = 512;
		} else {
			length = parameterLength + 13;
			vendor->packetType = 3;
		}
		BtPack(vendor->parameterLengthBe, parameterLength + 2, "H#",
		       parameterLength, gBtAvrcpTitle + state->continuationPosition,
		       parameterLength);
		avc->ctype = 12;
		state->continuationPosition += parameterLength;
		if (state->continuationPosition >= (int)gBtAvrcpTitleLength)
			state->continuationPosition = 0;
		goto sendResponse;
	case 0x110:
		if (length <= 13 || vendor->packetType ||
		    vendor->parameterLengthBe[0] ||
		    vendor->parameterLengthBe[1] != 1)
			goto unsupportedPdu;
		if (vendor->parameters[0] == 2) {
			vendor->parameters[1] = 1;
			length = 18;
			vendor->parameters[2] = 0;
			vendor->parameterLengthBe[1] = 5;
			vendor->parameters[3] = 0x19;
			vendor->parameters[4] = 0x58;
			avc->ctype = 12;
			goto sendResponse;
		}
		if (vendor->parameters[0] == 3) {
			vendor->parameters[2] = 1;
			vendor->parameterLengthBe[1] = 4;
			length = 17;
			vendor->parameters[1] = 2;
			vendor->parameters[3] = 2;
			avc->ctype = 12;
			goto sendResponse;
		}
		goto invalidParameter;
	case 0x60:
		if (length <= 14 || vendor->packetType ||
		    vendor->parameterLengthBe[0] ||
		    vendor->parameterLengthBe[1] != 2)
			goto unsupportedPdu;
		vendor->parameterLengthBe[1] = 1;
		if (!vendor->parameters[0] && vendor->parameters[1] == 1) {
			avc->ctype = 9;
			length = 14;
			vendor->parameters[0] = 4;
		} else {
			avc->ctype = 10;
			length = 14;
			vendor->parameters[0] = 17;
		}
		goto sendResponse;
	case 0x14:
	case 0x112:
	case 0x113:
		goto invalidParameter;
	default:
		goto unsupportedPdu;
	}
invalidParameter:
	avc->ctype = 10;
	length = 14;
	vendor->parameterLengthBe[1] = 1;
	vendor->parameters[0] = 1;
	goto sendResponse;
unsupportedPdu:
	avc->ctype = 10;
	length = 14;
	vendor->parameterLengthBe[1] = 1;
	vendor->parameters[0] = 0;
	goto sendResponse;
rejectAvc:
	avc->ctype = 10;
	goto sendResponse;
receiveButton:
	avc->ctype = 9;
	state->buttonReleaseTimer = BtCancelTimer(state->buttonReleaseTimer);
	button = passthrough->operationId;
	if (state->translatePlayPause && (unsigned int)((button & 0x7f) - 0x44) <= 2) {
		if (!(button & 0x80)) {
			button = gBtAvrcpPlayStatus == 1 ? 0x46 : 0x44;
			goto dispatchButton;
		}
		if (state->receivedButton >= 0)
			button = state->receivedButton | 0x80;
	}
	if ((button & 0x17f) == 0x7e) {
		if (length != 13 || memcmp(&passthrough->operandLength,
					 gBtAvrcpGroupNavigation, 5))
			goto sendResponse;
		if (passthrough->operands[4] == 1)
			button = (button & 0x80) | 0x101;
		else if (passthrough->operands[4] == 2)
			button = (button & 0x80) | 0x102;
		else
			goto sendResponse;
	}
dispatchButton:
	if (button & 0x80) {
		if (state->receivedButton != (button & ~0x80))
			goto sendResponse;
		translated = state->translatedButton;
		if (translated >= 0) {
			if (!state->translatedButtonTimer) {
				BtNotifyEvent(13, 0, translated | 0x80,
					      connection->deviceAddress);
				goto releaseTranslated;
			}
			BtNotifyEvent(13, 0, state->receivedButton,
				      connection->deviceAddress);
		}
		BtNotifyEvent(13, 0, button, connection->deviceAddress);
releaseTranslated:
		state->receivedButton = -1;
		state->translatedButton = -1;
		state->translatedButtonTimer = BtCancelTimer(state->translatedButtonTimer);
		goto sendResponse;
	}
	if (state->receivedButton >= 0) {
		if (button == state->receivedButton) {
			BtQueueTimer(&state->buttonReleaseTimer,
				     BtAvrcpReleaseButton, channel, 3000000);
			goto sendResponse;
		}
		translated = state->translatedButton;
		if (translated < 0) {
			state->receivedButton |= 0x80;
			BtNotifyEvent(13, 0, state->receivedButton,
				      connection->deviceAddress);
		} else {
			if (!state->translatedButtonTimer) {
				state->translatedButton = translated | 0x80;
				BtNotifyEvent(13, 0, state->translatedButton,
					      connection->deviceAddress);
			}
			state->translatedButton = -1;
			state->translatedButtonTimer = BtCancelTimer(state->translatedButtonTimer);
		}
	}
	state->receivedButton = button;
	BtQueueTimer(&state->buttonReleaseTimer, BtAvrcpReleaseButton,
		     channel, 3000000);
	if (button == 0x4b || button == 0x4c) {
		state->translatedButton = button == 0x4b ? 0x49 : 0x48;
		BtQueueTimer(&state->translatedButtonTimer,
			     BtAvrcpReleaseTranslatedButton, channel, 1000000);
	} else {
		BtNotifyEvent(13, 0, button, connection->deviceAddress);
	}
sendResponse:
	mtu = channel->mtu;
	if (length <= mtu) {
		avc->transport.header &= 0xf3;
		{
			BtSendL2capPacket(connection, channel->remoteCid,
					"#", packet, length);
			return;
		}
	}
	remaining = length - 1;
	BtSendL2capPacket(connection, channel->remoteCid, "11#",
		(avc->transport.header & 0xf3) | 4,
		remaining / (mtu - 1) + 1, packet + 1, mtu - 2);
	mtu = channel->mtu;
	position = packet + mtu - 1;
	remaining += 2 - mtu;
	while (remaining > 0) {
		count = mtu - 1;
		if (remaining <= count) {
				BtSendL2capPacket(connection, channel->remoteCid,
				"1#", (avc->transport.header & 0xf3) | 12,
				position, remaining);
				return;
			}
		remaining -= count;
		BtSendL2capPacket(connection, channel->remoteCid,
			"1#", (avc->transport.header & 0xf3) | 8, position, count);
		position += count;
		mtu = channel->mtu;
	}
	return;
}

int BtSetAvrcpTitle(BtStack *stack, const void *title, unsigned int length)
{
	BtConnection *connection;
	BtChannel *channel;

	if (gBtAvrcpTitleLength == length && !memcmp(gBtAvrcpTitle, title, length))
		return 0;
#if FW_VERSION >= 0x04000000
	__builtin___memcpy_chk(gBtAvrcpTitle, title, length, sizeof(gBtAvrcpTitle));
#else
	memcpy(gBtAvrcpTitle, title, length);
#endif
	gBtAvrcpTitleLength = length;
	if ((stack->mode & 0x38) != 8)
		return 0;
	for (connection = stack->connections;
	     connection < stack->connections + 7; connection++) {
		__builtin_prefetch(&connection[4].device);
		if (!connection->deviceAddress || !(connection->device->profiles & 0x10))
			continue;
		channel = BtFindChannel(connection, 4, 23);
		if (!channel || !channel->protocol.avrcp.trackRegistered)
			continue;
		BtSendL2capPacket(channel->connection, channel->remoteCid,
			"1H 11# 11H 18",
			(channel->protocol.avrcp.trackLabel & 0xf0) | 2,
			0x110e, 13, 0x48, &gBtAvrcpVendorHeader, 4, 0x31,
			0, 9, 2, (uint64_t)0);
		channel->protocol.avrcp.trackRegistered = 0;
	}
	return 0;
}

int BtSetAvrcpPlayStatus(BtStack *stack, int status)
{
	BtConnection *connection;
	BtChannel *channel;

	gBtAvrcpPlayStatusKnown = 1;
	if (gBtAvrcpPlayStatus == status)
		return 0;
	gBtAvrcpPlayStatus = status;
	if ((stack->mode & 0x38) != 8)
		return 0;
	for (connection = stack->connections;
	     connection < stack->connections + 7; connection++) {
		__builtin_prefetch(&connection[4].device);
		if (!connection->deviceAddress || !(connection->device->profiles & 0x10))
			continue;
		channel = BtFindChannel(connection, 4, 23);
		if (!channel || !channel->protocol.avrcp.playStatusRegistered)
			continue;
		BtSendL2capPacket(channel->connection, channel->remoteCid,
			"1H 11# 11H 11",
			(channel->protocol.avrcp.playStatusLabel & 0xf0) | 2,
			0x110e, 13, 0x48, &gBtAvrcpVendorHeader, 4, 0x31,
			0, 2, 1, gBtAvrcpPlayStatus);
		channel->protocol.avrcp.playStatusRegistered = 0;
	}
	return 0;
}

uintptr_t BtClearAvrcpConnection(BtConnection *connection)
{
	BtChannel *channel;
	BtAvrcpChannelState *state;
	uintptr_t result = (uintptr_t)connection;
	int button, translated;

	for (channel = connection->channels;
	     channel < connection->channels + 16; channel++) {
		__builtin_prefetch(&channel[2].psm);
		if (channel->psm != 23)
			continue;
		state = &channel->protocol.avrcp;
		button = state->receivedButton;
		if (button >= 0) {
			translated = state->translatedButton;
			if (translated >= 0) {
				state->translatedButton = translated | 0x80;
				BtNotifyEvent(13, 0, translated | 0x80,
					      connection->deviceAddress);
			} else {
				state->receivedButton = button | 0x80;
				BtNotifyEvent(13, 0, button | 0x80,
					      connection->deviceAddress);
			}
			state->receivedButton = -1;
			state->translatedButton = -1;
		}
		__builtin_prefetch(&channel[2].protocol.avrcp.requestTimer);
		state->requestTimer = BtCancelTimer(state->requestTimer);
		state->buttonReleaseTimer = BtCancelTimer(state->buttonReleaseTimer);
		state->buttonRepeatTimer = BtCancelTimer(state->buttonRepeatTimer);
		result = BtCancelTimer(state->translatedButtonTimer);
		state->translatedButtonTimer = result;
	}
	return result;
}
