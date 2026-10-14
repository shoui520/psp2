/* DHCP reply construction; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>

unsigned int BtInternetChecksum(unsigned int initialSum,
				const uint16_t *words, int length);
unsigned int BtTransportChecksum(unsigned int protocol,
				 unsigned int sourceAddress,
				 unsigned int destinationAddress,
				 const void *packet, unsigned int length);
int BtSendBnepFrame(BtConnection *connection, const void *frame, int length,
		    int userMode);
#if FW_VERSION >= 0x04000000
void *__memset_chk(void *destination, int value, size_t length, size_t capacity);
#endif

static inline uint8_t *BtWriteDhcpAddressOption(uint8_t *cursor, uint8_t code,
					      const uint32_t *address)
{
	BtDhcpOption *option = (BtDhcpOption *)cursor;

	option->code = code;
	option->length = 4;
	BtWriteNetwork32(option->data, *address);
	return option->data + 4;
}

int BtSendDhcpReply(BtNetworkPacketContext *context,
		    unsigned int transactionId, const BtDhcpOptions *options,
		    uint8_t messageType)
{
	struct ether_header *ethernet = (void *)gBtNetworkPacketBuffer.bytes;
	SceNetIpHeader *ip = (void *)(ethernet + 1);
	NetUdpHeader *udp = (void *)(ip + 1);
	BtDhcpPacket *reply = (void *)(udp + 1);
	const uint32_t leaseTime = 3600;
	uint8_t *cursor;
	unsigned int vendorLength, udpLength, ipLength;
	uint16_t checksum;

	if (context->destinationAddress == UINT32_MAX) {
		context->sourceAddress = UINT32_MAX;
		context->destinationAddress = 0;
	}
	if (options->flags & 0x10)
		context->sourceAddress = UINT32_MAX;
	memset(&gBtNetworkPacketBuffer, 0, sizeof(gBtNetworkPacketBuffer));
	__builtin_memcpy(reply->cookie, gBtDhcpMagicCookie, sizeof(reply->cookie));
	cursor = reply->options;
	((BtDhcpOption *)cursor)->code = 53;
	((BtDhcpOption *)cursor)->length = 1;
	((BtDhcpOption *)cursor)->data[0] = messageType;
	cursor += sizeof(BtDhcpOption) + 1;
	cursor = BtWriteDhcpAddressOption(cursor, 54, &options->serverAddress);
	cursor = BtWriteDhcpAddressOption(cursor, 51, &leaseTime);
	if (options->flags & 1)
		cursor = BtWriteDhcpAddressOption(cursor, 1, &options->subnetMask);
	if (options->flags & 2)
		cursor = BtWriteDhcpAddressOption(cursor, 3, &options->serverAddress);
	if (options->flags & 4)
		cursor = BtWriteDhcpAddressOption(cursor, 6, &options->serverAddress);
	*cursor++ = 255;
	vendorLength = sizeof(reply->cookie) + (cursor - reply->options);
	if (vendorLength < 64) {
#if FW_VERSION >= 0x04000000
		__memset_chk(cursor, 0, 64 - vendorLength, 1750);
#else
		memset(cursor, 0, 64 - vendorLength);
#endif
		vendorLength = 64;
	}
	BtWriteNetwork32(reply->xidBe, transactionId);
	reply->op = 2;
	reply->htype = 1;
	reply->hlen = 6;
	BtWriteNetwork32(reply->yiaddrBe, options->assignedAddress);
	BtWriteNetwork32(reply->siaddrBe, options->serverAddress);
	__builtin_memcpy(reply->chaddr, options->hardwareAddress, 6);
	udp->sourcePortBe[0] = context->destinationPort >> 8;
	udp->sourcePortBe[1] = context->destinationPort;
	udp->destinationPortBe[0] = context->sourcePort >> 8;
	udp->destinationPortBe[1] = context->sourcePort;
	udpLength = vendorLength + sizeof(NetUdpHeader) +
		    offsetof(BtDhcpPacket, cookie);
	udp->lengthBe[0] = udpLength >> 8;
	udp->lengthBe[1] = udpLength;
	checksum = ~BtTransportChecksum(17, context->destinationAddress,
					context->sourceAddress, udp, udpLength);
	__builtin_memcpy(udp->checksumBe, &checksum, sizeof(checksum));
	ipLength = udpLength + sizeof(*ip);
	ip->un.ver_hl = 0x45;
	ip->ip_len = __builtin_bswap16((uint16_t)ipLength);
	ip->ip_ttl = 255;
	ip->ip_p = 17;
	ip->ip_src.s_addr = __builtin_bswap32(context->destinationAddress);
	ip->ip_dst.s_addr = __builtin_bswap32(context->sourceAddress);
	ip->ip_sum = ~BtInternetChecksum(0, (const uint16_t *)ip, sizeof(*ip));
	BtWriteNetworkMac(ethernet->ether_dhost, context->connection->deviceAddress);
	ethernet->ether_type = __builtin_bswap16(0x800);
	BtWriteNetworkMac(ethernet->ether_shost,
			  context->connection->stack->deviceAddress);
	BtSendBnepFrame(context->connection, ethernet,
		       ipLength + sizeof(*ethernet), 0);
	return 1;
}
