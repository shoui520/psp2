/*
 * RSA-2048 (file name unknown): signatures made and checked in the kernel,
 * on a bignum library of 32-bit words.
 */

#include <marlin_hci/rsa.h>
#include <marlin_hci/rsa_arithmetic.h>
#include "post_ss_mgr_internal.h"

#define RSA2048_WORDS	0x40	/* names unknown */
#define RSA2048_BYTES	0x100

int rsaEncodeDigest(unsigned char *output, int size, const MarlinRsaData *digest, int algorithm);

/* the source of random bytes the RSA code uses */
int rsaNullRandom(void *buf, SceSize size);
int (*g_rsaRandom)(void *buf, SceSize size) = rsaNullRandom;	/* name unknown */

int rsaCreateSignature(void *signature, const void *hash, const void *key,
		       int words, SceSize size, int type)	/* name unknown */
{
	MarlinRsaData *output = signature;
	const MarlinRsaData *digest = hash;
	const MarlinRsaPrivateKey *privKey = key;
	unsigned int block[64];
	unsigned char *front;
	unsigned char *back;
	unsigned int *first;
	unsigned int *last;
	int half;
	int lastByteOffset;
	int remaining;
	int result;

	if (rsaEncodeDigest((unsigned char *)block, size, digest, type) == -1)
		return -1;
	lastByteOffset = 4 * (words - 1);
	front = (unsigned char *)block;
	half = words / 2;
	result = 0;
	back = front + lastByteOffset;
	if (half > 0) {
		last = block + words;
		first = block;
		remaining = half;
		do {
			unsigned int high = back[3] | (back[2] << 8) |
				(back[1] << 16) | ((unsigned int)back[0] << 24);
			unsigned int low = front[3] | (front[2] << 8) |
				(front[1] << 16) | ((unsigned int)front[0] << 24);

			back -= 4;
			front += 4;
			*first++ = high;
			--remaining;
			*--last = low;
		} while (remaining != 0);
	}
	if (MarlinRsaPrivateOperation(block, block, privKey, words) == -1)
		return -1;
	front = output->data;
	back = front + lastByteOffset;
	if (half > 0) {
		last = block + words;
		first = block;
		remaining = half;
		do {
			unsigned int high = *--last;
			unsigned int low = *first++;

			front[3] = high;
			front[0] = high >> 24;
			front[1] = high >> 16;
			front[2] = high >> 8;
			back[0] = low >> 24;
			--remaining;
			back[1] = low >> 16;
			front += 4;
			back[2] = low >> 8;
			back[3] = low;
			back -= 4;
		} while (remaining != 0);
	}
	output->size = size;
	return result;
}

int SceSblPostSsMgrForDriver_07BAD056(void *signature, const void *hash, const void *key, int type)
{
	return rsaCreateSignature(signature, hash, key, RSA2048_WORDS, RSA2048_BYTES, type);
}

int rsaVerifySignature(const void *signature, const void *hash, const void *key,
		       int words, SceSize size, int type)	/* name unknown */
{
	const MarlinRsaData *sig = signature;
	const MarlinRsaData *digest = hash;
	const MarlinRsaExponentKey *pubKey = key;
	unsigned int decoded[64];
	unsigned int encoded[64];
	unsigned char *front;
	unsigned char *back;
	unsigned int *first;
	unsigned int *last;
	int half;
	int lastByteOffset;
	int remaining;
	int index;
	int result;

	if (sig->size != (int)size)
		return -1;
	front = sig->data;
	lastByteOffset = 4 * (words - 1);
	half = words / 2;
	result = 0;
	back = front + lastByteOffset;
	if (half > 0) {
		first = encoded;
		last = encoded + words;
		remaining = half;
		do {
			unsigned int high = back[3] | (back[2] << 8) |
				(back[1] << 16) | ((unsigned int)back[0] << 24);
			unsigned int low = front[3] | (front[2] << 8) |
				(front[1] << 16) | ((unsigned int)front[0] << 24);

			back -= 4;
			front += 4;
			*first++ = high;
			--remaining;
			*--last = low;
		} while (remaining != 0);
	}
	if (MarlinRsaExponentOperation(decoded, encoded, pubKey, words) == -1)
		return -1;
	front = (unsigned char *)decoded;
	back = front + lastByteOffset;
	if (half > 0) {
		last = decoded + words;
		first = decoded;
		remaining = half;
		do {
			unsigned int high = *--last;
			unsigned int low = *first++;

			front[3] = high;
			front[0] = high >> 24;
			front[1] = high >> 16;
			front[2] = high >> 8;
			back[0] = low >> 24;
			--remaining;
			back[1] = low >> 16;
			front += 4;
			back[2] = low >> 8;
			back[3] = low;
			back -= 4;
		} while (remaining != 0);
	}
	if (rsaEncodeDigest((unsigned char *)encoded, size, digest, type) == -1)
		return -1;
	first = decoded;
	index = result;
	if (words <= 0)
		return -1;
	do {
		if (*first != encoded[index])
			break;
		++index;
		++first;
	} while (index < words);
	if (words <= 0 || words != index)
		return -1;
	return result;
}

int SceSblPostSsMgrForDriver_3DB69911(const void *signature, const void *hash, const void *key,
				      int type)
{
	return rsaVerifySignature(signature, hash, key, RSA2048_WORDS, RSA2048_BYTES, type);
}

int rsaEncodeDigest(unsigned char *output, int size, const MarlinRsaData *digest, int algorithm)
{
	/* DER DigestInfo: SEQUENCE, AlgorithmIdentifier/OID, OCTET STRING. */
	unsigned char digestInfo[19];
	unsigned char *destination;
	const unsigned char *source;
	int digestSize;
	int prefixSize;
	int paddingSize;
	int remaining;
	int index;
	int result = 0;

	switch (algorithm) {
	case 2: /* MD2, 1.2.840.113549.2.2 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x20;
		digestInfo[3] = 0x0c;
		digestInfo[5] = 0x08;
		digestInfo[6] = 0x2a;
		digestInfo[9] = 0x86;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestSize = 16;
		digestInfo[10] = 0xf7;
		prefixSize = 18;
		digestInfo[11] = 0x0d;
		digestInfo[12] = 0x02;
		digestInfo[13] = 0x02;
		digestInfo[14] = 0x05;
		digestInfo[15] = 0x00;
		digestInfo[16] = 0x04;
		digestInfo[17] = 16;
		break;
	case 4: /* MD5, 1.2.840.113549.2.5 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x20;
		digestInfo[3] = 0x0c;
		digestInfo[5] = 0x08;
		digestInfo[6] = 0x2a;
		digestInfo[9] = 0x86;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestSize = 16;
		digestInfo[10] = 0xf7;
		prefixSize = 18;
		digestInfo[11] = 0x0d;
		digestInfo[12] = 0x02;
		digestInfo[13] = 0x05;
		digestInfo[14] = 0x05;
		digestInfo[15] = 0x00;
		digestInfo[16] = 0x04;
		digestInfo[17] = 16;
		break;
	case 5: /* SHA-1, 1.3.14.3.2.26 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x21;
		digestInfo[3] = 0x09;
		digestInfo[5] = 0x05;
		digestInfo[6] = 0x2b;
		digestInfo[7] = 0x0e;
		digestSize = 20;
		digestInfo[8] = 0x03;
		prefixSize = 15;
		digestInfo[9] = 0x02;
		digestInfo[10] = 0x1a;
		digestInfo[11] = 0x05;
		digestInfo[12] = 0x00;
		digestInfo[13] = 0x04;
		digestInfo[14] = 20;
		break;
	case 11: /* SHA-256, 2.16.840.1.101.3.4.2.1 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x31;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[14] = 0x01;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestSize = 32;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestInfo[13] = 0x02;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 32;
		break;
	case 12: /* SHA-384, 2.16.840.1.101.3.4.2.2 */
		digestInfo[4] = 0x06;
		digestInfo[0] = 0x30;
		digestInfo[1] = 0x41;
		digestInfo[2] = 0x30;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestSize = 48;
		digestInfo[13] = 0x02;
		digestInfo[14] = 0x02;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 48;
		break;
	case 13: /* SHA-512, 2.16.840.1.101.3.4.2.3 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x51;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestSize = 64;
		digestInfo[14] = 0x03;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestInfo[13] = 0x02;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 64;
		break;
	case 14: /* SHA-224, 2.16.840.1.101.3.4.2.4 */
		digestInfo[0] = 0x30;
		digestInfo[2] = 0x30;
		digestInfo[4] = 0x06;
		digestInfo[1] = 0x2d;
		digestInfo[3] = 0x0d;
		digestInfo[5] = 0x09;
		digestInfo[6] = 0x60;
		digestInfo[7] = 0x86;
		digestInfo[8] = 0x48;
		digestInfo[9] = 0x01;
		digestInfo[10] = 0x65;
		digestSize = 28;
		digestInfo[11] = 0x03;
		prefixSize = 19;
		digestInfo[12] = 0x04;
		digestInfo[13] = 0x02;
		digestInfo[14] = 0x04;
		digestInfo[15] = 0x05;
		digestInfo[16] = 0x00;
		digestInfo[17] = 0x04;
		digestInfo[18] = 28;
		break;
	default:
		return -1;
	}
	if (digest->size != digestSize)
		return -1;
	paddingSize = size - (prefixSize + digestSize) - 3;
	if (size < prefixSize + digestSize + 11)
		return -1;
	output[0] = 0;
	output[1] = 1;
	destination = output + 2;
	if (paddingSize > 0) {
		unsigned char *padding = destination;

		remaining = paddingSize;
		do {
			--remaining;
			*padding++ = 0xff;
		} while (remaining != 0);
	}
	destination += paddingSize;
	output[paddingSize + 2] = 0;
	++destination;
	if (prefixSize > 0) {
		unsigned char *prefix = destination;

		source = digestInfo;
		remaining = prefixSize;
		do {
			unsigned char byte = *source++;

			--remaining;
			*prefix++ = byte;
		} while (remaining != 0);
	}
	if (digestSize > 0) {
		destination += prefixSize;
		index = 0;
		do {
			*destination++ = digest->data[index];
			++index;
		} while (index < digestSize);
	}
	return result;
}

int rsaNullRandom(void *buf, SceSize size)	/* name unknown */
{
	(void)buf;
	(void)size;
	return 0;
}

int sceSblRsaSetRandom(int (*random)(void *buf, SceSize size))
{
	g_rsaRandom = random;
	return 0;
}

/* a big-endian number of size bytes as words, lowest first, zero-padded to count words */
int postSsCopyKey(void *dst, SceSize count, const void *src, SceSize size)
{
	SceUInt32 *word = (SceUInt32 *)dst - 1;
	const SceUInt8 *start = src;
	const SceUInt8 *end = start + size;
	SceUInt32 shift = 32;

	if ((int)(count * 4) < (int)size)
		start = end - count * 4;
	for (; start < end; end--) {
		if (shift == 32) {
			shift = 0;
			*++word = 0;
		}
		*word |= (SceUInt32)end[-1] << (shift & 0xff);
		shift += 8;
	}
	while (++word < (SceUInt32 *)dst + count)
		*word = 0;
	return 0;
}

__attribute__((cold)) int MarlinRsaExponentOperation(unsigned int *output, const unsigned int *input,
			       const MarlinRsaExponentKey *key, int count)
{
	unsigned int temporary[128];
	unsigned int inverse;
	unsigned int mask;
	unsigned int bit;
	int index;
	int remaining;

	if (count <= 0 || count > 64)
		return -1;
	if ((key->modulus[count - 1] & 0x80000000) == 0)
		return -1;
	if ((key->modulus[0] & 1) == 0)
		return -1;
	if (!MarlinIntegerWordsGreater(key->modulus, input, count))
		return -1;
	if (count > 0) {
		index = 0;
		remaining = count;
		do {
			unsigned int word = *input++;

			remaining--;
			temporary[index] = 0;
			temporary[count + index] = word;
			index++;
		} while (remaining != 0);
	}
	inverse = 1;
	for (mask = 2; mask != 0; mask *= 2) {
		bit = (inverse * -key->modulus[0]) & mask;
		inverse |= bit;
	}
	MarlinRemainderIntegerWords(temporary, temporary, 2 * count,
				    key->modulus, count);
	MarlinMontgomeryPower(temporary, temporary, key->exponent,
			      key->modulus, count, inverse);
	MarlinMontgomeryReduce(output, temporary, count,
			       key->modulus, count, inverse);
	return 0;
}

__attribute__((cold)) int MarlinRsaPrivateOperation(unsigned int *output, const unsigned int *input,
			      const MarlinRsaPrivateKey *key, int count)
{
	unsigned int residueP[32];
	unsigned int residueQ[32];
	unsigned int product[64];
	int half;
	unsigned int *temporary;
	unsigned int inverseP;
	unsigned int inverseQ;
	unsigned int maskP;
	unsigned int maskQ;
	unsigned int bit;
	unsigned int carry;
	int index;

	if (count <= 0 || count > 64)
		return -1;
	half = count / 2;
	temporary = product + half;
	MarlinMultiplyIntegerWords(product, key->primeP, half, key->primeQ, half);
	if ((product[count - 1] & key->primeP[half - 1] &
	     key->primeQ[half - 1] & 0x80000000) == 0)
		return -1;
	if ((product[0] & key->primeP[0] & key->primeQ[0] & 1) == 0)
		return -1;
	if (!MarlinIntegerWordsGreater(product, input, count))
		return -1;
	for (index = 0; index < half; index++)
		product[index] = 0;
	inverseQ = 1;
	maskQ = 2;
	inverseP = 1;
	maskP = 2;
	do {
		bit = (inverseQ * -key->primeQ[0]) & maskQ;
		maskQ *= 2;
		inverseQ |= bit;
	} while (maskQ != 0);
	MarlinRemainderIntegerWords(temporary, input, count, key->primeQ, half);
	MarlinRemainderIntegerWords(temporary, product, count, key->primeQ, half);
	MarlinMontgomeryPower(residueQ, temporary, key->exponentQ,
			      key->primeQ, half, inverseQ);
	MarlinMontgomeryReduce(residueQ, residueQ, half, key->primeQ,
			       half, inverseQ);
	do {
		bit = (inverseP * -key->primeP[0]) & maskP;
		maskP *= 2;
		inverseP |= bit;
	} while (maskP != 0);
	MarlinRemainderIntegerWords(temporary, input, count, key->primeP, half);
	MarlinRemainderIntegerWords(temporary, product, count, key->primeP, half);
	MarlinMontgomeryPower(residueP, temporary, key->exponentP,
			      key->primeP, half, inverseP);
	for (index = 0; index < half; index++)
		temporary[index] = residueQ[index];
	MarlinRemainderIntegerWords(temporary, product, count, key->primeP, half);
	if (MarlinSubtractIntegerWords(residueP, residueP, temporary, half, 0))
		MarlinAddIntegerWords(residueP, residueP, key->primeP, half, 0);
	MarlinMontgomeryMultiply(residueP, residueP, key->coefficient,
				 key->primeP, half, inverseP);
	MarlinMultiplyIntegerWords(product, residueP, half, key->primeQ, half);
	carry = MarlinAddIntegerWords(output, residueQ, product, half, 0);
	for (index = half; index < count; index++) {
		unsigned int word = product[index];
		unsigned int sum = carry + word;

		output[index] = sum;
		carry = (word & ~sum) >> 31;
	}
	return 0;
}

__attribute__((cold)) unsigned int MarlinAddIntegerWords(unsigned int *output,
				   const unsigned int *first,
				   const unsigned int *second,
				   int count, unsigned int carry)
{
	unsigned int result = carry & 1;
	int offset = 0;
	int index = 0;

	while (index < count) {
		unsigned int left = *(unsigned int *)((unsigned char *)first + offset);
		unsigned int right;
		unsigned int sum;

		index++;
		right = *(unsigned int *)((unsigned char *)second + offset);
		sum = left + right + result;
		result = (((left | right) & ~sum) | (left & right)) >> 31;
		*(unsigned int *)((unsigned char *)output + offset) = sum;
		offset += 4;
	}
	return result;
}

__attribute__((cold)) unsigned int MarlinSubtractIntegerWords(unsigned int *output,
					const unsigned int *first,
					const unsigned int *second, int count,
					unsigned int borrow)
{
	unsigned int result = borrow & 1;
	int offset = 0;
	int index = 0;

	while (index < count) {
		unsigned int right = *(unsigned int *)((unsigned char *)second + offset);
		unsigned int left;
		unsigned int difference;

		index++;
		left = *(unsigned int *)((unsigned char *)first + offset);
		difference = left - right - result;
		result = ((right & ~left) | ((right | ~left) & difference)) >> 31;
		*(unsigned int *)((unsigned char *)output + offset) = difference;
		offset += 4;
	}
	return result;
}

__attribute__((cold, noinline, noclone)) void MarlinMultiplyIntegerWords(unsigned int *output,
				const unsigned int *first, int firstCount,
				const unsigned int *second, int secondCount)
{
	unsigned int product[65];
	unsigned int accumulator[128];
	unsigned int *cursor;
	unsigned int *copy;
	int index;
	int total;
	int productCount;
	int wordCount;
	unsigned int carry;

	if (firstCount > 0 && firstCount <= 64 &&
	    secondCount > 0 && secondCount <= 64) {
		productCount = firstCount + 1;
		wordCount = firstCount;
		total = firstCount + secondCount;
		carry = 0;
		cursor = accumulator;
		copy = accumulator;
		index = total;
		if (index > 0) {
			do {
				index--;
				*copy++ = carry;
			} while (index != 0);
		}
		index = secondCount;
		if (index > 0) {
			do {
				MarlinMultiplyIntegerByWord(product, first,
							    wordCount, *second++);
				MarlinAddIntegerWords(cursor, cursor, product,
						      productCount, carry);
				index--;
				cursor++;
			} while (index != 0);
		}
		copy = accumulator;
		index = total;
		if (index > 0) {
			do {
				index--;
				*output++ = *copy++;
			} while (index != 0);
		}
	}
}

__attribute__((cold)) void MarlinRemainderIntegerWords(unsigned int *output,
				 const unsigned int *input, int inputCount,
				 const unsigned int *modulus, int modulusCount)
{
	unsigned int normalizedModulus[65];
	unsigned int product[66];
	unsigned int remainder[129];
	unsigned int inputZeros;
	unsigned int modulusZeros;
	unsigned int shift;
	unsigned int top;
	int zeros;
	int count;
	int usedInput;
	int less;
	int index;

	if (inputCount <= 0 || inputCount > 128 ||
	    modulusCount <= 0 || modulusCount > 64)
		return;
	inputZeros = 0;
	zeros = 32;
	for (index = inputCount - 1; index >= 0 && zeros == 32; index--) {
		unsigned int word = input[index];
		int upperHalf = ((int)-(word >> 16) >> 16) & 16;
		int upperByte;
		int upperNibble;
		int upperPair;

		word >>= upperHalf;
		upperByte = ((int)(word - 256) >> 16) & 8;
		word <<= upperByte;
		upperNibble = ((int)(word - 4096) >> 16) & 4;
		word <<= upperNibble;
		upperPair = ((int)(word - 0x4000) >> 16) & 2;
		word <<= upperPair;
		zeros = 16 - upperHalf + upperByte + upperNibble + upperPair
			- ((word >> 14) & ~(word >> 15)) + 2;
		inputZeros += zeros;
	}
	modulusZeros = 0;
	zeros = 32;
	for (index = modulusCount - 1; index >= 0 && zeros == 32; index--) {
		unsigned int word = modulus[index];
		int upperHalf = ((int)-(word >> 16) >> 16) & 16;
		int upperByte;
		int upperNibble;
		int upperPair;

		word >>= upperHalf;
		upperByte = ((int)(word - 256) >> 16) & 8;
		word <<= upperByte;
		upperNibble = ((int)(word - 4096) >> 16) & 4;
		word <<= upperNibble;
		upperPair = ((int)(word - 0x4000) >> 16) & 2;
		word <<= upperPair;
		zeros = 16 - upperHalf + upperByte + upperNibble + upperPair
			- ((word >> 14) & ~(word >> 15)) + 2;
		modulusZeros += zeros;
	}
	count = modulusCount - (modulusZeros >> 5);
	shift = modulusZeros & 31;
	if (count == 0)
		return;
	usedInput = inputCount - (inputZeros >> 5);
	if (count == usedInput)
		less = MarlinIntegerWordsGreater(modulus, input, count);
	else
		less = count > usedInput;
	if (less == 1) {
		for (index = 0; index < usedInput; index++)
			output[index] = input[index];
		for (index = usedInput; index < modulusCount; index++)
			output[index] = 0;
		return;
	}
	normalizedModulus[count] = 0;
	MarlinShiftLeftIntegerWords(normalizedModulus, modulus, count, shift);
	remainder[usedInput] = 0;
	if ((inputZeros & 31) < shift)
		remainder[usedInput] = input[usedInput - 1] >> (32 - shift);
	MarlinShiftLeftIntegerWords(remainder, input, usedInput, shift);
	top = normalizedModulus[count - 1];
	for (index = usedInput; index >= count; index--) {
		unsigned int high = remainder[index];
		unsigned int quotient;
		unsigned int borrow;
		unsigned int *window = remainder + index - count;

		if (high < top) {
			unsigned int low = remainder[index - 1];
			int bit;

			for (bit = 0; bit < 32; bit++) {
				unsigned int previous = high;

				high = (low >> 31) | (high * 2);
				low *= 2;
				if ((high | (unsigned int)((int)previous >> 31)) >= top) {
					high -= top;
					low |= 1;
				}
			}
			quotient = low;
		} else {
			quotient = -1;
		}
		MarlinMultiplyIntegerByWord(product, normalizedModulus, count, quotient);
		borrow = MarlinSubtractIntegerWords(window, window, product, count + 1, 0);
		while (borrow != 0)
			borrow ^= MarlinAddIntegerWords(window, window,
						       normalizedModulus, count + 1, 0);
	}
	MarlinShiftRightIntegerWords(output, remainder, modulusCount, shift);
	for (index = count; index < modulusCount; index++)
		output[index] = 0;
}

__attribute__((cold)) int MarlinIntegerWordsGreater(const unsigned int *first,
			      const unsigned int *second, int count)
{
	const unsigned int *end = first + count;
	int result = 0;

	second += count;
	while (end > first && *--end == *--second)
		;
	if (count > 0) {
		if (*end > *second)
			result = 1;
	}
	return result;
}

__attribute__((cold)) void MarlinMontgomeryReduce(unsigned int *output, const unsigned int *input,
			    int inputCount, const unsigned int *modulus, int count,
			    unsigned int inverse)
{
	unsigned int carries[64];
	unsigned int product[65];
	unsigned int accumulator[128];
	unsigned int *high;
	unsigned int carry;
	int index;

	if (count <= 0 || count > 64 ||
	    inputCount < count || inputCount > 2 * count)
		return;
	high = accumulator + count;
	for (index = 0; index < count; index++) {
		accumulator[index] = input[index];
		high[index] = 0;
	}
	for (index = count; index < inputCount; index++)
		accumulator[index] = input[index];
	for (index = 0; index < count; index++) {
		MarlinMultiplyIntegerByWord(product, modulus, count,
					    accumulator[index] * inverse);
		carry = MarlinAddIntegerWords(accumulator + index,
			accumulator + index, product, count, 0);
		carries[index] = product[count] + carry;
	}
	carry = MarlinAddIntegerWords(high, high, carries, count, 0);
	if (carry != 0 || !MarlinIntegerWordsGreater(modulus, high, count))
		MarlinSubtractIntegerWords(high, high, modulus, count, 0);
	for (index = 0; index < count; index++)
		*output++ = high[index];
}

__attribute__((cold)) void MarlinMontgomeryMultiply(unsigned int *output, const unsigned int *first,
			      const unsigned int *second, const unsigned int *modulus,
			      int count, unsigned int inverse)
{
	unsigned int accumulator[65];
	unsigned int product[65];
	unsigned int correction[65];
	unsigned int carry;
	unsigned int highCarry;
	const unsigned int *firstCursor;
	int index;

	if (count <= 0 || count > 64)
		return;
	for (index = 0; index < count + 1; index++)
		accumulator[index] = 0;
	firstCursor = first;
	for (index = 0; index < count; index++) {
		MarlinMultiplyIntegerByWord(correction, modulus, count,
			(accumulator[0] + *firstCursor * *second) * inverse);
		MarlinMultiplyIntegerByWord(product, second, count, *firstCursor);
		carry = MarlinAddIntegerWords(correction, correction, product,
					      count + 1, 0);
		highCarry = MarlinAddIntegerWords(accumulator, accumulator + 1,
			correction + 1, count,
			(correction[0] | accumulator[0]) != 0);
		accumulator[count] = carry + highCarry;
		firstCursor++;
	}
	if (carry + highCarry != 0 ||
	    !MarlinIntegerWordsGreater(modulus, accumulator, count))
		MarlinSubtractIntegerWords(accumulator, accumulator, modulus,
					   count, 0);
	{
		unsigned int *write = output;
		const unsigned int *read = accumulator;
		int words = count;

		do {
			unsigned int word = *read++;

			--words;
			*write++ = word;
		} while (words != 0);
	}
}

__attribute__((cold)) void MarlinMontgomeryPower(unsigned int *output, const unsigned int *input,
			   const unsigned int *exponent, const unsigned int *modulus,
			   int count, unsigned int inverse)
{
	unsigned int shiftedExponent[64];
	unsigned int accumulator[65];
	unsigned int square[128];
	unsigned int top;
	unsigned int carry;
	unsigned int leadingZeros;
	unsigned int shift;
	unsigned int *cursor;
	int usedWords;
	int squareCount;
	int zeros;
	int index;

	if (count <= 0 || count > 64)
		return;
	squareCount = 2 * count;
	top = modulus[count - 1];
	carry = 0;
	for (index = 0; index < count; index++) {
		unsigned int word = modulus[index];
		unsigned int negative = -word - carry;

		accumulator[index] = negative;
		carry = (word | negative) >> 31;
	}
	if ((top & 0x80000000) == 0)
		MarlinRemainderIntegerWords(accumulator, accumulator, count,
					    modulus, count);
	leadingZeros = 0;
	zeros = 32;
	for (index = count - 1; index >= 0 && zeros == 32; index--) {
		unsigned int word = exponent[index];
		int upperHalf = ((int)-(word >> 16) >> 16) & 16;
		int upperByte;
		int upperNibble;
		int upperPair;

		word >>= upperHalf;
		upperByte = ((int)(word - 256) >> 16) & 8;
		word <<= upperByte;
		upperNibble = ((int)(word - 4096) >> 16) & 4;
		word <<= upperNibble;
		upperPair = ((int)(word - 0x4000) >> 16) & 2;
		word <<= upperPair;
		zeros = 16 - upperHalf;
		zeros += upperByte;
		zeros += upperNibble;
		zeros += upperPair;
		zeros -= (word >> 14) & ~(word >> 15);
		zeros += 2;
		leadingZeros += zeros;
	}
	usedWords = count - (leadingZeros >> 5);
	shift = leadingZeros & 31;
	MarlinShiftLeftIntegerWords(shiftedExponent, exponent, usedWords, shift);
	cursor = shiftedExponent;
	while (usedWords > 0) {
		unsigned int bit = 0;

		for (index = 0; index < usedWords; index++) {
			unsigned int word = cursor[index];

			cursor[index] = (bit & 1) | (word * 2);
			bit = word >> 31;
		}
		shift++;
		if (shift >= 32) {
			usedWords--;
			cursor++;
			shift &= 31;
		}
		MarlinSquareIntegerWords(square, accumulator, count);
		MarlinMontgomeryReduce(accumulator, square, squareCount,
				       modulus, count, inverse);
		if (bit != 0)
			MarlinMontgomeryMultiply(accumulator, accumulator, input,
						 modulus, count, inverse);
	}
	for (index = 0; index < count; index++)
		*output++ = accumulator[index];
}

__attribute__((cold)) void MarlinShiftLeftIntegerWords(unsigned int *output,
				 const unsigned int *input, int count,
				 unsigned int shift)
{
	unsigned int carry;

	shift &= 63;
	if (shift == 0) {
		if (count > 0) {
			do {
				unsigned int word = *input++;

				count--;
				*output++ = word;
			} while (count != 0);
		}
	} else {
		carry = 0;
		if (count > 0) {
			do {
				unsigned int word = *input++;

				count--;
				*output++ = (word << shift) | carry;
				carry = word >> (32 - shift);
			} while (count != 0);
		}
	}
}

__attribute__((cold)) void MarlinShiftRightIntegerWords(unsigned int *output,
				  const unsigned int *input, int count,
				  unsigned int shift)
{
	unsigned int carry;

	shift &= 63;
	if (shift == 0) {
		if (count > 0) {
			do {
				unsigned int word = *input++;

				count--;
				*output++ = word;
			} while (count != 0);
		}
	} else {
		count--;
		carry = 0;
		if (count >= 0) {
			input += count;
			output += count;
			do {
				unsigned int word = *input--;

				count--;
				*output-- = (word >> shift) | carry;
				carry = word << (32 - shift);
			} while (count >= 0);
		}
	}
}

__attribute__((cold)) void MarlinMultiplyIntegerByWord(unsigned int *output,
				 const unsigned int *input, int count,
				 unsigned int factor)
{
	unsigned int *cursor = output;
	unsigned int carry;
	unsigned int lowFactor;
	unsigned int highFactor;
	int index;

	if (count <= 0)
		return;
	carry = 0;
	index = 0;
	lowFactor = factor & 0xffff;
	highFactor = factor >> 16;
	for (index = 0; index < count; index++) {
		unsigned int word = *input++;
		unsigned short lowWord = word;
		unsigned int lowProduct = lowWord * lowFactor;
		unsigned int crossFirst = (word >> 16) * lowFactor;
		unsigned int crossSecond = lowWord * highFactor;
		unsigned int highProduct = (word >> 16) * highFactor;
		unsigned int middle = crossFirst + (lowProduct >> 16);
		unsigned int low = lowProduct + (crossFirst << 16)
			+ (crossSecond << 16);
		unsigned int sum = low + carry;

		*cursor++ = sum;
		carry = highProduct + (middle >> 16)
			+ ((crossSecond + (middle & 0xffff)) >> 16)
			+ (((low & carry) | ((low | carry) & ~sum)) >> 31);
	}
	output[index] = carry;
}

__attribute__((cold)) void MarlinSquareIntegerWords(unsigned int *output,
			      const unsigned int *input, int count)
{
	unsigned int product[64];
	unsigned int diagonal[129];
	unsigned int crossProducts[128];
	unsigned int *cursor;
	const unsigned int *first;
	const unsigned int *second;
	int index;
	int remaining;
	int total;

	if (count <= 0 || count > 64)
		return;
	remaining = count - 1;
	cursor = diagonal;
	first = input;
	for (index = count; index > 0; index--) {
		unsigned int word = *first++;
		unsigned short low = word;
		unsigned int high = word >> 16;
		unsigned int lowSquare = low * low;
		unsigned int cross = low * high;
		unsigned int middle = cross + (lowSquare >> 16);

		cursor[0] = lowSquare + (cross << 17);
		cursor[1] = high * high + (middle >> 16)
			+ (((unsigned short)middle + cross) >> 16);
		cursor += 2;
	}
	total = 2 * count;
	cursor = crossProducts;
	index = total;
	if (index > 0) {
		do {
			index--;
			*cursor++ = 0;
		} while (index != 0);
	}
	cursor = crossProducts + 1;
	first = input;
	second = input + 1;
	if (remaining > 0) {
		do {
			MarlinMultiplyIntegerByWord(product, second, remaining, *first++);
			MarlinAddIntegerWords(cursor, cursor, product, remaining + 1, 0);
			remaining--;
			cursor += 2;
			second++;
		} while (remaining != 0);
	}
	MarlinShiftLeftIntegerWords(crossProducts + 1, crossProducts + 1,
				    total - 1, 1);
	MarlinAddIntegerWords(output, diagonal, crossProducts, total, 0);
}
