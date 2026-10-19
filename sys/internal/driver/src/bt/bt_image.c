/* Bluetooth imaging exchange; original source filename unavailable. */
#include <string.h>
#include <stdio.h>
#include <acmgr/acmgr.h>
#include <threadmgr/threadmgr.h>
#include "bt_internal.h"

BtImageSendState gBtImageSend;
BtImageReceiveState gBtImageReceive;

int BtParseImageHeaders(BtConnection *connection,
		       const uint64_t *deviceAddress, int headerOffset)
{
	uint8_t *cursor = gBtImageReceive.packet + headerOffset;
	uint8_t *end = gBtImageReceive.packet + gBtImageReceive.packetLength;
	unsigned int command = gBtImageReceive.packet[0];

	while (cursor < end) {
		BtObexVariableHeader *header = (BtObexVariableHeader *)cursor;
		int length;
		uint8_t *next;

		switch (header->id & 0xc0) {
		case 0:
		case 0x40:
			length = header->lengthBe[1] | header->lengthBe[0] << 8;
			if (length <= 2)
				return -1;
			break;
		case 0x80:
			length = sizeof(BtObexByteHeader);
			break;
		case 0xc0:
			length = sizeof(BtObexWordHeader);
			break;
		default:
			return -1;
		}
		next = cursor + length;
		if (next > end)
			return 0;
		switch (header->id) {
		case 1:
			if ((unsigned int)(length - 4) <= 123)
				BtUtf16BeToUtf8((uint8_t *)gBtImageReceive.name,
						header->data, sizeof(gBtImageReceive.name));
			break;
		case 0x42:
			if (length > 3 &&
			    !strncmp((char *)header->data, "x-bt/img-capabilities", 22))
				gBtImageReceive.capabilitiesRequested = 1;
			break;
		case 0xc3:
			if (length == sizeof(BtObexWordHeader)) {
				BtObexWordHeader *word = (BtObexWordHeader *)cursor;

				BtUnpack(word->valueBe, sizeof(word->valueBe), "W",
					 &gBtImageReceive.remainingImageBytes);
			}
			break;
		default:
			if (length <= 3 ||
			    !((command == 2 && header->id == 0x48) ||
			      (command == 0x82 && header->id == 0x49)))
				break;
			if (gBtImageReceive.file < 0) {
				const char *name = "bipd.dat";
				char *path = (char *)connection->stack->scratch;

				if (gBtImageReceive.name[0]) {
					const char *separator = strrchr(gBtImageReceive.name, '/');

					if (!separator)
						separator = strrchr(gBtImageReceive.name, ':');
					name = separator ? separator + 1 : gBtImageReceive.name;
				}
				snprintf(path, sizeof(connection->stack->scratch), "%s%s",
					 gBtImageReceive.path, name);
				gBtImageReceive.file = BtOpenFile((char *)connection->stack->scratch, 2);
				if (gBtImageReceive.file < 0)
					return 0;
			}
			BtWriteFile(gBtImageReceive.file, header->data, length - 3);
			{
				int remaining = gBtImageReceive.remainingImageBytes + 3 - length;
				uint64_t address = *deviceAddress;

				gBtImageReceive.remainingImageBytes = remaining;
				BtNotifyEvent(23, 0, remaining, address);
			}
			if (header->id == 0x49) {
				BtCloseFile(gBtImageReceive.file);
				gBtImageReceive.file = -1;
			}
			break;
		}
		cursor = next;
	}
	return 0;
}

int BtPushImage(BtConnection *connection, const char *path)
{
	const char *name;
	uint8_t *packet;
	unsigned int channel;
	int result, length;

	if (sceKernelGetProcessId() != 0x10005 &&
	    !sceSblACMgrIsSystemProgram(0))
		return (int)0x80010058;
	if (gBtImageSend.state)
		return (int)0x802f0f0d;
	gBtImageSend.deviceAddress = connection->deviceAddress;
	name = strrchr(path, '/');
	if (!name)
		name = strrchr(path, ':');
	strncpy(gBtImageSend.name, name ? name + 1 : path, 127);
	gBtImageSend.file = BtOpenFile(path, 1);
	if (gBtImageSend.file < 0)
		return (int)0x802f0f02;
	gBtImageSend.thumbnail = (BtImageThumbnail){ 0 };
	result = BtReadJpegDimensions(&gBtImageSend.width, &gBtImageSend.height,
				    &gBtImageSend.imageSize);
	if (result < 0) {
		BtCloseFile(gBtImageSend.file);
		return result;
	}
	gBtImageSend.connectionId = 0;
	gBtImageSend.error = 0;
	gBtImageSend.packetOffset = 0;
	gBtImageSend.state = 1;
	length = BtPack(gBtImageSend.packet, sizeof(gBtImageSend.packet),
			"1H11H 1H#", 0x80, 26, 16, 0, 1024, 0x46, 19,
			gBtImagingTargetUuid, 16);
	channel = connection->imagingChannel;
	packet = gBtImageSend.packet + gBtImageSend.packetOffset;
	gBtImageSend.responseLength = 0;
	gBtImageSend.command = *packet;
	return BtSendRfcommData(connection, channel, packet, length, 0);
}

int BtSetImageReceivePath(const char *path)
{
	if (sceKernelGetProcessId() != 0x10005 &&
	    !sceSblACMgrIsSystemProgram(0))
		return (int)0x80010058;
	if (gBtImageReceive.deviceAddress)
		return (int)0x802f0f0d;
	strncpy(gBtImageReceive.path, path, sizeof(gBtImageReceive.path));
	return 0;
}

enum {
	IMAGE_HANDLE_UTF16_POSITION = 256,
	IMAGE_HANDLE_UTF16_CAPACITY = 512,
	IMAGE_HANDLE_ASCII_POSITION = 512,
	IMAGE_HANDLE_ASCII_CAPACITY = 8,
};

int BtReceiveImage(BtConnection *connection, uint8_t *incoming, int length)
{
	int received;
	int responseLength;
	uint8_t *packet = gBtImageReceive.packet;
	unsigned int command;

	if (gBtImageReceive.deviceAddress) {
		if (gBtImageReceive.deviceAddress != connection->deviceAddress ||
		    !gBtImageReceive.path[0])
			goto reject;
	} else {
		uint64_t deviceAddress;

		if (!gBtImageReceive.path[0])
			goto reject;
		deviceAddress = connection->deviceAddress;
		gBtImageReceive.file = -1;
		gBtImageReceive.name[0] = 0;
		gBtImageReceive.deviceAddress = deviceAddress;
	}
	if (gBtImageReceive.packetLength) {
		received = gBtImageReceive.receivedLength;
	} else {
		if (length <= 2)
			return 0;
		BtUnpack(incoming + 1, 2, "H", &gBtImageReceive.packetLength);
		gBtImageReceive.receivedLength = 0;
		received = 0;
	}
	if (length + received >= (int)sizeof(gBtImageReceive.packet))
		return 0;
	memcpy(packet + received, incoming, length);
	gBtImageReceive.receivedLength += length;
	if (gBtImageReceive.packetLength > gBtImageReceive.receivedLength) {
		responseLength = 0;
		goto send;
	}
	command = packet[0];
	switch (command) {
	case 0x80:
		if (gBtImageReceive.packetLength <= 6 ||
		    BtParseImageHeaders(connection, &connection->deviceAddress, 7))
			goto complete;
		BtUnpack(packet + 5, 2, "H", &gBtImageReceive.mtu);
		if (gBtImageReceive.mtu > 512)
			gBtImageReceive.mtu = 512;
		++gBtImageReceive.connectionId;
		responseLength = BtPack(packet, sizeof(gBtImageReceive.packet),
					"1H11H 1W 1H#", 0xa0, 31, 16, 0, 1024,
					0xcb, gBtImageReceive.connectionId,
					0x4a, 19, gBtImagingTargetUuid, 16);
		break;
	case 2:
		if (gBtImageReceive.packetLength <= 2 ||
		    BtParseImageHeaders(connection, &connection->deviceAddress, 3))
			goto complete;
		responseLength = BtPack(packet, 8, "1H 1W", 0x90, 8,
					0xcb, gBtImageReceive.connectionId);
		break;
	case 0x81:
		if (gBtImageReceive.packetLength <= 2 ||
		    BtParseImageHeaders(connection, &connection->deviceAddress, 3))
			goto complete;
		responseLength = BtPack(packet, 8, "1H 1W", 0xa0, 8,
					0xcb, gBtImageReceive.connectionId);
		break;
	case 0x82: {
		int handle;
		int encodedLength;
		uint8_t *handleUtf16 = packet + IMAGE_HANDLE_UTF16_POSITION;
		uint8_t *handleAscii = packet + IMAGE_HANDLE_ASCII_POSITION;

		if (gBtImageReceive.packetLength <= 2 ||
		    BtParseImageHeaders(connection, &connection->deviceAddress, 3))
			goto complete;
		handle = gBtImageReceive.nextImageHandle++;
		snprintf((char *)handleAscii, IMAGE_HANDLE_ASCII_CAPACITY, "%07d", handle);
		encodedLength = BtUtf8ToUtf16Be(handleUtf16, handleAscii,
						IMAGE_HANDLE_UTF16_CAPACITY);
		responseLength = BtPack(packet, sizeof(gBtImageReceive.packet),
					"1H 1W 1H#", 0xa0, encodedLength + 11,
					0xcb, gBtImageReceive.connectionId,
					0x30, encodedLength + 3,
					handleUtf16, encodedLength);
		break;
	}
	case 3:
	case 0x83: {
		uint8_t *body;
		char *content;

		gBtImageReceive.capabilitiesRequested = 0;
		if (gBtImageReceive.packetLength <= 2 ||
		    BtParseImageHeaders(connection, &connection->deviceAddress, 3) ||
		    !gBtImageReceive.capabilitiesRequested)
			goto complete;
		body = packet + 3;
		body += BtPack(body, 5, "1W", 0xcb, gBtImageReceive.connectionId);
		content = (char *)body + 3;
		content += snprintf(content,
				    (char *)packet - content + sizeof(gBtImageReceive.packet),
				    "<imaging-capabilities version=\"1.0\">\r\n");
		content += snprintf(content,
				    (char *)packet - content + sizeof(gBtImageReceive.packet),
				    "<image-formats encoding=\"JPEG\" pixel=\"1*1-65535*65535\"/>\r\n");
		content += snprintf(content,
				    (char *)packet - content + sizeof(gBtImageReceive.packet),
				    "</imaging-capabilities>\r\n");
		responseLength = (uint8_t *)content - packet;
		BtPack(packet, 3, "1H", 0xa0, responseLength);
		BtPack(body, 3, "1H", 0x49, (uint8_t *)content - body);
		break;
	}
	default:
		return 0;
	}
send:
	BtSendRfcommData(connection, connection->imagingChannel,
			responseLength > 0 ? packet : NULL, responseLength, 0);
complete:
	gBtImageReceive.packetLength = 0;
	return 0;
reject:
	BtPack(incoming, 3, "1H", 0xd3, 3);
	return BtSendRfcommData(connection, connection->imagingChannel,
				incoming, 3, 0);
}

