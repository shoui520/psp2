/* Descriptive source name: the original library has no debug records. */
#include "storage.h"

#define MARLIN_READ_SIZE(cursor) \
	(unsigned short)((unsigned short)((cursor)[0] << 8) | (cursor)[1])

int MarlinExtractStoredKeyPair(const MarlinBuffer *data, MarlinBuffer **first,
			      MarlinBuffer **second)
{
	int result = 0;
	MarlinBuffer *firstKey;
	MarlinBuffer *secondKey;
	unsigned int headerSize = sizeof(MarlinStoredKeyPairHeader);
	const unsigned char *payload = 0;
	const unsigned char *cursor = 0;
	MarlinStoredKeyPairHeader header;

	firstKey = 0;
	secondKey = 0;
	if (data == 0 || first == 0 || second == 0) {
		result = 3;
		goto out;
	}
	if (data->size < headerSize) {
		result = 0x23110004;
		goto out;
	}
	cursor = data->data;
	header.formatTag = *cursor;
	cursor++;
	cursor += sizeof(((MarlinStoredKeyPairHeader *)0)->reservedPrefix);
	header.leadingDataSize = MARLIN_READ_SIZE(cursor);
	cursor += sizeof(((MarlinStoredKeyPairHeader *)0)->leadingDataSize);
	header.firstKeySize = MARLIN_READ_SIZE(cursor);
	cursor += sizeof(((MarlinStoredKeyPairHeader *)0)->firstKeySize);
	header.secondKeySize = MARLIN_READ_SIZE(cursor);
	cursor += sizeof(((MarlinStoredKeyPairHeader *)0)->secondKeySize);
	cursor += sizeof(((MarlinStoredKeyPairHeader *)0)->reservedSuffix);
	payload = cursor;
	if (header.leadingDataSize + header.firstKeySize + header.secondKeySize !=
	    data->size - headerSize) {
		result = 0x23110004;
		goto out;
	}
	cursor = payload + header.leadingDataSize;
	firstKey = MarlinBufferCreate(0, header.firstKeySize, cursor);
	if (firstKey == 0) {
		result = 2;
		goto out;
	}
	cursor += header.firstKeySize;
	secondKey = MarlinBufferCreate(0, header.secondKeySize, cursor);
	if (secondKey == 0) {
		result = 2;
		goto out;
	}
	*first = firstKey;
	firstKey = 0;
	*second = secondKey;
	secondKey = 0;
out:
	MarlinBufferEraseAndDestroy(secondKey);
	MarlinBufferEraseAndDestroy(firstKey);
	return result;
}


#define MARLIN_PARSE_SIZE(field) \
	field = MARLIN_READ_SIZE(cursor); \
	cursor += sizeof(field)

int MarlinExtractStoredSections(const MarlinBuffer *data, int selector,
			       MarlinBuffer **first, MarlinBuffer **second)
{
	int result = 0;
	MarlinBuffer *secondSection;
	MarlinBuffer *firstSection;
	int firstOffset;
	unsigned int firstSize;
	int secondOffset;
	unsigned int secondSize;
	unsigned int headerSize = sizeof(MarlinStoredSectionsHeader);
	const unsigned char *payload = 0;
	const unsigned char *cursor = 0;
	MarlinStoredBasicPair *basic = 0;
	MarlinStoredExtendedPair *extended = 0;
	MarlinStoredInfoPair *info = 0;
	MarlinStoredSectionsHeader header;

	secondSection = 0;
	firstSection = 0;
	firstOffset = 0;
	firstSize = 0;
	secondOffset = 0;
	secondSize = 0;
	if (data == 0) {
		result = 3;
		goto out;
	}
	if (data->size < headerSize) {
		result = 0x23110004;
		goto out;
	}
	cursor = data->data;
	header.keyPair.formatTag = *cursor;
	cursor++;
	cursor += sizeof(header.keyPair.reservedPrefix);
	MARLIN_PARSE_SIZE(header.keyPair.leadingDataSize);
	MARLIN_PARSE_SIZE(header.keyPair.firstKeySize);
	MARLIN_PARSE_SIZE(header.keyPair.secondKeySize);
	cursor += sizeof(header.keyPair.reservedSuffix);
	MARLIN_PARSE_SIZE(header.basicPair.firstOffset);
	MARLIN_PARSE_SIZE(header.basicPair.firstSize);
	MARLIN_PARSE_SIZE(header.basicPair.secondSize);
	cursor += sizeof(header.basicPair.reserved);
	MARLIN_PARSE_SIZE(header.extendedPair.firstOffset);
	MARLIN_PARSE_SIZE(header.extendedPair.firstSize);
	MARLIN_PARSE_SIZE(header.extendedPair.gapSize);
	MARLIN_PARSE_SIZE(header.extendedPair.secondSize);
	MARLIN_PARSE_SIZE(header.extendedPair.trailingDataSizes[0]);
	MARLIN_PARSE_SIZE(header.extendedPair.trailingDataSizes[1]);
	MARLIN_PARSE_SIZE(header.extendedPair.trailingDataSizes[2]);
	cursor += sizeof(header.extendedPair.reserved);
	MARLIN_PARSE_SIZE(header.infoPair.firstOffset);
	MARLIN_PARSE_SIZE(header.infoPair.firstSize);
	MARLIN_PARSE_SIZE(header.infoPair.intermediateDataSizes[0]);
	MARLIN_PARSE_SIZE(header.infoPair.intermediateDataSizes[1]);
	MARLIN_PARSE_SIZE(header.infoPair.secondSize);
	cursor += sizeof(header.infoPair.reserved);
	payload = cursor;
	if (header.keyPair.formatTag != 1) {
		result = 5;
		goto out;
	}
	switch ((unsigned int)selector) {
	case 0:
		basic = &header.basicPair;
		firstOffset = basic->firstOffset;
		firstSize = basic->firstSize;
		secondOffset = firstOffset + firstSize;
		secondSize = basic->secondSize;
		break;
	case 1:
		extended = &header.extendedPair;
		firstOffset = extended->firstOffset;
		firstSize = extended->firstSize;
		secondOffset = firstOffset + firstSize + extended->gapSize;
		secondSize = extended->secondSize;
		break;
	case 2:
		info = &header.infoPair;
		firstOffset = info->firstOffset;
		firstSize = info->firstSize;
		secondOffset = firstOffset + firstSize +
			info->intermediateDataSizes[0] + info->intermediateDataSizes[1];
		secondSize = info->secondSize;
		break;
	default:
		result = 3;
		goto out;
	}
	if (firstOffset + firstSize > data->size - headerSize) {
		result = 0x23110004;
		goto out;
	}
	firstSection = MarlinBufferCreate(0, firstSize, payload + firstOffset);
	if (firstSection == 0) {
		result = 2;
		goto out;
	}
	if (secondOffset + secondSize > data->size - headerSize) {
		result = 0x23110004;
		goto out;
	}
	secondSection = MarlinBufferCreate(0, secondSize, payload + secondOffset);
	if (secondSection == 0) {
		result = 2;
		goto out;
	}
	*first = firstSection;
	firstSection = 0;
	*second = secondSection;
	secondSection = 0;
out:
	MarlinBufferDestroy(0, firstSection);
	MarlinBufferEraseAndDestroy(secondSection);
	return result;
}

#undef MARLIN_PARSE_SIZE
#undef MARLIN_READ_SIZE

int MarlinUpdateStoredData(int selector, unsigned int size, const void *data)
{
	unsigned int result = 0;
	unsigned int entrySize;
	MarlinBuffer buffer;

	switch (selector) {
	case 1:
		entrySize = 16;
		break;
	case 0:
		entrySize = 4;
		break;
	default:
		result = 3;
		goto out;
	}
	if (data == 0) {
		result = MarlinWriteStoredData(selector, 0);
	} else {
		buffer.size = size;
		buffer.data = (unsigned char *)data;
		result = MarlinWriteStoredData(selector, &buffer);
	}
out:
	return result;
}

int MarlinCheckStoredData(int selector)
{
	int result = 0;
	unsigned int entrySize;

	switch (selector) {
	case 1:
		entrySize = 16;
		break;
	case 0:
		entrySize = 4;
		break;
	default:
		result = 3;
		goto out;
	}
	result = MarlinLoadStoredData(selector, 0);
	if (result == 1)
		result = 0x23110005;
out:
	return result;
}
