/* Bluetooth network packet processing; original filename unavailable. */
#include "bt_internal.h"
#include <net_ps/netps_gateway.h>
#include <string.h>
#include "bt_transport.h"

#if FW_VERSION < 0x04000000
struct SceNetKernelGateway gBtNetworkGateway;
#endif
unsigned int gBtNetworkPortBase;
BtNetworkPacketBuffer gBtNetworkPacketBuffer;
BtNatMapping *gBtNetworkMappingEnd;
unsigned int gBtNetworkDnsServers[2];
BtNetworkState gBtNetworkState;
unsigned int gBtNetworkPortCount;
struct SceNetKernelGateway *gBtNetworkGatewayPointer;
#if FW_VERSION >= 0x04000000
struct SceNetKernelGateway gBtNetworkGateway;
#endif

extern int gBtNetworkAccessPointEnabled;
int BtProcessIcmpPacket(BtNetworkPacketContext *context, const void *packet,
			int length);
int BtForwardNetworkPacket(BtNetworkPacketContext *context, int protocol,
			   const void *packet, int length,
			   const void *header, unsigned int headerLength);
int BtReceiveGatewayPacket(struct SceNetKernelGateway *interface,
			   struct NetDriver *driver,
			   struct NetDriverPacket *packet);
int BtMatchNetworkMac(const unsigned char *bytes, uint64_t address)
{
	return bytes[0] == ((address >> 40) & 0xff) &&
	       bytes[1] == ((address >> 32) & 0xff) &&
	       bytes[2] == ((address >> 24) & 0xff) &&
	       bytes[3] == ((address >> 16) & 0xff) &&
	       bytes[4] == ((address >> 8) & 0xff) &&
	       bytes[5] == (address & 0xff);
}

int BtNotifyGateway(struct SceNetKernelGateway *interface, int event,
		    const SceNetPsDnsConfig *config, int length);

static inline unsigned int BtReadNetwork16(const void *field)
{
	const uint8_t *bytes = field;

	return bytes[1] | (bytes[0] << 8);
}

static inline unsigned int BtReadNetwork32(const void *field)
{
	const uint8_t *bytes = field;

	return ((unsigned int)bytes[0] << 24) |
	       ((unsigned int)bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

static inline void BtWriteNetwork16(uint8_t *bytes, unsigned int value)
{
	bytes[0] = value >> 8;
	bytes[1] = value;
}

static inline int BtProcessIpPacket(BtNetworkPacketContext *context,
				    const SceNetIpHeader *ip, int remaining)
{
	const uint16_t *checksumWords;
	const uint8_t *payload;
	const NetUdpHeader *udp;
	const NetTcpHeader *tcp;
	unsigned int checksum, headerLength, totalLength, tcpHeaderLength;
	int payloadLength;

	if (ip->un.ip_ver_hl.ver != 4)
		return 0;
	if ((uintptr_t)ip & 1)
		return 0;
	headerLength = ip->un.ip_ver_hl.hl * 4;
	if (headerLength <= 1)
		return 0;
	checksum = 0;
	checksumWords = (const uint16_t *)ip;
	for (unsigned int i = 0; i < headerLength / 2; i++)
		checksum += checksumWords[i];
	while (checksum >> 16)
		checksum = (checksum & 0xffff) + (checksum >> 16);
	if ((uint16_t)checksum != 0xffff)
		return 0;
	totalLength = BtReadNetwork16(&ip->ip_len);
	if (headerLength < 20 || headerLength > totalLength ||
	    remaining < (int)totalLength)
		return 0;
	payloadLength = totalLength - headerLength;
	payload = (const uint8_t *)ip + headerLength;
	context->sourceAddress = BtReadNetwork32(&ip->ip_src.s_addr);
	context->destinationAddress = BtReadNetwork32(&ip->ip_dst.s_addr);
	switch (ip->ip_p) {
	case 1:
		return BtProcessIcmpPacket(context, payload, payloadLength);
	case 17:
		udp = (const NetUdpHeader *)payload;
		if (payloadLength <= 7)
			break;
		if (BtReadNetwork16(udp->checksumBe) &&
		    BtTransportChecksum(17, context->sourceAddress,
					context->destinationAddress,
					udp, payloadLength) != 0xffff)
			break;
		context->destinationPort = BtReadNetwork16(udp->destinationPortBe);
		context->sourcePort = BtReadNetwork16(udp->sourcePortBe);
		if (context->destinationPort == 67)
			return BtProcessDhcpPacket(context, udp + 1, payloadLength - 8);
		if (context->destinationPort == 53 || context->sourcePort == 53)
			return BtForwardNetworkPacket(context, 17, udp + 1,
					      payloadLength - 8, NULL, 0);
		break;
	case 6:
		tcp = (const NetTcpHeader *)payload;
		tcpHeaderLength = (tcp->dataOffsetAndReserved >> 4) * 4;
		if (tcpHeaderLength <= 19 || payloadLength <= 19 ||
		    payloadLength < (int)tcpHeaderLength)
			break;
		if (BtTransportChecksum(6, context->sourceAddress,
					context->destinationAddress,
					tcp, payloadLength) != 0xffff)
			break;
		context->destinationPort = BtReadNetwork16(tcp->destinationPortBe);
		context->sourcePort = BtReadNetwork16(tcp->sourcePortBe);
		if (context->destinationPort == 80 || context->sourcePort == 80)
			return BtForwardNetworkPacket(context, 6,
					      payload + tcpHeaderLength,
					      payloadLength - tcpHeaderLength,
					      tcp, tcpHeaderLength);
		break;
	}
	return 0;
}

int BtNotifyGateway(struct SceNetKernelGateway *interface, int event,
		    const SceNetPsDnsConfig *config, int length)
{
	unsigned int primaryAddress, secondaryAddress;

	if (event != 1 || length != sizeof(*config))
		return 0;
	primaryAddress = BtReadNetwork32(&config->dns.dns_addr[0].s_addr);
	secondaryAddress = BtReadNetwork32(&config->dns.dns_addr[1].s_addr);
	gBtNetworkDnsServers[0] = primaryAddress;
	gBtNetworkDnsServers[1] = secondaryAddress;
	return 0;
}

int BtForwardNetworkPacket(BtNetworkPacketContext *context, int protocol,
			   const void *packet, int length,
			   const void *header, unsigned int headerLength)
{
	unsigned int sourceAddress = context->sourceAddress;
	unsigned int destinationAddress = context->destinationAddress;
	unsigned int sourcePort, destinationPort, translatedAddress;
	unsigned int translatedPort, totalLength;
	BtConnection *connection;
	BtNatMapping *mapping, *available, *oldest;
	BtDhcpLease *lease;
	BtEthernetFrame *frame;
	SceNetIpHeader *ip;
	NetUdpHeader *udp;
	NetTcpHeader *tcp;
	uint8_t *transport;
	void *allocated;
	int transportLength;

	if (sourceAddress - 1 > 0xfffffffdU ||
	    destinationAddress - 1 > 0xfffffffdU)
		return 0;
	sourcePort = context->sourcePort;
	if (!sourcePort)
		return 0;
	destinationPort = context->destinationPort;
	if (!destinationPort)
		return 0;
	connection = context->connection;
	if (!connection) {
		mapping = gBtNetworkState.mappings +
			  (destinationPort - gBtNetworkPortBase);
		if (mapping < gBtNetworkState.mappings ||
		    mapping >= gBtNetworkMappingEnd ||
		    sourceAddress != mapping->translatedDestinationAddress ||
		    sourcePort != mapping->destinationPort)
			return 0;
		memset(&gBtNetworkPacketBuffer, 0, sizeof(gBtNetworkPacketBuffer));
		frame = (BtEthernetFrame *)gBtNetworkPacketBuffer.bytes;
		ip = (SceNetIpHeader *)frame->payload;
		transport = (uint8_t *)(ip + 1);
		if (protocol == 17) {
			udp = (NetUdpHeader *)transport;
			if (length > 0) {
#if FW_VERSION >= 0x04000000
				__builtin___memcpy_chk(udp + 1, packet, length,
					sizeof(gBtNetworkPacketBuffer.bytes) -
					sizeof(frame->ethernet) - sizeof(*ip) - sizeof(*udp));
#else
				memcpy(udp + 1, packet, length);
#endif
			}
			transportLength = (unsigned int)length + sizeof(*udp);
			BtWriteNetwork16(udp->sourcePortBe, sourcePort);
			BtWriteNetwork16(udp->destinationPortBe, mapping->sourcePort);
			BtWriteNetwork16(udp->lengthBe, transportLength);
			*(uint16_t *)udp->checksumBe = ~BtTransportChecksum(17,
				mapping->destinationAddress, mapping->sourceAddress,
				udp, transportLength);
		} else {
			tcp = (NetTcpHeader *)transport;
			if (length > 0)
				memcpy(transport + headerLength, packet, length);
			transportLength = (unsigned int)length + headerLength;
#if FW_VERSION >= 0x04000000
			__builtin___memcpy_chk(tcp, header, headerLength,
				sizeof(gBtNetworkPacketBuffer.bytes) -
				sizeof(frame->ethernet) - sizeof(*ip));
#else
			memcpy(tcp, header, headerLength);
#endif
			BtWriteNetwork16(tcp->destinationPortBe, mapping->sourcePort);
			BtWriteNetwork16(tcp->sourcePortBe, sourcePort);
			*(uint16_t *)tcp->checksumBe = 0;
			*(uint16_t *)tcp->checksumBe = ~BtTransportChecksum(6,
				mapping->destinationAddress, mapping->sourceAddress,
				tcp, transportLength);
		}
		BtWriteNetwork32((uint8_t *)&ip->ip_src.s_addr,
				 mapping->destinationAddress);
		sourceAddress = mapping->sourceAddress;
		BtWriteNetwork32((uint8_t *)&ip->ip_dst.s_addr, sourceAddress);
		ip->ip_p = protocol;
		ip->un.ver_hl = 0x45;
		BtWriteNetwork16((uint8_t *)&ip->ip_len, transportLength + sizeof(*ip));
		ip->ip_ttl = 64;
		ip->ip_sum = ~BtInternetChecksum(0, (const uint16_t *)ip, sizeof(*ip));
		connection = mapping->connection;
		lease = gBtNetworkState.leases +
			(sourceAddress - gBtNetworkState.networkAddress);
		memcpy(frame->ethernet.ether_dhost, lease->hardwareAddress,
		       sizeof(frame->ethernet.ether_dhost));
		if (!connection->stack)
			return 0;
		BtWriteNetworkMac(frame->ethernet.ether_shost,
				  connection->stack->deviceAddress);
		frame->ethernet.ether_type = 0x0008;
		BtSendBnepFrame(connection, frame,
				transportLength + sizeof(frame->ethernet) + sizeof(*ip), 0);
		return 1;
	}
	if (sourceAddress < gBtNetworkState.networkAddress + 2 ||
	    sourceAddress > gBtNetworkState.networkAddress + 254)
		return 0;
	lease = gBtNetworkState.leases +
		(sourceAddress - gBtNetworkState.networkAddress);
	if (lease->state != 2 ||
	    !BtMatchNetworkMac(lease->hardwareAddress, connection->deviceAddress))
		return 0;
	if (protocol == 17 && destinationPort == 53) {
		translatedAddress = gBtNetworkDnsServers[0];
		if (!translatedAddress)
			return 0;
	} else {
		translatedAddress = destinationAddress;
	}
	available = NULL;
	oldest = NULL;
	for (mapping = gBtNetworkState.mappings;
	     mapping < gBtNetworkMappingEnd; mapping++) {
		if (sourceAddress == mapping->sourceAddress) {
			if (destinationAddress == mapping->destinationAddress &&
			    translatedAddress == mapping->translatedDestinationAddress &&
			    sourcePort == mapping->sourcePort &&
			    destinationPort == mapping->destinationPort)
				goto mapped;
		} else if (!mapping->sourceAddress && !available) {
			available = mapping;
		}
		if (!oldest || mapping->lastUsed < oldest->lastUsed)
			oldest = mapping;
	}
	if (!available) {
		if (!oldest)
			return 0;
		available = oldest;
	}
	mapping = available;
	mapping->sourceAddress = sourceAddress;
	mapping->destinationAddress = destinationAddress;
	mapping->sourcePort = sourcePort;
	mapping->destinationPort = destinationPort;
mapped:
	translatedPort = gBtNetworkPortBase +
		(mapping - gBtNetworkState.mappings);
	mapping->connection = connection;
	mapping->translatedDestinationAddress = translatedAddress;
	mapping->lastUsed = BtGetSystemTimeWide();
	memset(&gBtNetworkPacketBuffer, 0, sizeof(gBtNetworkPacketBuffer));
	ip = (SceNetIpHeader *)gBtNetworkPacketBuffer.bytes;
	transport = (uint8_t *)(ip + 1);
	if (protocol == 17) {
		udp = (NetUdpHeader *)transport;
#if FW_VERSION >= 0x04000000
		__builtin___memcpy_chk(udp + 1, packet, length,
			sizeof(gBtNetworkPacketBuffer.bytes) - sizeof(*ip) - sizeof(*udp));
#else
		memcpy(udp + 1, packet, length);
#endif
		transportLength = (unsigned int)length + sizeof(*udp);
		BtWriteNetwork16(udp->sourcePortBe, translatedPort);
		BtWriteNetwork16(udp->destinationPortBe, mapping->destinationPort);
		BtWriteNetwork16(udp->lengthBe, transportLength);
	} else {
		tcp = (NetTcpHeader *)transport;
		if (length > 0)
			memcpy(transport + headerLength, packet, length);
		transportLength = (unsigned int)length + headerLength;
#if FW_VERSION >= 0x04000000
		__builtin___memcpy_chk(tcp, header, headerLength,
			sizeof(gBtNetworkPacketBuffer.bytes) - sizeof(*ip));
#else
		memcpy(tcp, header, headerLength);
#endif
		BtWriteNetwork16(tcp->sourcePortBe, translatedPort);
		BtWriteNetwork16(tcp->destinationPortBe, mapping->destinationPort);
		*(uint16_t *)tcp->checksumBe = 0;
	}
	totalLength = transportLength + sizeof(*ip);
	ip->un.ver_hl = 0x45;
	ip->ip_len = totalLength;
	ip->ip_ttl = 64;
	ip->ip_p = protocol;
	BtWriteNetwork32((uint8_t *)&ip->ip_dst.s_addr,
			 mapping->translatedDestinationAddress);
	ip->ip_src.s_addr = 0;
	ip->ip_sum = 0;
	if (!gBtNetworkGatewayPointer)
		return 0;
	allocated = gBtNetworkGatewayPointer->allocate(gBtNetworkGatewayPointer,
						      totalLength, 1);
	if (allocated) {
		memcpy(allocated, &gBtNetworkPacketBuffer, totalLength);
		BtUnlockTransport(0);
		gBtNetworkGatewayPointer->send(gBtNetworkGatewayPointer, allocated,
					      totalLength, translatedAddress);
		BtLockTransport();
		gBtNetworkGatewayPointer->free(gBtNetworkGatewayPointer, allocated);
	}
	return 1;
}

int BtProcessIcmpPacket(BtNetworkPacketContext *context, const void *packet,
			int length)
{
	const SceNetIcmpHeader *icmp = packet;
	BtConnection *connection = context->connection;
	BtDhcpLease *lease;
	BtEthernetFrame *frame;
	SceNetIpHeader *ip;
	SceNetIcmpHeader *reply;

	if (!connection || length <= 7)
		return 0;
	if ((uintptr_t)icmp & 1)
		return 0;
	if (BtInternetChecksum(0, (const uint16_t *)icmp, length) != 0xffff ||
	    context->destinationAddress != (gBtNetworkState.networkAddress | 1) ||
	    icmp->type != 8)
		return 0;
	for (lease = gBtNetworkState.leases + 2;
	     lease < gBtNetworkState.leases + 255; lease++) {
		if (lease->state &&
		    BtMatchNetworkMac(lease->hardwareAddress,
				      connection->deviceAddress))
			break;
	}
	if (lease > gBtNetworkState.leases + 254 ||
	    context->sourceAddress != gBtNetworkState.networkAddress +
				      (lease - gBtNetworkState.leases))
		return 0;
	memset(&gBtNetworkPacketBuffer, 0, sizeof(gBtNetworkPacketBuffer));
	frame = (BtEthernetFrame *)gBtNetworkPacketBuffer.bytes;
	ip = (SceNetIpHeader *)frame->payload;
	reply = (SceNetIcmpHeader *)(ip + 1);
	memcpy(reply + 1, icmp + 1, (unsigned int)length - sizeof(*icmp));
	reply->code = icmp->code;
	reply->un = icmp->un;
	reply->checksum = ~BtInternetChecksum(0, (const uint16_t *)reply, length);
	ip->un.ver_hl = 0x45;
	BtWriteNetwork16((uint8_t *)&ip->ip_len, (unsigned int)length + sizeof(*ip));
	ip->ip_ttl = 64;
	ip->ip_p = 1;
	BtWriteNetwork32((uint8_t *)&ip->ip_src.s_addr, context->destinationAddress);
	BtWriteNetwork32((uint8_t *)&ip->ip_dst.s_addr, context->sourceAddress);
	ip->ip_sum = ~BtInternetChecksum(0, (const uint16_t *)ip, sizeof(*ip));
	BtWriteNetworkMac(frame->ethernet.ether_dhost,
			  context->connection->deviceAddress);
	frame->ethernet.ether_type = 0x0008;
	BtWriteNetworkMac(frame->ethernet.ether_shost,
			  context->connection->stack->deviceAddress);
	BtSendBnepFrame(context->connection, frame,
		       (unsigned int)length + sizeof(frame->ethernet) + sizeof(*ip), 0);
	return 1;
}

int BtReceiveGatewayPacket(struct SceNetKernelGateway *interface,
			   struct NetDriver *driver,
			   struct NetDriverPacket *packet)
{
	const struct ether_header *ethernet;
	BtNetworkPacketContext context;
	int length, remaining, handled;

	if (BtLockTransport() < 0)
		return 1;
	length = packet->length;
	ethernet = (const struct ether_header *)packet->buffer->data;
	if (!gBtNetworkAccessPointEnabled) {
		BtUnlockTransport(0);
		return 1;
	}
	remaining = (int)((unsigned int)length - sizeof(*ethernet));
	context.connection = NULL;
	context.sourceAddress = 0;
	context.destinationAddress = 0;
	context.sourcePort = 0;
	context.destinationPort = 0;
	if (remaining < 0 || BtReadNetwork16(&ethernet->ether_type) != 0x0800)
		handled = 0;
	else
		handled = BtProcessIpPacket(&context,
					    (const SceNetIpHeader *)(ethernet + 1),
					    remaining);
	BtUnlockTransport(0);
	return handled == 0;
}

int BtProcessNetworkPacket(BtConnection *connection, const void *packet,
			   int length)
{
	const struct ether_header *ethernet = packet;
	const struct ether_arp *arp;
	struct EthernetArpFrame *reply;
	BtNetworkPacketContext context;
	unsigned int protocol, targetAddress;
	int remaining;

	if (!gBtNetworkAccessPointEnabled)
		return 0;
	remaining = (int)((unsigned int)length - sizeof(*ethernet));
	context.connection = connection;
	context.sourceAddress = 0;
	context.destinationAddress = 0;
	context.sourcePort = 0;
	context.destinationPort = 0;
	if (remaining < 0)
		return 0;
	protocol = BtReadNetwork16(&ethernet->ether_type);
	if (protocol == 0x0800) {
		BtProcessIpPacket(&context, (const SceNetIpHeader *)(ethernet + 1),
				  remaining);
		return 0;
	}
	if (protocol != 0x0806 || !connection || remaining <= 27)
		return 0;
	arp = (const struct ether_arp *)(ethernet + 1);
	if (BtReadNetwork16(&arp->ea_hdr.ar_hrd) != 1 ||
	    BtReadNetwork16(&arp->ea_hdr.ar_pro) != 0x0800 ||
	    arp->ea_hdr.ar_hln != 6 || arp->ea_hdr.ar_pln != 4 ||
	    BtReadNetwork16(&arp->ea_hdr.ar_op) != 1)
		return 0;
	targetAddress = BtReadNetwork32(arp->arp_tpa);
	if (targetAddress != (gBtNetworkState.networkAddress | 1))
		return 0;
	memset(&gBtNetworkPacketBuffer, 0, sizeof(gBtNetworkPacketBuffer));
	reply = &gBtNetworkPacketBuffer.arp;
	reply->arp.ea_hdr.ar_hrd = 0x0100;
	reply->arp.ea_hdr.ar_pro = 0x0008;
	reply->arp.ea_hdr.ar_hln = 6;
	reply->arp.ea_hdr.ar_pln = 4;
	reply->arp.ea_hdr.ar_op = 0x0200;
	BtWriteNetworkMac(reply->arp.arp_sha, connection->stack->deviceAddress);
	BtWriteNetwork32(reply->arp.arp_spa, targetAddress);
	memcpy(reply->arp.arp_tha, arp->arp_sha, sizeof(reply->arp.arp_tha));
	memcpy(reply->arp.arp_tpa, arp->arp_spa, sizeof(reply->arp.arp_tpa));
	BtWriteNetworkMac(reply->ethernet.ether_dhost, connection->deviceAddress);
	reply->ethernet.ether_type = 0x0608;
	BtWriteNetworkMac(reply->ethernet.ether_shost, connection->stack->deviceAddress);
	BtSendBnepFrame(connection, reply, sizeof(*reply), 0);
	return 0;
}

int BtInitializeNetwork(void)
{
	unsigned int count;
	int result;

	gBtNetworkGateway.receive = BtReceiveGatewayPacket;
	gBtNetworkGateway.notify = BtNotifyGateway;
	gBtNetworkState.networkAddress = 0xc0a86500;
	gBtNetworkState.subnetMask = 0xffffff00;
	result = SceNetKernelGateway(&gBtNetworkGateway);
	if (result < 0)
		return result;
	count = gBtNetworkGateway.portCount;
	if (count >= 1000)
		count = 1000;
	gBtNetworkPortCount = gBtNetworkGateway.portCount;
	gBtNetworkMappingEnd = gBtNetworkState.mappings + count;
	gBtNetworkGatewayPointer = &gBtNetworkGateway;
	gBtNetworkPortBase = gBtNetworkGateway.portBase;
	return 0;
}

