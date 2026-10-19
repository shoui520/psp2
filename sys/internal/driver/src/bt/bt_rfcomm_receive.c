/* RFCOMM frame reception; original source filename unavailable. */
#include "bt_internal.h"

int BtSendRfcommFrame(BtConnection *connection, BtChannel *channel,
		      unsigned int address, ...);
int BtSendL2capDisconnectRequest(BtConnection *connection, BtChannel *channel);
BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			 int identifier);
int BtUnpack(const uint8_t *source, int remaining, const char *format, ...);
uintptr_t BtSetConnectedProfiles(BtConnection *connection, BtChannel *channel,
				unsigned int profiles);
void BtDeliverConnectionData(BtConnection *connection, uint32_t profile,
				 uint8_t *packet, unsigned int length);
uintptr_t BtHspOpened(BtConnection *connection);
uintptr_t BtSerialPortOpened(BtConnection *connection);
int BtSendRfcommData(BtConnection *connection, unsigned int dlci,
		     const void *data, int length, int userMode);
uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
			unsigned int argument, uint64_t deviceAddress);

uintptr_t BtCompleteRfcommConnection(BtConnection *connection,
				    BtChannel *channel)
{
	BtRfcommChannelState *state = &channel->protocol.rfcomm;
	BtRfcommRemotePortSettings query;
	unsigned int profile = state->profile;
	unsigned int flags;

	if (profile == 0x40000 || profile == 0x80000) {
		flags = state->flags;
		if (!(channel->flags & 2)) {
			if (!(flags & 0x80)) {
				state->flags = flags | 0x40;
				query.header.type = 0x93;
				query.header.length = 0x11;
				query.dlci = (state->dlci << 2) | 3;
				query.baudRate = 7;
				query.dataFormat = 3;
				query.flowControl = 12;
				query.xon = 0x11;
				query.xoff = 0x13;
				query.parameterMask[0] = 0x7f;
				query.parameterMask[1] = 0x3f;
				return BtSendRfcommFrame(connection, channel, 3, 0xff,
							 &query, 10);
			}
		} else if (!(flags & 0x200)) {
			state->flags = flags | 0x100;
			return (uintptr_t)connection;
		}
	} else {
		flags = state->flags;
	}
	if (!(flags & 0x1000)) {
		state->pendingCredits = 18;
		BtSendRfcommFrame(connection, channel, (state->dlci << 2) | 3,
				  0xff, NULL, 0);
		profile = state->profile;
		flags = state->flags;
	}
	state->flags = flags | 0x2000;
	if (profile == 0x10000 || profile == 0x20000)
		return BtHspOpened(connection);
	if (profile == 0x80000)
		return BtSerialPortOpened(connection);
	return BtSetConnectedProfiles(connection, NULL, profile);
}

void BtReceiveRfcomm(BtConnection *connection, BtChannel *channel,
			uint8_t *packet, int length)
{
	BtRfcommChannelState *state = &channel->protocol.rfcomm;
	union {
		BtRfcommParameterNegotiation negotiation;
		BtRfcommModemStatus modem;
		BtRfcommRemotePortSettings port;
	} response;
	BtRfcommCommandHeader *command;
	BtRfcommParameterNegotiation *negotiation;
	uint8_t *payload;
	unsigned int address, control, frameLength, dlci, frameAddress, flags;
	unsigned int commandType, credits, profile;
	uintptr_t result;
	int maximumFrameSize, commandLength;

	(void)length;
	address = *packet++;
	if (!(address & 1))
		address |= (unsigned int)*packet++ << 8;
	control = *packet++;
	frameLength = *packet++;
	if (!(frameLength & 1))
		frameLength |= (unsigned int)*packet++ << 8;
	payload = packet;
	dlci = state->dlci;
	result = dlci << 2;
	frameAddress = result | 3;
	if (!dlci && (address & ~3U))
		goto dataChannel;
	if ((address | 2) == 3) {
		switch (control) {
		case 0x3f:
			if (channel->flags & 2)
				result = BtSendRfcommFrame(connection, channel,
						   (uint8_t)address, 0x73);
			goto out;
		case 0x73:
			if (channel->flags & 2)
				goto out;
			flags = state->flags;
			result = flags << 31;
			if (!(flags & 1))
				goto out;
			state->flags = (flags & ~3U) | 2;
			response.negotiation.header.type = 0x83;
			response.negotiation.header.length = 0x11;
			response.negotiation.dlci = (uint8_t)frameAddress >> 2;
			response.negotiation.flowControl = 0xf0;
			response.negotiation.priority = 0;
			response.negotiation.acknowledgementTimer = 0;
			response.negotiation.maxRetransmissions = 0;
			response.negotiation.credits = 0;
			credits = (uint8_t)frameAddress >> 2;
			if (credits) {
				if (credits <= 7)
					response.negotiation.priority = 7;
				else if (credits <= 15)
					response.negotiation.priority = 15;
				else if (credits <= 23)
					response.negotiation.priority = 23;
				else if (credits <= 31)
					response.negotiation.priority = 31;
				else if (credits <= 39)
					response.negotiation.priority = 39;
				else if (credits <= 47)
					response.negotiation.priority = 47;
				else if (credits <= 55)
					response.negotiation.priority = 55;
				else if (credits <= 61)
					response.negotiation.priority = 61;
			}
			maximumFrameSize = channel->mtu - 6;
			response.negotiation.frameSizeLe[0] = maximumFrameSize;
			response.negotiation.frameSizeLe[1] = maximumFrameSize >> 8;
			result = BtSendRfcommFrame(connection, channel, 3, 0xff,
						   &response.negotiation, 10);
			goto out;
		case 0x53:
			BtSendRfcommFrame(connection, channel,
					  (channel->flags & 2) ? 3 : 1, 0x73);
			goto disconnect;
		default:
			break;
		}
		if (control != 0xef)
			goto out;
		command = (BtRfcommCommandHeader *)payload;
		commandType = command->type;
		switch (commandType) {
		case 0x83:
			if (!(channel->flags & 2))
				break;
			negotiation = (BtRfcommParameterNegotiation *)payload;
			state->dlci = negotiation->dlci;
			switch (negotiation->dlci >> 1) {
			case 3:
				state->profile = 0x20000;
				connection->hspAudioGatewayChannel = negotiation->dlci;
				break;
			case 4:
				state->profile = 0x10000;
				connection->hspHeadsetChannel = negotiation->dlci;
				break;
			case 5:
				state->profile = 0x4000;
				connection->objectPushChannel = negotiation->dlci;
				break;
			case 6:
				state->profile = 0x1000;
				connection->imagingChannel = negotiation->dlci;
				break;
			case 7:
				state->profile = 0x40000;
				connection->serialPortChannel = negotiation->dlci;
				break;
			}
			frameAddress = (uint8_t)((negotiation->dlci << 2) | 3);
			BtUnpack(negotiation->frameSizeLe, 2, "2", &maximumFrameSize);
			if (channel->mtu - 5 <= maximumFrameSize)
				maximumFrameSize = channel->mtu - 6;
			if (negotiation->flowControl & 0xf0)
				state->transmitCredits = negotiation->credits;
			else
				state->flags |= 0x1000;
			response.negotiation.header.type = 0x81;
			response.negotiation.header.length = 0x11;
			response.negotiation.dlci = frameAddress >> 2;
			response.negotiation.flowControl = (state->flags & 0x1000) ? 0 : 0xe0;
			response.negotiation.priority = negotiation->priority;
			response.negotiation.acknowledgementTimer = 0;
			response.negotiation.frameSizeLe[0] = maximumFrameSize;
			response.negotiation.frameSizeLe[1] = maximumFrameSize >> 8;
			response.negotiation.maxRetransmissions = 0;
			response.negotiation.credits = 0;
			result = BtSendRfcommFrame(connection, channel, 3, 0xff,
						   &response.negotiation, 10);
			goto out;
		case 0x81:
			flags = state->flags;
			if (!(flags & 2))
				goto out;
			if (flags & 0x800) {
				state->flags = flags & ~2U;
				goto out;
			}
			state->flags = (flags & ~6U) | 4;
			negotiation = (BtRfcommParameterNegotiation *)payload;
			if (negotiation->flowControl & 0xf0)
				state->transmitCredits = negotiation->credits;
			else
				state->flags = (flags & ~2U) | 0x1004;
			result = BtSendRfcommFrame(connection, channel,
						   (uint8_t)result | 3, 0x3f);
			goto out;
		case 0xe3:
			response.modem.header.type = 0xe1;
			response.modem.header.length = 5;
			response.modem.dlci = frameAddress;
			response.modem.signals = ((BtRfcommModemStatus *)payload)->signals;
			result = BtSendRfcommFrame(connection, channel, 3, 0xff,
						   &response.modem, 4);
			flags = state->flags;
			if (((BtRfcommModemStatus *)payload)->signals & 2)
				flags |= 0x400;
			else
				flags &= ~0x400U;
			if (flags & 0x10) {
				state->flags = flags & ~0x10U;
				if (!(flags & 8))
					goto complete;
			} else {
				state->flags = flags;
			}
			goto out;
		case 0xe1:
			flags = state->flags;
			result = flags << 28;
			if (!(flags & 8))
				goto out;
			state->flags = flags & ~8U;
			if (!(flags & 0x810))
				goto complete;
			goto out;
		case 0x23:
		case 0x53:
			commandLength = command->length >> 1;
			goto echoCommand;
		case 0x93:
			commandLength = command->length >> 1;
			if (commandLength == 8)
				goto echoCommand;
			if (commandLength != 1)
				goto out;
			response.port.header.type = 0x91;
			response.port.header.length = 0x11;
			response.port.dlci = ((BtRfcommCommandPacket *)payload)->payload[0];
			response.port.baudRate = 3;
			response.port.dataFormat = 3;
			response.port.flowControl = 0;
			response.port.xon = 0x11;
			response.port.xoff = 0x13;
			response.port.parameterMask[0] = 0x7f;
			response.port.parameterMask[1] = 0x3f;
			result = BtSendRfcommFrame(connection, channel, 3, 0xff,
						   &response.port, 10);
			goto out;
		case 0x63:
		case 0xa3:
			if (command->length == 1) {
				commandLength = 0;
				goto echoCommand;
			}
			goto out;
		case 0x91:
			flags = state->flags;
			if (!(flags & 0x40)) {
				state->flags = flags | 0x80;
				goto out;
			}
			state->flags = (flags & ~0xc0U) | 0x80;
			if (!(flags & 0x800))
				goto complete;
			goto out;
		default:
			goto out;
		}
		goto out;
	}
	if (address != frameAddress) {
		result |= 1;
		if (result != address)
			goto out;
		if (control == 0x73) {
			if (state->flags & 0x20)
				goto disconnect;
			goto out;
		}
		if ((control & ~0x10U) == 0xef)
			goto deliver;
		if (control == 0x53) {
			result = BtSendRfcommFrame(connection, channel,
						   ((dlci << 2) & 0xfc) | 1, 0x73);
			goto out;
		}
		if (control == 0x1f)
			goto disconnect;
		goto out;
	}
dataChannel:
	if (control == 0x73) {
		flags = state->flags;
		if (!(flags & 4)) {
			if (flags & 0x20)
				goto disconnect;
			goto out;
		}
		state->flags = (flags & ~0x1cU) | 0x18;
		goto sendModemStatus;
	}
	if (control == 0x3f) {
		state->flags |= 0x18;
		BtSendRfcommFrame(connection, channel,
				  (channel->flags & 2) ? (uint8_t)address : address & 0xfd,
				  0x73);
		goto sendModemStatus;
	}
	if ((control & ~0x10U) == 0xef)
		goto deliver;
	if (control == 0x53) {
		result = BtSendRfcommFrame(connection, channel,
					   (channel->flags & 2) ? (uint8_t)address : address & 0xfd,
					   0x73);
		goto out;
	}
	if (control == 0x1f) {
		profile = state->profile;
		if (!(profile & connection->excludedProfiles)) {
			connection->excludedProfiles |= profile | 0x20000000;
			result = BtSetConnectedProfiles(connection, NULL, 0);
			goto out;
		}
		goto disconnect;
	}
	goto out;
deliver:
	if (control & 0x10) {
		result = *payload++;
		state->transmitCredits += result;
		if (state->transmit.used > 0)
			result = BtSendRfcommFrame(connection, channel,
						   frameAddress, 0xff, NULL, 0);
	}
	if (frameLength >> 1) {
		state->pendingCredits++;
		BtDeliverConnectionData(connection, state->profile,
						 payload, frameLength >> 1);
	}
	goto out;
sendModemStatus:
	response.modem.header.type = 0xe3;
	response.modem.header.length = 5;
	response.modem.dlci = address;
	response.modem.signals = 13;
	result = BtSendRfcommFrame(connection, channel, 3, 0xff,
				   &response.modem, 4);
	goto out;
echoCommand:
	command->type &= ~2U;
	result = BtSendRfcommFrame(connection, channel, address, 0xef,
				   payload, commandLength + 2);
	switch (command->type) {
	case 0x61:
		state->flags |= 0x400;
		break;
	case 0xa1:
		state->flags &= ~0x400U;
		result = BtSendRfcommFrame(connection, channel,
					   (state->dlci << 2) | 3, 0xff, NULL, 0);
		break;
	case 0x91:
		flags = state->flags;
		if (flags & 0x100) {
			state->flags = (flags & ~0x300U) | 0x200;
			goto complete;
		}
		state->flags = flags | 0x200;
		break;
	}
	goto out;
disconnect:
	state->flags = (state->flags & ~0x20U) | 0x800;
	result = BtSendL2capDisconnectRequest(connection, channel);
	goto out;
complete:
	result = BtCompleteRfcommConnection(connection, channel);
out:
	return;
}

int BtSendRfcommData(BtConnection *connection, unsigned int dlci,
		     const void *data, int length, int userMode)
{
	BtChannel *channel = BtFindChannel(connection, 7, (uint16_t)dlci);

	if (!channel)
		return (int)0x802f0703;
	if (userMode)
		return (int)0x802f0704;
	return BtSendRfcommFrame(connection, channel,
				 (channel->protocol.rfcomm.dlci << 2) | 3,
				 0xff, data, length);
}

int BtSendSerialData(BtConnection *connection, const void *data,
		     unsigned int length)
{
	return BtSendRfcommData(connection, connection->serialPortChannel,
				data, length, 0);
}

uintptr_t BtSerialPortOpened(BtConnection *connection)
{
	return BtSetConnectedProfiles(connection, NULL, 0x80000);
}

int BtReceiveSerialPort(BtConnection *connection, const uint8_t *packet,
			int length)
{
	BtRingBuffer *receive = &connection->serialReceive;
	int capacity;

	if (!receive->data || receive->capacity <= 0) {
		BtRingReset(receive);
		receive->capacity = 256;
		receive->data = connection->serialReceiveData;
		capacity = 256;
	} else {
		capacity = receive->capacity;
	}
	if ((int32_t)((uint32_t)capacity - (uint32_t)receive->used) >= length) {
		BtRingWrite(receive, packet, length, 0);
		BtNotifyEvent(27, 0, receive->used, connection->deviceAddress);
	}
	BtSendRfcommData(connection, connection->serialPortChannel, NULL, 0, 0);
	return 0;
}
