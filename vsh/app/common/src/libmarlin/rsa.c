/*
 * RSA over the library's big-number routines: MGF1, keys from their
 * big-endian values, the private operation, random bytes, signature
 * verification (0x8105530c-). The file's name is ours.
 */

#include "mln_internal.h"

/* the public key and the private key as the big-number routines take them */
typedef struct MlnRsaPublic {
	unsigned int *modulus;
	unsigned int *publicExponent;
} MlnRsaPublic;

typedef struct MlnRsaPrivate {
	int version;
	unsigned int *modulus;
	unsigned int *publicExponent;
	unsigned int *privateExponent;
	unsigned int *prime1;
	unsigned int *prime2;
	unsigned int *exponent1;
	unsigned int *exponent2;
	unsigned int *coefficient;
} MlnRsaPrivate;

typedef struct MlnRsaKey {
	int bits;			/* 512, 1024 or 2048 */
	unsigned int halfBytes;
	unsigned int bytes;
	unsigned int halfWords;
	unsigned int words;
	unsigned int *modulus;
	unsigned int *publicExponent;
	unsigned int *privateExponent;
	unsigned int *prime1;
	unsigned int *prime2;
	unsigned int *exponent1;
	unsigned int *exponent2;
	unsigned int *coefficient;
	MlnRsaPublic publicKey;
	MlnRsaPrivate privateKey;
} MlnRsaKey;

/* an RSA key's values as buffers */
typedef struct MlnRsaKeyValues {
	MlnBuffer *modulus;
	MlnBuffer *publicExponent;
	MlnBuffer *privateExponent;
	MlnBuffer *prime1;
	MlnBuffer *prime2;
	MlnBuffer *exponent1;
	MlnBuffer *exponent2;
	MlnBuffer *coefficient;
} MlnRsaKeyValues;

/* random bytes, made 256 at a time */
static struct {
	unsigned char data[256];
	unsigned char *next;
} s_random;

int MlnImpl_8105714C(void **context);
int MlnImpl_810571C4(void *context, unsigned int size, void *out);
int MlnImpl_81057244(void *context);
int MlnImpl_8107E30C(unsigned int *number, unsigned int words, const unsigned char *data,
		     int size);
int MlnImpl_8107E428(void *out, const void *in, const MlnRsaPrivate *key);
int MlnImpl_8107E432(void *out, const void *in, const MlnRsaPrivate *key);
int MlnImpl_8107E43C(void *out, const void *in, const MlnRsaPrivate *key);
int MlnImpl_8107E6D8(void *out, const void *in, const MlnRsaPublic *key);
int MlnImpl_8107E6E2(void *out, const void *in, const MlnRsaPublic *key);
int MlnImpl_8107E6EC(void *out, const void *in, const MlnRsaPublic *key);

/* MGF1 with SHA-1: the mask is the digests of the seed and a big-endian counter */
unsigned int MlnImpl_8105530C(const void *seed, unsigned int seedSize, int maskSize,
			      unsigned char *mask)
{
	unsigned char digest[20];
	unsigned char *buffer;
	unsigned char *p;
	unsigned int count;
	unsigned int i;
	unsigned int ret;

	buffer = MlnAllocate(MLN_HEAP_GENERAL, seedSize + 4);
	if (buffer == NULL)
		return 2;
	MlnMemcpy(buffer, seed, seedSize);
	count = (maskSize + 19) / 20;
	p = buffer + seedSize;
	for (i = 0; i != count; i++) {
		p[0] = i >> 24;
		p[1] = i >> 16;
		p[2] = i >> 8;
		p[3] = i;
		if (MlnImpl_810570A8(buffer, seedSize + 4, digest) != 0) {
			ret = 0x2206000C;
			goto out;
		}
		MlnMemcpy(mask, digest, maskSize);
		maskSize -= 20;
		mask += 20;
	}
	ret = 0;
out:
	MlnFree(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* a big number of a number of words from a big-endian value, its leading zeros dropped */
unsigned int MlnImpl_810553F4(const MlnBuffer *value, unsigned int words, unsigned int **out)
{
	const unsigned char *data;
	unsigned int *number;
	int size;

	if (value == NULL) {
		*out = NULL;
		return 0;
	}
	size = value->size;
	data = value->data;
	while (size > 0 && *data == 0) {
		size--;
		data++;
	}
	number = MlnAllocate(MLN_HEAP_GENERAL, words * 4);
	if (number == NULL)
		return 2;
	MlnImpl_8107E30C(number, words, data, size);
	*out = number;
	return 0;
}

/* the private operation: by the Chinese remainder theorem when the key has its primes */
unsigned int MlnImpl_81055484(MlnRsaKey *key, const void *in, void *out)
{
	MlnRsaPublic exponent;
	int ret;

	if (key->prime1 != NULL && key->prime2 != NULL && key->exponent1 != NULL &&
	    key->exponent2 != NULL && key->coefficient != NULL) {
		if (key->bits == 512)
			ret = MlnImpl_8107E428(out, in, &key->privateKey);
		else if (key->bits == 1024)
			ret = MlnImpl_8107E432(out, in, &key->privateKey);
		else
			ret = MlnImpl_8107E43C(out, in, &key->privateKey);
	} else {
		if (key->privateExponent == NULL)
			return 0x2206000A;
		exponent.modulus = key->modulus;
		exponent.publicExponent = key->privateExponent;
		if (key->bits == 512)
			ret = MlnImpl_8107E6D8(out, in, &exponent);
		else if (key->bits == 1024)
			ret = MlnImpl_8107E6E2(out, in, &exponent);
		else
			ret = MlnImpl_8107E6EC(out, in, &exponent);
	}
	if (ret != 0)
		return 0x2206000A;
	return 0;
}

/* random bytes */
unsigned int MlnImpl_81055598(unsigned char *out, unsigned int size)
{
	unsigned char *end = out + size;
	unsigned char *next;
	void *context;

	MlnImpl_8105714C(&context);
	MlnImpl_810571C4(context, sizeof(s_random.data), s_random.data);
	MlnImpl_81057244(context);
	s_random.next = s_random.data;
	next = s_random.next;
	while (out < end)
		*out++ = *next++;
	s_random.next = next;
	return 0;
}

/* an RSA key from its values; the public exponent and the private values may be absent */
int MlnImpl_81055670(void **rsa, const MlnBuffer *modulus, const MlnBuffer *publicExponent,
		     const MlnBuffer *privateExponent, const MlnBuffer *prime1,
		     const MlnBuffer *prime2, const MlnBuffer *exponent1, const MlnBuffer *exponent2,
		     const MlnBuffer *coefficient)
{
	const unsigned char *data = modulus->data;
	MlnRsaKey *key;
	int size = modulus->size;
	int bits;
	int ret;

	*rsa = NULL;
	while (size > 0 && *data == 0) {
		size--;
		data++;
	}
	key = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*key));
	if (key == NULL)
		return 2;
	bits = size * 8;
	MlnMemset(key, 0, sizeof(*key));
	if (bits <= 512) {
		key->bits = 512;
		key->halfBytes = 32;
		key->bytes = 64;
		key->halfWords = 8;
		key->words = 16;
	} else if (bits <= 1024) {
		key->bits = 1024;
		key->halfBytes = 64;
		key->bytes = 128;
		key->halfWords = 16;
		key->words = 32;
	} else {
		key->bits = 2048;
		key->halfBytes = 128;
		key->bytes = 256;
		key->halfWords = 32;
		key->words = 64;
	}
	ret = MlnImpl_810553F4(modulus, key->words, &key->modulus);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_810553F4(publicExponent, key->words, &key->publicExponent);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_810553F4(privateExponent, key->words, &key->privateExponent);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_810553F4(prime1, key->halfWords, &key->prime1);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_810553F4(prime2, key->halfWords, &key->prime2);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_810553F4(exponent1, key->halfWords, &key->exponent1);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_810553F4(exponent2, key->halfWords, &key->exponent2);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_810553F4(coefficient, key->halfWords, &key->coefficient);
	if (ret != 0)
		goto fail;

	key->publicKey.modulus = key->modulus;
	key->publicKey.publicExponent = key->publicExponent;
	key->privateKey.version = 0;
	key->privateKey.modulus = key->modulus;
	key->privateKey.privateExponent = key->privateExponent;
	key->privateKey.prime1 = key->prime1;
	key->privateKey.prime2 = key->prime2;
	key->privateKey.exponent1 = key->exponent1;
	key->privateKey.exponent2 = key->exponent2;
	key->privateKey.coefficient = key->coefficient;
	*rsa = key;
	return 0;

fail:
	if (*rsa == NULL) {
		MlnFree(MLN_HEAP_GENERAL, key->modulus);
		MlnFree(MLN_HEAP_GENERAL, key->publicExponent);
		MlnFree(MLN_HEAP_GENERAL, key->privateExponent);
		MlnFree(MLN_HEAP_GENERAL, key->prime1);
		MlnFree(MLN_HEAP_GENERAL, key->prime2);
		MlnFree(MLN_HEAP_GENERAL, key->exponent1);
		MlnFree(MLN_HEAP_GENERAL, key->exponent2);
		MlnFree(MLN_HEAP_GENERAL, key->coefficient);
		MlnFree(MLN_HEAP_GENERAL, key);
	}
	return ret;
}

int MlnImpl_81055ABC(void *rsa)
{
	MlnRsaKey *key = rsa;

	if (key != NULL) {
		MlnFree(MLN_HEAP_GENERAL, key->modulus);
		MlnFree(MLN_HEAP_GENERAL, key->publicExponent);
		MlnFree(MLN_HEAP_GENERAL, key->privateExponent);
		MlnFree(MLN_HEAP_GENERAL, key->prime1);
		MlnFree(MLN_HEAP_GENERAL, key->prime2);
		MlnFree(MLN_HEAP_GENERAL, key->exponent1);
		MlnFree(MLN_HEAP_GENERAL, key->exponent2);
		MlnFree(MLN_HEAP_GENERAL, key->coefficient);
		MlnFree(MLN_HEAP_GENERAL, key);
	}
	return 0;
}

/* a public key's values, the buffers taken over */
int MlnImpl_81055B3C(void **values, MlnBuffer *modulus, MlnBuffer *publicExponent)
{
	MlnRsaKeyValues **out = (MlnRsaKeyValues **)values;

	*out = MlnAllocate(MLN_HEAP_GENERAL, sizeof(**out));
	if (*out == NULL)
		return 2;
	(*out)->modulus = modulus;
	(*out)->publicExponent = publicExponent;
	(*out)->privateExponent = NULL;
	(*out)->prime1 = NULL;
	(*out)->prime2 = NULL;
	(*out)->exponent1 = NULL;
	(*out)->exponent2 = NULL;
	(*out)->coefficient = NULL;
	return 0;
}

/* a private key's values, the buffers taken over */
int MlnImpl_81055BB4(void **values, MlnBuffer *modulus, MlnBuffer *publicExponent,
		     MlnBuffer *privateExponent, MlnBuffer *prime1, MlnBuffer *prime2,
		     MlnBuffer *exponent1, MlnBuffer *exponent2, MlnBuffer *coefficient)
{
	MlnRsaKeyValues **out = (MlnRsaKeyValues **)values;

	if (values == NULL || modulus == NULL || publicExponent == NULL ||
	    privateExponent == NULL || prime1 == NULL || prime2 == NULL || exponent1 == NULL ||
	    exponent2 == NULL || coefficient == NULL)
		return 3;
	*out = MlnAllocate(MLN_HEAP_GENERAL, sizeof(**out));
	if (*out == NULL)
		return 2;
	(*out)->modulus = modulus;
	(*out)->publicExponent = publicExponent;
	(*out)->privateExponent = privateExponent;
	(*out)->prime1 = prime1;
	(*out)->prime2 = prime2;
	(*out)->exponent1 = exponent1;
	(*out)->exponent2 = exponent2;
	(*out)->coefficient = coefficient;
	return 0;
}

int MlnImpl_81055CA8(void *values)
{
	MlnRsaKeyValues *key = values;

	if (key != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->modulus);
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->publicExponent);
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->privateExponent);
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->prime1);
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->prime2);
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->exponent1);
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->exponent2);
		MlnBufferDestroy(MLN_HEAP_GENERAL, key->coefficient);
		MlnFree(MLN_HEAP_GENERAL, key);
	}
	return 0;
}
