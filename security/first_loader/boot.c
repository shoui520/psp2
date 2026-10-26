/*
 * first_loader's program, run at 0x5c000 after reset copies it there: it brings up the
 * Cmep's keyrings, reads second_loader from the eMMC (Sdif) or the SD card, decrypts and
 * checks it, and starts it. It uses the Cmep control block (0xe0000000), the keyring
 * controller (0xe0030000), the bignum unit (0xe0040000), Bigmac (0xe0050000), Sdif
 * (0xe0b00000), GPIO 0 (0xe20a0000) and Pervasive (0xe3100000).
 *
 * The program is incomplete. These helpers are compiled but are not linked into
 * the ROM until the program's startup and its callers are implemented.
 */

#ifndef FL_BUILD_DATE
#error "FL_BUILD_DATE (build, YYYYMMDD in hex) not set"
#endif

/* code that differs between the builds: FL_SINCE(0x20110616) */
#define FL_SINCE(date)	(FL_BUILD_DATE >= (date))

struct SecondLoaderHeader {
	unsigned int magic;
	unsigned int imageSize;
	unsigned int firstPartSize;
	unsigned int secondPartSize;
	unsigned int decodedSize;
	unsigned short keyIndex;
	unsigned short signingKeyIndex;
	unsigned char signedHeaderData[0x28];
};

struct BigmacRegisters {
	volatile unsigned int input;
	volatile unsigned int output;
	volatile unsigned int size;
	volatile unsigned int mode;
	volatile unsigned int keySlot;
	volatile unsigned int parameter;
	unsigned int reserved0;
	volatile unsigned int start;
	unsigned int reserved1;
	volatile unsigned int status;
	unsigned char reserved2[0x200 - 0x28];
	volatile unsigned int key[8];
};

struct BignumRegisters {
	volatile unsigned int work[66];
	volatile unsigned int input[66];
	unsigned char reserved0[0x400 - 0x210];
	volatile unsigned int modulus[66];
	volatile unsigned int result[66];
	unsigned char reserved1[0x800 - 0x610];
	volatile unsigned int command;
	volatile unsigned int status;
	volatile unsigned int exponent;
};

struct KeyPolicy {
	unsigned int bank2;
	unsigned int bank6;
	unsigned int reserved[2];
	unsigned char bank0[16];
	unsigned char bank1[16];
	unsigned char bank3[32];
	unsigned char bank4[16];
	unsigned char bank5[16];
	unsigned char bank7[32];
};

#define BIGMAC ((struct BigmacRegisters *)0xe0050000UL)
#define BIGNUM ((struct BignumRegisters *)0xe0040000UL)
#define BIGNUM_ACK (*(volatile unsigned int *)0xe0020020UL)
#define KEYRING_COMMAND (*(volatile unsigned int *)0xe0030024UL)
#define BIGMAC_SLOT_OUTPUT 0x10000000U
#define BIGMAC_INLINE_KEY  0x00000080U
#define BIGMAC_BUSY        0x00000001U
#define BIGMAC_ERROR_MASK  0x00078000U
#define BIGNUM_BUSY        0x80000000U
#define BIGNUM_ERROR       0x02000000U
#define BIGNUM_NEED_INPUT  0x04000000U
#define BIGNUM_INPUT_READY 0x08000000U

/* Apply the image's key-slot masks and per-slot permission records. */
int applyKeyPolicy(const struct KeyPolicy *policy, const unsigned char *records,
		   unsigned int size)
{
	static const unsigned char permissionBits[8] = { 28, 27, 26, 20, 19, 18, 17, 16 };
	unsigned int i;
	unsigned int offset = 0;

	for (i = 0; i < 24; i++) {
		if (policy->bank2 & (1U << i))
			KEYRING_COMMAND = 0x1c1f0200U | i;
	}
	for (i = 0; i < 8; i++) {
		if (policy->bank6 & (1U << i))
			KEYRING_COMMAND = 0x1c1f0600U | i;
	}
	for (i = 0; i < 128; i++) {
		unsigned int bit = 1U << (i & 7);
		unsigned int byte = i >> 3;

		if ((policy->bank0[byte] & bit) && i != 10)
			KEYRING_COMMAND = 0x1c1f0000U | i;
		if (policy->bank1[byte] & bit)
			KEYRING_COMMAND = 0x1c1f0100U | i;
		if (policy->bank4[byte] & bit)
			KEYRING_COMMAND = 0x1c1f0400U | i;
		if (policy->bank5[byte] & bit)
			KEYRING_COMMAND = 0x1c1f0500U | i;
	}
	for (i = 0; i < 256; i++) {
		unsigned int bit = 1U << (i & 7);
		unsigned int byte = i >> 3;

		if (policy->bank3[byte] & bit)
			KEYRING_COMMAND = 0x1c1f0300U | i;
		if (policy->bank7[byte] & bit)
			KEYRING_COMMAND = 0x1c1f0700U | i;
	}
	while (offset + 2 < size) {
		unsigned int slot = records[offset] | ((unsigned int)records[offset + 1] << 8);
		unsigned int flags;
		unsigned int permissions = 0;

		if (slot == 0xffff)
			break;
		flags = records[offset + 2];
		offset += 3;
		if (slot == 10)
			continue;
		for (i = 0; i < 8; i++) {
			if (flags & (1U << i))
				permissions |= 1U << permissionBits[i];
		}
		KEYRING_COMMAND = (~permissions & 0x1c1f0000U) | slot;
	}
	return 0;
}

/* Submit a 2048-bit modular operation and return its big-endian result. */
int bignumPowmod(unsigned int *output, const unsigned int *input,
		 const volatile unsigned int *modulus, const unsigned int *exponent,
		 unsigned int mode)
{
	unsigned int i;
	int result = 0;

	for (i = 0; i < 64; i++)
		BIGNUM->input[i] = input[i];
	for (i = 0; i < 64; i++)
		BIGNUM->modulus[i] = modulus[i];
	BIGNUM->exponent = *exponent++;
	BIGNUM->command = 0x91000000U | (mode << 9);
	while (BIGNUM->status & BIGNUM_BUSY) {
		if (BIGNUM->status & BIGNUM_ERROR)
			break;
		if ((BIGNUM->status & BIGNUM_NEED_INPUT) &&
		    (BIGNUM->status & BIGNUM_INPUT_READY))
			BIGNUM->exponent = *exponent++;
	}
	if (BIGNUM->status & BIGNUM_ERROR) {
		result = -1;
	} else {
		for (i = 0; i < 64; i++) {
			unsigned int word = BIGNUM->result[i];

			output[63 - i] = (word << 24) | ((word & 0xff00U) << 8) |
					 ((word & 0xff0000U) >> 8) | (word >> 24);
		}
	}
	BIGNUM_ACK = BIGNUM_ACK;
	for (i = 0; i < 66; i++) {
		BIGNUM->input[i] = 0;
		BIGNUM->work[i] = 0;
		BIGNUM->result[i] = 0;
		BIGNUM->modulus[i] = 0;
	}
	return result;
}

/* Submit one operation. A value below 0x1000 identifies a hardware key slot;
 * otherwise key identifies 32 bytes to load into the engine.
 */
__attribute__((noinline))
int bigmacRun(unsigned int output, unsigned int input, unsigned int size,
	      unsigned int key, unsigned int parameter, unsigned int mode)
{
	unsigned int i;

	BIGMAC->output = output;
	BIGMAC->input = input;
	BIGMAC->size = size;
	if (parameter != 0)
		BIGMAC->parameter = parameter;
	if (output < 0x1000)
		mode |= BIGMAC_SLOT_OUTPUT;
	if (key < 0x1000) {
		BIGMAC->keySlot = key;
	} else {
		const unsigned char *bytes = (const unsigned char *)(unsigned long)key;

		for (i = 0; i < 8; i++) {
			BIGMAC->key[i] = (unsigned int)bytes[0] |
				((unsigned int)bytes[1] << 8) |
				((unsigned int)bytes[2] << 16) |
				((unsigned int)bytes[3] << 24);
			bytes += 4;
		}
		mode |= BIGMAC_INLINE_KEY;
	}
	BIGMAC->mode = mode;
	BIGMAC->start = 1;
	while (BIGMAC->status & BIGMAC_BUSY)
		;
	return (BIGMAC->status & BIGMAC_ERROR_MASK) ? -1 : 0;
}

int bigmacRunMode301(unsigned int output, unsigned int input, unsigned int size,
		     unsigned int key)
{
	return bigmacRun(output, input, size, key, 0, 0x301);
}

int bigmacRunMode10a(unsigned int output, unsigned int input, unsigned int size,
		     unsigned int key, unsigned int parameter)
{
	return bigmacRun(output, input, size, key, parameter, 0x10a);
}

int bigmacRunMode30a(unsigned int output, unsigned int input, unsigned int size,
		     unsigned int key, unsigned int parameter)
{
	return bigmacRun(output, input, size, key, parameter, 0x30a);
}

int bigmacRunMode33(unsigned int output, unsigned int input, unsigned int size,
		    unsigned int key)
{
	return bigmacRun(output, input, size, key, 0, 0x33);
}

int bigmacClearMemory(unsigned int address, unsigned int value, unsigned int size)
{
	(void)value;
	return bigmacRun(address, address, size, 0, 0, 0x0c);
}

int bigmacClearRegisters(void)
{
	BIGMAC->output = 0;
	BIGMAC->input = 0;
	BIGMAC->size = 0;
	BIGMAC->parameter = 0;
	BIGMAC->mode = 0;
	return 0;
}

/* Check the lengths and key selectors before processing the image. */
int checkSecondLoaderHeader(const struct SecondLoaderHeader *header)
{
	unsigned int size = header->imageSize;
	unsigned int decodedSize;

	if (header->magic != 0x64b2c8e5 || size < 0x2b0 || size >= 0x10000)
		return -1;
	if (header->firstPartSize >= size || header->secondPartSize >= size)
		return -1;
	if (header->firstPartSize + header->secondPartSize + 0x2b0 != size)
		return -1;
	decodedSize = header->decodedSize;
	if (FL_SINCE(0x20110616) && decodedSize >= 0x1bc00)
		return -1;
	if (decodedSize + size >= 0x1bc00 || header->signingKeyIndex >= 16 ||
	    header->keyIndex >= 6)
		return -1;
	return 0;
}

/* Copy whole words and return their sum. When size / 4 is zero the original
 * hardware loop counter wraps, so that case requires a full counter cycle.
 */
unsigned int copyWordsAndSum(unsigned int *dst, const unsigned int *src,
			     unsigned int size)
{
	unsigned int words = size >> 2;
	unsigned int sum = 0;

	do {
		unsigned int word = *src++;

		*dst++ = word;
		sum += word;
	} while (--words != 0);

	return sum;
}

void *fillBytes(void *dst, int value, unsigned int size)
{
	unsigned char *p = dst;
	unsigned int i;

	for (i = 0; i < size; i++)
		p[i] = value;
	return dst;
}

/* Returns the OR of all byte differences, not a lexicographic comparison. */
unsigned int compareBytes(const void *left, const void *right, unsigned int size)
{
	const unsigned char *a = left;
	const unsigned char *b = right;
	unsigned int difference = 0;
	unsigned int i;

	for (i = 0; i < size; i++)
		difference |= a[i] ^ b[i];
	return difference;
}

/* Read bits in little-endian bit order, as used by the card registers. */
unsigned int readBits(const unsigned char *data, unsigned int first,
		      unsigned int count)
{
	unsigned int result = 0;
	unsigned int i;

	data += first >> 3;
	first &= 7;
	for (i = 0; i < count; i++) {
		if (*data & (1U << first))
			result |= 1U << (i & 31);
		if (++first == 8) {
			first = 0;
			data++;
		}
	}
	return result;
}

static unsigned char slot8Seed[32] = {
	0x1e, 0xb2, 0x56, 0x43, 0x23, 0x4e, 0xf6, 0xef,
	0x38, 0x0f, 0x9e, 0x57, 0xa2, 0x16, 0xfa, 0x22,
	0xd8, 0x35, 0xff, 0x05, 0x08, 0xbe, 0xbb, 0xe9,
	0x76, 0x76, 0xfa, 0x47, 0x8c, 0x29, 0xc2, 0x1d,
};
static unsigned char slot9Seed[32] = {
	0x50, 0x97, 0x62, 0x38, 0xee, 0xd2, 0x32, 0x34,
	0xf9, 0xc7, 0x46, 0xcc, 0xb0, 0x7d, 0x6d, 0x71,
	0x3f, 0x5b, 0xa6, 0x7b, 0xc8, 0xc0, 0x97, 0xda,
	0xe5, 0x2d, 0x85, 0x12, 0x08, 0x46, 0x83, 0xe7,
};
static unsigned char slot8Iv[16] = {
	0xfd, 0x4f, 0xa8, 0xfa, 0x4f, 0xe7, 0x94, 0x30,
	0xa0, 0xca, 0x30, 0x5c, 0x88, 0xe5, 0x24, 0xdd,
};
static unsigned char slot9Iv[16] = {
	0x98, 0x6d, 0x5e, 0xe1, 0xe7, 0x75, 0x9f, 0x4f,
	0x52, 0xdb, 0x43, 0xf3, 0x3f, 0xa5, 0x96, 0x0a,
};

int prepareSecondLoaderKeys(unsigned int bootMode, const struct SecondLoaderHeader *header,
			    unsigned char *authentication)
{
	unsigned int address = (unsigned int)(unsigned long)authentication;
	int result;

	result = bigmacRunMode301(8, (unsigned int)(unsigned long)slot8Seed,
				  sizeof(slot8Seed), 0x206);
	fillBytes(slot8Seed, 0, sizeof(slot8Seed));
	if (result < 0)
		return result;
	if (bootMode & 0x20) {
		fillBytes(slot9Seed, 0, sizeof(slot9Seed));
		fillBytes(slot9Iv, 0, sizeof(slot9Iv));
		result = bigmacRunMode10a(address, address, header->decodedSize + 0x1e0,
					  8, (unsigned int)(unsigned long)slot8Iv);
		fillBytes(slot8Iv, 0, sizeof(slot8Iv));
	} else {
		fillBytes(slot8Iv, 0, sizeof(slot8Iv));
		result = bigmacRunMode301(9, (unsigned int)(unsigned long)slot9Seed,
					  sizeof(slot9Seed), 0x207);
		fillBytes(slot9Seed, 0, sizeof(slot9Seed));
		if (result < 0)
			return result;
		result = bigmacRunMode10a(address, address, header->decodedSize + 0x1e0,
					  9, (unsigned int)(unsigned long)slot9Iv);
		fillBytes(slot9Iv, 0, sizeof(slot9Iv));
	}
	if (result < 0)
		return result;
	KEYRING_COMMAND = 0x1c1e0008U;
	KEYRING_COMMAND = 0x1c1f0009U;
	KEYRING_COMMAND = 0x1c1f0206U;
	KEYRING_COMMAND = 0x1c1f0207U;
	return 0;
}

/* The image payload IV is erased immediately after the Bigmac operation. */
static unsigned char payloadIv[16] = {
	0xaf, 0x5f, 0x2c, 0xb0, 0x4a, 0xc1, 0x75, 0x1a,
	0xbf, 0x51, 0xce, 0xf1, 0xc8, 0x09, 0x62, 0x10,
};

int decryptSecondLoader(unsigned int output, unsigned int input, unsigned int size)
{
	int result;

	result = bigmacRunMode10a(output, input, size, 10,
				 (unsigned int)(unsigned long)payloadIv);
	fillBytes(payloadIv, 0, sizeof(payloadIv));
	if (result < 0)
		return result;
	KEYRING_COMMAND = 0x1c1f000aU;
	return 0;
}

static unsigned char keyIvs[6][16] = {
	{ 0xb0, 0x7f, 0xeb, 0x18, 0xe4, 0x22, 0x16, 0x7a, 0xec, 0xdb, 0x60, 0xa8, 0xee, 0x0c, 0xa4, 0x27 },
	{ 0x69, 0x42, 0x55, 0x51, 0xe9, 0xae, 0x39, 0x45, 0xf3, 0x68, 0x73, 0xf1, 0x10, 0xfd, 0xa6, 0xfb },
	{ 0x1c, 0x48, 0xb1, 0xc8, 0xa5, 0x99, 0xcd, 0x59, 0xc7, 0xae, 0xcf, 0x8e, 0x46, 0x98, 0x30, 0xbe },
	{ 0x7c, 0x59, 0x8f, 0xb4, 0x5b, 0xcf, 0x00, 0xa9, 0xf4, 0x9e, 0x5a, 0xfa, 0xd3, 0x55, 0x47, 0x54 },
	{ 0x98, 0xe3, 0xf5, 0x08, 0xd1, 0x6e, 0xf7, 0x8c, 0x8f, 0xae, 0x96, 0xf4, 0x16, 0x8f, 0x20, 0xa6 },
	{ 0x76, 0x64, 0x10, 0xae, 0x51, 0x09, 0xf2, 0x20, 0x1d, 0xda, 0xe4, 0x1f, 0xb0, 0x7f, 0x6e, 0xf6 },
};
static unsigned char derivedKeyIvs[5][16] = {
	{ 0x09, 0x62, 0x93, 0x9d, 0xc5, 0xbe, 0x56, 0x0f, 0x52, 0xf6, 0xbb, 0x45, 0xb3, 0x4c, 0xe0, 0x97 },
	{ 0xd4, 0x6c, 0xf8, 0x2f, 0xc8, 0xba, 0x5d, 0x5b, 0x64, 0x36, 0xf9, 0xf7, 0xe4, 0xe2, 0x04, 0x34 },
	{ 0xea, 0x57, 0x4c, 0x6c, 0xb6, 0xb1, 0x15, 0x05, 0xd5, 0xda, 0x35, 0x44, 0x6d, 0x18, 0x78, 0x08 },
	{ 0x8f, 0x62, 0xaa, 0xed, 0xbd, 0x4e, 0x02, 0x5b, 0xe6, 0xe8, 0xff, 0x8e, 0x7d, 0x65, 0x47, 0x04 },
	{ 0xc8, 0x98, 0x61, 0x0b, 0xf8, 0xe6, 0xcc, 0xc1, 0x2c, 0x9b, 0xa4, 0x84, 0x14, 0xcc, 0x2b, 0xcf },
};
static unsigned char digestIv[16] = {
	0xee, 0xb5, 0xec, 0x5c, 0xcf, 0x1c, 0xf4, 0x31,
	0x13, 0xf5, 0x21, 0x3a, 0xd5, 0xb8, 0xd6, 0xda,
};
static unsigned char expectedDigest[32];
static unsigned char actualDigest[32];

int verifySecondLoaderPayload(const struct SecondLoaderHeader *header,
			      const unsigned char *authentication,
			      const unsigned char *payload)
{
	unsigned int i;
	int result;

	result = bigmacRunMode30a(10, (unsigned int)(unsigned long)authentication, 32,
				  header->keyIndex + 0x208,
				  (unsigned int)(unsigned long)keyIvs[header->keyIndex]);
	fillBytes(keyIvs, 0, sizeof(keyIvs));
	if (result < 0)
		return result;
	for (i = 0; i < 5; i++) {
		result = bigmacRunMode30a(i + 11,
			(unsigned int)(unsigned long)(authentication + 0x20 + i * 32),
			32, header->keyIndex + 0x208,
			(unsigned int)(unsigned long)derivedKeyIvs[i]);
		fillBytes(derivedKeyIvs[i], 0, sizeof(derivedKeyIvs[i]));
		if (result < 0)
			return result;
	}
	for (i = 0; i < 6; i++)
		KEYRING_COMMAND = 0x1c1f0208U + i;
	result = bigmacRunMode30a((unsigned int)(unsigned long)expectedDigest,
				  (unsigned int)(unsigned long)(authentication + 0xc0),
				  32, 10, (unsigned int)(unsigned long)digestIv);
	fillBytes(digestIv, 0, sizeof(digestIv));
	if (result < 0)
		return result;
	result = bigmacRunMode33((unsigned int)(unsigned long)actualDigest,
				 (unsigned int)(unsigned long)payload, header->decodedSize, 0x20);
	if (result < 0)
		return result;
	KEYRING_COMMAND = 0x1c1f0020U;
	return compareBytes(expectedDigest, actualDigest, sizeof(expectedDigest)) ? -1 : 0;
}

static unsigned char signatureSeed[32] = {
	0x4a, 0x41, 0xde, 0xde, 0xff, 0xba, 0xc0, 0x4f,
	0x52, 0x60, 0x2d, 0x00, 0x34, 0xda, 0xd6, 0x66,
	0x16, 0x4e, 0x3c, 0xf1, 0xfc, 0x23, 0xbe, 0x4f,
	0x4d, 0x0f, 0x84, 0x71, 0xa7, 0xe6, 0x05, 0x63,
};
static const unsigned int signatureExponent[4] = { 0x10001, 0, 0, 0 };
static const unsigned char signatureDigestPrefix[18] = {
	0x30, 0x30, 0x30, 0x0c, 0x06, 0x08, 0x2a, 0x86, 0x48,
	0x86, 0xf7, 0x0d, 0x02, 0x09, 0x05, 0x00, 0x04, 0x20,
};
static const unsigned int fallbackModulus[64] = {
	0xe9d7d33bU, 0x59f3178aU, 0xf93a9ef1U, 0xe9952b80U,
	0xa86c94d6U, 0x58363190U, 0x889cfc2eU, 0x3d1ef0f4U,
	0xfae20131U, 0x231a5433U, 0xf0454e1eU, 0xc636855eU,
	0x0eb6694dU, 0x4d00b3f4U, 0xbf72ad23U, 0x95861b80U,
	0xbe43a8eeU, 0xa6625858U, 0xe72b6b3dU, 0xa85500f9U,
	0xcaae90c6U, 0xea853bd6U, 0xbb798a11U, 0x6f1510d5U,
	0x62b7d353U, 0x8fc11138U, 0x4a362908U, 0x6c1afbbaU,
	0x201f849dU, 0x963188a0U, 0x645b0d3cU, 0x470936fbU,
	0x8a746c20U, 0x1517dbefU, 0x4dca6b89U, 0x22ee8029U,
	0x3f24c3b0U, 0x2a3f3cb7U, 0x588c860aU, 0x7ab7577eU,
	0x982bbefeU, 0x42d64630U, 0x1923728cU, 0xc6a598a5U,
	0x4e9cfb0eU, 0x93f7dc9fU, 0xeecda3dfU, 0x49a2c8c6U,
	0x31256542U, 0x706d6d70U, 0x660c0b1bU, 0x41d2790eU,
	0xb8e1389dU, 0x9fbd76e9U, 0x1ecdd4b4U, 0x861e466fU,
	0xfac695b2U, 0x92c7d110U, 0xc066b27cU, 0xcc25308bU,
	0x7e83a476U, 0x8b20d8f0U, 0x595030baU, 0xbcda3de2U,
};
static unsigned int signatureDigest[8];

int verifySecondLoaderSignature(const struct SecondLoaderHeader *header,
				const unsigned char *authentication)
{
	const volatile unsigned int *modulus;
	unsigned int decodedWords[64];
	unsigned char *decoded = (unsigned char *)decodedWords;
	unsigned int keyMask;
	unsigned int i;
	int result;

	result = bigmacRunMode301(0x20, (unsigned int)(unsigned long)signatureSeed,
				  sizeof(signatureSeed), 0x344);
	fillBytes(signatureSeed, 0, sizeof(signatureSeed));
	if (result < 0)
		return result;
	KEYRING_COMMAND = 0x1c1f0344U;
	result = bigmacRunMode33((unsigned int)(unsigned long)signatureDigest,
				 (unsigned int)(unsigned long)header,
				 header->imageSize - 0x100, 0x20);
	if (result < 0)
		return result;
	keyMask = *(volatile unsigned int *)0xe0064060UL;
	if (keyMask & 0xffffU) {
		if (!(keyMask & (1U << header->signingKeyIndex)))
			return -1;
		modulus = (const volatile unsigned int *)(0xe0066000UL +
							 ((header->signingKeyIndex * 8U & 0xffU) * 32U));
	} else {
		modulus = fallbackModulus;
	}
	result = bignumPowmod(decodedWords, (const unsigned int *)(authentication + 0xe0),
			      modulus, signatureExponent, 1);
	if (result < 0 || decoded[0] != 0 || decoded[1] != 1)
		return -1;
	for (i = 2; i < 0xcd; i++) {
		if (decoded[i] != 0xff)
			return -1;
	}
	if (decoded[0xcd] != 0 ||
	    compareBytes(decoded + 0xce, signatureDigestPrefix, sizeof(signatureDigestPrefix)) ||
	    compareBytes(decoded + 0xe0, signatureDigest, sizeof(signatureDigest)))
		return -1;
	for (i = 0; i < 128; i++)
		KEYRING_COMMAND = 0x10000700U | i;
	return 0;
}

int processSecondLoader(unsigned int bootMode, unsigned char *destination,
			struct SecondLoaderHeader *header, unsigned int *bootArgument)
{
	unsigned char *image = (unsigned char *)header;
	struct KeyPolicy *policy = (struct KeyPolicy *)(image + sizeof(*header) +
						       header->firstPartSize);
	unsigned char *records = (unsigned char *)(policy + 1);
	unsigned char *authentication = records + header->secondPartSize;
	unsigned char *payload = authentication + 0x1e0;
	unsigned int keyIndex;
	unsigned int decodedSize;
	int result;

	result = checkSecondLoaderHeader(header);
	if (result < 0)
		return result;
	result = prepareSecondLoaderKeys(bootMode, header, authentication);
	if (result < 0)
		return result;
	result = verifySecondLoaderSignature(header, authentication);
	if (result < 0)
		return result;
	result = verifySecondLoaderPayload(header, authentication, payload);
	if (result < 0)
		return result;
	applyKeyPolicy(policy, records, header->secondPartSize);
	keyIndex = header->keyIndex;
	decodedSize = header->decodedSize;
	result = decryptSecondLoader((unsigned int)(unsigned long)destination,
				     (unsigned int)(unsigned long)payload, decodedSize);
	if (result < 0)
		return result;
	result = bigmacClearMemory((unsigned int)(unsigned long)(destination + decodedSize),
				   0, 0x1c000 - decodedSize);
	if (result < 0)
		return result;
	bigmacClearRegisters();
	*bootArgument = keyIndex;
	return 0;
}
