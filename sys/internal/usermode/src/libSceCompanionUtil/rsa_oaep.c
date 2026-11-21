/*
 * RSA-OAEP encryption (PKCS #1 v2): the data block (the label's hash, zeros, 1, the message)
 * masked with MGF1 of a random seed, the seed masked with MGF1 of the masked block, and the
 * encoded message raised to the public exponent by GcAuthMgr's RSA arithmetic.
 */
#include "rsa_oaep.h"

int MarlinRsaEncryptOaep(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key, int count, int size,
			 const MarlinRsaHash *hash, const MarlinRsaData *label)
{
	unsigned int encodedWords[64];
	unsigned int block[64];
	unsigned char *encoded = (unsigned char *)encodedWords;
	unsigned char *db = (unsigned char *)block;
	unsigned char *seed;
	unsigned char *maskedDb;
	unsigned char *front;
	unsigned char *back;
	unsigned int *first;
	unsigned int *last;
	int inputSize;
	int hashSize;
	int dbSize;
	int paddingSize;
	int half;
	int lastByteOffset;
	int remaining;
	int i;

	if (!MarlinRsaHasRandomGenerator())
		return -1;
	inputSize = input->size;
	if (inputSize < 0)
		return -1;
	hashSize = hash->size;
	if (hashSize < 20)
		return -1;
	if (hashSize >= size)
		return -1;
	if (inputSize > size - 2 * hashSize - 2)
		return -1;
	paddingSize = size - 2 * hashSize - 2 - inputSize;
	dbSize = size - hashSize - 1;

	/* DB = Hash(label) || PS || 1 || M */
	if (hash->digest(db, label->data, label->size) != 0)
		return -1;
	hashSize = hash->size;
	lastByteOffset = 4 * (count - 1);
	half = count / 2;
	for (i = 0; i < paddingSize; i++)
		db[hashSize + i] = 0;
	db[hashSize + paddingSize] = 1;
	for (i = 0; i < inputSize; i++)
		db[dbSize - inputSize + i] = input->data[i];

	/* EM = 0 || seed ^ MGF(maskedDB) || DB ^ MGF(seed) */
	encoded[0] = 0;
	seed = encoded + 1;
	maskedDb = seed + hashSize;
	MarlinRsaGenerateRandom(seed, hashSize);
	MarlinRsaMgf1(maskedDb, dbSize, seed, hash->size, hash);
	for (i = 0; i < dbSize; i++)
		maskedDb[i] = db[i] ^ maskedDb[i];
	MarlinRsaMgf1(db, hash->size, maskedDb, dbSize, hash);
	for (i = 0; i < hash->size; i++)
		seed[i] ^= db[i];

	front = encoded;
	back = encoded + lastByteOffset;
	remaining = half;
	if (remaining > 0) {
		first = block;
		last = block + count;
		do {
			unsigned int high = back[3] | (back[2] << 8) |
				(back[1] << 16) | ((unsigned int)back[0] << 24);
			unsigned int low = front[3] | (front[2] << 8) |
				(front[1] << 16) | ((unsigned int)front[0] << 24);

			back -= 4;
			front += 4;
			*first++ = high;
			*--last = low;
		} while (--remaining != 0);
	}
	if (MarlinRsaExponentOperation(encodedWords, block, key, count) == -1)
		return -1;
	front = output->data;
	back = front + lastByteOffset;
	remaining = half;
	if (remaining > 0) {
		first = encodedWords;
		last = encodedWords + count;
		do {
			unsigned int high = *--last;
			unsigned int low = *first++;

			front[3] = high;
			front[0] = high >> 24;
			front[1] = high >> 16;
			front[2] = high >> 8;
			back[0] = low >> 24;
			back[1] = low >> 16;
			front += 4;
			back[2] = low >> 8;
			back[3] = low;
			back -= 4;
		} while (--remaining != 0);
	}
	output->size = size;
	return 0;
}

int MarlinRsaEncryptOaep2048(MarlinRsaData *output, const MarlinRsaData *input,
			     const MarlinRsaExponentKey *key, const MarlinRsaHash *hash,
			     const MarlinRsaData *label)
{
	return MarlinRsaEncryptOaep(output, input, key, 64, 256, hash, label);
}
