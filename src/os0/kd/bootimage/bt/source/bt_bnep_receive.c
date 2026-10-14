/* BNEP packet reception; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>

int BtPack(uint8_t *destination, int capacity, const char *format, ...);
int BtUnpack(const uint8_t *source, int remaining, const char *format, ...);
int BtSendL2capPacket(BtConnection *connection, int channelId,
		     const char *format, ...);
unsigned int BtCancelTimer(unsigned int timer);
uintptr_t BtSetConnectedProfiles(BtConnection *connection, BtChannel *channel,
				uint32_t profiles);
void BtDeliverConnectionData(BtConnection *connection, uint32_t profile,
				 uint8_t *packet, unsigned int length);
int BtProcessNetworkPacket(BtConnection *connection, const void *packet,
			   int length);
extern const uint8_t gBtSdpBaseUuidSuffix[12];
extern int gBtPanUserEnabled;
extern int gBtNetworkAccessPointEnabled;

static inline int BtProcessBnepControl(BtConnection *connection,
				     BtChannel *channel,
				     const BtBnepControlPacket *control,
				     int length)
{
	BtBnepChannelState *state = &channel->protocol.bnep;
	const BtBnepSetupRequest *setup = (const BtBnepSetupRequest *)control;
	const BtBnepSetupResponse *response = (const BtBnepSetupResponse *)control;
	const BtBnepFilterRequest *filter = (const BtBnepFilterRequest *)control;
	const BtBnepUuid *destinationUuid, *sourceUuid;
	unsigned int destinationService, sourceService;
	unsigned int uuidLength, responseCode, filterLength, profiles;
	int consumed;

	switch (control->command & 0x7f) {
	case 0:
		return 1;
	case 1:
		if (length == 1)
			return length;
		uuidLength = setup->uuidLength;
		if (length <= (int)(2 * uuidLength + 1))
			return length;
		consumed = sizeof(*setup);
		responseCode = 3;
		switch (uuidLength) {
		case 2:
			consumed += 2 * uuidLength;
			BtUnpack(setup->uuids, 2, "H", &destinationService);
			BtUnpack(setup->uuids + uuidLength, uuidLength, "H",
				 &sourceService);
			break;
		case 4:
			consumed += 2 * uuidLength;
			BtUnpack(setup->uuids, 4, "W", &destinationService);
			BtUnpack(setup->uuids + uuidLength, uuidLength, "W",
				 &sourceService);
			if (destinationService >> 16) {
				responseCode = 1;
				goto setupResponse;
			}
			if (sourceService >> 16) {
				responseCode = 2;
				goto setupResponse;
			}
			break;
		case 16:
			consumed += 2 * uuidLength;
			destinationUuid = (const BtBnepUuid *)setup->uuids;
			sourceUuid = destinationUuid + 1;
			BtUnpack(destinationUuid->serviceBe, 4, "W", &destinationService);
			BtUnpack(sourceUuid->serviceBe, 4, "W", &sourceService);
			if ((destinationService >> 16) ||
			    memcmp(gBtSdpBaseUuidSuffix, destinationUuid->suffix, 12)) {
				responseCode = 1;
				goto setupResponse;
			}
			if ((sourceService >> 16) ||
			    memcmp(gBtSdpBaseUuidSuffix, sourceUuid->suffix, 12)) {
				responseCode = 2;
				goto setupResponse;
			}
			break;
		default:
			goto setupResponse;
		}
		responseCode = 4;
		if (state->localService == 0x1116 &&
		    (!gBtNetworkAccessPointEnabled || !gBtPanUserEnabled))
			goto setupResponse;
		if (state->connected)
			goto setupResponse;
		state->remoteService = sourceService;
		state->localService = destinationService;
		switch (destinationService) {
		case 0x1116:
			if (sourceService == 0x1116)
				goto setupResponse;
			responseCode = 2;
			if (sourceService != 0x1115)
				goto setupResponse;
			profiles = 0x08000100;
			break;
		case 0x1117:
			if (sourceService == 0x1116)
				goto setupResponse;
			responseCode = 2;
			if (sourceService != 0x1115)
				goto setupResponse;
			profiles = 0x04000100;
			break;
		case 0x1115:
			responseCode = 2;
			if (sourceService == 0x1115)
				profiles = 0x02000100;
			else if (sourceService == 0x1116)
				profiles = 0x02000200;
			else
				goto setupResponse;
			break;
		default:
			responseCode = 1;
			goto setupResponse;
		}
		BtSetConnectedProfiles(connection, NULL, profiles);
		state->connected = 1;
		responseCode = 0;
setupResponse:
		BtSendL2capPacket(connection, channel->remoteCid, "11H",
				  1, 2, responseCode);
		return consumed;
	case 2:
		if (length <= 2)
			return length;
		state->retryTimer = BtCancelTimer(state->retryTimer);
		if (!(response->responseCodeBe[0] | response->responseCodeBe[1])) {
			state->connected = 1;
			if (state->profile == 0x100)
				profiles = 0x02000100;
			else if (state->profile == 0x400)
				profiles = 0x02000400;
			else
				profiles = 0x02000200;
			BtSetConnectedProfiles(connection, NULL, profiles);
		}
		return sizeof(*response);
	case 3:
	case 5:
		BtUnpack(filter->listLengthBe, 2, "H", &filterLength);
		BtSendL2capPacket(connection, channel->remoteCid, "11H", 1,
				  (control->command & 0x7f) == 3 ? 4 : 6, 1);
		return sizeof(*filter) + filterLength;
	case 4:
	case 6:
		return 2;
	default:
		BtSendL2capPacket(connection, channel->remoteCid, "111", 1,
				  0, control->command);
		return length;
	}
}

void BtReceiveBnep(BtConnection *connection, BtChannel *channel,
		   uint8_t *packet, int length)
{
	BtStack *stack = connection->stack;
	BtEthernetFrame *frame = (BtEthernetFrame *)stack->scratch;
	const uint8_t *payload;
	unsigned int packetType;
	int remaining = (int32_t)((uint32_t)length - 1);
	int consumed;

	if (remaining < 0)
		return;
	memset(&frame->ethernet, 0, sizeof(frame->ethernet));
	packetType = packet[0];
	switch (packetType & 0x7f) {
	case 0: {
		const BtBnepGeneralPacket *general = (const BtBnepGeneralPacket *)packet;

		remaining = (int32_t)((uint32_t)length - sizeof(*general));
		if (remaining < 0)
			return;
		memcpy(&frame->ethernet, &general->ethernet, sizeof(frame->ethernet));
		payload = general->payload;
		break;
	}
	case 1:
		payload = packet + 1;
		consumed = remaining;
		if (remaining)
			consumed = (uint8_t)BtProcessBnepControl(connection, channel,
					(const BtBnepControlPacket *)payload, remaining);
		payload += consumed;
		remaining -= consumed;
		break;
	case 2: {
		const BtBnepCompressedPacket *compressed =
			(const BtBnepCompressedPacket *)packet;

		remaining = (int32_t)((uint32_t)length - sizeof(*compressed));
		if (remaining < 0)
			return;
		BtPack(stack->scratch, 12, "EE", stack->deviceAddress,
		       connection->deviceAddress);
		memcpy(&frame->ethernet.ether_type, compressed->protocolBe, 2);
		payload = compressed->payload;
		break;
	}
	case 3: {
		const BtBnepSourcePacket *source = (const BtBnepSourcePacket *)packet;

		remaining = (int32_t)((uint32_t)length - sizeof(*source));
		if (remaining < 0)
			return;
		BtPack(frame->ethernet.ether_dhost, 6, "E", stack->deviceAddress);
		memcpy(frame->ethernet.ether_shost, source->source, 6);
		memcpy(&frame->ethernet.ether_type, source->protocolBe, 2);
		payload = source->payload;
		break;
	}
	case 4: {
		const BtBnepDestinationPacket *destination =
			(const BtBnepDestinationPacket *)packet;

		remaining = (int32_t)((uint32_t)length - sizeof(*destination));
		if (remaining < 0)
			return;
		memcpy(frame->ethernet.ether_dhost, destination->destination, 6);
		BtPack(frame->ethernet.ether_shost, 6, "E", connection->deviceAddress);
		memcpy(&frame->ethernet.ether_type, destination->protocolBe, 2);
		payload = destination->payload;
		break;
	}
	default:
		return;
	}
	if (packetType & 0x80) {
		for (;;) {
			const BtBnepExtension *extension = (const BtBnepExtension *)payload;
			unsigned int type, extensionLength;

			if (remaining <= 0)
				return;
			type = extension->type;
			extensionLength = extension->length;
			if (!(type & 0x7f) && extensionLength)
				BtProcessBnepControl(connection, channel,
					(const BtBnepControlPacket *)extension->data,
					extensionLength);
			payload = extension->data + extensionLength;
			remaining -= sizeof(*extension) + extensionLength;
			if (!(type & 0x80))
				break;
		}
	}
	if (remaining <= 0)
		return;
	memcpy(frame->payload, payload, remaining);
	if (channel->protocol.bnep.localService == 0x1115) {
		if (gBtPanUserEnabled)
			BtDeliverConnectionData(connection, 0x200, stack->scratch,
					remaining + 14);
	} else if (channel->protocol.bnep.localService == 0x1116 &&
		   gBtNetworkAccessPointEnabled) {
		BtProcessNetworkPacket(connection, frame, remaining + 14);
	}
}
