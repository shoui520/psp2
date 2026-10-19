/* DHCP request parsing and lease selection; original filename unavailable. */
#include "bt_internal.h"
#include <string.h>

int BtProcessDhcpPacket(BtNetworkPacketContext *context, const void *packet,
			int length)
{
	const BtDhcpPacket *request = packet;
	BtConnection *connection = context->connection;
	BtDhcpOptions options;
	BtDhcpLease *lease, *available;
	uint64_t deviceAddress;
	unsigned int networkAddress, assignedAddress, serverAddress;

	if (connection == NULL)
		goto ignored;
	if (length <= 243)
		goto ignored;
	if (request->op != 1)
		goto ignored;
	if (request->htype != 1)
		goto ignored;
	if (request->hlen != 6)
		goto ignored;
	deviceAddress = connection->deviceAddress;
	if (!BtMatchNetworkMac(request->chaddr, deviceAddress))
		goto ignored;
	memset(&options, 0, sizeof(options));
	options.flags = 7;
	if (memcmp(gBtDhcpMagicCookie, request->cookie, 4) == 0) {
		const uint8_t *end = (const uint8_t *)packet + length;
		const uint8_t *cursor = request->options;

		while (cursor < end) {
			const BtDhcpOption *option = (const BtDhcpOption *)cursor;
			int size, step;

			if (option->code == 0) {
				step = 1;
			} else {
				if (option->code == 255 || cursor >= end - 1)
					break;
				size = option->length;
				step = size + sizeof(*option);
				if (cursor + step > end)
					break;
				switch (option->code) {
				case 50:
					if (size == 4)
						options.assignedAddress =
							((uint32_t)option->data[0] << 24) |
							(option->data[1] << 16) |
							(option->data[2] << 8) | option->data[3];
					break;
				case 53:
					if (size == 1)
						options.messageType = option->data[0];
					break;
				case 54:
					if (size == 4)
						options.serverAddress =
							((uint32_t)option->data[0] << 24) |
							(option->data[1] << 16) |
							(option->data[2] << 8) | option->data[3];
					break;
				case 55: {
					unsigned int flags = options.flags & ~7U;
					int index;

					for (index = 0; index < size; index++) {
						switch (option->data[index]) {
						case 3:
							flags |= 2;
							break;
						case 6:
							flags |= 4;
							break;
						case 1:
							flags |= 1;
							break;
						}
					}
					options.flags = flags;
					break;
				}
				case 61:
					if (size == 7 && option->data[0] == 1) {
						__builtin_memcpy(options.hardwareAddress,
							option->data + 1, 6);
						if (!BtMatchNetworkMac(options.hardwareAddress,
							deviceAddress))
							goto ignored;
					}
					break;
				}
			}
			cursor += step;
		}
	}
	if ((request->flagsBe[0] >> 7) != 0)
		options.flags |= 0x10;
	available = NULL;
	lease = &gBtNetworkState.leases[2];
	do {
		if (memcmp(options.hardwareAddress, lease->hardwareAddress, 6) == 0)
			goto found;
		if (available == NULL && lease->state == 0)
			available = lease;
		lease++;
	} while (lease != &gBtNetworkState.leases[255]);
	if (available == NULL)
		goto ignored;
	lease = available;
	__builtin_memcpy(lease->hardwareAddress, options.hardwareAddress, 6);
found:
	networkAddress = gBtNetworkState.networkAddress;
	assignedAddress = networkAddress + (lease - gBtNetworkState.leases);
	if (options.messageType == 1) {
		lease->state = 1;
		options.serverAddress = networkAddress | 1;
		options.assignedAddress = assignedAddress;
		options.subnetMask = gBtNetworkState.subnetMask;
		return BtSendDhcpReply(context,
			((uint32_t)request->xidBe[0] << 24) |
			(request->xidBe[1] << 16) | (request->xidBe[2] << 8) |
			request->xidBe[3], &options, 2);
	}
	if (options.messageType != 3)
		goto ignored;
	serverAddress = networkAddress | 1;
	lease->state = 2;
	if (options.serverAddress == 0) {
		options.serverAddress = serverAddress;
	} else if (options.serverAddress != serverAddress ||
		   options.assignedAddress != assignedAddress) {
		goto ignored;
	}
	options.subnetMask = gBtNetworkState.subnetMask;
	return BtSendDhcpReply(context,
		((uint32_t)request->xidBe[0] << 24) |
		(request->xidBe[1] << 16) | (request->xidBe[2] << 8) |
		request->xidBe[3], &options, 5);
ignored:
	return 0;
}
