/* ACL and L2CAP reception; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>

typedef struct BtL2capPacket {
	uint8_t payloadLengthLe[2];
	uint8_t channelIdLe[2];
	uint8_t payload[];
} BtL2capPacket;

typedef struct BtL2capSignalPacket {
	uint8_t code;
	uint8_t identifier;
	uint8_t parameterLengthLe[2];
	uint8_t parameters[];
} BtL2capSignalPacket;

typedef struct BtL2capOption {
	uint8_t type;
	uint8_t length;
	uint8_t value[];
} BtL2capOption;

_Static_assert(sizeof(BtL2capPacket) == 4, "L2CAP header");
_Static_assert(offsetof(BtL2capPacket, payload) == 4, "L2CAP payload");
_Static_assert(sizeof(BtL2capSignalPacket) == 4, "L2CAP signal header");
_Static_assert(offsetof(BtL2capSignalPacket, parameters) == 4, "L2CAP signal parameters");
_Static_assert(sizeof(BtL2capOption) == 2, "L2CAP option header");
_Static_assert(offsetof(BtL2capOption, value) == 2, "L2CAP option value");

int BtUnpack(const uint8_t *source, int remaining, const char *format, ...);
int BtPack(uint8_t *destination, int remaining, const char *format, ...);
BtConnection *BtFindConnection(BtStack *stack, unsigned char selector,
			       uint64_t identifier);
BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			 int identifier);
int BtSendL2capPacket(BtConnection *connection, int channelId,
		     const char *format, ...);
int BtSendL2capConnectionResponse(BtConnection *connection, BtChannel *channel,
				  int result, int status);
int BtSendL2capConfigurationRequest(BtConnection *connection, BtChannel *channel);
int BtInitializeSdpChannel(BtConnection *connection, BtChannel *channel);
int BtInitializeRfcomm(BtConnection *connection, BtChannel *channel,
		      unsigned int profile, unsigned int dlci);
int BtInitializeBnep(BtConnection *connection, BtChannel *channel,
		     unsigned int profile);
int BtInitializeHidChannels(BtConnection *connection, BtChannel *channel);
int BtInitializeAvrcp(BtConnection *connection, BtChannel *channel);
int BtInitializeAvdtp(BtConnection *connection, BtChannel *channel,
		      unsigned int profile);
int BtInitializeAvrcpBrowsing(BtConnection *connection, BtChannel *channel);
int BtAdvanceConnection(BtConnection *connection);
int BtDisconnectConnection(BtStack *stack, BtConnection *connection);
uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
			unsigned int argument, uint64_t deviceAddress);

void BtDispatchAclPacket(BtStack *stack, uint8_t *packet, int length)
{
	BtAclPacket *acl = (BtAclPacket *)packet;
	BtL2capPacket *l2cap;
	BtL2capSignalPacket *command;
	BtConnection *connection;
	BtChannel *channel;
	uint32_t handleAndFlags, aclLength;
	uint32_t l2capLength, channelId;
	uint32_t code, identifier, commandLength;
	uint32_t psm, localCid, remoteCid, response, status, configFlags;
	uint32_t informationType;
	uint8_t options[44];
	uint8_t *optionEnd;
	BtL2capOption *option;
	int packetLength, remaining, optionBytes, optionLength;
	unsigned int flags;

	if (length <= 3)
		return;
	BtUnpack(packet, sizeof(*acl), "22", &handleAndFlags, &aclLength);
	packetLength = aclLength + sizeof(*acl);
	if (packetLength != length || (handleAndFlags & 0x4000))
		return;
	connection = BtFindConnection(stack, 2, handleAndFlags & 0xfff);
	if (!connection)
		return;
	connection->statistics.aclReceivePackets++;
	connection->statistics.aclReceiveBytes += packetLength;
	if ((handleAndFlags & 0x3000) == 0x1000) {
		if ((int)(connection->l2capReceive.used + aclLength) > 2050)
			return;
		if ((int)aclLength > 0) {
			memcpy(connection->l2capReceive.data + connection->l2capReceive.used,
			       acl->payload, aclLength);
			connection->l2capReceive.used += aclLength;
			if (connection->l2capReceive.expectedLength > connection->l2capReceive.used)
				return;
		}
		aclLength = connection->l2capReceive.expectedLength - 4;
		l2capLength = connection->l2capReceive.expectedLength - 8;
		channelId = connection->l2capReceive.channelId;
		acl = (BtAclPacket *)connection->l2capReceive.data;
	} else {
		if ((handleAndFlags & 0x3000) != 0x2000)
			return;
		BtUnpack(acl->payload, sizeof(BtL2capPacket), "22", &l2capLength, &channelId);
		connection->l2capReceive.used = packetLength;
		if ((int)(uint16_t)packetLength - 7 <= (int)l2capLength) {
			if (packetLength <= 2050) {
				memcpy(connection->l2capReceive.data, acl, packetLength);
				connection->l2capReceive.expectedLength = l2capLength + 8;
				connection->l2capReceive.channelId = channelId;
			}
			return;
		}
	}
	l2cap = (BtL2capPacket *)acl->payload;
	if (channelId != 1) {
		channel = BtFindChannel(connection, 5, (uint16_t)channelId);
		if (channel && channel->ops && channel->ops->receive)
			channel->ops->receive(connection, channel, l2cap->payload, l2capLength);
		return;
	}
	command = (BtL2capSignalPacket *)l2cap->payload;
	remaining = l2capLength;
	for (;;) {
		BtUnpack((uint8_t *)command, remaining, "112", &code, &identifier, &commandLength);
		if ((int32_t)((uint32_t)remaining - 3) <= (int)commandLength)
			return;
		switch (code) {
		case 1:
			break;
		case 2:
			BtUnpack(command->parameters, commandLength, "22", &psm, &remoteCid);
			channel = BtFindChannel(connection, 20, (uint16_t)psm);
			if (!channel)
				break;
			channel->flags |= 2;
			channel->remoteCid = remoteCid;
			channel->identifier = identifier;
			switch (psm) {
			case 1:
				BtInitializeSdpChannel(connection, channel);
				break;
			case 3:
				BtInitializeRfcomm(connection, channel, 0, 0);
				break;
			case 15:
				BtInitializeBnep(connection, channel, 0);
				break;
			case 17: case 19:
				BtInitializeHidChannels(connection, channel);
				break;
			case 23:
				BtInitializeAvrcp(connection, channel);
				break;
			case 25:
				if ((connection->flags & 0x20) &&
				    (connection->avdtp.signalingChannel->flags & 1)) {
					BtSendL2capConnectionResponse(connection, channel, 4, 0);
					channel->psm = 0;
					return;
				}
				BtInitializeAvdtp(connection, channel, 0);
				break;
			case 27:
				BtInitializeAvrcpBrowsing(connection, channel);
				break;
			default:
				BtSendL2capConnectionResponse(connection, channel, 2, 0);
				channel->psm = 0;
				return;
			}
			BtSendL2capPacket(connection, 1, "112 2222", 3, channel->identifier,
					  8, channel->localCid, channel->remoteCid, 1, 2);
			if (psm == 1)
				BtSendL2capConnectionResponse(connection, channel, 0, 0);
			else
				BtAdvanceConnection(connection);
			break;
		case 3:
			BtUnpack(command->parameters, commandLength, "2222", &remoteCid,
				 &localCid, &response, &status);
			if (response) {
				if ((int)response < 0 || response - 2 > 2)
					break;
				channel = BtFindChannel(connection, 5, (uint16_t)localCid);
				if (!channel)
					break;
				if (connection->avdtp.signalingChannel == channel) {
					connection->flags &= ~0x20u;
					connection->avdtp.signalingChannel = NULL;
				}
				channel->psm = 0;
			} else {
				channel = BtFindChannel(connection, 5, (uint16_t)localCid);
				if (!channel)
					break;
				channel->flags |= 8;
				channel->remoteCid = remoteCid;
				BtSendL2capConfigurationRequest(connection, channel);
			}
			break;
		case 4:
			BtUnpack(command->parameters, commandLength, "22", &localCid, &configFlags);
			channel = BtFindChannel(connection, 5, (uint16_t)localCid);
			if (!channel)
				break;
			channel->flags |= 0x20;
			channel->identifier = identifier;
			option = (BtL2capOption *)(command->parameters + 4);
			optionBytes = commandLength - 4;
			while (optionBytes > 1 && option->length + 1 < optionBytes) {
				optionLength = option->length;
				switch (option->type & 0x7f) {
				case 1:
					if (optionLength != 2)
						goto unknownOption;
					BtUnpack(option->value, 2, "2", &channel->mtu);
					break;
				case 2:
					if (optionLength != 2)
						goto unknownOption;
					BtUnpack(option->value, 2, "2", &channel->flushTimeout);
					channel->flushConfiguration = 1;
					break;
				case 3:
					if (optionLength != sizeof(BtL2capQos))
						goto unknownOption;
					channel->qos = *(const BtL2capQos *)option->value;
					channel->qosConfiguration = 1;
					break;
				case 4:
					if (optionLength != sizeof(BtL2capRetransmission))
						goto unknownOption;
					channel->retransmission = *(const BtL2capRetransmission *)option->value;
					channel->retransmissionConfiguration = 1;
					break;
				default:
unknownOption:
					if (!(option->type & 0x80)) {
						BtSendL2capPacket(connection, 1, "112 222 #", 5,
							identifier, optionLength + 8, channel->remoteCid,
							configFlags, 3, option, optionLength + 2);
						goto nextCommand;
					}
					break;
				}
				option = (BtL2capOption *)(option->value + optionLength);
				optionBytes -= optionLength + 2;
			}
			optionEnd = options;
			if (channel->mtu >= 0 && !(configFlags & 1))
				optionEnd += BtPack(options, 4, "112", 1, 2, channel->mtu);
			if (channel->flushConfiguration == 1 && !(configFlags & 1)) {
				optionEnd += BtPack(optionEnd, 4, "112", 2, 2, channel->flushTimeout);
				channel->flushConfiguration = 2;
			}
			if (channel->qosConfiguration == 1 && !(configFlags & 1)) {
				BtPack(channel->qos.tokenRate, 2, "4", 0);
				optionEnd += BtPack(optionEnd, 24, "11#", 3, 22, &channel->qos, 22);
				channel->qosConfiguration = 2;
			}
			if (channel->retransmissionConfiguration == 1 && !(configFlags & 1)) {
				optionEnd += BtPack(optionEnd, 11, "11#", 4, 9, &channel->retransmission, 9);
				channel->retransmissionConfiguration = 2;
			}
			channel->flags |= 0x40;
			BtSendL2capPacket(connection, 1, "112 222 #", 5, channel->identifier,
					 optionEnd - options + 6, channel->remoteCid,
					 (uint16_t)configFlags, 0, options, optionEnd - options);
			if (configFlags & 1)
				break;
			flags = channel->flags;
			if (flags & 0x80)
				goto channelOpened;
			if (!(flags & 0x10)) {
				channel->flags = flags | 0x10;
				BtSendL2capConfigurationRequest(connection, channel);
			}
			break;
		case 5:
			BtUnpack(command->parameters, commandLength, "222", &localCid,
				 &configFlags, &response);
			channel = BtFindChannel(connection, 5, (uint16_t)localCid);
			if (!channel)
				break;
			if (!response) {
				flags = channel->flags | 0x80;
				channel->flags = flags;
				if (!(flags & 0x40))
					break;
channelOpened:
				if (!(flags & 0x1000)) {
					channel->flags = flags | 0x1000;
					if (channel->ops && channel->ops->opened)
						channel->ops->opened(connection, channel);
				}
			} else if (response == 1) {
				option = (BtL2capOption *)(command->parameters + 6);
				optionBytes = commandLength - 6;
				while (optionBytes > 1 && option->length + 1 < optionBytes) {
					optionLength = option->length;
					if ((option->type & 0x7f) == 1 && optionLength == 2) {
						int mtu = channel->mtu >> 1;

						if (mtu < 48)
							mtu = 48;
						channel->mtu = mtu;
					}
					option = (BtL2capOption *)(option->value + optionLength);
					optionBytes -= optionLength + 2;
				}
				BtSendL2capConfigurationRequest(connection, channel);
			}
			break;
		case 6:
			BtUnpack(command->parameters, commandLength, "22", &localCid, &remoteCid);
			channel = BtFindChannel(connection, 5, (uint16_t)localCid);
			if (!channel)
				break;
			channel->identifier = identifier;
			channel->flags |= 0x600;
			BtSendL2capPacket(connection, 1, "112 22", 7, channel->identifier,
					 4, channel->localCid, channel->remoteCid);
			channel->psm = 0;
			if (channel->ops && channel->ops->closed)
				channel->ops->closed(connection, channel);
			break;
		case 7:
			BtUnpack(command->parameters, commandLength, "22", &remoteCid, &localCid);
			channel = BtFindChannel(connection, 5, (uint16_t)localCid);
			if (!channel)
				break;
			channel->psm = 0;
			channel->flags |= 0x800;
			if (channel->ops && channel->ops->closed)
				channel->ops->closed(connection, channel);
			else
				BtDisconnectConnection(stack, connection);
			break;
		case 8:
			BtSendL2capPacket(connection, 1, "112 #", 9, identifier,
					 aclLength, command->parameters, commandLength);
			break;
		case 9:
			if (connection->echoResponseBuffer &&
			    (int)commandLength <= connection->echoResponseCapacity &&
			    !BtCopyOutProcess(command->parameters, connection->echoResponseBuffer,
					 commandLength, connection->echoResponseProcessId))
				BtNotifyEvent(20, 0, commandLength, connection->deviceAddress);
			break;
		case 10:
			if (commandLength != 2)
				break;
			BtUnpack(command->parameters, 2, "2", &informationType);
			if (informationType == 2)
				BtSendL2capPacket(connection, 1, "112 224", 11, identifier, 8, 2, 0, 0);
			else if (informationType == 1)
				BtSendL2capPacket(connection, 1, "112 222", 1, 0, 48);
			break;
		default:
			BtSendL2capPacket(connection, 1, "112 2", 1, identifier, 2, 0);
			break;
		}
nextCommand:
		remaining = (int32_t)((uint32_t)remaining - 4 - commandLength);
		if (remaining <= 0)
			return;
		command = (BtL2capSignalPacket *)(command->parameters + commandLength);
	}
}
