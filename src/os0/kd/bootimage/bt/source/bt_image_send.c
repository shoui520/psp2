/* Bluetooth imaging response processing; original filename unavailable. */
#include <stdio.h>
#include <string.h>
#include "bt_internal.h"

int BtPack(uint8_t *destination, int remaining, const char *format, ...);
int BtUnpack(const uint8_t *source, int remaining, const char *format, ...);
int BtReadFile(SceUID file, void *destination, SceSize length);
int BtCloseFile(SceUID file);
int BtSeekFile(SceUID file, int offset, int whence);
int BtSendRfcommData(BtConnection *connection, unsigned int dlci,
		     const void *data, int length, int userMode);
uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
		       unsigned int argument, uint64_t deviceAddress);

int BtReceiveImageResponse(BtConnection *connection, const uint8_t *packet,
			  int length)
{
	int received = gBtImageSend.responseLength;
	int responseLength;
	int packetOffset;
	uint8_t *cursor;
	uint8_t *end;
	uint8_t *bodyHeader;
	uint8_t *imageData;
	int packetLength;
	int imageWidth;
	int imageHeight;
	int chunkLength;
	int remaining;
	unsigned int channel;
	unsigned int command;

	if (received) {
		received = gBtImageSend.receivedLength;
	} else {
		if (length <= 2)
			return 0;
		BtUnpack(packet + 1, 2, "H", &gBtImageSend.responseLength);
		gBtImageSend.receivedLength = 0;
	}
	if (length + received > 1024 - gBtImageSend.packetOffset)
		return 0;
	memcpy(gBtImageSend.packet + received + gBtImageSend.packetOffset,
	       packet, length);
	responseLength = gBtImageSend.responseLength;
	gBtImageSend.receivedLength += length;
	if (responseLength > gBtImageSend.receivedLength)
		return 0;
	packetOffset = gBtImageSend.packetOffset;
	cursor = gBtImageSend.packet + packetOffset +
		(gBtImageSend.command == 0x80 ? 7 : 3);
	end = gBtImageSend.packet + responseLength + packetOffset;
	while (cursor < end) {
		BtObexVariableHeader *header = (BtObexVariableHeader *)cursor;
		unsigned int headerLength;
		uint8_t *next;

		switch (header->id & 0xc0) {
		case 0:
		case 0x40:
			headerLength = header->lengthBe[1] | header->lengthBe[0] << 8;
			if (headerLength <= 2) {
				gBtImageSend.responseLength = 0;
				return 0;
			}
			break;
		case 0x80:
			headerLength = sizeof(BtObexByteHeader);
			break;
		case 0xc0:
			headerLength = sizeof(BtObexWordHeader);
			break;
		default:
			headerLength = 0;
			break;
		}
		next = cursor + headerLength;
		if (next > end)
			break;
		if (header->id == 0xcb) {
			BtObexWordHeader *word = (BtObexWordHeader *)cursor;

			BtUnpack(word->valueBe, 4, "W", &gBtImageSend.connectionId);
			packetOffset = gBtImageSend.packetOffset;
			responseLength = gBtImageSend.responseLength;
			end = gBtImageSend.packet + responseLength + packetOffset;
		} else if (header->id == 0x30 && headerLength == 19) {
			memcpy(gBtImageSend.imageHandle, header->data,
			       sizeof(gBtImageSend.imageHandle));
		}
		cursor = next;
	}
	switch (gBtImageSend.state) {
	case 1:
		if (gBtImageSend.packet[0] != 0xa0 || responseLength <= 30) {
			gBtImageSend.error = (int)0x802f0f05;
			goto closeImage;
		}
		BtUnpack(gBtImageSend.packet + 5, 2, "H", &gBtImageSend.mtu);
		if (gBtImageSend.mtu > 512)
			gBtImageSend.mtu = 512;
		gBtImageSend.state = 2;
		gBtImageSend.packetOffset = 0;
		packetLength = BtPack(gBtImageSend.packet, 1024, "1H 1W 1H#",
				      0x83, 33, 0xcb, gBtImageSend.connectionId,
				      0x42, 25, "x-bt/img-capabilities", 22);
		goto sendPacket;
	case 2: {
		int response = gBtImageSend.packet[packetOffset];
		int bodyLength;
		int minWidth = 0;
		int minHeight = 0;
		int maxWidth = 0;
		int maxHeight = 0;
		int thumbnailMatches = 0;
		const char *text;
		const char *textEnd;
		const char *next;
		const char *number;
		unsigned int digit;

		if (response != 0xa0 && response != 0x90) {
			gBtImageSend.error = (int)0x802f0f07;
			goto closeImage;
		}
		if (responseLength > 6) {
			BtUnpack(gBtImageSend.packet + packetOffset + 4,
				 2, "H", &bodyLength);
			packetOffset = gBtImageSend.packetOffset;
			if (gBtImageSend.responseLength - 2 > bodyLength &&
			    packetOffset + 6 + bodyLength <= 1026) {
				bodyLength -= 3;
				memmove(gBtImageSend.packet + packetOffset,
					gBtImageSend.packet + packetOffset + 6,
					bodyLength);
				packetOffset = gBtImageSend.packetOffset + bodyLength;
				gBtImageSend.packetOffset += bodyLength;
			}
		}
		if (response == 0x90) {
			packetLength = BtPack(gBtImageSend.packet + packetOffset,
					      1024 - packetOffset, "1H 1W",
					      0x83, 8, 0xcb, gBtImageSend.connectionId);
			goto sendPacket;
		}
		textEnd = (char *)gBtImageSend.packet + packetOffset;
		*(char *)textEnd = 0;
		text = (char *)gBtImageSend.packet;
		while (text < textEnd) {
			next = text + 1;
			if (*text != '<' || strncmp("image-formats", next, 13))
				goto nextFormat;
			next = BtSkipSpaces(text + 14);
			if (strncmp("encoding=\"JPEG\"", next, 15))
				goto nextFormat;
			next = BtSkipSpaces(next + 15);
			if (strncmp("pixel=\"", next, 7))
				goto nextFormat;
			digit = (uint8_t)next[7];
			if (digit - '0' <= 9) {
				number = next + 7;
				minWidth = 0;
				do {
					minWidth = (char)digit + 10 * minWidth - '0';
					digit = (uint8_t)*++number;
				} while (digit - '0' <= 9);
			} else {
				number = textEnd;
				digit = (uint8_t)*number;
			}
			next = number + 1;
			if (digit != '*')
				goto nextFormat;
			digit = (uint8_t)number[1];
			if (digit - '0' <= 9) {
				minHeight = 0;
				do {
					minHeight = (char)digit + 10 * minHeight - '0';
					digit = (uint8_t)*++next;
				} while (digit - '0' <= 9);
				number = next;
			} else {
				number = textEnd;
			}
			next = number + 1;
			if (*number != '-') {
				if (gBtImageSend.width == minWidth &&
				    gBtImageSend.height == minHeight)
					goto useFullImage;
				if (gBtImageSend.thumbnail.length > 0 &&
				    gBtImageSend.thumbnail.width == minWidth &&
				    gBtImageSend.thumbnail.height == minHeight)
					thumbnailMatches++;
				goto nextFormat;
			}
			digit = (uint8_t)number[1];
			if (digit - '0' <= 9) {
				maxWidth = 0;
				do {
					maxWidth = (char)digit + 10 * maxWidth - '0';
					digit = (uint8_t)*++next;
				} while (digit - '0' <= 9);
				number = next;
			} else {
				number = textEnd;
			}
			next = number + 1;
			if (*number != '*')
				goto nextFormat;
			if ((unsigned int)(uint8_t)number[1] - '0' <= 9) {
				number++;
				maxHeight = 0;
				for (;;) {
					next = number;
					digit = (uint8_t)*number++;
					if (digit - '0' > 9)
						break;
					maxHeight = (char)digit + 10 * maxHeight - '0';
				}
			} else {
				next = textEnd;
			}
			if (gBtImageSend.width >= minWidth &&
			    gBtImageSend.width <= maxWidth &&
			    gBtImageSend.height >= minHeight &&
			    gBtImageSend.height <= maxHeight)
				goto useFullImage;
			if (gBtImageSend.thumbnail.length > 0 &&
			    gBtImageSend.thumbnail.width >= minWidth &&
			    gBtImageSend.thumbnail.width <= maxWidth &&
			    gBtImageSend.thumbnail.height >= minHeight &&
			    gBtImageSend.thumbnail.height <= maxHeight)
				thumbnailMatches++;
nextFormat:
			text = next;
		}
		if (!thumbnailMatches) {
			gBtImageSend.useThumbnail = -1;
			gBtImageSend.error = (int)0x802f0f0b;
			goto closeImage;
		}
		gBtImageSend.useThumbnail = 1;
		gBtImageSend.packetOffset = 0;
		if (!gBtImageSend.thumbnail.offset || !gBtImageSend.thumbnail.length) {
			gBtImageSend.error = (int)0x802f0f0c;
			goto closeImage;
		}
		if (BtSeekFile(gBtImageSend.file, gBtImageSend.thumbnail.offset, 0) < 0)
			goto seekError;
		imageWidth = gBtImageSend.thumbnail.width;
		imageHeight = gBtImageSend.thumbnail.height;
		gBtImageSend.remainingImageBytes = gBtImageSend.thumbnail.length;
		goto buildImage;
useFullImage:
		gBtImageSend.useThumbnail = 0;
		gBtImageSend.packetOffset = 0;
		gBtImageSend.remainingImageBytes = gBtImageSend.imageSize;
		imageWidth = gBtImageSend.width;
		imageHeight = gBtImageSend.height;
buildImage:
		gBtImageSend.state = 3;
		cursor = gBtImageSend.packet + 3;
		cursor += BtPack(cursor, 5, "1W", 0xcb, gBtImageSend.connectionId);
		packetLength = BtUtf8ToUtf16Be(cursor + 3,
					      (uint8_t *)gBtImageSend.name, 256);
		cursor += BtPack(cursor, 3, "1H", 1, packetLength + 3) + packetLength;
		cursor += BtPack(cursor, 16, "1H#", 0x42, 16, "x-bt/img-img", 13);
		bodyHeader = cursor;
		cursor += 3 + snprintf((char *)cursor + 3,
				       gBtImageSend.packet + 1024 - (cursor + 3),
				       "<image-descriptor version=\"1.0\">\r\n");
		cursor += snprintf((char *)cursor, gBtImageSend.packet + 1024 - cursor,
				   "<image encoding=\"JPEG\" pixel=\"%d*%d\" size=\"%d\"/>\r\n",
				   imageWidth, imageHeight, gBtImageSend.remainingImageBytes);
		cursor += snprintf((char *)cursor, gBtImageSend.packet + 1024 - cursor,
				   "</image-descriptor>\r\n");
		BtPack(bodyHeader, 3, "1H", 0x71, cursor - bodyHeader);
		bodyHeader = cursor;
		imageData = cursor + 3;
		remaining = gBtImageSend.remainingImageBytes;
		goto sendImageChunk;
	}
	case 3:
		remaining = gBtImageSend.remainingImageBytes;
		if (remaining <= 0) {
			if (gBtImageSend.packet[0] == 0xa6) {
				if (gBtImageSend.useThumbnail) {
					gBtImageSend.remainingImageBytes = gBtImageSend.thumbnail.length;
					if (!gBtImageSend.thumbnail.length) {
						gBtImageSend.error = (int)0x802f0f0c;
						goto closeImage;
					}
				}
				if (BtSeekFile(gBtImageSend.file,
					       gBtImageSend.thumbnail.offset, 0) < 0)
					goto seekError;
				gBtImageSend.remainingImageBytes = gBtImageSend.thumbnail.length;
				cursor = gBtImageSend.packet + 3;
				cursor += BtPack(cursor, 5, "1W", 0xcb, gBtImageSend.connectionId);
				packetLength = BtUtf8ToUtf16Be(cursor + 3,
							      (uint8_t *)gBtImageSend.name, 256);
				cursor += BtPack(cursor, 3, "1H", 1, packetLength + 3) + packetLength;
				cursor += BtPack(cursor, 16, "1H#", 0x42, 16, "x-bt/img-thm", 13);
				cursor += BtPack(cursor, 19, "1H#", 0x30, 19,
						 gBtImageSend.imageHandle, 16);
				bodyHeader = cursor;
				imageData = cursor + 3;
				remaining = gBtImageSend.remainingImageBytes;
				goto sendImageChunk;
			}
			if (gBtImageSend.packet[0] == 0xa0)
				goto closeImage;
		} else if (gBtImageSend.packet[0] == 0x90) {
			bodyHeader = gBtImageSend.packet + 3;
			imageData = gBtImageSend.packet + 6;
			if (!gBtImageSend.useThumbnail) {
				BtNotifyEvent(22, 0, gBtImageSend.remainingImageBytes,
					      connection->deviceAddress);
				remaining = gBtImageSend.remainingImageBytes;
			}
			goto sendImageChunk;
		}
		gBtImageSend.error = (int)0x802f0f08;
		goto closeImage;
	case 4:
		if (gBtImageSend.packet[0] != 0xa0 && !gBtImageSend.error)
			gBtImageSend.error = (int)0x802f0f06;
		gBtImageSend.state = 0;
		BtNotifyEvent(22, 0, gBtImageSend.error, connection->deviceAddress);
		return 0;
	default:
		return 0;
	}
seekError:
	gBtImageSend.error = (int)0x802f0f03;
closeImage:
	BtCloseFile(gBtImageSend.file);
	gBtImageSend.state = 4;
	gBtImageSend.file = -1;
	packetLength = BtPack(gBtImageSend.packet, 1024, "1H 1W",
			      0x81, 8, 0xcb, gBtImageSend.connectionId);
	goto sendPacket;
sendImageChunk:
	chunkLength = gBtImageSend.packet - imageData + gBtImageSend.mtu;
	if (chunkLength > remaining) {
		chunkLength = remaining;
		gBtImageSend.remainingImageBytes = 0;
	} else {
		gBtImageSend.remainingImageBytes = remaining - chunkLength;
	}
	BtPack(gBtImageSend.packet, 3, "1H",
	       gBtImageSend.remainingImageBytes > 0 ? 2 : 0x82,
	       imageData - gBtImageSend.packet + chunkLength);
	BtPack(bodyHeader, 3, "1H",
	       gBtImageSend.remainingImageBytes > 0 ? 0x48 : 0x49, chunkLength + 3);
	if (BtReadFile(gBtImageSend.file, imageData, chunkLength) != chunkLength) {
		gBtImageSend.error = (int)0x802f0f09;
		goto closeImage;
	}
	packetLength = imageData - gBtImageSend.packet + chunkLength;
sendPacket:
	channel = connection->imagingChannel;
	command = gBtImageSend.packet[gBtImageSend.packetOffset];
	gBtImageSend.responseLength = 0;
	gBtImageSend.command = command;
	BtSendRfcommData(connection, channel,
			 gBtImageSend.packet + gBtImageSend.packetOffset, packetLength, 0);
	return 0;
}
