/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"
#include <sysmem/sysmem.h>

MarlinBuffer *MarlinGetRecordSecret(void *allocator);
int MarlinSha256Digest(const void *data, unsigned int size, void *digest);
int MarlinRandomCreate(SceMt19937Context **context);
int MarlinRandomRead(SceMt19937Context *context, int size, void *data);
int MarlinRandomDestroy(SceMt19937Context *context);
int MarlinEncryptData(const void *iv, const void *key, const void *data,
		      unsigned int size, void *encrypted,
		      unsigned int *encryptedSize);
int MarlinDecryptData(const void *iv, const void *key, const void *encrypted,
		      unsigned int size, void *data, unsigned int *dataSize);

MarlinBuffer *MarlinCreateEncryptedRecordBuffer(unsigned int dataSize)
{
	unsigned int size = 0;

	size += ((dataSize + 16) / 16) * 16;
	size += 16;
	size += 20;
	return MarlinBufferCreate(0, size, 0);
}

int MarlinDeriveRecordKey(unsigned char *key)
{
	int result;
	int index;
	MarlinBuffer *secret = 0;
	unsigned char digest[32];

	secret = MarlinGetRecordSecret(0);
	if (secret == 0) {
		result = 2;
	} else {
		result = MarlinSha256Digest(secret->data, secret->size, digest);
		if (result == 0) {
			for (index = 0; index < 16; index++)
				digest[index] ^= digest[index + 16];
			for (index = 0; index < 16; index++)
				key[index] = digest[index];
		}
	}
	MarlinBufferDestroy(0, secret);
	return result;
}

int MarlinEncryptRecord(const MarlinBuffer *data, unsigned char *output)
{
	int result;
	int index;
	unsigned int unusedRandomState;
	SceMt19937Context *random = 0;
	unsigned char iv[16];
	unsigned char key[16];
	unsigned int randomValue;
	unsigned int encryptedSize;

	unusedRandomState = 0;
	result = MarlinRandomCreate(&random);
	if (result != 0)
		goto out;
	for (index = 0; index < 16; index++) {
		result = MarlinRandomRead(random, sizeof(randomValue), &randomValue);
		if (result != 0)
			goto out;
		iv[index] = (unsigned char)(randomValue >> 16) ^
			(unsigned char)(randomValue >> 24);
	}
	result = MarlinDeriveRecordKey(key);
	if (result != 0)
		goto out;
	encryptedSize = ((data->size + 16) / 16) * 16;
	result = MarlinEncryptData(iv, key, data->data, data->size,
				   output + 16, &encryptedSize);
	if (result == 0)
		MarlinCopyMemory(output, iv, sizeof(iv));
out:
	MarlinRandomDestroy(random);
	return result;
}

int MarlinDecryptRecord(const MarlinBuffer *record, MarlinBuffer **output)
{
	int result;
	MarlinBuffer *buffer = 0;
	unsigned char key[16];
	unsigned int size;

	if (record->size < 16) {
		result = 6;
	} else {
		result = MarlinDeriveRecordKey(key);
		if (result == 0) {
			size = record->size - 16;
			buffer = MarlinBufferCreate(0, size, 0);
			if (buffer == 0) {
				result = 2;
			} else {
				result = MarlinDecryptData(record->data, key,
							  record->data + 16,
							  record->size - 16,
							  buffer->data, &size);
				if (result != 0 && result != 2) {
					result = 6;
				} else if (result == 0) {
					buffer->size = size;
					*output = buffer;
					buffer = 0;
				}
			}
		}
	}
	MarlinBufferDestroy(0, buffer);
	return result;
}
