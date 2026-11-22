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

/* a byte string as the big-number routines take it */
typedef struct MlnRsaOctets {
	unsigned char *data;
	unsigned int size;
} MlnRsaOctets;

typedef int (*MlnRsaPublicFunc)(MlnRsaOctets *out, const MlnRsaOctets *in,
				const MlnRsaPublic *key);

/* the digests a signature is made over */
#define MLN_RSA_HASH_SHA1	0
#define MLN_RSA_HASH_SHA256	1

/* the DigestInfo algorithm identifiers: the digest's OID and NULL parameters */
static const unsigned char s_sha1Algorithm[9] = {
	0x06, 0x05, 0x2B, 0x0E, 0x03, 0x02, 0x1A, 0x05, 0x00,
};

static const unsigned char s_sha256Algorithm[13] = {
	0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00,
};

/* random bytes, made 256 at a time */
static struct {
	unsigned char data[256];
	unsigned char *next;
} s_random;

int MarlinDecodeIntegerWords(unsigned int *number, unsigned int words, const unsigned char *data,
		     int size);
int MarlinRsaPrivate512(void *out, const void *in, const MlnRsaPrivate *key);
int MarlinRsaPrivate1024(void *out, const void *in, const MlnRsaPrivate *key);
int MarlinRsaPrivate2048(void *out, const void *in, const MlnRsaPrivate *key);
int MarlinRsaExponent512(void *out, const void *in, const MlnRsaPublic *key);
int MarlinRsaExponent1024(void *out, const void *in, const MlnRsaPublic *key);
int MarlinRsaExponent2048(void *out, const void *in, const MlnRsaPublic *key);
int MarlinRsaSetRandomGenerator(unsigned int (*random)(unsigned char *out, unsigned int size));
int MarlinRsaEncrypt512(MlnRsaOctets *out, const MlnRsaOctets *in, const MlnRsaPublic *key);
int MarlinRsaEncrypt1024(MlnRsaOctets *out, const MlnRsaOctets *in, const MlnRsaPublic *key);
int MarlinRsaEncrypt2048(MlnRsaOctets *out, const MlnRsaOctets *in, const MlnRsaPublic *key);
int MarlinRsaSignPrivate512(void *out, const void *in, const MlnRsaPrivate *key);
int MarlinRsaSignPrivate1024(void *out, const void *in, const MlnRsaPrivate *key);
int MarlinRsaSignPrivate2048(void *out, const void *in, const MlnRsaPrivate *key);
int MarlinRsaSignExponent512(void *out, const void *in, const MlnRsaPublic *key);
int MarlinRsaSignExponent1024(void *out, const void *in, const MlnRsaPublic *key);
int MarlinRsaSignExponent2048(void *out, const void *in, const MlnRsaPublic *key);
typedef int (*MlnRsaVerifyFunc)(const MlnRsaOctets *signature, const MlnRsaOctets *digest,
				const MlnRsaPublic *key, int hash);
int GcAuthRsaVerify512(const MlnRsaOctets *signature, const MlnRsaOctets *digest,
		     const MlnRsaPublic *key, int hash);
int GcAuthRsaVerify1024(const MlnRsaOctets *signature, const MlnRsaOctets *digest,
		     const MlnRsaPublic *key, int hash);
int GcAuthRsaVerify2048(const MlnRsaOctets *signature, const MlnRsaOctets *digest,
		     const MlnRsaPublic *key, int hash);
int MarlinEncodeIntegerWords(unsigned char *out, unsigned int size, const unsigned int *number,
		     unsigned int words);

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
	MarlinDecodeIntegerWords(number, words, data, size);
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
			ret = MarlinRsaPrivate512(out, in, &key->privateKey);
		else if (key->bits == 1024)
			ret = MarlinRsaPrivate1024(out, in, &key->privateKey);
		else
			ret = MarlinRsaPrivate2048(out, in, &key->privateKey);
	} else {
		if (key->privateExponent == NULL)
			return 0x2206000A;
		exponent.modulus = key->modulus;
		exponent.publicExponent = key->privateExponent;
		if (key->bits == 512)
			ret = MarlinRsaExponent512(out, in, &exponent);
		else if (key->bits == 1024)
			ret = MarlinRsaExponent1024(out, in, &exponent);
		else
			ret = MarlinRsaExponent2048(out, in, &exponent);
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

/* the bytes of a key's modulus, its leading zeros not counted */
static inline int MlnRsaModulusSize(const MlnRsaKey *key)
{
	unsigned int word;
	int unused = 0;
	int i;

	for (i = key->words - 1; i >= 0; i--) {
		word = key->modulus[i];
		if (word & 0xFF000000) {
			unused = 0;
			break;
		}
		if (word & 0xFF0000) {
			unused = 1;
			break;
		}
		if (word & 0xFF00) {
			unused = 2;
			break;
		}
		if (word & 0xFF) {
			unused = 3;
			break;
		}
	}
	return (i + 1) * 4 - unused;
}

/*
 * EME-OAEP encoding with SHA-1 for a modulus of a size: the seed and the masked data block,
 * without the leading zero byte; the seed is the digest of the tick.
 */
static inline unsigned int MlnRsaOaepEncode(int size, const MlnBuffer *in,
					    const MlnBuffer *label, MlnBuffer **out)
{
	unsigned int tick = 0;
	unsigned char seed[20];
	unsigned char *mask = NULL;
	unsigned char *data;
	unsigned char *block;
	MlnBuffer *encoded;
	int blockSize = size - 21;
	int padding;
	int i;
	unsigned int ret;

	padding = size - 2 * 20 - 2 - (int)in->size;
	if (padding < 0) {
		ret = 0x22060004;
		encoded = NULL;
		goto out;
	}
	encoded = MlnBufferCreate(MLN_HEAP_GENERAL, size - 1, NULL);
	if (encoded == NULL) {
		ret = 2;
		goto out;
	}
	data = encoded->data;
	block = data + 20;
	ret = MlnImpl_810570A8(label->data, label->size, block);
	if (ret != 0)
		goto fail;
	MlnMemset(block + 20, 0, padding);
	block[20 + padding] = 1;
	MlnMemcpy(block + 20 + padding + 1, in->data, in->size);
	MlnTimeGetTick(&tick);
	ret = MlnImpl_810570A8(&tick, sizeof(tick), seed);
	if (ret != 0)
		goto fail;

	mask = MlnAllocate(MLN_HEAP_GENERAL, blockSize);
	if (mask == NULL) {
		ret = 2;
		goto fail;
	}
	ret = MlnImpl_8105530C(seed, sizeof(seed), blockSize, mask);
	if (ret != 0)
		goto fail;
	for (i = 0; i < blockSize; i++)
		block[i] ^= mask[i];
	ret = MlnImpl_8105530C(block, blockSize, sizeof(seed), data);
	if (ret != 0)
		goto fail;
	for (i = 0; i < (int)sizeof(seed); i++)
		data[i] ^= seed[i];
	goto done;
fail:
	MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
	encoded = NULL;
done:
	if (mask != NULL)
		MlnFree(MLN_HEAP_GENERAL, mask);
out:
	*out = encoded;
	return ret;
}

/* RSA-OAEP encryption with SHA-1 under the public key */
unsigned int MlnImpl_81055D28(void *rsa, const MlnBuffer *in, const MlnBuffer *label,
			      MlnBuffer **out)
{
	MlnRsaKey *key = rsa;
	unsigned char bytes[256];
	unsigned int value[64];
	MlnBuffer *encoded = NULL;
	MlnBuffer *cipher;
	unsigned int *number = NULL;
	unsigned char *p;
	unsigned int words;
	int size;
	int length;
	unsigned int ret;

	size = MlnRsaModulusSize(key);
	ret = MlnRsaOaepEncode(size, in, label, &encoded);
	if (ret != 0)
		goto out;
	ret = MlnImpl_810553F4(encoded, key->words, &number);
	if (ret != 0)
		goto out;
	if (key->bits == 512)
		ret = MarlinRsaExponent512(value, number, &key->publicKey);
	else if (key->bits == 1024)
		ret = MarlinRsaExponent1024(value, number, &key->publicKey);
	else
		ret = MarlinRsaExponent2048(value, number, &key->publicKey);
	if (ret != 0) {
		ret = 0x22060002;
		goto out;
	}
	words = key->words;
	MarlinEncodeIntegerWords(bytes, sizeof(bytes), value, words);
	p = bytes;
	length = words * 4;
	while (length > 0 && *p == 0) {
		length--;
		p++;
	}
	if (size < length) {
		ret = 0x22060008;
		goto out;
	}
	cipher = MlnBufferCreateFilled(MLN_HEAP_GENERAL, size, 0);
	if (cipher == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemcpy(cipher->data + size - length, p, length);
	*out = cipher;
out:
	if (encoded != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
	if (number != NULL)
		MlnFree(MLN_HEAP_GENERAL, number);
	return ret;
}

/*
 * EME-OAEP decoding with SHA-1 of an encoded message without its leading zero byte: the
 * message after the separator, the label's digest checked.
 */
static inline unsigned int MlnRsaOaepDecode(const MlnBuffer *encoded, const MlnBuffer *label,
					    MlnBuffer **out)
{
	unsigned char seed[20];
	unsigned char digest[20];
	unsigned char *mask = NULL;
	unsigned char *data;
	unsigned char *block;
	unsigned int blockSize;
	unsigned int i;
	unsigned int ret;

	if (encoded->size <= 2 * 20) {
		ret = 0x2206000B;
		goto out;
	}
	blockSize = encoded->size - 20;
	data = encoded->data;
	block = data + 20;
	ret = MlnImpl_8105530C(block, blockSize, sizeof(seed), seed);
	if (ret != 0)
		goto out;
	for (i = 0; i < sizeof(seed); i++)
		seed[i] ^= data[i];
	mask = MlnAllocate(MLN_HEAP_GENERAL, blockSize);
	if (mask == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_8105530C(seed, sizeof(seed), blockSize, mask);
	if (ret != 0)
		goto out;
	for (i = 0; i < blockSize; i++)
		mask[i] ^= block[i];
	if (MlnImpl_810570A8(label->data, label->size, digest) != 0) {
		ret = 0x2206000B;
		goto out;
	}
	for (i = 20; i < blockSize; i++) {
		if (mask[i] == 1)
			break;
	}
	if (i == blockSize) {
		ret = 0x2206000B;
		goto out;
	}
	i++;
	if (MlnMemcmp(mask, digest, sizeof(digest)) != 0) {
		ret = 0x2206000B;
		goto out;
	}
	*out = MlnBufferCreate(MLN_HEAP_GENERAL, blockSize - i, mask + i);
	if (*out == NULL)
		ret = 2;
out:
	MlnFree(MLN_HEAP_GENERAL, mask);
	return ret;
}

/* RSA-OAEP decryption with SHA-1 under the private key */
unsigned int MlnImpl_81056394(void *rsa, const MlnBuffer *in, const MlnBuffer *label,
			      MlnBuffer **out)
{
	MlnRsaKey *key = rsa;
	unsigned char bytes[256];
	unsigned char value[256];
	MlnBuffer *encoded;
	unsigned int *number;
	unsigned char *p;
	unsigned int words;
	int size;
	int length;
	unsigned int ret;

	size = MlnRsaModulusSize(key);
	if (in->size != size)
		return 0x2206000A;
	ret = MlnImpl_810553F4(in, key->words, &number);
	if (ret != 0)
		return ret;
	ret = MlnImpl_81055484(key, number, value);
	if (ret != 0) {
		if (ret == 0x22060001)
			ret = 0x2206000A;
		goto out;
	}
	words = key->words;
	MarlinEncodeIntegerWords(bytes, sizeof(bytes), (const unsigned int *)value, words);
	size--;
	p = bytes;
	length = words * 4;
	while (length > 0 && *p == 0) {
		length--;
		p++;
	}
	if (size < length) {
		ret = 0x2206000A;
		goto out;
	}
	encoded = MlnBufferCreateFilled(MLN_HEAP_GENERAL, size, 0);
	if (encoded == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemcpy(encoded->data + size - length, p, length);
	ret = MlnRsaOaepDecode(encoded, label, out);
	if (ret == 0x2206000B)
		ret = 0x2206000A;
	MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
out:
	MlnFree(MLN_HEAP_GENERAL, number);
	return ret;
}

/* RSAES-PKCS1-v1_5 encryption under the public key */
unsigned int MlnImpl_81056744(void *rsa, const MlnBuffer *in, MlnBuffer **out)
{
	MlnRsaKey *key = rsa;
	unsigned char data[256];
	MlnRsaOctets result;
	MlnRsaOctets message;
	MlnRsaPublicFunc encrypt;
	MlnBuffer *buffer;

	message.data = in->data;
	message.size = in->size;
	result.data = data;
	MarlinRsaSetRandomGenerator(MlnImpl_81055598);
	if (key->bits == 512)
		encrypt = MarlinRsaEncrypt512;
	else if (key->bits == 1024)
		encrypt = MarlinRsaEncrypt1024;
	else
		encrypt = MarlinRsaEncrypt2048;
	if (encrypt(&result, &message, &key->publicKey) != 0)
		return 0x22060002;
	buffer = MlnBufferCreate(MLN_HEAP_GENERAL, result.size, result.data);
	if (buffer == NULL)
		return 2;
	*out = buffer;
	return 0;
}

/* RSAES-PKCS1-v1_5 decryption under the private key */
unsigned int MlnImpl_810567EC(void *rsa, const MlnBuffer *in, MlnBuffer **out)
{
	MlnRsaKey *key = rsa;
	unsigned char bytes[256];
	unsigned char value[256];
	MlnBuffer *encoded;
	unsigned int *number;
	unsigned char *data;
	unsigned char *p;
	unsigned int words;
	unsigned int i;
	int size;
	int length;
	unsigned int ret;

	size = MlnRsaModulusSize(key);
	if (in->size != size)
		return 0x2206000A;
	ret = MlnImpl_810553F4(in, key->words, &number);
	if (ret != 0)
		return ret;
	ret = MlnImpl_81055484(key, number, value);
	if (ret != 0)
		goto out;
	words = key->words;
	MarlinEncodeIntegerWords(bytes, sizeof(bytes), (const unsigned int *)value, words);
	size--;
	p = bytes;
	length = words * 4;
	while (length > 0 && *p == 0) {
		length--;
		p++;
	}
	if (length > size) {
		ret = 0x2206000A;
		goto out;
	}
	encoded = MlnBufferCreateFilled(MLN_HEAP_GENERAL, size, 0);
	if (encoded == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemcpy(encoded->data + size - length, p, length);

	/* 02, at least eight nonzero padding bytes, 00, the message */
	if (encoded->size < 10)
		goto invalid;
	data = encoded->data;
	if (data[0] != 2)
		goto invalid;
	p = data;
	i = 1;
	for (;;) {
		i++;
		if (*++p == 0)
			break;
		if (i == encoded->size)
			goto invalid;
	}
	if (p - (data + 1) < 8)
		goto invalid;
	*out = MlnBufferCreate(MLN_HEAP_GENERAL, encoded->size - i, data + i);
	ret = *out == NULL ? 2 : 0;
	goto done;
invalid:
	ret = 0x2206000A;
done:
	MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
out:
	MlnFree(MLN_HEAP_GENERAL, number);
	return ret;
}

/*
 * EMSA-PKCS1-v1_5 encoding of a message's digest for a modulus of a size, without the
 * leading zero byte: 01, FF padding, 00, the DigestInfo.
 */
static inline unsigned int MlnRsaPkcs1Encode(int size, int hash, const MlnBuffer *in,
					     MlnBuffer **out)
{
	unsigned char digest[32];
	unsigned char info[19 + 32];
	unsigned int headerSize;
	unsigned int digestSize;
	unsigned int infoSize;
	unsigned int padding;
	MlnBuffer *encoded;

	if (hash == MLN_RSA_HASH_SHA1) {
		MlnMemcpy(info, "\x30\x21\x30\x09", 4);
		MlnMemcpy(info + 4, s_sha1Algorithm, sizeof(s_sha1Algorithm));
		MlnMemcpy(info + 4 + sizeof(s_sha1Algorithm), "\x04\x14", 2);
		if (MlnImpl_810570A8(in->data, in->size, digest) != 0)
			return 0x22060004;
		headerSize = 15;
		digestSize = 20;
	} else if (hash == MLN_RSA_HASH_SHA256) {
		MlnMemcpy(info, "\x30\x31\x30\x0D", 4);
		MlnMemcpy(info + 4, s_sha256Algorithm, sizeof(s_sha256Algorithm));
		MlnMemcpy(info + 4 + sizeof(s_sha256Algorithm), "\x04\x20", 2);
		if (MlnImpl_8105713C(in->data, in->size, digest) != 0)
			return 0x22060004;
		headerSize = 19;
		digestSize = 32;
	} else {
		return 0x22060003;
	}
	infoSize = headerSize + digestSize;
	MlnMemcpy(info + headerSize, digest, digestSize);
	if (size - 1 < infoSize + 10)
		return 0x22060009;
	encoded = MlnBufferCreate(MLN_HEAP_GENERAL, size - 1, NULL);
	if (encoded == NULL)
		return 2;
	padding = size - 1 - infoSize;
	encoded->data[0] = 1;
	MlnMemset(encoded->data + 1, -1, padding - 2);
	encoded->data[padding - 1] = 0;
	MlnMemcpy(encoded->data + padding, info, infoSize);
	*out = encoded;
	return 0;
}

/* RSASSA-PKCS1-v1_5 signature of a message under the private key */
unsigned int MlnImpl_81056A74(void *rsa, int hash, const MlnBuffer *in, MlnBuffer **out)
{
	MlnRsaKey *key = rsa;
	unsigned char bytes[256];
	unsigned char value[256];
	MlnRsaPublic exponent;
	MlnBuffer *encoded;
	MlnBuffer *signature;
	unsigned int *number = NULL;
	unsigned char *p;
	unsigned int words;
	int size;
	int length;
	unsigned int ret;

	size = MlnRsaModulusSize(key);
	ret = MlnRsaPkcs1Encode(size, hash, in, &encoded);
	if (ret != 0)
		return ret;
	ret = MlnImpl_810553F4(encoded, key->words, &number);
	if (ret != 0)
		goto out;
	if (key->prime1 != NULL && key->prime2 != NULL && key->exponent1 != NULL &&
	    key->exponent2 != NULL && key->coefficient != NULL) {
		if (key->bits == 512)
			ret = MarlinRsaSignPrivate512(value, number, &key->privateKey);
		else if (key->bits == 1024)
			ret = MarlinRsaSignPrivate1024(value, number, &key->privateKey);
		else
			ret = MarlinRsaSignPrivate2048(value, number, &key->privateKey);
	} else {
		if (key->privateExponent == NULL) {
			ret = 0x22060002;
			goto out;
		}
		exponent.modulus = key->modulus;
		exponent.publicExponent = key->privateExponent;
		if (key->bits == 512)
			ret = MarlinRsaSignExponent512(value, number, &exponent);
		else if (key->bits == 1024)
			ret = MarlinRsaSignExponent1024(value, number, &exponent);
		else
			ret = MarlinRsaSignExponent2048(value, number, &exponent);
	}
	if (ret != 0) {
		ret = 0x22060002;
		goto out;
	}
	words = key->words;
	MarlinEncodeIntegerWords(bytes, sizeof(bytes), (const unsigned int *)value, words);
	p = bytes;
	length = words * 4;
	while (length > 0 && *p == 0) {
		length--;
		p++;
	}
	if (length > size) {
		ret = 0x22060008;
		goto out;
	}
	signature = MlnBufferCreateFilled(MLN_HEAP_GENERAL, size, 0);
	if (signature == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemcpy(signature->data + size - length, p, length);
	ret = 0;
	*out = signature;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
	if (number != NULL)
		MlnFree(MLN_HEAP_GENERAL, number);
	return ret;
}

/* RSASSA-PKCS1-v1_5 verification of a message's signature under the public key */
int MlnImpl_81056F0C(void *rsa, int hash, const MlnBuffer *message, const MlnBuffer *signature)
{
	MlnRsaKey *key = rsa;
	unsigned char sha1[20];
	unsigned char sha256[32];
	MlnRsaOctets octets;
	MlnRsaOctets digest;
	MlnRsaVerifyFunc verify;
	int ret;

	if (hash == MLN_RSA_HASH_SHA1) {
		ret = MlnImpl_810570A8(message->data, message->size, sha1);
		if (ret != 0)
			return ret;
		digest.size = sizeof(sha1);
		digest.data = sha1;
	} else if (hash == MLN_RSA_HASH_SHA256) {
		ret = MlnImpl_8105713C(message->data, message->size, sha256);
		if (ret != 0)
			return ret;
		digest.size = sizeof(sha256);
		digest.data = sha256;
	} else {
		return 0x22060003;
	}
	octets.data = signature->data;
	octets.size = signature->size;
	if (key->bits == 512)
		verify = GcAuthRsaVerify512;
	else if (key->bits == 1024)
		verify = GcAuthRsaVerify1024;
	else
		verify = GcAuthRsaVerify2048;
	if (hash == MLN_RSA_HASH_SHA1)
		ret = verify(&octets, &digest, &key->publicKey, 5);
	else if (hash == MLN_RSA_HASH_SHA256)
		ret = verify(&octets, &digest, &key->publicKey, 11);
	else
		return 0x22060003;
	if (ret != 0)
		return 0x22060003;
	return 0;
}
