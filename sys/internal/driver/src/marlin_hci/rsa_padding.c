/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"

/* DER DigestInfo: SEQUENCE, AlgorithmIdentifier/OID, OCTET STRING. */
static const char marlinSha1DigestSequences[] = "0!0\t";
static const unsigned char marlinSha1DigestAlgorithm[] = {
	0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00
};
static const unsigned char marlinSha1DigestOctets[] = { 0x04, 20 };
static const char marlinSha256DigestSequences[] = "010\r";
static const unsigned char marlinSha256DigestAlgorithm[] = {
	0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65,
	0x03, 0x04, 0x02, 0x01, 0x05, 0x00
};
static const unsigned char marlinSha256DigestOctets[] = { 0x04, 32 };

int MarlinSha256Digest(const void *data, unsigned int size, void *digest);

int MarlinSha1Digest(const void *data, unsigned int size,
		     void *digest);

int MarlinGenerateSha1Mask(const void *seed, unsigned int seedSize,
			   int maskSize, unsigned char *mask);

int MarlinRemoveOaepPadding(const MarlinBuffer *buffer,
			   const MarlinBuffer *label, MarlinBuffer **output)
{
	int result = 0;
	unsigned int cursor = 0;
	unsigned char *memory;
	int seedIndex;
	unsigned int dataIndex;
	unsigned int size = buffer->size - 20;
	unsigned char *maskedSeed = 0;
	unsigned char *maskedData = 0;
	unsigned char *decodedSeed = 0;
	unsigned char *decodedData;
	unsigned char *labelHash;
	unsigned char *padding;
	unsigned char *message;
	unsigned char mask[20];
	unsigned char digest[20];

	memory = 0;
	decodedData = 0;
	labelHash = 0;
	padding = 0;
	message = 0;
	if (buffer->size <= 40) {
		result = 0x2206000b;
		goto out;
	}
	maskedSeed = buffer->data;
	maskedData = buffer->data + 20;
	result = MarlinGenerateSha1Mask(maskedData, size, 20, mask);
	if (result == 0) {
		decodedSeed = mask;
		for (seedIndex = 0; seedIndex <= 19; seedIndex++)
			decodedSeed[seedIndex] = maskedSeed[seedIndex] ^ mask[seedIndex];
		memory = MarlinAllocateMemory(0, size);
		if (memory == 0) {
			result = 2;
			goto out;
		}
		result = MarlinGenerateSha1Mask(decodedSeed, 20, size, memory);
		if (result == 0) {
			decodedData = memory;
			for (dataIndex = 0; dataIndex < size; dataIndex++)
				decodedData[dataIndex] = maskedData[dataIndex] ^ memory[dataIndex];
			result = MarlinSha1Digest(label->data, label->size, digest);
			if (result != 0) {
				result = 0x2206000b;
				goto out;
			}
			labelHash = decodedData + cursor;
			cursor += 20;
			padding = decodedData + cursor;
			while (decodedData[cursor++] != 1) {
				if (cursor >= size) {
					result = 0x2206000b;
					goto out;
				}
			}
			message = decodedData + cursor;
			if (MarlinCompareMemory(labelHash, digest, 20) != 0) {
				result = 0x2206000b;
				goto out;
			}
			*output = MarlinBufferCreate(0, size - cursor, message);
			if (*output == 0)
				result = 2;
		}
	}
out:
	MarlinFreeMemory(0, memory);
	return result;
}

int MarlinRemovePkcs1Padding(const MarlinBuffer *buffer, MarlinBuffer **output)
{
	int result = 0;
	unsigned int cursor = 0;
	unsigned char *padding = 0;
	unsigned char *data = 0;

	if (buffer->size <= 9) {
		result = 0x2206000b;
		goto out;
	}
	if (buffer->data[cursor++] != 2) {
		result = 0x2206000b;
		goto out;
	}
	padding = buffer->data + cursor;
	while (buffer->data[cursor++] != 0) {
		if (cursor >= buffer->size) {
			result = 0x2206000b;
			goto out;
		}
	}
	data = buffer->data + cursor;
	if ((data - 1) - padding <= 7) {
		result = 0x2206000b;
		goto out;
	}
	*output = MarlinBufferCreate(0, buffer->size - cursor, data);
	if (*output == 0)
		result = 2;
out:
	return result;
}

int MarlinCreateSignaturePadding(const MarlinBuffer *data, int algorithm,
				unsigned int size, MarlinBuffer **output)
{
	int result = 0;
	unsigned int digestSize;
	unsigned int digestInfoSize;
	int prefixSize;
	int cursor = 0;
	unsigned int paddingSize;
	unsigned char digest[32];
	unsigned char digestInfo[52];

	if (algorithm == 0) {
		digestSize = 20;
		digestInfoSize = 35;
		prefixSize = 15;
		MarlinCopyMemory(digestInfo, marlinSha1DigestSequences, 4);
		MarlinCopyMemory(digestInfo + 4, marlinSha1DigestAlgorithm, 9);
		MarlinCopyMemory(digestInfo + 13, marlinSha1DigestOctets, 2);
		result = MarlinSha1Digest(data->data, data->size, digest);
		if (result != 0) {
			result = 0x22060004;
			goto out;
		}
	} else if (algorithm == 1) {
		digestSize = 32;
		digestInfoSize = 51;
		prefixSize = 19;
		MarlinCopyMemory(digestInfo, marlinSha256DigestSequences, 4);
		MarlinCopyMemory(digestInfo + 4, marlinSha256DigestAlgorithm, 13);
		MarlinCopyMemory(digestInfo + 17, marlinSha256DigestOctets, 2);
		result = MarlinSha256Digest(data->data, data->size, digest);
		if (result != 0) {
			result = 0x22060004;
			goto out;
		}
	} else {
		result = 0x22060003;
		goto out;
	}
	MarlinCopyMemory(digestInfo + prefixSize, digest, digestSize);
	if (digestInfoSize + 10 > size) {
		result = 0x22060006;
		goto out;
	}
	paddingSize = size - digestInfoSize - 2;
	*output = MarlinBufferCreate(0, size, 0);
	if (*output == 0) {
		result = 2;
		goto out;
	}
	(*output)->data[cursor] = 1;
	cursor++;
	MarlinFillMemory((*output)->data + cursor, -1, paddingSize);
	cursor += paddingSize;
	(*output)->data[cursor] = 0;
	cursor++;
	MarlinCopyMemory((*output)->data + cursor, digestInfo, digestInfoSize);
out:
	return result;
}
