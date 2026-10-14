/* Descriptive source name: the original library has no debug records. */
#include "key.h"

int MarlinIntegerByteLength(int count, const unsigned int *words);
int MarlinCreateIntegerWords(const MarlinBuffer *buffer, int count,
			    unsigned int **words);
int MarlinCreateIntegerBuffer(int count, const unsigned int *words,
			     int size, MarlinBuffer **output);
int MarlinRsaPrivateTransform(MarlinRsaContext *context,
			      const unsigned int *input, unsigned int *output);
int MarlinRemoveOaepPadding(const MarlinBuffer *buffer,
			   const MarlinBuffer *label, MarlinBuffer **output);

int MarlinRemovePkcs1Padding(const MarlinBuffer *buffer, MarlinBuffer **output);

int MarlinRsaSignTransform(MarlinRsaContext *context,
			   const unsigned int *input, unsigned int *output);
int MarlinCreateSignaturePadding(const MarlinBuffer *data, int algorithm,
				unsigned int size, MarlinBuffer **output);

typedef int (*MarlinRsaEncryptOperation)(MarlinRsaData *output,
				       const MarlinRsaData *input,
				       const MarlinRsaExponentKey *key);
int MarlinFillRandomBytes(unsigned char *data, int size);
int MarlinRsaSetRandomGenerator(int (*generator)(unsigned char *data, int size));
int MarlinRsaEncrypt512(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key);
int MarlinRsaEncrypt1024(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key);
int MarlinRsaEncrypt2048(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key);

int MarlinRsaDecryptOaep(MarlinRsaContext *context, const MarlinBuffer *encrypted,
			const MarlinBuffer *label, MarlinBuffer **output)
{
	int result = 0;
	MarlinRsaContext *rsa = context;
	int size;
	MarlinBuffer *buffer = 0;
	unsigned int words[64];
	unsigned int *memory = 0;

	size = MarlinIntegerByteLength(rsa->modulusWords, rsa->modulus);
	if (encrypted->size != size) {
		result = 0x2206000a;
		goto out;
	}
	result = MarlinCreateIntegerWords(encrypted, rsa->modulusWords, &memory);
	if (result == 0) {
		result = MarlinRsaPrivateTransform(rsa, memory, words);
		if (result != 0) {
			result = result == 0x22060001 ? 0x2206000a : result;
			goto out;
		}
		result = MarlinCreateIntegerBuffer(rsa->modulusWords, words,
						   size - 1, &buffer);
		if (result != 0) {
			result = result == 0x22060008 ? 0x2206000a : result;
			goto out;
		}
		result = MarlinRemoveOaepPadding(buffer, label, output);
		if (result != 0)
			result = result == 0x2206000b ? 0x2206000a : result;
	}
out:
	if (buffer != 0)
		MarlinBufferDestroy(0, buffer);
	if (memory != 0)
		MarlinFreeMemory(0, memory);
	return result;
}

int MarlinRsaEncrypt(MarlinRsaContext *context, const MarlinBuffer *data,
		     MarlinBuffer **output)
{
	unsigned int result = 0;
	MarlinBuffer *buffer;
	MarlinRsaEncryptOperation operation;
	MarlinRsaContext *rsa = context;
	MarlinRsaData encoded;
	MarlinRsaData input;
	unsigned char bytes[256];

	buffer = 0;
	input.data = data->data;
	input.size = data->size;
	encoded.data = bytes;
	MarlinRsaSetRandomGenerator(MarlinFillRandomBytes);
	switch (rsa->bits) {
	case 512:
		operation = MarlinRsaEncrypt512;
		break;
	case 1024:
		operation = MarlinRsaEncrypt1024;
		break;
	default:
		operation = MarlinRsaEncrypt2048;
		break;
	}
	result = operation(&encoded, &input, &rsa->publicKey);
	if (result != 0) {
		result = 0x22060002;
		goto out;
	}
	buffer = MarlinBufferCreate(0, encoded.size, encoded.data);
	if (buffer == 0) {
		result = 2;
		goto out;
	}
	*output = buffer;
	buffer = 0;
out:
	if (buffer != 0)
		MarlinBufferDestroy(0, buffer);
	return result;
}

int MarlinRsaDecryptPkcs1(MarlinRsaContext *context, const MarlinBuffer *encrypted,
			 MarlinBuffer **output)
{
	int result = 0;
	MarlinRsaContext *rsa = context;
	int size;
	unsigned int *memory = 0;
	unsigned int words[64];
	MarlinBuffer *buffer = 0;

	size = MarlinIntegerByteLength(rsa->modulusWords, rsa->modulus);
	if (encrypted->size != size) {
		result = 0x2206000a;
		goto out;
	}
	result = MarlinCreateIntegerWords(encrypted, rsa->modulusWords, &memory);
	if (result == 0) {
		result = MarlinRsaPrivateTransform(rsa, memory, words);
		if (result == 0) {
			result = MarlinCreateIntegerBuffer(rsa->modulusWords, words,
							   size - 1, &buffer);
			if (result != 0) {
				result = result != 0x22060008 ? result : 0x2206000a;
				goto out;
			}
			result = MarlinRemovePkcs1Padding(buffer, output);
			if (result != 0)
				result = result != 0x2206000b ? result : 0x2206000a;
		}
	}
out:
	if (buffer != 0)
		MarlinBufferDestroy(0, buffer);
	if (memory != 0)
		MarlinFreeMemory(0, memory);
	return result;
}

int MarlinRsaSign(MarlinRsaContext *context, int algorithm,
		  const MarlinBuffer *data, MarlinBuffer **signature)
{
	int result = 0;
	MarlinRsaContext *rsa = context;
	int size = 0;
	MarlinBuffer *buffer = 0;
	unsigned int *memory = 0;
	unsigned int words[64];

	size = MarlinIntegerByteLength(rsa->modulusWords, rsa->modulus);
	result = MarlinCreateSignaturePadding(data, algorithm, size - 1, &buffer);
	if (result != 0) {
		result = result != 0x22060006 ? result : 0x22060009;
		goto out;
	}
	result = MarlinCreateIntegerWords(buffer, rsa->modulusWords, &memory);
	if (result == 0) {
		result = MarlinRsaSignTransform(rsa, memory, words);
		if (result == 0)
			result = MarlinCreateIntegerBuffer(rsa->modulusWords, words,
							   size, signature);
	}
out:
	if (buffer != 0)
		MarlinBufferDestroy(0, buffer);
	if (memory != 0)
		MarlinFreeMemory(0, memory);
	return result;
}
