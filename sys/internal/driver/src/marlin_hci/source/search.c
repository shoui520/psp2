#include "search.h"

#define MARLIN_STARFISH_ALGORITHM_ID \
	"http://marlin-drm.com/starfish/algorithmID/1.0"

int MarlinFindBytes(const void *data, unsigned int size,
		    const void *pattern, unsigned int patternSize)
{
	const unsigned char *dataBytes = data;
	const unsigned char *patternBytes = pattern;
	unsigned int position = 0;
	unsigned int matched = 0;

	do {
		if (dataBytes[position] == patternBytes[matched]) {
			++position;
			++matched;
		} else {
			position = position - matched + 1;
			matched = 0;
		}
	} while (position < size && matched < patternSize);
	if (matched == patternSize)
		return position - matched;
	return -1;
}

int MarlinBufferContains(const MarlinBuffer *buffer, const void *pattern,
			 unsigned int size, int *contains)
{
	int result = 0;
	int position = 0;

	if (buffer == 0 || pattern == 0 || size == 0 || contains == 0) {
		result = 3;
	} else {
		position = MarlinFindBytes(buffer->data, buffer->size, pattern, size);
		if (position < 0)
			*contains = 0;
		else
			*contains = 1;
	}
	return result;
}

int MarlinBufferIsStarfishEncrypted(const MarlinBuffer *buffer, int *encrypted)
{
	int result = 0;
	int contains = 0;
	char encryptedData[] = "EncryptedData";
	char cipherData[] = "CipherData";

	if (buffer == 0 || encrypted == 0) {
		result = 3;
	} else {
		result = MarlinBufferContains(buffer, encryptedData,
					      MarlinStringLength(encryptedData), &contains);
		if (result == 0) {
			if (contains == 0) {
				*encrypted = contains;
			} else {
				contains = 0;
				result = MarlinBufferContains(buffer, MARLIN_STARFISH_ALGORITHM_ID,
					MarlinStringLength(MARLIN_STARFISH_ALGORITHM_ID),
					&contains);
				if (result == 0) {
					if (contains == 0) {
						*encrypted = contains;
					} else {
						contains = 0;
						result = MarlinBufferContains(buffer, cipherData,
							MarlinStringLength(cipherData), &contains);
						if (result == 0)
							*encrypted = contains;
					}
				}
			}
		}
	}
	return result;
}

int MarlinBufferGetKeyFormat(const MarlinBuffer *buffer, unsigned int *format)
{
	int result = 0;
	unsigned int detectedFormat = 3;
	int encrypted = 0;

	if (buffer == 0 || format == 0) {
		result = 3;
	} else if (buffer->size <= 15) {
		*format = 3;
	} else {
		if (buffer->size == 16) {
			detectedFormat = 0;
		} else {
			result = MarlinBufferIsStarfishEncrypted(buffer, &encrypted);
			if (result != 0)
				goto out;
			if (encrypted == 1)
				detectedFormat = 2;
			else
				detectedFormat = 1;
		}
		*format = detectedFormat;
	}
out:
	return result;
}
