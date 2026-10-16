/* AVDTP signaling reception; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>

int BtSendL2capPacket(BtConnection *connection, int channelId,
		     const char *format, ...);
BtChannel *BtFindChannel(BtConnection *connection, unsigned char selector,
			 int identifier);
int BtSendL2capConnectionRequest(BtConnection *connection, BtChannel *channel);
int BtSendL2capDisconnectRequest(BtConnection *connection, BtChannel *channel);
int BtSelectAudioCapabilities(BtConnection *connection, unsigned int profile,
			      uint8_t *capabilities, int length);
uintptr_t BtNotifyAudioStarted(BtConnection *connection, int status,
			     unsigned int profiles);
uintptr_t BtNotifyAudioStopped(BtConnection *connection, int status,
			     unsigned int profiles);
uintptr_t BtSetConnectedProfiles(BtConnection *connection, BtChannel *channel,
				unsigned int profiles);
int BtUnpack(const uint8_t *source, int remaining, const char *format, ...);
int BtQueueTimer(unsigned int *timer, void (*callback)(void *argument),
		 void *argument, SceInt64 delay);
void BtDisconnectTimedOut(void *argument);

void BtReceiveAvdtpSignaling(BtConnection *connection, BtChannel *channel,
				uint8_t *packet, int length)
{
	BtAvdtpState *avdtp = &connection->avdtp;
	BtAvdtpStream *receive = &avdtp->streams[0];
	BtAvdtpStream *transmit = &avdtp->streams[1];
	BtAvdtpStream *stream = receive;
	BtAvdtpStream *scratch = &avdtp->streams[2];
	BtAvdtpHeader *header = (BtAvdtpHeader *)packet;
	BtAvdtpSeidPacket *seidPacket = (BtAvdtpSeidPacket *)packet;
	uintptr_t result = (uintptr_t)connection;
	unsigned int label, signal, messageType, nextLabel;
	int error, extra = 0;

	if (length <= 0) {
			BtSendL2capPacket(connection, channel->remoteCid, "11", 3, 0);
			return;
		}
	label = header->header & 0xf0;
	if (length == 1) {
			BtSendL2capPacket(connection, channel->remoteCid,
					"11", label | 3, 0);
			return;
		}
	if (header->header & 0x0c)
		return;
	messageType = header->header & 3;
	if (messageType != 2) {
		if (messageType == 3) {
			signal = header->signalId & 0x3f;
			if (signal != 7 && signal != 9)
				return;
			stream = transmit;
			if (receive->transactionLabel == label)
				stream = receive;
			else if (transmit->transactionLabel != label &&
				 (connection->device->profiles & 4))
				return;
			if (signal == 7) {
				if (stream->state == 9) {
					stream->startDeferred = 1;
					stream->state = 8;
				}
			} else if (stream->state == 11) {
				stream->state = 10;
			}
			return;
		}
		if (messageType != 0)
			return;
		signal = header->signalId;
		result = signal;
		switch (signal & 0x3f) {
		case 1:
			if (channel)
				result = BtSendL2capPacket(connection, channel->remoteCid,
					"11 #", label | 2, 1, gBtAvdtpLocalEndpoints, 4);
			return;
		case 2:
			if (length == 2)
				goto badLength;
			if (receive->localSeid == seidPacket->seid)
				stream = receive;
			else if (transmit->localSeid == seidPacket->seid)
				stream = transmit;
			else
				goto badSeid;
			if (!channel)
				return;
			{
				BtSendL2capPacket(connection, channel->remoteCid,
				"11 #", label | 2, 2, seidPacket->seid == 8 ?
				&gBtAvdtpReceiveCapabilities : &gBtAvdtpTransmitCapabilities, 14);
				return;
			}
		case 3: {
			BtAvdtpSetConfiguration *configuration =
				(BtAvdtpSetConfiguration *)packet;
			uint8_t *capabilities;

			if ((unsigned int)(length - 4) > 16)
				goto badLength;
			if (receive->localSeid == configuration->acceptorSeid)
				stream = receive;
			else if (transmit->localSeid == configuration->acceptorSeid)
				stream = transmit;
			else
				goto badSeid;
			if (stream->state != 1)
				goto busy;
			stream->capabilityLength = length - 4;
			stream->remoteSeid = configuration->initiatorSeid;
			capabilities = memcpy(stream->capabilities,
				configuration->capabilities, length - 4);
			result = BtSelectAudioCapabilities(connection,
				configuration->acceptorSeid == 8 ? 4 : 8,
				capabilities, stream->capabilityLength);
			if (result) {
				extra = result;
				error = 35;
				signal = header->signalId;
				goto reject;
			}
			stream->state = 5;
			goto acknowledge;
		}
		case 4:
		case 5:
		case 11:
			error = 25;
			goto reject;
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
			if (length == 2)
				goto badLength;
			if (receive->localSeid == seidPacket->seid)
				stream = receive;
			else if (transmit->localSeid == seidPacket->seid)
				stream = transmit;
			else
				goto badSeid;
			switch (signal & 0x3f) {
			case 6:
				if (stream->state != 5)
					goto badState;
				avdtp->pendingLocalEndpoint = seidPacket->seid;
				stream->state = 6;
				break;
			case 7:
				if ((unsigned int)(stream->state - 8) > 1)
					goto badState;
				if ((connection->startingProfiles | connection->pendingStartProfiles |
				     connection->activeProfiles) & 0x20000000)
					goto busy;
				if (stream->startDeferred || stream->state == 9)
					result = BtNotifyAudioStarted(connection, 0,
						seidPacket->seid == 8 ? 4 : 8);
				stream->state = 10;
				break;
			case 8:
				if ((stream->state & ~2) != 8)
					goto badState;
				result = BtQueueTimer(&connection->disconnectTimer,
					BtDisconnectTimedOut, connection, 1000000);
				stream->state = 1;
				break;
			case 9:
				if (stream->state != 10)
					goto badState;
				stream->state = 8;
				break;
			case 10:
				stream->state = 1;
				break;
			}
			goto acknowledge;
		default:
			{
				BtSendL2capPacket(connection, channel->remoteCid,
						"11", label | 3, signal);
				return;
			}
		}
	}

	signal = header->signalId;
	result = signal;
	switch (signal & 0x3f) {
	case 1: {
		BtAvdtpEndpoint *endpoint = (BtAvdtpEndpoint *)
			((BtAvdtpSignalPacket *)packet)->payload;
		int count, sources = 0, sinks = 0;

		if (length <= 3)
			goto badLength;
		count = length - 2;
		if (count >= 16)
			count = 16;
		avdtp->remainingEndpointBytes = count;
		memcpy(avdtp->remoteEndpoints, endpoint, count);
		do {
			if (endpoint->mediaFlags & 8)
				++sinks;
			else
				++sources;
			count -= 2;
			++endpoint;
		} while (count > 1);
		if (!sources && (connection->device->profiles & 4)) {
			connection->excludedProfiles |= 4;
			if (avdtp->requestedProfile == 4 && sinks &&
			    (connection->device->profiles & 8))
				avdtp->requestedProfile = 8;
		}
		if (!sinks && (connection->device->profiles & 8))
			connection->excludedProfiles |= 8;
		if ((connection->excludedProfiles & 0x0c) == 0x0c) {
				BtSetConnectedProfiles(connection, NULL, 0);
				return;
			}
		result = avdtp->remoteEndpoints[0].seidFlags;
		scratch->remoteSeid = result;
		nextLabel = (avdtp->transactionLabel + 16) & 0xf0;
		while (nextLabel == receive->transactionLabel ||
		       nextLabel == transmit->transactionLabel)
			nextLabel = (nextLabel + 16) & 0xf0;
		avdtp->transactionLabel = nextLabel;
		scratch->transactionLabel = nextLabel;
		avdtp->endpointPosition = 0;
		if (channel)
			result = BtSendL2capPacket(connection, channel->remoteCid,
					"11 1", nextLabel, 2, result & 0xfc);
		return;
	}
	case 2: {
		BtAvdtpEndpoint *endpoint = (BtAvdtpEndpoint *)
			((uint8_t *)avdtp->remoteEndpoints + avdtp->endpointPosition);
		BtAvdtpServiceCapability *capability = (BtAvdtpServiceCapability *)
			((BtAvdtpSignalPacket *)packet)->payload;
		int remaining = length - 2, capabilityLength, protectionType;
		int accept;

		scratch->capabilityLength = 0;
		scratch->remoteSeid = endpoint->seidFlags;
		result = remaining > 16 ? (uintptr_t)scratch : (uintptr_t)endpoint;
		if (remaining <= 16 && remaining > 1) {
			result = capability->header.length;
			if (remaining < (int)result)
				goto badCapability;
			for (;;) {
				capabilityLength = capability->header.length;
				accept = 0;
				switch (capability->header.category) {
				case 4:
					if (capabilityLength == 2) {
						BtAvdtpContentProtection *protection =
							(BtAvdtpContentProtection *)capability;
						result = BtUnpack(protection->typeLe, 2, "2", &protectionType);
						capabilityLength = capability->header.length;
						accept = protectionType == 2;
					}
					break;
				case 7: {
					BtAvdtpSbcCapability *sbc = (BtAvdtpSbcCapability *)capability;
					unsigned int mediaFlags;

					if (capabilityLength != 6 || (sbc->mediaType & 0xf0) ||
					    sbc->codecType || (endpoint->seidFlags & 2))
						break;
					mediaFlags = endpoint->mediaFlags & 0xf8;
					if (mediaFlags == 0)
						stream = receive;
					else if (mediaFlags == 8)
						stream = transmit;
					else
						break;
					if (stream->state == 1) {
						stream->state = 2;
						capabilityLength = capability->header.length;
					} else {
						capabilityLength = 6;
					}
					accept = 1;
					break;
				}
				case 1:
					accept = capabilityLength == 0;
					break;
				}
				if (accept) {
					result = (uintptr_t)memcpy(scratch->capabilities +
						scratch->capabilityLength, capability, capabilityLength + 2);
					scratch->capabilityLength += capability->header.length + 2;
					capabilityLength = capability->header.length;
				}
				capability = (BtAvdtpServiceCapability *)
					((uint8_t *)capability + capabilityLength + 2);
				remaining -= capabilityLength + 2;
				if (remaining <= 1)
					break;
				result = capability->header.length;
				if ((int)result > remaining) {
badCapability:
					packet = (uint8_t *)capability;
					signal = result;
					error = 17;
					goto reject;
				}
			}
		}
		stream = receive;
		if (receive->state != 2)
			stream = transmit;
		if (stream->state == 2) {
			stream->capabilityLength = scratch->capabilityLength;
			result = (uintptr_t)memcpy(stream->capabilities,
				scratch->capabilities, scratch->capabilityLength);
			stream->state = 3;
			stream->remoteSeid = scratch->remoteSeid;
		}
		avdtp->remainingEndpointBytes -= 2;
		avdtp->endpointPosition += 2;
		if (avdtp->remainingEndpointBytes <= 1) {
			if (avdtp->requestedProfile == 4) {
				if (receive->state != 3)
					return;
				stream = receive;
			} else {
				if (avdtp->requestedProfile != 8 || transmit->state != 3)
					return;
				stream = transmit;
			}
			stream->state = 4;
			nextLabel = (avdtp->transactionLabel + 16) & 0xf0;
			while (nextLabel == receive->transactionLabel ||
			       nextLabel == transmit->transactionLabel)
				nextLabel = (nextLabel + 16) & 0xf0;
			avdtp->transactionLabel = nextLabel;
			stream->transactionLabel = nextLabel;
			BtSelectAudioCapabilities(connection, avdtp->requestedProfile,
						  stream->capabilities, stream->capabilityLength);
			result = stream->localSeid;
			if (channel)
				result = BtSendL2capPacket(connection, channel->remoteCid,
					"11 11#", stream->transactionLabel, 3, stream->remoteSeid,
					stream->localSeid, stream->capabilities, stream->capabilityLength);
		} else {
			nextLabel = (avdtp->transactionLabel + 16) & 0xf0;
			while (nextLabel == receive->transactionLabel ||
			       nextLabel == transmit->transactionLabel)
				nextLabel = (nextLabel + 16) & 0xf0;
			avdtp->transactionLabel = nextLabel;
			stream->transactionLabel = nextLabel;
			endpoint = (BtAvdtpEndpoint *)
				((uint8_t *)avdtp->remoteEndpoints + avdtp->endpointPosition);
			result = endpoint->seidFlags;
			if (channel)
				result = BtSendL2capPacket(connection, channel->remoteCid,
						"11 1", nextLabel, 2, result & 0xfc);
		}
		return;
	}
	case 3:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
		stream = transmit;
		if (receive->transactionLabel == label)
			stream = receive;
		else if (transmit->transactionLabel != label &&
			 (connection->device->profiles & 4))
			return;
		switch (signal & 0x3f) {
		case 3:
			if (stream->state != 4)
				break;
			stream->state = 6;
			nextLabel = (avdtp->transactionLabel + 16) & 0xf0;
			while (nextLabel == receive->transactionLabel ||
			       nextLabel == transmit->transactionLabel)
				nextLabel = (nextLabel + 16) & 0xf0;
			avdtp->transactionLabel = nextLabel;
			stream->transactionLabel = nextLabel;
			if (channel)
				result = BtSendL2capPacket(connection, channel->remoteCid,
					"11 1", nextLabel, 6, stream->remoteSeid & 0xfc);
			break;
		case 6:
			if (stream->state == 6) {
				BtChannel *transport = BtFindChannel(connection, 20, 25);

				stream->channel = transport;
				result = (uintptr_t)transport;
				if (transport) {
					transport->ops = stream->localSeid == 8 ?
						&gBtAvdtpReceiveOps : &gBtAvdtpTransmitOps;
					stream->state = 7;
					result = BtSendL2capConnectionRequest(connection, transport);
				}
			}
			break;
		case 7:
			if (stream->state == 9) {
				result = BtNotifyAudioStarted(connection, 0,
					stream->localSeid == 8 ? 4 : 8);
				stream->state = 10;
			}
			break;
		case 8:
			if (stream->state == 12) {
				stream->state = 13;
				result = BtSendL2capDisconnectRequest(connection, stream->channel);
			}
			break;
		case 9:
			if (stream->state == 11) {
				result = BtNotifyAudioStopped(connection, 0,
					stream->localSeid == 8 ? 4 : 8);
				stream->state = 8;
			}
			break;
		case 10:
			if (stream->state == 10)
				result = BtNotifyAudioStopped(connection, 0,
					stream->localSeid == 8 ? 4 : 8);
			if (stream->channel) {
				stream->state = 15;
				result = BtSendL2capDisconnectRequest(connection, stream->channel);
			} else {
				stream->state = 1;
			}
			break;
		}
		break;
	}
	return;

badLength:
	error = 17;
	goto reject;
badSeid:
	error = 18;
	goto reject;
badState:
	error = 49;
	goto reject;
busy:
	error = 19;
reject:
	if (signal == 3 || signal == 5) {
			BtSendL2capPacket(connection, channel->remoteCid,
			"1111", label | 3, signal, extra, error);
			return;
		}
	if (signal == 7 || signal == 9) {
			BtSendL2capPacket(connection, channel->remoteCid,
			"1111", label | 3, signal, ((BtAvdtpSeidPacket *)packet)->seid, error);
			return;
		}
	{
		BtSendL2capPacket(connection, channel->remoteCid,
				"111", label | 3, signal, error);
		return;
	}
acknowledge:
	if (channel)
		result = BtSendL2capPacket(connection, channel->remoteCid,
				"11 #", (header->header & 0xf0) | 2, header->signalId & 0x3f, NULL, 0);
	return;
}
