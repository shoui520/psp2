/* WLAN packet methods; the filename is inferred, not a surviving source name. */
#include "robin/wlan.h"
#include "robin/rom.h"

/* Payload C-library implementation remains unconverted. */
int memcmp(const void *left, const void *right, size_t size);

static const uint8_t gWlanAarpProtocol[2] = { 0x80, 0xf3 };
static const uint8_t gWlanIpxProtocol[2] = { 0x81, 0x37 };
static const WlanLlcSnapPrefix gWlanRfc1042Prefix = {
	.destinationSap = 0xaa,
	.sourceSap = 0xaa,
	.control = 3,
	.organization = { 0, 0, 0 },
};
static const WlanLlcSnapPrefix gWlanBridgeTunnelPrefix = {
	.destinationSap = 0xaa,
	.sourceSap = 0xaa,
	.control = 3,
	.organization = { 0, 0, 0xf8 },
};

int wlanReleasePacket(OsaBuffer *buffer)
{
	if (buffer == NULL)
		return 0;
	if (buffer->pool != NULL)
		return osa_BufferFree(buffer);
	return wlanReleaseDmaSpan(buffer->ringStart, buffer->ringEnd, 3);
}

int wlanIsSecurityProtocol(uint32_t protocol)
{
	/* The original callers supply the two wire bytes in little-endian order. */
	return protocol == 0x8e88 || protocol == 0x0300;
}

uint32_t wlanPacketHeadroom(void)
{
	return 176;
}

int wlanConvertWirelessToEthernet(OsaBuffer *buffer, struct ether_header **output,
				 const WlanWirelessFrame *wirelessHeader,
				 uint8_t *payload, int stripSnap)
{
	WlanSnapHeader snap;
	WlanWirelessFrame header;
	struct ether_header *ethernet;
	const uint8_t *address;
	uint8_t *protocol;
	int packetType = 3;

	if (stripSnap) {
		__aeabi_memcpy(&snap, payload, sizeof(snap));
		if (!memcmp(&snap.prefix, &gWlanBridgeTunnelPrefix, sizeof(snap.prefix)) ||
		    (!memcmp(&snap.prefix, &gWlanRfc1042Prefix, sizeof(snap.prefix)) &&
		     memcmp(snap.protocol.wire, gWlanAarpProtocol, sizeof(gWlanAarpProtocol)) &&
		     memcmp(snap.protocol.wire, gWlanIpxProtocol, sizeof(gWlanIpxProtocol)))) {
			payload += sizeof(snap);
			packetType = 2;
		}
	}
	__aeabi_memcpy(&header, wirelessHeader, sizeof(header));
	ethernet = (struct ether_header *)(payload - sizeof(*ethernet));
	if (packetType == 2) {
		buffer->length -= sizeof(snap);
	} else {
		protocol = (uint8_t *)&ethernet->ether_type;
		protocol[0] = buffer->length >> 8;
		protocol[1] = buffer->length;
	}
	address = (header.frameControl[1] & 1) ? header.address3 : header.address1;
	__aeabi_memcpy(ethernet->ether_dhost, address, sizeof(ethernet->ether_dhost));
	if (header.frameControl[1] & 2)
		address = (header.frameControl[1] & 1) ? header.address4 : header.address3;
	else
		address = header.address2;
	__aeabi_memcpy(ethernet->ether_shost, address, sizeof(ethernet->ether_shost));
	buffer->length += sizeof(*ethernet);
	*output = ethernet;
	return packetType;
}
