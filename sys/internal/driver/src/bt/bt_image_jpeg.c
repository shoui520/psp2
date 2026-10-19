/* Bluetooth image dimensions and Exif thumbnail parsing. */
#include <string.h>
#include "bt_internal.h"

int BtReadJpegDimensions(int *width, int *height, int *imageSize)
{
	uint8_t marker;
	uint8_t integerBytes[4];
	uint8_t probe[6];
	int result;

	if (BtReadFile(gBtImageSend.file, &marker, 1) != 1 ||
	    marker != 0xff ||
	    BtReadFile(gBtImageSend.file, &marker, 1) != 1 ||
	    marker != 0xd8)
		return (int)0x802f0f04;

	for (;;) {
		unsigned int segmentLength;

		do {
			result = BtReadFile(gBtImageSend.file, &marker, 1);
			if (result < 0)
				return (int)0x802f0f04;
			if (result != 1)
				return (int)0x802f0f04;
		} while (marker != 0xff);
		do {
			result = BtReadFile(gBtImageSend.file, &marker, 1);
			if (result < 0)
				return (int)0x802f0f04;
			if (result != 1)
				return (int)0x802f0f04;
		} while (marker == 0xff);
		if (!marker)
			continue;
		if ((unsigned int)marker - 0xe0 > 15) {
			if (marker == 0xc0)
				break;
			continue;
		}
		if (BtReadFile(gBtImageSend.file, probe, 2) != 2)
			return (int)0x802f0f04;
		segmentLength = probe[1] | probe[0] << 8;
		if (segmentLength <= 1)
			return (int)0x802f0f04;
		if (marker == 0xe1 && !gBtImageSend.thumbnail.length) {
			int savedPosition = BtSeekFile(gBtImageSend.file, 0, 1);
			int tiffBase;
			int bigEndian;
			int magic;

			if (BtReadFile(gBtImageSend.file, probe, 6) != 6 ||
			    memcmp("Exif\0", probe, 6))
				goto restoreSegment;
			tiffBase = BtSeekFile(gBtImageSend.file, 0, 1);
			if (BtReadFile(gBtImageSend.file, probe, 2) != 2)
				goto restoreSegment;
			if (strncmp("MM", (char *)probe, 2)) {
				if (strncmp("II", (char *)probe, 2) ||
				    BtReadFile(gBtImageSend.file, integerBytes, 2) != 2)
					goto restoreSegment;
				bigEndian = 0;
				magic = integerBytes[0] | integerBytes[1] << 8;
			} else {
				if (BtReadFile(gBtImageSend.file, integerBytes, 2) != 2)
					goto restoreSegment;
				bigEndian = 1;
				magic = integerBytes[1] | integerBytes[0] << 8;
			}
			if (magic != 42)
				goto restoreSegment;
			for (;;) {
				int directoryOffset;
				int entryCount;
				int entryIndex;

				if (BtReadFile(gBtImageSend.file, integerBytes, 4) != 4)
					goto restoreSegment;
				if (bigEndian)
					directoryOffset = (uint32_t)integerBytes[1] << 16 |
						(uint32_t)integerBytes[0] << 24 |
						integerBytes[3] | integerBytes[2] << 8;
				else
					directoryOffset = integerBytes[0] |
						(uint32_t)integerBytes[2] << 16 |
						integerBytes[1] << 8 |
						(uint32_t)integerBytes[3] << 24;
				if (directoryOffset < 0)
					goto restoreSegment;
				if (!directoryOffset)
					break;
				if (BtSeekFile(gBtImageSend.file,
					       directoryOffset + tiffBase, 0) < 0 ||
				    BtReadFile(gBtImageSend.file, integerBytes, 2) != 2)
					goto restoreSegment;
				if (bigEndian)
					entryCount = integerBytes[1] | integerBytes[0] << 8;
				else
					entryCount = integerBytes[0] | integerBytes[1] << 8;
				for (entryIndex = 0; entryIndex != entryCount; entryIndex++) {
					int tag;
					int type;
					int count;
					int value;

					if (BtReadFile(gBtImageSend.file, integerBytes, 2) != 2)
						goto restoreSegment;
					if (bigEndian) {
						tag = integerBytes[1] | integerBytes[0] << 8;
						if (BtReadFile(gBtImageSend.file, integerBytes, 2) != 2)
							goto restoreSegment;
						type = integerBytes[1] | integerBytes[0] << 8;
						if (BtReadFile(gBtImageSend.file, integerBytes, 4) != 4)
							goto restoreSegment;
						count = (uint32_t)integerBytes[1] << 16 |
							(uint32_t)integerBytes[0] << 24 |
							integerBytes[3] | integerBytes[2] << 8;
					} else {
						tag = integerBytes[0] | integerBytes[1] << 8;
						if (BtReadFile(gBtImageSend.file, integerBytes, 2) != 2)
							goto restoreSegment;
						type = integerBytes[0] | integerBytes[1] << 8;
						if (BtReadFile(gBtImageSend.file, integerBytes, 4) != 4)
							goto restoreSegment;
						count = integerBytes[0] |
							(uint32_t)integerBytes[2] << 16 |
							integerBytes[1] << 8 |
							(uint32_t)integerBytes[3] << 24;
					}
					if (count < 0 ||
					    BtReadFile(gBtImageSend.file, integerBytes, 4) != 4)
						goto restoreSegment;
					if (bigEndian)
						value = (uint32_t)integerBytes[1] << 16 |
							(uint32_t)integerBytes[0] << 24 |
							integerBytes[3] | integerBytes[2] << 8;
					else
						value = integerBytes[0] |
							(uint32_t)integerBytes[2] << 16 |
							integerBytes[1] << 8 |
							(uint32_t)integerBytes[3] << 24;
					if (value < 0)
						goto restoreSegment;
					if (tag == 0x201 && type == 4)
						gBtImageSend.thumbnail.offset = value + tiffBase;
					else if (tag == 0x202 && type == 4)
						gBtImageSend.thumbnail.length = value;
				}
			}
			if (gBtImageSend.thumbnail.offset &&
			    gBtImageSend.thumbnail.length &&
			    BtSeekFile(gBtImageSend.file,
				       gBtImageSend.thumbnail.offset, 0) >= 0 &&
			    BtReadJpegDimensions(&gBtImageSend.thumbnail.width,
					 &gBtImageSend.thumbnail.height, NULL) < 0)
				gBtImageSend.thumbnail.length = 0;
restoreSegment:
			if (BtSeekFile(gBtImageSend.file, savedPosition, 0) < 0)
				return (int)0x802f0f03;
		}
		result = BtSeekFile(gBtImageSend.file, segmentLength - 2, 1);
		if (result < 0)
			return result;
	}
	if (BtSeekFile(gBtImageSend.file, 3, 1) < 0)
		return (int)0x802f0f03;
	result = BtReadFile(gBtImageSend.file, probe, 2);
	if (result >= 0)
		result = result == 2 ? probe[1] | probe[0] << 8 : -1;
	*height = result;
	result = BtReadFile(gBtImageSend.file, probe, 2);
	if (result >= 0)
		result = result == 2 ? probe[1] | probe[0] << 8 : -1;
	*width = result;
	if (imageSize) {
		result = BtSeekFile(gBtImageSend.file, 0, 2);
		if (result < 0)
			return (int)0x802f0f03;
		*imageSize = result;
	}
	return BtSeekFile(gBtImageSend.file, 0, 0) < 0 ? (int)0x802f0f03 : 0;
}
